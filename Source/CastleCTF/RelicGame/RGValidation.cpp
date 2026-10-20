#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RGBotController.h"
#include "Camera/CameraComponent.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGGameMode.h"
#include "RGGameState.h"
#include "RGWorldActors.h"
#include "RGOutpost.h"
#include "TimerManager.h"

void ARGGameMode::RunAbilityTest()
{
    ARGGameState* State = GetGameState<ARGGameState>();
    State->Phase = ERGPhase::Playing;
    State->SecondsRemaining = 900;
    // AI controllers activate authoritative movement without requiring a local viewport/player connection.
    ARGCharacter* Source = SpawnFighter(GetWorld()->SpawnActor<AAIController>(), ERGTeam::Red);
    ARGCharacter* Target = SpawnFighter(GetWorld()->SpawnActor<AAIController>(), ERGTeam::Blue);
    TSharedRef<int32> Stage = MakeShared<int32>(0);
    TSharedRef<TArray<FString>> Results = MakeShared<TArray<FString>>();
    TSharedRef<int32> Failures = MakeShared<int32>(0);
    TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
    auto Check = [Results, Failures](const TCHAR* Name, bool Pass) {
        Results->Add(FString(Pass ? TEXT("PASS ") : TEXT("FAIL ")) + Name);
        if (!Pass)
            ++*Failures;
        UE_LOG(LogTemp, Display, TEXT("[CrownfallAbilities] %s"), *Results->Last());
    };
    GetWorldTimerManager().SetTimer(
        *Handle,
        FTimerDelegate::CreateWeakLambda(
            this,
            [this, Source, Target, Stage, Results, Failures, Handle, Check]() {
                const int32 N = *Stage;
                ARGGameState* S = GetGameState<ARGGameState>();
                if (N > 0 && N <= 6)
                {
                    const TCHAR* Names[] = {
                        TEXT("Arrow projectile damages target"), TEXT("Blunderbuss pellets damage target"),
                        TEXT("Fire circle damages target"),         TEXT("Ice circle damages and slows target"),
                        TEXT("Priest circle heals caster"),     TEXT("Dark Priest circle drains life")};
                    bool Pass = Target->Health->Health < 500;
                    if (N==5) Pass=Source->Health->Health>Source->Health->MaxHealth-25 && Target->Health->Health==500;
                    if (N == 4)
                        Pass &= Target->Combat->SlowUntil > GetWorld()->GetTimeSeconds();
                    if (N == 6)
                        Pass &= Source->Health->Health > Source->Health->MaxHealth - 25;
                    Check(Names[N - 1], Pass);
                }
                if (N < 6)
                {
                    const ERGClass Class = N < 2 ? ERGClass::Archer : N < 4 ? ERGClass::Mage : ERGClass::Priest;
                    S->Red.Upgrades = N % 2 ? (1 << static_cast<uint8>(Class)) : 0;
                    Source->Classes->SetClass(Class);
                    Source->Combat->NextAttackTime = 0;
                    Source->Health->Health = Source->Health->MaxHealth - 25;
                    Source->SetActorLocation(FVector(-550, 0, 90));
                    Target->SetActorLocation(FVector(-150, 0, 90));
                    Source->GetCharacterMovement()->StopMovementImmediately();
                    Target->GetCharacterMovement()->StopMovementImmediately();
                    Target->Health->MaxHealth = 500;
                    Target->Health->Health = 500;
                    Target->ProtectedUntil = 0;
                    Target->Combat->SlowUntil = 0;
                    Source->AimDirection = FVector::ForwardVector;
                    Source->Combat->Primary();
                }
                else if (N == 6)
                {
                    S->Red.Upgrades = 0;
                    Source->Classes->SetClass(ERGClass::Priest);
                    Source->Health->Health = 30;
                    Source->Combat->NextAbilityTime = 0;
                    Source->Combat->Secondary(true);
                    Check(TEXT("Priest secondary heals nearby ally/self"), Source->Health->Health > 30);
                    const float Next = Source->Combat->NextAbilityTime;
                    Source->Combat->Secondary(true);
                    Check(TEXT("Ability cooldown prevents repeat cast"), Source->Combat->NextAbilityTime == Next);
                    Source->Classes->SetClass(ERGClass::Knight);
                    Source->Combat->Secondary(true);
                    Source->SetActorRotation(FRotator::ZeroRotator);
                    Source->ProtectedUntil = 0;
                    float Before = Source->Health->Health;
                    Source->Health->Damage(25, Target, -FVector::ForwardVector);
                    Check(TEXT("Facing Knight block reduces incoming damage"),
                          FMath::IsNearlyEqual(Before - Source->Health->Health, 5.f));
                    Source->Combat->Secondary(false);
                    Check(TEXT("Releasing block clears defense"), !Source->Combat->bBlocking);
                    Source->Classes->SetClass(ERGClass::Ninja);
                    Source->Combat->NextAbilityTime = 0;
                    Source->SetActorLocation(FVector(-550, 0, 90));
                    Source->Combat->Secondary(true);
                    UE_LOG(LogTemp,Display,TEXT("[DashProbe] Start %s dir %s cooldown %.2f mode %d"),*Source->GetActorLocation().ToCompactString(),*Source->AimDirection.ToCompactString(),Source->Combat->NextAbilityTime,(int32)Source->GetCharacterMovement()->MovementMode);
                }
                else
                {
                    UE_LOG(LogTemp,Display,TEXT("[DashProbe] End %s velocity %s mode %d"),*Source->GetActorLocation().ToCompactString(),*Source->GetVelocity().ToCompactString(),(int32)Source->GetCharacterMovement()->MovementMode);
                    Check(TEXT("Ninja dash travels through battlefield"), Source->GetActorLocation().X > -300);
                    Check(TEXT("Generated character animated head stays above feet"),
                          Source->GetMesh()->GetSocketLocation(TEXT("head")).Z >
                              Source->GetMesh()->GetComponentLocation().Z + 50);
                    Check(TEXT("Elevated camera keeps battlefield angle"),
                          FMath::IsNearlyEqual(Source->Camera->GetComponentRotation().Pitch, -42.f, 1.f));
                    FFileHelper::SaveStringArrayToFile(*Results,
                                                       *(FPaths::ProjectSavedDir() / TEXT("Validation/Abilities.txt")));
                    UE_LOG(LogTemp, Display, TEXT("[CrownfallAbilities] COMPLETE: %d checks, %d failures"),
                           Results->Num(), *Failures);
                    GetWorldTimerManager().ClearTimer(*Handle);
                    FPlatformMisc::RequestExitWithStatus(false, *Failures ? 1 : 0);
                }
                ++*Stage;
            }),
        1.f, true);
}

void ARGGameMode::ReportBotSoak()
{
    TArray<FString> Lines;
    ARGGameState* S = GetGameState<ARGGameState>();
    Lines.Add(FString::Printf(TEXT("Team stock: Red W%d O%d / Blue W%d O%d; captures %d:%d"), S->Red.Wood, S->Red.Ore,
                              S->Blue.Wood, S->Blue.Ore, S->Red.Captures, S->Blue.Captures));
    for (TActorIterator<ARGBotController> It(GetWorld()); It; ++It)
    {
        ARGCharacter* P = Cast<ARGCharacter>(It->GetPawn());
        ARGPlayerState* PS = It->GetPlayerState<ARGPlayerState>();
        if (P)
            Lines.Add(FString::Printf(TEXT("%s role%d class%s position%s HP%.0f Pack %d/%d K%d D%d target%s"),
                                      *RG::TeamName(P->Team), (int32)It->BotRole, *RG::ClassName(P->Classes->ClassID,P->Classes->bUpgraded),
                                      *P->GetActorLocation().ToCompactString(), P->Health->Health, P->PackWood,
                                      P->PackOre, PS ? PS->Kills : 0, PS ? PS->Deaths : 0,
                                      *It->Destination.ToCompactString()));
    }
    for(TActorIterator<ARGOutpost> It(GetWorld());It;++It)
        Lines.Add(FString::Printf(TEXT("Outpost %s owner%d control%.2f contested%d"),*It->Name,(int)It->Team,It->Control,It->bContested));
    for(TActorIterator<ARGStation> It(GetWorld());It;++It)
        if(It->Kind==ERGStationType::Gate) Lines.Add(FString::Printf(TEXT("Gate %s health%.0f"),*RG::TeamName(It->Team),It->Health));
    FFileHelper::SaveStringArrayToFile(Lines, *(FPaths::ProjectSavedDir() / TEXT("Validation/BotSoak.txt")));
    FPlatformMisc::RequestExit(false);
}

void ARGGameMode::RunSelfTest()
{
    ARGGameState* S = GetGameState<ARGGameState>();
    S->Phase = ERGPhase::Playing;
    TSharedRef<TArray<FString>> Results = MakeShared<TArray<FString>>();
    TSharedRef<int32> Failures = MakeShared<int32>(0);
    auto Check = [Results, Failures](const TCHAR* Name, bool Pass) {
        Results->Add(FString(Pass ? TEXT("PASS ") : TEXT("FAIL ")) + Name);
        if (!Pass)
            ++*Failures;
        UE_LOG(LogTemp, Display, TEXT("[CrownfallTest] %s %s"), Pass ? TEXT("PASS") : TEXT("FAIL"), Name);
    };
    ARGPlayerController* RC = GetWorld()->SpawnActor<ARGPlayerController>();
    ARGPlayerController* BC = GetWorld()->SpawnActor<ARGPlayerController>();
    ARGCharacter* R = SpawnFighter(RC, ERGTeam::Red);
    ARGCharacter* B = SpawnFighter(BC, ERGTeam::Blue);
    Check(TEXT("Spawn as Peasant with custom skeletal mesh"),
          R && R->Classes->ClassID == ERGClass::Peasant && R->GetMesh()->GetSkeletalMeshAsset() != nullptr);
    auto FindStation = [this](ERGStationType Kind, ERGTeam Team, ERGClass Class) -> ARGStation* {
        for (TActorIterator<ARGStation> It(GetWorld()); It; ++It)
            if (It->Kind == Kind && It->Team == Team &&
                (Kind != ERGStationType::Hat && Kind != ERGStationType::Upgrade || It->ClassID == Class))
                return *It;
        return nullptr;
    };
    ARGStation* Hat = FindStation(ERGStationType::Hat, ERGTeam::Red, ERGClass::Knight);
    R->SetActorLocation(Hat->GetActorLocation() + FVector(120, 0, 60));
    Hat->Interact(R);
    Check(TEXT("Knight hat interaction changes class and custom mesh"),
          R->Classes->ClassID == ERGClass::Knight &&
              R->GetMesh()->GetSkeletalMeshAsset() == ClassCatalog->Get(ERGClass::Knight).BodyMesh);
    B->SetActorLocation(R->GetActorLocation() + FVector(110, 0, 0));
    R->AimDirection = FVector::ForwardVector;
    B->ProtectedUntil = 0;
    R->ProtectedUntil = 0;
    float Before = B->Health->Health;
    R->Combat->Primary();
    Check(TEXT("Knight melee damages enemy"), B->Health->Health < Before);
    Before = B->Health->Health;
    R->Combat->Primary();
    Check(TEXT("Attack cooldown rejects repeated hit"), B->Health->Health == Before);
    Before = R->Health->Health;
    R->Health->Damage(50, R, FVector::ForwardVector);
    Check(TEXT("Friendly damage is rejected"), R->Health->Health == Before);
    ARGCarryable* BlueRelic = nullptr;
    ARGCarryable* RedRelic = nullptr;
    for (TActorIterator<ARGCarryable> It(GetWorld()); It; ++It)
        if (It->bRelic)
        {
            if (It->Team == ERGTeam::Blue)
                BlueRelic = *It;
            else
                RedRelic = *It;
        }
    R->SetActorLocation(BlueRelic->Home + FVector(70, 0, 0));
    Check(TEXT("Enemy relic pickup"), R->Carry->Pickup(BlueRelic));
    R->RefreshSpeed();
    Check(TEXT("Carrier moves slower and cannot attack"),
          R->GetCharacterMovement()->MaxWalkSpeed < R->Classes->Definition().MoveSpeed && !R->Combat->CanAct());
    R->SetActorLocation(RedRelic->Home);
    CheckCaptures();
    Check(TEXT("Capture increments score and resets relic"),
          S->Red.Captures == 1 && BlueRelic->State == ERGRelicState::Safe && !R->Carry->Carried);
    R->SetActorLocation(BlueRelic->Home + FVector(60, 0, 0));
    R->Carry->Pickup(BlueRelic);
    R->Carry->Drop();
    Check(TEXT("Manual drop detaches relic"),
          BlueRelic->State == ERGRelicState::Dropped && !BlueRelic->Carrier && !R->Carry->Carried);
    B->SetActorLocation(BlueRelic->GetActorLocation() + FVector(50, 0, 0));
    B->LastInteract = 0;
    B->Interact();
    Check(TEXT("Owner returns dropped relic"), BlueRelic->State == ERGRelicState::Safe);
    R->SetActorLocation(BlueRelic->Home + FVector(60, 0, 0));
    R->Carry->Pickup(BlueRelic);
    R->Health->Damage(500, B, FVector::ForwardVector);
    Check(TEXT("Death automatically drops carried relic"),
          R->Health->bDead && BlueRelic->State == ERGRelicState::Dropped && !BlueRelic->Carrier);
    BlueRelic->ReturnHome();
    B->Classes->SetClass(ERGClass::Worker);
    ARGStation* Wood = FindStation(ERGStationType::Wood, ERGTeam::Neutral, ERGClass::Worker);
    B->SetActorLocation(Wood->GetActorLocation() + FVector(130, 0, 0));
    Wood->Hit(B, 20);
    Check(TEXT("Worker harvests resources"), B->PackWood == Wood->Yield);
    ARGStation* Deposit = FindStation(ERGStationType::Deposit, ERGTeam::Blue, ERGClass::Worker);
    B->SetActorLocation(Deposit->GetActorLocation() + FVector(110, 0, 60));
    Deposit->Interact(B);
    Check(TEXT("Deposit credits team stockpile"), B->PackWood == 0 && S->Blue.Wood == Wood->Yield);
    const FRGClassDefinition& Knight = ClassCatalog->Get(ERGClass::Knight);
    Check(TEXT("Insufficient resources cannot buy upgrade"), !S->BuyUpgrade(ERGTeam::Blue, ERGClass::Knight, Knight));
    S->Blue.Wood = 60;
    S->Blue.Ore = 80;
    Check(TEXT("Team upgrade debits exact cost"), S->BuyUpgrade(ERGTeam::Blue, ERGClass::Knight, Knight) &&
                                                      S->Blue.Wood == 60 - Knight.WoodCost &&
                                                      S->Blue.Ore == 80 - Knight.OreCost);
    Check(TEXT("Duplicate upgrade cannot spend resources"), !S->BuyUpgrade(ERGTeam::Blue, ERGClass::Knight, Knight));
    B->Classes->SetClass(ERGClass::Knight);
    Check(TEXT("Upgraded class uses pike loadout"),
          B->Classes->bUpgraded && B->GetMesh()->GetSkeletalMeshAsset() == Knight.UpgradeBodyMesh);
    ARGStation* Gate = FindStation(ERGStationType::Gate, ERGTeam::Blue, ERGClass::Worker);
    Gate->Hit(R, 500); // dead attackers must not damage structures
    Check(TEXT("Dead attacker cannot damage gate"), Gate->Health == Gate->MaxHealth);
    Gate->Health = 0;
    Gate->RefreshVisual();
    B->Classes->SetClass(ERGClass::Worker);
    B->SetActorLocation(Gate->GetActorLocation() + FVector(100, 0, 0));
    Gate->Interact(B);
    Check(TEXT("Worker repairs destroyed gate using team wood"), Gate->Health > 0);
    for (int32 I = 0; I < 7; ++I)
    {
        B->Classes->SetClass(static_cast<ERGClass>(I));
        Check(*FString::Printf(TEXT("Class %d has compatible generated body and idle/run clips"), I),
              B->GetMesh()->GetSkeletalMeshAsset() != nullptr && B->Classes->Definition().IdleAnimation &&
                  B->Classes->Definition().RunAnimation);
    }
    FTimerHandle Finish;
    GetWorldTimerManager().SetTimer(
        Finish,
        FTimerDelegate::CreateWeakLambda(
            this,
            [this, RC, Results, Failures, Check]() {
                ARGCharacter* Respawned = Cast<ARGCharacter>(RC->GetPawn());
                Check(TEXT("Death respawns controller as living Peasant"),
                      Respawned && Respawned->IsAlive() && Respawned->Classes->ClassID == ERGClass::Peasant);
                ARGGameState* GS = GetGameState<ARGGameState>();
                GS->Red.Captures = GS->CaptureTarget;
                Win(ERGTeam::Red);
                Check(TEXT("Victory state and restart countdown"), GS->Winner == ERGTeam::Red &&
                                                                       GS->Phase == ERGPhase::Victory &&
                                                                       GS->SecondsRemaining == VictorySeconds);
                FFileHelper::SaveStringArrayToFile(*Results,
                                                   *(FPaths::ProjectSavedDir() / TEXT("Validation/CoreLoop.txt")));
                UE_LOG(LogTemp, Display, TEXT("[CrownfallTest] COMPLETE: %d checks, %d failures"), Results->Num(),
                       *Failures);
                FPlatformMisc::RequestExitWithStatus(false, *Failures == 0 ? 0 : 1);
            }),
        RespawnSeconds + 1, false);
}
