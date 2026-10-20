#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RGTypes.h"
#include "RGOutpost.generated.h"

UCLASS()
class CASTLECTF_API ARGOutpost : public AActor
{
    GENERATED_BODY()
public:
    ARGOutpost();
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Platform;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Flag;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UTextRenderComponent> Label;
    UPROPERTY(ReplicatedUsing=RefreshVisual, EditAnywhere) ERGTeam Team = ERGTeam::Neutral;
    UPROPERTY(ReplicatedUsing=RefreshVisual, EditAnywhere) FString Name = TEXT("OUTPOST");
    UPROPERTY(ReplicatedUsing=RefreshVisual) float Control = 0;
    UPROPERTY(ReplicatedUsing=RefreshVisual) bool bContested = false;
    UPROPERTY(EditAnywhere) float CaptureRadius = 360;
    UPROPERTY(EditAnywhere) float CaptureSeconds = 9;
    FTimerHandle CaptureTimer;
    UFUNCTION() void RefreshVisual();
    void CaptureStep();
    bool CanSpawn(ERGTeam ForTeam) const;
    FVector SpawnLocation(ERGTeam ForTeam) const;
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};

// A short-lived spell perimeter, separate from the removed player markers.
UCLASS()
class CASTLECTF_API ARGAreaEffect : public AActor
{
    GENERATED_BODY()
public:
    ARGAreaEffect();
    UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Segments;
    float Age = 0;
    float Radius = 480;
    void Init(float InRadius, FLinearColor Color);
    virtual void Tick(float Delta) override;
};
