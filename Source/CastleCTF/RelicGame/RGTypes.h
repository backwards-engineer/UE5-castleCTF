#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RGTypes.generated.h"

UENUM(BlueprintType) enum class ERGTeam : uint8
{
    Neutral,
    Red,
    Blue
};
UENUM(BlueprintType) enum class ERGClass : uint8
{
    Peasant,
    Worker,
    Knight,
    Priest,
    Mage,
    Archer,
    Ninja
};
UENUM(BlueprintType) enum class ERGPhase : uint8
{
    WaitingForPlayers,
    Countdown,
    Playing,
    Victory,
    Restarting
};
UENUM(BlueprintType) enum class ERGRelicState : uint8
{
    Safe,
    Stolen,
    Dropped,
    Returning
};
UENUM(BlueprintType) enum class ERGBotRole : uint8
{
    Attacker,
    Defender,
    Worker,
    Support
};
UENUM(BlueprintType) enum class ERGStationType : uint8
{
    Hat,
    Deposit,
    Upgrade,
    Gate,
    Wood,
    Ore
};

namespace RG
{
inline constexpr float CastleGateX = 4000.f;
inline constexpr float CastleBackX = 5600.f;
inline constexpr float CastleHalfWidth = 1400.f;
inline constexpr float RelicX = 5050.f;
inline constexpr float SpawnX = 5250.f;
inline constexpr float MapHalfX = 6400.f;
inline constexpr float MapHalfY = 3300.f;
inline ERGTeam Enemy(ERGTeam T)
{
    return T == ERGTeam::Red ? ERGTeam::Blue : ERGTeam::Red;
}
inline float Side(ERGTeam T)
{
    return T == ERGTeam::Red ? -1.f : 1.f;
}
inline FLinearColor Color(ERGTeam T)
{
    if (T == ERGTeam::Neutral) return FLinearColor(.65f, .65f, .55f);
    return T == ERGTeam::Red ? FLinearColor(0.85f, 0.075f, 0.14f) : FLinearColor(0.04f, 0.38f, 0.95f);
}
inline FString TeamName(ERGTeam T)
{
    return T == ERGTeam::Red ? TEXT("CRIMSON") : TEXT("AZURE");
}
inline FString ClassName(ERGClass C, bool U = false)
{
    switch (C)
    {
    case ERGClass::Worker:
        return TEXT("Worker");
    case ERGClass::Knight:
        return U ? TEXT("Pike Knight") : TEXT("Knight");
    case ERGClass::Priest:
        return U ? TEXT("Dark Priest") : TEXT("Priest");
    case ERGClass::Mage:
        return U ? TEXT("Ice Mage") : TEXT("Fire Mage");
    case ERGClass::Archer:
        return U ? TEXT("Gunman") : TEXT("Archer");
    case ERGClass::Ninja:
        return TEXT("Ninja");
    default:
        return TEXT("Peasant");
    }
}
} // namespace RG

USTRUCT(BlueprintType)
struct FRGClassDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ERGClass ClassID = ERGClass::Peasant;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkeletalMesh> BodyMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkeletalMesh> UpgradeBodyMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> RunAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> AttackAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> UpgradeAttackAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> CarryAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> BlockAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<UAnimInstance> AnimationClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> HatMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> WeaponMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> UpgradeHatMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> UpgradeWeaponMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> StationHatMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> UpgradeStationHatMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform BodyTransform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform HatTransform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform WeaponTransform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HatSocket = TEXT("head");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WeaponSocket = TEXT("hand_r");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MoveSpeed = 600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackDamage = 14;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackRate = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AttackRange = 170;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AbilityCooldown = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AreaRadius = 480;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HealingAmount = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChargeSeconds = 1.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChargedDamageMultiplier = 2.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WoodCost = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 OreCost = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UTexture2D> Icon;
};

UCLASS(BlueprintType)
class CASTLECTF_API URGClassCatalog : public UDataAsset
{
    GENERATED_BODY()
  public:
    URGClassCatalog();
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FRGClassDefinition> Classes;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") TObjectPtr<USoundBase> HitSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") TObjectPtr<USoundBase> SwingSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") TObjectPtr<USoundBase> PickupSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") TObjectPtr<USoundBase> UpgradeSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") TObjectPtr<USoundBase> RelicSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") TObjectPtr<USoundBase> CaptureSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") TObjectPtr<class UNiagaraSystem> HitEffect;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback") TSubclassOf<class UCameraShakeBase> HitShake;
    const FRGClassDefinition& Get(ERGClass Class) const;
};

USTRUCT(BlueprintType)
struct FRGTeamStock
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Wood = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Ore = 0;
    UPROPERTY(BlueprintReadOnly) int32 Captures = 0;
    UPROPERTY(BlueprintReadOnly) int32 Upgrades = 0;
    bool HasUpgrade(ERGClass C) const
    {
        return (Upgrades & (1 << static_cast<uint8>(C))) != 0;
    }
};
