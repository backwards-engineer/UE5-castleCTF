#include "RGGameMode.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "RGArena.h"
#include "RGBotController.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGGameState.h"
#include "RGHUD.h"
#include "RGWorldActors.h"
#include "RGOutpost.h"
#include "TimerManager.h"
#include "UnrealClient.h"
ARGGameMode::ARGGameMode()
{
    GameStateClass = ARGGameState::StaticClass();
    PlayerStateClass = ARGPlayerState::StaticClass();
    PlayerControllerClass = ARGPlayerController::StaticClass();
    DefaultPawnClass = ARGCharacter::StaticClass();
    HUDClass = ARGHUD::StaticClass();
}
void ARGGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (!ClassCatalog)
        ClassCatalog = LoadObject<URGClassCatalog>(nullptr, TEXT("/Game/Crownfall/Data/DA_Classes.DA_Classes"));
    bool Found = false;
    for (TActorIterator<ARGArena> It(GetWorld()); It; ++It)
        Found = true;
    if (!Found)
    {
        ARGArena* Arena = GetWorld()->SpawnActorDeferred<ARGArena>(ARGArena::StaticClass(), FTransform::Identity);
        Arena->Catalog = ClassCatalog;
        UGameplayStatics::FinishSpawningActor(Arena, FTransform::Identity);
    }
    ARGGameState* S = GetGameState<ARGGameState>();
    S->CaptureTarget = CapturesToWin;
    S->Phase = ERGPhase::Countdown;
    S->SecondsRemaining = CountdownSeconds;
    GetWorldTimerManager().SetTimer(MatchTimer, this, &ARGGameMode::MatchStep, 1, true);
    GetWorldTimerManager().SetTimer(CaptureTimer, this, &ARGGameMode::CheckCaptures, .2f, true);
    StartBots();
    if (FParse::Param(FCommandLine::Get(),TEXT("FrontierSelfTest")))
    {
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle,this,&ARGGameMode::RunFrontierTest,2,false);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("MotionSelfTest")))
    {
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, this, &ARGGameMode::RunMotionTest, 2, false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("RestartSelfTest")))
    {
        static bool bTravelRequested=false;
        if(!bTravelRequested) { bTravelRequested=true; S->Red.Captures=CapturesToWin; Win(ERGTeam::Red); }
        else
        {
            const bool Pass=S->Phase==ERGPhase::Countdown && S->Red.Captures==0 && S->Blue.Captures==0;
            FFileHelper::SaveStringToFile(Pass?TEXT("PASS Victory automatically travels to a fresh match after the configured delay"):TEXT("FAIL Match did not reset"),*(FPaths::ProjectSavedDir()/TEXT("Validation/Restart.txt")));
            FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
        }
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("AbilitySelfTest")))
    {
        FTimerHandle Handle;
        GetWorldTimerManager().SetTimer(Handle, this, &ARGGameMode::RunAbilityTest, 2, false);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("RelicPreview")))
    {
        FTimerHandle Shot, Exit;
        GetWorldTimerManager().SetTimer(Shot,
                                        FTimerDelegate::CreateWeakLambda(this,
                                                                         [this]() {
                                                                             FScreenshotRequest::RequestScreenshot(
                                                                                 FPaths::ProjectSavedDir() /
                                                                                     TEXT("Validation/Gameplay.png"),
                                                                                 true, false);
                                                                         }),
                                        18, false);
        GetWorldTimerManager().SetTimer(
            Exit, FTimerDelegate::CreateWeakLambda(this, []() { FPlatformMisc::RequestExit(false); }), 22, false);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("RelicSelfTest")))
    {
        FTimerHandle H;
        GetWorldTimerManager().SetTimer(H, this, &ARGGameMode::RunSelfTest, 2, false);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BotSoak")))
    {
        FTimerHandle H;
        GetWorldTimerManager().SetTimer(H, this, &ARGGameMode::ReportBotSoak, 100, false);
    }
}
ERGTeam ARGGameMode::BalancedTeam() const
{
    int32 R = 0, B = 0;
    for (TActorIterator<ARGCharacter> It(GetWorld()); It; ++It)
        if (It->IsAlive())
        {
            if (It->Team == ERGTeam::Red)
                ++R;
            else if (It->Team == ERGTeam::Blue)
                ++B;
        }
    return R <= B ? ERGTeam::Red : ERGTeam::Blue;
}
void ARGGameMode::PostLogin(APlayerController* PC)
{
    Super::PostLogin(PC);
}
void ARGGameMode::HandleStartingNewPlayer_Implementation(APlayerController* PC)
{
    const ERGTeam T = BalancedTeam();
    if (ARGPlayerState* PS = PC->GetPlayerState<ARGPlayerState>())
        PS->Team = T;
    SpawnFighter(PC, T);
}
ARGOutpost* ARGGameMode::ForwardOutpost(ERGTeam T) const
{
    ARGOutpost* Best=nullptr;
    float Progress=-MAX_flt;
    for (TActorIterator<ARGOutpost> It(GetWorld());It;++It)
        if (It->CanSpawn(T) && -RG::Side(T)*It->GetActorLocation().X>Progress)
        { Best=*It; Progress=-RG::Side(T)*It->GetActorLocation().X; }
    return Best;
}
ARGCharacter* ARGGameMode::SpawnFighter(AController* C, ERGTeam T, bool bForwardSpawn)
{
    if (!IsValid(C))
        return nullptr;
    const auto Previous=C->GetPlayerState<ARGPlayerState>();
    const ERGClass Last=Previous?Previous->LastClass:ERGClass::Peasant;
    const auto Bot=Cast<ARGBotController>(C);
    ARGOutpost* Outpost=bForwardSpawn && !(Bot && Bot->BotRole==ERGBotRole::Worker) ? ForwardOutpost(T) : nullptr;
    const FVector Location=Outpost?Outpost->SpawnLocation(T):FVector(RG::Side(T)*RG::SpawnX,-50,100);
    // Reserve enough room for each arriving fighter instead of stacking capsules.
    FVector SpawnPosition=Location;
    for(int32 Slot=0;Slot<15;++Slot)
    {
        const FVector Candidate=Location+FVector(RG::Side(T)*(Slot%3)*90,((Slot/3)%5-2)*90,0);
        bool Occupied=false;
        for(TActorIterator<ARGCharacter> It(GetWorld());It;++It)
            if(It->IsAlive() && FVector::DistSquared2D(It->GetActorLocation(),Candidate)<FMath::Square(85.f)) {Occupied=true;break;}
        if(!Occupied) {SpawnPosition=Candidate;break;}
    }
    FTransform Transform(FRotator(0, T == ERGTeam::Red ? 0 : 180, 0),SpawnPosition);
    ARGCharacter* P = GetWorld()->SpawnActorDeferred<ARGCharacter>(
        ARGCharacter::StaticClass(), Transform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    P->Team = T;
    P->Classes->Catalog = ClassCatalog;
    P->bBot = Cast<ARGBotController>(C) != nullptr;
    P->ProtectedUntil = GetWorld()->GetTimeSeconds() + 3;
    UGameplayStatics::FinishSpawningActor(P, Transform);
    C->Possess(P);
    P->Classes->SetClass(Outpost?Last:ERGClass::Peasant);
    if (P->bBot)
    {
        P->GetCharacterMovement()->bUseRVOAvoidance = true;
        P->GetCharacterMovement()->AvoidanceConsiderationRadius = 220;
    }
    if (ARGPlayerState* PS = C->GetPlayerState<ARGPlayerState>())
    {
        PS->Team = T;
        PS->SpawnName=Outpost?Outpost->Name:TEXT("Castle");
    }
    return P;
}
void ARGGameMode::Respawn(AController* C, ERGTeam T)
{
    if (!C)
        return;
    FTimerHandle Handle;
    TWeakObjectPtr<AController> Weak = C;
    GetWorldTimerManager().SetTimer(Handle,
                                    FTimerDelegate::CreateWeakLambda(this,
                                                                     [this, Weak, T]() {
                                                                         if (Weak.IsValid())
                                                                             SpawnFighter(Weak.Get(), T, true);
                                                                     }),
                                    RespawnSeconds, false);
}
void ARGGameMode::StartBots()
{
    if (FParse::Param(FCommandLine::Get(), TEXT("NoBots")))
        return;
    int32 TeamSlots[2]={0,0};
    const ERGClass Roster[]={ERGClass::Worker,ERGClass::Knight,ERGClass::Mage,ERGClass::Archer,ERGClass::Priest,ERGClass::Ninja,ERGClass::Worker,ERGClass::Mage,ERGClass::Priest};
    for (int I = 0; I < BotCount; ++I)
    {
        ARGBotController* C = GetWorld()->SpawnActor<ARGBotController>();
        C->Team = BalancedTeam();
        const int32 Slot=TeamSlots[C->Team==ERGTeam::Red?0:1]++%9;
        C->PreferredClass=Roster[Slot];
        C->BotRole=C->PreferredClass==ERGClass::Worker?ERGBotRole::Worker:C->PreferredClass==ERGClass::Priest?ERGBotRole::Support:Slot==1?ERGBotRole::Defender:ERGBotRole::Attacker;
        SpawnFighter(C, C->Team);
    }
}
void ARGGameMode::MatchStep()
{
    ARGGameState* S = GetGameState<ARGGameState>();
    if (!S)
        return;
    S->SecondsRemaining = FMath::Max(0, S->SecondsRemaining - 1);
    if (S->SecondsRemaining > 0)
        return;
    if (S->Phase == ERGPhase::Countdown)
    {
        S->Phase = ERGPhase::Playing;
        S->SecondsRemaining = MatchSeconds;
        S->Announce(TEXT("TAKE THEIR RELIC. DEFEND YOUR OWN."));
    }
    else if (S->Phase == ERGPhase::Playing)
    {
        if (S->Red.Captures == S->Blue.Captures)
        {
            S->SecondsRemaining = 60;
            S->Announce(TEXT("SUDDEN DEATH / NEXT CAPTURE WINS"));
            S->CaptureTarget = S->Red.Captures + 1;
        }
        else
            Win(S->Red.Captures > S->Blue.Captures ? ERGTeam::Red : ERGTeam::Blue);
    }
    else if (S->Phase == ERGPhase::Victory)
    {
        S->Phase = ERGPhase::Restarting;
        GetWorld()->ServerTravel(TEXT("/Game/Crownfall/Maps/Crownfall"));
    }
}
void ARGGameMode::CheckCaptures()
{
    ARGGameState* S = GetGameState<ARGGameState>();
    if (!S || !S->IsPlaying())
        return;
    for (TActorIterator<ARGCharacter> It(GetWorld()); It; ++It)
    {
        ARGCarryable* Item = It->Carry->Carried;
        if (!It->IsAlive() || !Item || !Item->bRelic || Item->Team == It->Team)
            continue;
        ERGRelicState Own = It->Team == ERGTeam::Red ? S->RedRelic : S->BlueRelic;
        if (Own != ERGRelicState::Safe ||
            FVector::Dist2D(It->GetActorLocation(), FVector(RG::Side(It->Team) * RG::RelicX, 530, 0)) > 210)
            continue;
        S->Stock(It->Team).Captures++;
        Item->ReturnHome();
        S->Announce(RG::TeamName(It->Team) + TEXT(" CAPTURED THE RELIC!"), It->Team);
        It->MulticastFeedback(It->GetActorLocation() + FVector(0, 0, 120), FLinearColor(1, .8f, .15f), 0);
        if (S->Stock(It->Team).Captures >= S->CaptureTarget)
        {
            Win(It->Team);
            break;
        }
    }
}
void ARGGameMode::Win(ERGTeam T)
{
    ARGGameState* S = GetGameState<ARGGameState>();
    S->Winner = T;
    S->Phase = ERGPhase::Victory;
    S->SecondsRemaining = VictorySeconds;
    S->Announce(RG::TeamName(T) + TEXT(" VICTORY"), T);
}
ARGPlayerController::ARGPlayerController()
{
    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Crosshairs;
}
void ARGPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController())
    {
        ApplyPlayerSettings();
        NetworkFailureHandle=GEngine->OnNetworkFailure().AddWeakLambda(this,[this](UWorld* World,UNetDriver*,ENetworkFailure::Type,const FString& Error) {
            if (World==GetWorld()) { MenuMessage=TEXT("Connection failed: ")+Error; ShowMenu(); }
        });
        if (GetWorld()->GetAuthGameMode<ARGFrontEndGameMode>())
        {
            ShowMenu();
            return;
        }
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
        SetInputMode(Mode);
    }
}
void ARGPlayerController::HostMatch()
{
    UGameplayStatics::OpenLevel(this, TEXT("/Game/Crownfall/Maps/Crownfall"), true, TEXT("listen"));
}
void ARGPlayerController::JoinMatch(const FString& Address)
{
    if (!Address.IsEmpty() && !Address.Contains(TEXT("?")))
        ClientTravel(Address, TRAVEL_Absolute);
}
