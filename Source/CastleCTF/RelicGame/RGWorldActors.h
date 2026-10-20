#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RGTypes.h"
#include "UObject/Interface.h"
#include "RGWorldActors.generated.h"
class ARGCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;
class UProjectileMovementComponent;
class USphereComponent;

UINTERFACE(MinimalAPI)
class URGCarryableInterface : public UInterface
{
    GENERATED_BODY()
};
class IRGCarryableInterface
{
    GENERATED_BODY()
  public:
    virtual bool CanPickup(ARGCharacter* Player) const = 0;
    virtual void OnPickedUp(ARGCharacter* Player) = 0;
    virtual void OnDropped() = 0;
    virtual float GetCarryMovementModifier() const = 0;
};

UCLASS(Blueprintable)
class CASTLECTF_API ARGCarryable : public AActor, public IRGCarryableInterface
{
    GENERATED_BODY()
  public:
    ARGCarryable();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Jewel;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> CrownPoints;
    UPROPERTY(ReplicatedUsing = OnRep_Carrier, BlueprintReadOnly) TObjectPtr<ARGCharacter> Carrier;
    UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_State, BlueprintReadOnly) ERGTeam Team = ERGTeam::Neutral;
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly) bool bRelic = true;
    UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly) ERGRelicState State = ERGRelicState::Safe;
    UPROPERTY(Replicated) FVector Home = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, Replicated) int32 Wood = 0;
    UPROPERTY(EditAnywhere, Replicated) int32 Ore = 0;
    UPROPERTY(EditAnywhere) float AutoReturnSeconds = 22;
    FTimerHandle ReturnTimer;
    UFUNCTION() void OnRep_Carrier();
    UFUNCTION() void OnRep_State();
    virtual bool CanPickup(ARGCharacter* Player) const override;
    virtual void OnPickedUp(ARGCharacter* Player) override;
    virtual void OnDropped() override;
    virtual float GetCarryMovementModifier() const override
    {
        return bRelic ? .68f : .88f;
    }
    void ReturnHome();
    void SetState(ERGRelicState NewState);
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS(Blueprintable)
class CASTLECTF_API ARGStation : public AActor
{
    GENERATED_BODY()
  public:
    ARGStation();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Accent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(EditAnywhere, ReplicatedUsing = RefreshVisual) ERGStationType Kind = ERGStationType::Hat;
    UPROPERTY(EditAnywhere, ReplicatedUsing = RefreshVisual) ERGTeam Team = ERGTeam::Neutral;
    UPROPERTY(EditAnywhere, ReplicatedUsing = RefreshVisual) ERGClass ClassID = ERGClass::Knight;
    UPROPERTY(EditAnywhere, ReplicatedUsing = RefreshVisual) float Health = 500;
    UPROPERTY(EditAnywhere) float MaxHealth = 500;
    UPROPERTY(EditAnywhere) int32 Yield = 5;
    UPROPERTY(EditAnywhere) float RegrowSeconds = 12;
    UPROPERTY(EditAnywhere) int32 RepairWoodCost = 2;
    UPROPERTY(ReplicatedUsing = RefreshVisual) bool bDepleted = false;
    UPROPERTY(ReplicatedUsing = RefreshVisual) bool bUpgraded = false;
    UPROPERTY(EditAnywhere) TObjectPtr<URGClassCatalog> Catalog;
    FTimerHandle VisualTimer;
    FTimerHandle RegrowTimer;
    UFUNCTION() void RefreshVisual();
    void Interact(ARGCharacter* Player);
    void Hit(ARGCharacter* Player, float Damage);
    FString Prompt(ARGCharacter* Player) const;
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class CASTLECTF_API ARGProjectile : public AActor
{
    GENERATED_BODY()
  public:
    ARGProjectile();
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
    UPROPERTY(ReplicatedUsing = RefreshVisual) ERGTeam Team = ERGTeam::Neutral;
    UPROPERTY(ReplicatedUsing = RefreshVisual) ERGClass ClassID = ERGClass::Mage;
    UPROPERTY(ReplicatedUsing = RefreshVisual) bool bIce = false;
    UPROPERTY() TObjectPtr<ARGCharacter> Shooter;
    float Damage = 30;
    float Knockback = 250;
    UFUNCTION()
    void Impact(UPrimitiveComponent* HitComponent, AActor* Other, UPrimitiveComponent* OtherComp, FVector Impulse,
                const FHitResult& Hit);
    UFUNCTION() void RefreshVisual();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class CASTLECTF_API ARGEffect : public AActor
{
    GENERATED_BODY()
  public:
    ARGEffect();
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Text;
    float Age = 0;
    virtual void Tick(float Delta) override;
    void Init(FLinearColor Color, float Amount);
};

namespace RGVisual
{
CASTLECTF_API void Tint(UStaticMeshComponent* Mesh, FLinearColor Color);
CASTLECTF_API void Shape(UStaticMeshComponent* Mesh, const TCHAR* Name);
} // namespace RGVisual
