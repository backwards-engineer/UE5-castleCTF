#include "RGComponents.h"
#include "EngineUtils.h"
#include "RGCharacter.h"
#include "RGGameState.h"
#include "RGWorldActors.h"

float URGCombatComponent::ChargeFraction() const
{
    const ARGCharacter* P=CastChecked<ARGCharacter>(GetOwner());
    const auto State=GetWorld()->GetGameState<ARGGameState>();
    const float Now=State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    return bCharging ? FMath::Clamp((Now-ChargeStarted)/FMath::Max(.1f,P->Classes->Definition().ChargeSeconds),0.f,1.f) : 0;
}
void URGCombatComponent::BeginCharge()
{
    ARGCharacter* P=CastChecked<ARGCharacter>(GetOwner());
    if (!CanAct() || bCharging || P->Classes->ClassID!=ERGClass::Mage || GetWorld()->GetTimeSeconds()<NextAttackTime) return;
    bCharging=true; ChargeStarted=GetWorld()->GetTimeSeconds(); P->ProtectedUntil=0;
    P->ForceNetUpdate();
}
void URGCombatComponent::CancelCharge()
{
    if (GetOwner()->HasAuthority()) bCharging=false;
}
void URGCombatComponent::ReleaseCharge()
{
    if (!bCharging || !GetOwner()->HasAuthority()) return;
    const float Charge=ChargeFraction(); bCharging=false;
    if (!CanAct()) return;
    ARGCharacter* P=CastChecked<ARGCharacter>(GetOwner());
    NextAttackTime=GetWorld()->GetTimeSeconds()+P->Classes->Definition().AttackRate;
    ++P->AttackSequence; P->OnRep_Attack(); AreaPulse(Charge); P->ForceNetUpdate();
}
void URGCombatComponent::AreaPulse(float Charge,bool bHealOnly)
{
    if (!CanAct()) return;
    ARGCharacter* P=CastChecked<ARGCharacter>(GetOwner());
    const auto& Def=P->Classes->Definition();
    const bool Priest=P->Classes->ClassID==ERGClass::Priest;
    const bool Healing=Priest && (!P->Classes->bUpgraded || bHealOnly);
    const float Radius=Def.AreaRadius*(Priest ? 1.f : FMath::Lerp(1.f,1.35f,Charge));
    const float Damage=Def.AttackDamage*FMath::Lerp(1.f,Def.ChargedDamageMultiplier,Charge);
    for (TActorIterator<ARGCharacter> It(GetWorld());It;++It)
    {
        const FVector To=It->GetActorLocation()-P->GetActorLocation();
        if (!It->IsAlive() || To.Size2D()>Radius || FMath::Abs(To.Z)>200) continue;
        FHitResult Hit; FCollisionQueryParams Query; Query.AddIgnoredActor(P);
        const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,P->GetActorLocation(),It->GetActorLocation(),ECC_Visibility,Query);
        if (Blocked && Hit.GetActor()!=*It) continue;
        if (Healing && It->Team==P->Team) It->Health->Heal(Def.HealingAmount*(bHealOnly?1.6f:1.f));
        if (!Healing && It->Team!=P->Team)
        {
            const float Before=It->Health->Health;
            It->Health->Damage(Damage,P,To.GetSafeNormal2D());
            if (Priest) P->Health->Heal(FMath::Max(0.f,Before-It->Health->Health)*.55f);
            else if (P->Classes->bUpgraded) It->Combat->SlowUntil=GetWorld()->GetTimeSeconds()+2.5f+Charge*2;
        }
    }
    if (!Healing)
        for (TActorIterator<ARGStation> It(GetWorld());It;++It)
            if (It->Kind==ERGStationType::Gate && It->Team!=P->Team && FVector::Dist2D(It->GetActorLocation(),P->GetActorLocation())<Radius+60)
                It->Hit(P,Damage);
    P->MulticastArea(Radius,Healing ? FLinearColor(.15f,1,.45f) : Priest ? FLinearColor(.65f,.2f,1) : P->Classes->bUpgraded ? FLinearColor(.2f,.8f,1) : FLinearColor(1,.35f,.05f));
}
