#include "RGBotController.h"
#include "EngineUtils.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGGameState.h"
#include "RGOutpost.h"
#include "RGWorldActors.h"

void ARGBotController::GoTo(FVector Target)
{
    ARGCharacter* P=Cast<ARGCharacter>(GetPawn());
    if (!P) return;
    const FVector Pos=P->GetActorLocation();
    Destination=Target;
    const float Near=Pos.X<0 ? -RG::CastleGateX : RG::CastleGateX;
    for (float Wall : {Near,-Near})
    {
        const bool Crossing=(Pos.X<Wall)!=(Target.X<Wall);
        const bool InGate=FMath::Abs(Pos.X-Wall)<220 && FMath::Abs(Target.X-Wall)>400;
        if (!Crossing && !InGate) continue;
        const float Sign=Target.X<Wall?-1.f:1.f;
        if (FMath::Abs(Pos.Y)>100) Destination=FVector(Wall-Sign*380,0,Pos.Z);
        else Destination=FVector(Wall+Sign*450,0,Pos.Z);
        break;
    }
}
void ARGBotController::Think()
{
    ARGCharacter* P=Cast<ARGCharacter>(GetPawn());
    auto State=GetWorld()->GetGameState<ARGGameState>();
    if (!P || !P->IsAlive() || !State || !State->IsPlaying()) return;
    if (P->Classes->ClassID==ERGClass::Peasant)
    {
        for (TActorIterator<ARGStation> It(GetWorld());It;++It)
            if (It->Team==Team && It->Kind==ERGStationType::Hat && It->ClassID==PreferredClass)
            { GoTo(It->GetActorLocation()); It->Interact(P); return; }
    }
    if (P->Carry->Carried && P->Carry->Carried->bRelic)
    { GoTo(FVector(RG::Side(Team)*RG::RelicX,530,80)); return; }
    const bool Healer=P->Classes->ClassID==ERGClass::Priest && !P->Classes->bUpgraded;
    if (P->Classes->ClassID==ERGClass::Priest)
    {
        for (TActorIterator<ARGCharacter> It(GetWorld());It;++It)
            if (It->Team==Team && It->IsAlive() && It->Health->Health<It->Health->MaxHealth-5 && FVector::Dist2D(It->GetActorLocation(),P->GetActorLocation())<P->Classes->Definition().AreaRadius)
            { if (Healer) P->Combat->Primary(); else P->Combat->Secondary(true); break; }
    }
    auto Attack=[P]() {
        if (P->Classes->ClassID==ERGClass::Mage)
        {
            if (P->Combat->bCharging && P->Combat->ChargeFraction()>=.72f) P->Combat->ReleaseCharge();
            else if (!P->Combat->bCharging) P->Combat->BeginCharge();
        }
        else P->Combat->Primary();
    };
    bool HasOutpost=false;
    for(TActorIterator<ARGOutpost> It(GetWorld());It;++It) if(It->Team==Team) HasOutpost=true;
    const bool Raider=PreferredClass==ERGClass::Ninja && HasOutpost;
    ARGCharacter* Enemy=nullptr;
    float Dist=Raider?400.f:1400.f;
    for (TActorIterator<ARGCharacter> It(GetWorld());It;++It)
    {
        const float D=FVector::Dist2D(P->GetActorLocation(),It->GetActorLocation());
        if (It->Team!=Team && It->IsAlive() && D<Dist && LineOfSightTo(*It)) { Enemy=*It; Dist=D; }
    }
    if (Enemy && BotRole!=ERGBotRole::Worker && !Healer)
    {
        P->AimDirection=(Enemy->GetActorLocation()-P->GetActorLocation()).GetSafeNormal2D();
        const ERGClass Class=P->Classes->ClassID;
        const float Range=(Class==ERGClass::Mage || Class==ERGClass::Priest) ? P->Classes->Definition().AreaRadius :
            Class==ERGClass::Archer ? (P->Classes->bUpgraded?650.f:1150.f) : P->Classes->Definition().AttackRange+(P->Classes->bUpgraded?140:0);
        if (Dist>Range*.75f) GoTo(Enemy->GetActorLocation()); else Destination=P->GetActorLocation();
        if (Dist<Range) Attack();
        if (Class==ERGClass::Ninja && Dist>300 && Dist<950) P->Combat->Secondary(true);
        if (Class==ERGClass::Knight) P->Combat->Secondary(P->Health->Health<65 && Dist<250);
        return;
    }
    if (BotRole==ERGBotRole::Worker)
    {
        ARGStation* Target=nullptr; float Best=MAX_flt;
        for (TActorIterator<ARGStation> It(GetWorld());It;++It)
        {
            const bool Deposit=P->PackWood+P->PackOre>=25;
            const auto Resource=State->Stock(Team).Wood<=State->Stock(Team).Ore?ERGStationType::Wood:ERGStationType::Ore;
            const bool Match=Deposit?(It->Kind==ERGStationType::Deposit && It->Team==Team):(It->Kind==Resource && !It->bDepleted);
            const float D=FVector::Dist2D(P->GetActorLocation(),It->GetActorLocation());
            if (Match && D<Best) { Best=D; Target=*It; }
        }
        if (Target)
        {
            GoTo(Target->GetActorLocation());
            P->AimDirection=(Target->GetActorLocation()-P->GetActorLocation()).GetSafeNormal2D();
            if (Best<210) { Destination=P->GetActorLocation(); if (Target->Kind==ERGStationType::Deposit) Target->Interact(P); else P->Combat->Primary(); }
        }
        for (TActorIterator<ARGStation> It(GetWorld());It;++It)
            if (It->Team==Team && It->Kind==ERGStationType::Upgrade && !It->bUpgraded)
            {
                const auto& Def=P->Classes->Catalog->Get(It->ClassID);
                if (State->Stock(Team).Wood>=Def.WoodCost && State->Stock(Team).Ore>=Def.OreCost)
                { GoTo(It->GetActorLocation()); It->Interact(P); break; }
            }
        return;
    }
    if (BotRole==ERGBotRole::Support || Healer)
    {
        ARGCharacter* Ally=nullptr; float Best=MAX_flt;
        for (TActorIterator<ARGCharacter> It(GetWorld());It;++It)
        {
            if (*It==P || It->Team!=Team || !It->IsAlive() || It->Classes->ClassID==ERGClass::Priest || It->Classes->ClassID==ERGClass::Worker || It->Classes->ClassID==ERGClass::Peasant) continue;
            const float D=FVector::Dist2D(It->GetActorLocation(),P->GetActorLocation());
            if (D<Best) { Best=D; Ally=*It; }
        }
        if (Ally) { GoTo(Ally->GetActorLocation()+FVector(RG::Side(Team)*160,100,0)); return; }
    }
    ARGOutpost* TargetPost=nullptr; float Best=MAX_flt;
    for (TActorIterator<ARGOutpost> It(GetWorld());It;++It)
    {
        if(Raider) break;
        if (It->Team==Team && !It->bContested) continue;
        if (BotRole==ERGBotRole::Defender && RG::Side(Team)*It->GetActorLocation().X<0) continue;
        const float D=FVector::Dist2D(P->GetActorLocation(),It->GetActorLocation());
        if (D<Best) { Best=D; TargetPost=*It; }
    }
    if (TargetPost)
    {
        GoTo(TargetPost->GetActorLocation()+FVector(RG::Side(Team)*100,80,0));
        return;
    }
    if (BotRole==ERGBotRole::Defender)
    {
        GoTo(FVector(RG::Side(Team)*(RG::CastleGateX-500),0,80));
        for (TActorIterator<ARGCarryable> It(GetWorld());It;++It)
            if (It->bRelic && It->Team==Team && It->State!=ERGRelicState::Safe)
            { GoTo(It->GetActorLocation()); if (FVector::Dist2D(P->GetActorLocation(),It->GetActorLocation())<180) P->Interact(); }
        return;
    }
    // Siege the actual enemy gate before attempting entry. All classes use normal combat.
    for (TActorIterator<ARGStation> It(GetWorld());It;++It)
        if (It->Kind==ERGStationType::Gate && It->Team!=Team && It->Health>0 && FMath::Abs(P->GetActorLocation().X)<RG::CastleGateX)
        {
            const FVector Staging=It->GetActorLocation()+FVector(RG::Side(Team)*170,0,-90);
            if(Raider && -RG::Side(Team)*P->GetActorLocation().X<RG::CastleGateX-900)
            {
                // Leave the bridge at its open end before turning toward the flank.
                if(FMath::Abs(P->GetActorLocation().X)<600 && FMath::Abs(P->GetActorLocation().Y+1700)>120)
                    GoTo(FVector((P->GetActorLocation().X<0?-1.f:1.f)*750,P->GetActorLocation().Y,80));
                else if(FMath::Abs(P->GetActorLocation().Y+1700)>120) GoTo(FVector(P->GetActorLocation().X,-1700,80));
                else GoTo(FVector(-RG::Side(Team)*(RG::CastleGateX-700),-1700,80));
                return;
            }
            GoTo(Staging);
            if (FVector::Dist2D(P->GetActorLocation(),It->GetActorLocation())<300)
            { Destination=P->GetActorLocation(); P->AimDirection=(It->GetActorLocation()-P->GetActorLocation()).GetSafeNormal2D(); Attack(); }
            return;
        }
    for (TActorIterator<ARGCarryable> It(GetWorld());It;++It)
        if (It->bRelic && It->Team!=Team)
        { GoTo(It->GetActorLocation()); if (FVector::Dist2D(P->GetActorLocation(),It->GetActorLocation())<180) P->Carry->Pickup(*It); break; }
}
