#include "RGComponents.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "RGCharacter.h"
#include "RGGameState.h"
#include "RGWorldActors.h"

URGHealthComponent::URGHealthComponent()
{
    SetIsReplicatedByDefault(true);
}
URGClassComponent::URGClassComponent()
{
    SetIsReplicatedByDefault(true);
}
URGCarryComponent::URGCarryComponent()
{
    SetIsReplicatedByDefault(true);
}
URGCombatComponent::URGCombatComponent()
{
    SetIsReplicatedByDefault(true);
}
void URGHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(URGHealthComponent, Health);
    DOREPLIFETIME(URGHealthComponent, MaxHealth);
    DOREPLIFETIME(URGHealthComponent, bDead);
}
void URGClassComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(URGClassComponent, ClassID);
    DOREPLIFETIME(URGClassComponent, bUpgraded);
}
void URGCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(URGCarryComponent, Carried);
}
void URGCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(URGCombatComponent, NextAttackTime);
    DOREPLIFETIME(URGCombatComponent, NextAbilityTime);
    DOREPLIFETIME(URGCombatComponent, bBlocking);
    DOREPLIFETIME(URGCombatComponent, SlowUntil);
    DOREPLIFETIME(URGCombatComponent, bCharging);
    DOREPLIFETIME(URGCombatComponent, ChargeStarted);
}

void URGHealthComponent::OnRep_Dead()
{
    if (ARGCharacter* P = Cast<ARGCharacter>(GetOwner()))
    {
        if (bDead)
            P->HandleDeath();
    }
}
void URGHealthComponent::Damage(float Amount, ARGCharacter* Source, const FVector& Direction)
{
    ARGCharacter* P = Cast<ARGCharacter>(GetOwner());
    ARGGameState* S = GetWorld()->GetGameState<ARGGameState>();
    if (!P || !P->HasAuthority() || bDead || !S || !S->IsPlaying() || Amount <= 0 || !Source ||
        Source->Team == P->Team || P->ProtectedUntil > GetWorld()->GetTimeSeconds())
        return;
    if (P->Combat->bBlocking && FVector::DotProduct(P->GetActorForwardVector(), -Direction) > .25f)
        Amount *= .2f;
    Health = FMath::Max(0.f, Health - Amount);
    P->MulticastFeedback(P->GetActorLocation() + FVector(0, 0, 95), FLinearColor(1, .25f, .13f), Amount);
    P->LaunchCharacter(Direction.GetSafeNormal2D() * 180 + FVector(0, 0, 60), false, false);
    if (Health <= 0)
    {
        bDead = true;
        if (ARGPlayerState* PS = P->GetPlayerState<ARGPlayerState>())
            PS->Deaths++;
        if (ARGPlayerState* PS = Source->GetPlayerState<ARGPlayerState>())
            PS->Kills++;
        P->HandleDeath();
    }
}
void URGHealthComponent::Heal(float Amount)
{
    if (!GetOwner()->HasAuthority() || bDead)
        return;
    float Actual = FMath::Min(Amount, MaxHealth - Health);
    Health += Actual;
    if (Actual > 0)
        CastChecked<ARGCharacter>(GetOwner())
            ->MulticastFeedback(GetOwner()->GetActorLocation() + FVector(0, 0, 90), FLinearColor(.1f, 1, .5f), -Actual);
}
const FRGClassDefinition& URGClassComponent::Definition() const
{
    return (Catalog ? Catalog.Get() : GetDefault<URGClassCatalog>())->Get(ClassID);
}
void URGClassComponent::SetClass(ERGClass NewClass)
{
    ARGCharacter* P = CastChecked<ARGCharacter>(GetOwner());
    if (!P->HasAuthority() || !P->IsAlive())
        return;
    ARGGameState* S = GetWorld()->GetGameState<ARGGameState>();
    const float Ratio = P->Health->MaxHealth > 0 ? P->Health->Health / P->Health->MaxHealth : 1;
    ClassID = NewClass;
    P->Combat->CancelCharge();
    if (ARGPlayerState* PS = P->GetPlayerState<ARGPlayerState>()) PS->LastClass = NewClass;
    bUpgraded = S && S->Stock(P->Team).HasUpgrade(NewClass);
    P->Health->MaxHealth = Definition().MaxHealth;
    P->Health->Health = P->Health->MaxHealth * Ratio;
    P->Combat->bBlocking = false;
    OnRep_Class();
    P->ForceNetUpdate();
}
void URGClassComponent::OnRep_Class()
{
    if (ARGCharacter* P = Cast<ARGCharacter>(GetOwner()))
    {
        P->RefreshAppearance();
        P->RefreshSpeed();
    }
}
bool URGCarryComponent::Pickup(ARGCarryable* Item)
{
    ARGCharacter* P = CastChecked<ARGCharacter>(GetOwner());
    if (!P->HasAuthority() || Carried || !IsValid(Item) || !Item->CanPickup(P))
        return false;
    Carried = Item;
    P->Combat->CancelCharge();
    Item->OnPickedUp(P);
    P->Combat->bBlocking = false;
    OnRep_Carried();
    return true;
}
void URGCarryComponent::Drop()
{
    if (!GetOwner()->HasAuthority())
        return;
    if (IsValid(Carried))
        Carried->OnDropped();
    Carried = nullptr;
    OnRep_Carried();
}
float URGCarryComponent::SpeedModifier() const
{
    return IsValid(Carried) ? Carried->GetCarryMovementModifier() : 1.f;
}
void URGCarryComponent::OnRep_Carried()
{
    if (ARGCharacter* P = Cast<ARGCharacter>(GetOwner()))
        P->RefreshSpeed();
}
bool URGCombatComponent::CanAct() const
{
    const ARGCharacter* P = Cast<ARGCharacter>(GetOwner());
    const ARGGameState* S = GetWorld()->GetGameState<ARGGameState>();
    return P && P->HasAuthority() && P->IsAlive() && S && S->IsPlaying() && !P->Carry->Carried;
}
void URGCombatComponent::Primary()
{
    if (!CanAct() || GetWorld()->GetTimeSeconds() < NextAttackTime)
        return;
    ARGCharacter* P = CastChecked<ARGCharacter>(GetOwner());
    const FRGClassDefinition& D = P->Classes->Definition();
    ERGClass C = P->Classes->ClassID;
    const bool U = P->Classes->bUpgraded;
    NextAttackTime = GetWorld()->GetTimeSeconds() + (C == ERGClass::Archer && U ? 1.35f : D.AttackRate);
    bBlocking = false;
    ++P->AttackSequence;
    P->OnRep_Attack();
    P->ProtectedUntil = 0;
    const FVector Dir = P->AimDirection.GetSafeNormal2D();
    if (C == ERGClass::Mage || C == ERGClass::Priest)
    {
        AreaPulse();
    }
    else if (C == ERGClass::Archer)
    {
        int32 Count = C == ERGClass::Archer && U ? 6 : 1;
        for (int32 I = 0; I < Count; ++I)
        {
            FVector Shot = Dir.RotateAngleAxis(Count > 1 ? (I - 2.5f) * 5 : 0, FVector::UpVector);
            FActorSpawnParameters Params;
            Params.Owner = P;
            Params.Instigator = P;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ARGProjectile* B = GetWorld()->SpawnActor<ARGProjectile>(
                P->GetActorLocation() + Shot * 75 + FVector(0, 0, 15), Shot.Rotation(), Params);
            B->Team = P->Team;
            B->ClassID = C;
            B->Shooter = P;
            B->bIce = U;
            B->Damage = Count > 1 ? 12 : D.AttackDamage;
            B->Knockback = Count > 1 ? 300 : 150;
            B->SetLifeSpan((Count > 1 ? 700 : D.AttackRange) / 1800);
            B->RefreshVisual();
        }
    }
    else
    {
        float Range = D.AttackRange + (C == ERGClass::Knight && U ? 150 : 0);
        for (TActorIterator<ARGCharacter> It(GetWorld()); It; ++It)
        {
            FVector To = It->GetActorLocation() - P->GetActorLocation();
            if (It->Team != P->Team && To.Size() < Range &&
                FVector::DotProduct(To.GetSafeNormal2D(), Dir) > (U && C == ERGClass::Knight ? .78f : .25f))
            {
                FHitResult Hit;
                FCollisionQueryParams Query;
                Query.AddIgnoredActor(P);
                bool Blocked = GetWorld()->LineTraceSingleByChannel(Hit, P->GetActorLocation(), It->GetActorLocation(),
                                                                    ECC_Visibility, Query);
                if (!Blocked || Hit.GetActor() == *It)
                    It->Health->Damage(D.AttackDamage + (U ? 8 : 0), P, Dir);
            }
        }
        for (TActorIterator<ARGStation> It(GetWorld()); It; ++It)
        {
            const FVector To = It->GetActorLocation() - P->GetActorLocation();
            const float Extra = It->Kind == ERGStationType::Gate ? 160 : 50;
            if (To.Size2D() < Range + Extra && FVector::DotProduct(To.GetSafeNormal2D(), Dir) > .15f)
                It->Hit(P, D.AttackDamage);
        }
    }
}
void URGCombatComponent::Secondary(bool bPressed)
{
    if (!bPressed)
    {
        bBlocking = false;
        return;
    }
    if (!CanAct())
        return;
    ARGCharacter* P = CastChecked<ARGCharacter>(GetOwner());
    ERGClass C = P->Classes->ClassID;
    if (C == ERGClass::Knight)
    {
        bBlocking = true;
        return;
    }
    if (GetWorld()->GetTimeSeconds() < NextAbilityTime)
        return;
    if (C != ERGClass::Ninja && C != ERGClass::Worker && C != ERGClass::Priest && C != ERGClass::Mage)
    {
        Dodge();
        return;
    }
    NextAbilityTime = GetWorld()->GetTimeSeconds() + P->Classes->Definition().AbilityCooldown;
    if (C == ERGClass::Ninja)
    {
        P->LaunchCharacter(P->AimDirection * 1350 + FVector(0, 0, 180), true, true);
        P->MulticastFeedback(P->GetActorLocation(), FLinearColor(.6f, .2f, 1), 0);
        return;
    }
    if (C == ERGClass::Worker)
    {
        P->Interact();
        return;
    }
    if (C == ERGClass::Priest || C == ERGClass::Mage)
    {
        AreaPulse(0,C == ERGClass::Priest);
        return;
    }
}
void URGCombatComponent::Dodge()
{
    if (!CanAct())
        return;
    ARGCharacter* P = CastChecked<ARGCharacter>(GetOwner());
    if (GetWorld()->GetTimeSeconds() < NextAbilityTime)
        return;
    NextAbilityTime = GetWorld()->GetTimeSeconds() + 3;
    const FVector Dir = P->GetVelocity().Size2D() > 20 ? P->GetVelocity().GetSafeNormal2D() : P->AimDirection;
    P->LaunchCharacter(Dir * 950 + FVector(0, 0, 180), true, true);
}
