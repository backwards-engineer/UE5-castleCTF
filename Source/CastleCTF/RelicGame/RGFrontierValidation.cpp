#include "RGGameMode.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGGameState.h"
#include "RGOutpost.h"
#include "RGWorldActors.h"

void ARGGameMode::RunFrontierTest()
{
    auto State=GetGameState<ARGGameState>();State->Phase=ERGPhase::Playing;State->SecondsRemaining=900;
    TArray<FString> Results;
    auto Check=[&](const TCHAR* Name,bool Pass){Results.Add(FString(Pass?TEXT("PASS "):TEXT("FAIL "))+Name);UE_LOG(LogTemp,Display,TEXT("[FrontierTest] %s"),*Results.Last());};
    auto Spawn=[&](ERGTeam Team,FVector Location){auto P=SpawnFighter(GetWorld()->SpawnActor<AAIController>(),Team);P->SetActorLocation(Location);P->ProtectedUntil=0;return P;};
    auto Caster=Spawn(ERGTeam::Red,FVector(-1200,0,85));
    auto Ally=Spawn(ERGTeam::Red,FVector(-1200,220,85));
    auto Enemy=Spawn(ERGTeam::Blue,FVector(-980,0,85));
    auto OtherEnemy=Spawn(ERGTeam::Blue,FVector(-1200,-220,85));
    auto Distant=Spawn(ERGTeam::Red,FVector(-1200,900,85));
    Caster->Classes->SetClass(ERGClass::Priest);Caster->Health->Health=20;Ally->Health->Health=20;Distant->Health->Health=20;
    const float EnemyBefore=Enemy->Health->Health;
    Caster->Combat->Primary();
    Check(TEXT("Priest primary heals self and allies inside its circle"),Caster->Health->Health>20 && Ally->Health->Health>20);
    Check(TEXT("Healing circle does not heal distant allies or damage enemies"),Distant->Health->Health==20 && Enemy->Health->Health==EnemyBefore);
    Caster->Classes->SetClass(ERGClass::Mage);Caster->Combat->NextAttackTime=0;
    Enemy->Health->MaxHealth=500;Enemy->Health->Health=500;OtherEnemy->Health->MaxHealth=500;OtherEnemy->Health->Health=500;
    const float AllyHealth=Ally->Health->Health;
    Caster->Combat->BeginCharge();Caster->Combat->ReleaseCharge();
    const float Quick=500-Enemy->Health->Health;
    Check(TEXT("Mage click bursts around caster and hits multiple enemies"),Quick>0 && OtherEnemy->Health->Health<500);
    Check(TEXT("Mage area damage filters teammates"),Ally->Health->Health==AllyHealth);
    Caster->Combat->NextAttackTime=0;Enemy->Health->Health=500;
    Caster->Combat->BeginCharge();Caster->Combat->ChargeStarted-=Caster->Classes->Definition().ChargeSeconds;
    Caster->Combat->ReleaseCharge();
    Check(TEXT("Holding a full mage charge increases damage"),500-Enemy->Health->Health>Quick*2);
    const float After=Enemy->Health->Health;Caster->Combat->BeginCharge();Caster->Combat->ReleaseCharge();
    Check(TEXT("Mage cooldown rejects repeated casts"),Enemy->Health->Health==After && !Caster->Combat->bCharging);
    Caster->Combat->NextAttackTime=0;Caster->Combat->BeginCharge();Caster->Classes->SetClass(ERGClass::Knight);
    Check(TEXT("Class switching cancels an in-progress charge"),!Caster->Combat->bCharging);
    ARGStation* Gate=nullptr;
    for (TActorIterator<ARGStation> It(GetWorld());It;++It) if(It->Kind==ERGStationType::Gate && It->Team==ERGTeam::Blue) Gate=*It;
    bool Sealed=Gate!=nullptr;
    if(Gate)
    {
        for(int Y=-1300;Y<=1300;Y+=100)
        {
            FHitResult Hit;
            Sealed &= GetWorld()->SweepSingleByChannel(Hit,FVector(RG::CastleGateX-260,Y,80),FVector(RG::CastleGateX+260,Y,80),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(32,65));
        }
        Check(TEXT("Entire enemy castle frontage is sealed while gate is intact"),Sealed);
        Caster->SetActorLocation(FVector(RG::CastleGateX-250,0,80));Caster->RefreshGateAccess();
        FHitResult Hit;Caster->SetActorLocation(FVector(RG::CastleGateX+250,0,80),true,&Hit);
        Check(TEXT("Enemy character cannot walk through intact gate"),Caster->GetActorLocation().X<RG::CastleGateX);
        Caster->SetActorLocation(FVector(2000,-2500,80));
        Enemy->SetActorLocation(FVector(RG::CastleGateX-250,0,80));Enemy->RefreshGateAccess();
        Enemy->SetActorLocation(FVector(RG::CastleGateX+250,0,80),true,&Hit);
        Check(TEXT("Friendly character passes through own intact gate"),Enemy->GetActorLocation().X>RG::CastleGateX);
        Enemy->SetActorLocation(FVector(2000,2500,80));
        Gate->Hit(Caster,Gate->Health);Caster->SetActorLocation(FVector(RG::CastleGateX-250,0,80));
        Caster->SetActorLocation(FVector(RG::CastleGateX+250,0,80),true,&Hit);
        Check(TEXT("Destroying gate enables enemy entry"),Gate->Health==0 && Caster->GetActorLocation().X>RG::CastleGateX);
    }
    TArray<ARGOutpost*> Posts;
    for(TActorIterator<ARGOutpost> It(GetWorld());It;++It) Posts.Add(*It);
    Posts.Sort([](const ARGOutpost& A,const ARGOutpost& B){return A.GetActorLocation().X<B.GetActorLocation().X;});
    Check(TEXT("Expanded map contains three capturable outposts"),Posts.Num()==3);
    if(Posts.Num()==3)
    {
        Caster->SetActorLocation(Posts[0]->GetActorLocation()+FVector(100,0,80));
        for(int I=0;I<50;++I) Posts[0]->CaptureStep();
        Check(TEXT("Standing in an uncontested circle captures ownership"),Posts[0]->Team==ERGTeam::Red);
        const float Before=Posts[0]->Control;Enemy->SetActorLocation(Posts[0]->GetActorLocation()+FVector(-100,0,80));Posts[0]->CaptureStep();
        Check(TEXT("Opposing teams contest capture and disable spawning"),Posts[0]->bContested && Posts[0]->Control==Before && !Posts[0]->CanSpawn(ERGTeam::Red));
        Enemy->SetActorLocation(FVector(2000,2500,80));Posts[0]->CaptureStep();
        Posts[2]->Team=ERGTeam::Red;Posts[2]->Control=-1;
        Check(TEXT("Respawn selects the furthest owned safe outpost"),ForwardOutpost(ERGTeam::Red)==Posts[2]);
        Enemy->SetActorLocation(Posts[2]->GetActorLocation()+FVector(200,0,80));
        Check(TEXT("Enemies near forward outpost force fallback spawn"),ForwardOutpost(ERGTeam::Red)==Posts[0]);
        Enemy->SetActorLocation(FVector(2000,2500,80));
        auto Controller=GetWorld()->SpawnActor<AAIController>();Controller->bWantsPlayerState=true;Controller->InitPlayerState();
        if(auto PS=Controller->GetPlayerState<ARGPlayerState>()) PS->LastClass=ERGClass::Archer;
        auto Respawned=SpawnFighter(Controller,ERGTeam::Red,true);
        Check(TEXT("Forward respawn appears at outpost with previous class"),FVector::Dist2D(Respawned->GetActorLocation(),Posts[2]->GetActorLocation())<400 && Respawned->Classes->ClassID==ERGClass::Archer);
    }
    FFileHelper::SaveStringArrayToFile(Results,*(FPaths::ProjectSavedDir()/TEXT("Validation/Frontier.txt")));
    FPlatformMisc::RequestExitWithStatus(false,Results.ContainsByPredicate([](const FString& S){return S.StartsWith(TEXT("FAIL"));})?1:0);
}
