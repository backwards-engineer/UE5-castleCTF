#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RGTypes.h"
#include "RGCharacter.generated.h"
class URGHealthComponent;
class URGClassComponent;
class URGCombatComponent;
class URGCarryComponent;
class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

UCLASS()
class CASTLECTF_API ARGCharacter : public ACharacter
{
    GENERATED_BODY()
  public:
    ARGCharacter();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<URGHealthComponent> Health;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<URGClassComponent> Classes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<URGCombatComponent> Combat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<URGCarryComponent> Carry;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Boom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Hat;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Weapon;
    UPROPERTY(ReplicatedUsing = OnRep_Team, BlueprintReadOnly) ERGTeam Team = ERGTeam::Neutral;
    UPROPERTY(Replicated, BlueprintReadOnly) float ProtectedUntil = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 PackWood = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 PackOre = 0;
    UPROPERTY(ReplicatedUsing = OnRep_Attack) int32 AttackSequence = 0;
    UPROPERTY(Replicated) bool bBot = false;
    float AttackVisualUntil = 0;
    float HitVisualUntil = 0;
    float LastAimSend = 0;
    float LastAttackRequest = -1;
    float LastInteract = 0;
    bool bAttackHeld = false;
    FVector AimDirection = FVector::ForwardVector;
    UPROPERTY() TObjectPtr<UAnimSequence> CurrentAnimation;
    UFUNCTION() void OnRep_Team();
    UFUNCTION() void OnRep_Attack();
    UFUNCTION(Server, Unreliable) void ServerAim(FVector_NetQuantizeNormal Direction);
    UFUNCTION(Server, Reliable) void ServerAction(uint8 Action, bool bPressed);
    UFUNCTION(NetMulticast, Unreliable) void MulticastFeedback(FVector Location, FLinearColor Color, float Amount);
    UFUNCTION(NetMulticast, Unreliable) void MulticastArea(float Radius, FLinearColor Color);
    float LastChargeVisual = 0;
    bool bAppliedLocalSettings = false;
    void RefreshGateAccess();
    void Act(uint8 Action, bool bPressed = true);
    void Interact();
    void RefreshAppearance();
    void RefreshSpeed();
    void HandleDeath();
    void MoveForward(float Value);
    void MoveRight(float Value);
    void PrimaryPressed();
    void PrimaryReleased();
    void SecondaryPressed();
    void SecondaryReleased();
    void InteractPressed();
    void DropPressed();
    void DodgePressed();
    void RemoveHatPressed();
    bool IsAlive() const;
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
