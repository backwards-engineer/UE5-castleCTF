#include "RGEditorValidation.h"
#if WITH_EDITOR
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PlayInEditorDataTypes.h"
#include "RGArena.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGGameMode.h"
#include "RGGameState.h"
#include "RGWorldActors.h"
#include "RGOutpost.h"
#include "Settings/LevelEditorPlaySettings.h"

namespace
{
struct FPIECheck : TSharedFromThis<FPIECheck>
{
    int32 Stage = 0, Failures = 0;
    double Started = FPlatformTime::Seconds(), Next = 0;
    TArray<FString> Results;
    ERGTeam ClientTeam = ERGTeam::Neutral;
    void Check(const TCHAR* Name, bool Pass)
    {
        Results.Add(FString(Pass ? TEXT("PASS ") : TEXT("FAIL ")) + Name);
        if (!Pass)
            ++Failures;
        UE_LOG(LogTemp, Display, TEXT("[CrownfallPIE] %s"), *Results.Last());
    }
    ARGStation* Station(UWorld* W, ERGStationType Kind, ERGTeam Team, ERGClass Class = ERGClass::Worker)
    {
        for (TActorIterator<ARGStation> It(W); It; ++It)
            if (It->Kind == Kind && It->Team == Team &&
                ((Kind != ERGStationType::Hat && Kind != ERGStationType::Upgrade) || It->ClassID == Class))
                return *It;
        return nullptr;
    }
    ARGCarryable* Relic(UWorld* W, ERGTeam Team)
    {
        for (TActorIterator<ARGCarryable> It(W); It; ++It)
            if (It->bRelic && It->Team == Team)
                return *It;
        return nullptr;
    }
    void Done()
    {
        FFileHelper::SaveStringArrayToFile(Results,
                                           *(FPaths::ProjectSavedDir() / TEXT("Validation/MultiplayerPIE.txt")));
        UE_LOG(LogTemp, Display, TEXT("[CrownfallPIE] COMPLETE: %d checks, %d failures"), Results.Num(), Failures);
        GEditor->RequestEndPlayMap();
    }
    bool Tick(float)
    {
        double Now = FPlatformTime::Seconds();
        if (Now < Next)
            return true;
        Next = Now + 1.5f;
        if (Now - Started > 100)
        {
            Check(TEXT("PIE test completed before timeout"), false);
            Done();
            return false;
        }
        UWorld* Server = nullptr;
        UWorld* Client = nullptr;
        for (const FWorldContext& C : GEngine->GetWorldContexts())
            if (C.WorldType == EWorldType::PIE && C.World())
            {
                if (C.World()->GetNetMode() == NM_Client)
                    Client = C.World();
                else if (C.World()->GetNetMode() == NM_ListenServer)
                    Server = C.World();
            }
        if (!Server || !Client)
            return true;
        ARGCharacter* Local = Cast<ARGCharacter>(
            Client->GetFirstPlayerController() ? Client->GetFirstPlayerController()->GetPawn() : nullptr);
        if (!Local)
            return true;
        ClientTeam = Local->Team;
        ARGCharacter* Authority = nullptr;
        ARGCharacter* Enemy = nullptr;
        for (TActorIterator<ARGCharacter> It(Server); It; ++It)
            if (!It->bBot && It->IsAlive())
            {
                if (It->Team == ClientTeam)
                    Authority = *It;
                else
                    Enemy = *It;
            }
        ARGGameState* SS = Server->GetGameState<ARGGameState>();
        ARGGameState* CS = Client->GetGameState<ARGGameState>();
        ARGGameMode* GM = Server->GetAuthGameMode<ARGGameMode>();
        if (!Authority || !Enemy || !SS || !CS || !GM)
            return true;
        if (Stage == 0)
        {
            SS->Phase = ERGPhase::Playing;
            SS->SecondsRemaining = 900;
            Check(TEXT("Two PIE players assigned opposite teams"),
                  Authority->Team != Enemy->Team && ClientTeam != ERGTeam::Neutral);
            bool Arena = false;
            for (TActorIterator<ARGArena> It(Client); It; ++It)
                Arena = true;
            Check(TEXT("Client received arena and replicated gameplay actors"),
                  Arena && Relic(Client, ERGTeam::Red) && Station(Client, ERGStationType::Gate, ERGTeam::Blue));
            ARGStation* Hat = Station(Server, ERGStationType::Hat, ClientTeam, ERGClass::Knight);
            Authority->SetActorLocation(Hat->GetActorLocation() + FVector(110, 0, 60));
        }
        else if (Stage == 1)
            Local->Act(2);
        else if (Stage == 2)
        {
            Check(TEXT("Remote E interaction replicates Knight class and original mesh"),
                  Authority->Classes->ClassID == ERGClass::Knight && Local->Classes->ClassID == ERGClass::Knight &&
                      Local->GetMesh()->GetSkeletalMeshAsset() == Local->Classes->Definition().BodyMesh);
            // Open ground keeps the melee fixture clear of the new bridge pillars.
            Authority->SetActorLocation(FVector(-1500, 0, 80));
            Enemy->SetActorLocation(FVector(-1390, 0, 80));
            Enemy->ProtectedUntil = 0;
        }
        else if (Stage == 3)
        {
            Local->ServerAim(FVector::ForwardVector);
            Local->Act(0);
        }
        else if (Stage == 4)
        {
            Check(TEXT("Remote attack RPC damages opposing player on server"),
                  Enemy->Health->Health < Enemy->Health->MaxHealth);
            ARGCharacter* EnemyProxy = nullptr;
            for (TActorIterator<ARGCharacter> It(Client); It; ++It)
                if (It->Team != ClientTeam && !It->bBot)
                    EnemyProxy = *It;
            Check(TEXT("Enemy health damage replicates to client"),
                  EnemyProxy && EnemyProxy->Health->Health < EnemyProxy->Health->MaxHealth);
            Authority->SetActorLocation(Relic(Server, RG::Enemy(ClientTeam))->Home + FVector(65, 0, 0));
        }
        else if (Stage == 5)
            Local->Act(2);
        else if (Stage == 6)
        {
            Check(TEXT("Remote relic pickup replicates carrier and stolen status"),
                  Authority->Carry->Carried && Local->Carry->Carried &&
                      Relic(Client, RG::Enemy(ClientTeam))->State == ERGRelicState::Stolen);
            Authority->ProtectedUntil = 0;
            Authority->Health->Damage(500, Enemy, FVector::ForwardVector);
            Next = Now + 5.5f;
        }
        else if (Stage == 7)
        {
            Check(TEXT("Death drops relic on both server and client"),
                  Relic(Server, RG::Enemy(ClientTeam))->State == ERGRelicState::Dropped &&
                      Relic(Client, RG::Enemy(ClientTeam))->State == ERGRelicState::Dropped);
            Check(TEXT("Remote player respawns as Peasant with retained team"),
                  Local->IsAlive() && Local->Classes->ClassID == ERGClass::Peasant && Authority->Team == ClientTeam);
            Authority->Classes->SetClass(ERGClass::Worker);
            ARGStation* Node = Station(Server, ERGStationType::Wood, ERGTeam::Neutral);
            Authority->SetActorLocation(Node->GetActorLocation() - FVector(140, 0, -10));
        }
        else if (Stage == 8)
        {
            Local->ServerAim(FVector::ForwardVector);
            Local->Act(0);
        }
        else if (Stage == 9)
        {
            Check(TEXT("Remote Worker harvest replicates inventory"),
                  Authority->PackWood > 0 && Local->PackWood == Authority->PackWood);
            Authority->SetActorLocation(Station(Server, ERGStationType::Deposit, ClientTeam)->GetActorLocation() +
                                        FVector(110, 0, 60));
        }
        else if (Stage == 10)
            Local->Act(2);
        else if (Stage == 11)
        {
            Check(TEXT("Team stockpile and emptied inventory replicate"),
                  SS->Stock(ClientTeam).Wood > 0 && CS->Stock(ClientTeam).Wood == SS->Stock(ClientTeam).Wood &&
                      Local->PackWood == 0);
            SS->Stock(ClientTeam).Wood = 100;
            SS->Stock(ClientTeam).Ore = 100;
            Authority->SetActorLocation(
                Station(Server, ERGStationType::Upgrade, ClientTeam, ERGClass::Knight)->GetActorLocation() +
                FVector(80, 0, 60));
        }
        else if (Stage == 12)
            Local->Act(2);
        else if (Stage == 13)
        {
            Check(TEXT("Remote purchase replicates team upgrade and exact cost"),
                  CS->Stock(ClientTeam).HasUpgrade(ERGClass::Knight) && CS->Stock(ClientTeam).Wood == 80 &&
                      CS->Stock(ClientTeam).Ore == 60);
            Relic(Server, RG::Enemy(ClientTeam))->ReturnHome();
            Authority->SetActorLocation(Relic(Server, RG::Enemy(ClientTeam))->Home + FVector(50, 0, 0));
        }
        else if (Stage == 14)
            Local->Act(2);
        else if (Stage == 15)
            Authority->SetActorLocation(Relic(Server, ClientTeam)->Home);
        else if (Stage == 16)
        {
            Check(TEXT("Capture score and reset replicate to both PIE worlds"),
                  SS->Stock(ClientTeam).Captures == 1 && CS->Stock(ClientTeam).Captures == 1 &&
                      Relic(Client, RG::Enemy(ClientTeam))->State == ERGRelicState::Safe && !Local->Carry->Carried);
            SS->Stock(ClientTeam).Captures = SS->CaptureTarget;
            GM->Win(ClientTeam);
        }
        else if (Stage == 17)
        {
            Check(TEXT("Victory state replicates to client HUD state"),
                  CS->Phase == ERGPhase::Victory && CS->Winner == ClientTeam);
            SS->Phase=ERGPhase::Playing;SS->SecondsRemaining=900;
            Authority->Classes->SetClass(ERGClass::Mage);
            Authority->Combat->NextAttackTime=0;
            Authority->SetActorLocation(FVector(-1200,0,80));
            Enemy->SetActorLocation(FVector(-950,0,80));Enemy->ProtectedUntil=0;Enemy->Health->Health=500;Enemy->Health->MaxHealth=500;
            for(TActorIterator<ARGOutpost> It(Server);It;++It) {It->Team=ClientTeam;It->Control=ClientTeam==ERGTeam::Red?-1:1;It->RefreshVisual();}
        }
        else if(Stage==18)
        {
            int Owned=0;for(TActorIterator<ARGOutpost> It(Client);It;++It) if(It->Team==ClientTeam) ++Owned;
            Check(TEXT("Outpost ownership replicates to the client minimap"),Owned==3);
            Local->Act(0,true);
        }
        else if(Stage==19)
        {
            Check(TEXT("Remote mage charge replicates with server timing"),Authority->Combat->bCharging && Local->Combat->bCharging && Authority->Combat->ChargeFraction()>.7f);
            Local->Act(0,false);
        }
        else if(Stage==20)
        {
            Check(TEXT("Remote release applies charged area damage and ends charging"),Enemy->Health->Health<450 && !Authority->Combat->bCharging && !Local->Combat->bCharging);
            Done();
            return false;
        }
        ++Stage;
        return true;
    }
};
} // namespace
#endif
void URGEditorValidation::StartMultiplayerTest()
{
#if WITH_EDITOR
    if (!GEditor || GEditor->PlayWorld)
        return;
    ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
    Settings->SetPlayNumberOfClients(2);
    Settings->SetPlayNetMode(PIE_ListenServer);
    Settings->SetRunUnderOneProcess(true);
    FRequestPlaySessionParams Params;
    Params.EditorPlaySettings = Settings;
    Params.GlobalMapOverride = TEXT("/Game/Crownfall/Maps/Crownfall");
    Params.GameModeOverride = ARGGameMode::StaticClass();
    Params.bAllowOnlineSubsystem = false;
    GEditor->RequestPlaySession(Params);
    TSharedRef<FPIECheck> Test = MakeShared<FPIECheck>();
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([Test](float Delta) { return Test->Tick(Delta); }));
#endif
}
