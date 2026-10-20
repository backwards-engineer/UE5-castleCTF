#pragma once
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "RGTypes.h"
#include "RGComponents.generated.h"
class ARGCharacter;
class ARGCarryable;

UCLASS(ClassGroup = Game, meta = (BlueprintSpawnableComponent))
class CASTLECTF_API URGHealthComponent : public UActorComponent
{
    GENERATED_BODY()
  public:
    URGHealthComponent();
    UPROPERTY(Replicated, BlueprintReadOnly) float Health = 100;
    UPROPERTY(Replicated, BlueprintReadOnly) float MaxHealth = 100;
    UPROPERTY(ReplicatedUsing = OnRep_Dead, BlueprintReadOnly) bool bDead = false;
    UFUNCTION() void OnRep_Dead();
    void Damage(float Amount, ARGCharacter* Source, const FVector& Direction);
    void Heal(float Amount);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS(ClassGroup = Game, meta = (BlueprintSpawnableComponent))
class CASTLECTF_API URGClassComponent : public UActorComponent
{
    GENERATED_BODY()
  public:
    URGClassComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<URGClassCatalog> Catalog;
    UPROPERTY(ReplicatedUsing = OnRep_Class, BlueprintReadOnly) ERGClass ClassID = ERGClass::Peasant;
    UPROPERTY(ReplicatedUsing = OnRep_Class, BlueprintReadOnly) bool bUpgraded = false;
    const FRGClassDefinition& Definition() const;
    void SetClass(ERGClass NewClass);
    UFUNCTION() void OnRep_Class();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS(ClassGroup = Game, meta = (BlueprintSpawnableComponent))
class CASTLECTF_API URGCarryComponent : public UActorComponent
{
    GENERATED_BODY()
  public:
    URGCarryComponent();
    UPROPERTY(ReplicatedUsing = OnRep_Carried, BlueprintReadOnly) TObjectPtr<ARGCarryable> Carried;
    UFUNCTION() void OnRep_Carried();
    bool Pickup(ARGCarryable* Item);
    void Drop();
    float SpeedModifier() const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS(ClassGroup = Game, meta = (BlueprintSpawnableComponent))
class CASTLECTF_API URGCombatComponent : public UActorComponent
{
    GENERATED_BODY()
  public:
    URGCombatComponent();
    UPROPERTY(Replicated, BlueprintReadOnly) float NextAttackTime = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float NextAbilityTime = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float SlowUntil = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bBlocking = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bCharging = false;
    UPROPERTY(Replicated, BlueprintReadOnly) float ChargeStarted = 0;
    void Primary();
    void BeginCharge();
    void ReleaseCharge();
    void CancelCharge();
    float ChargeFraction() const;
    void AreaPulse(float Charge = 0, bool bHealOnly = false);
    void Secondary(bool bPressed);
    void Dodge();
    bool CanAct() const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
