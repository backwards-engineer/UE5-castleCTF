#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RGTypes.h"
#include "RGArena.generated.h"
UCLASS()
class CASTLECTF_API ARGArena : public AActor
{
    GENERATED_BODY()
  public:
    ARGArena();
    UPROPERTY(EditAnywhere) TObjectPtr<URGClassCatalog> Catalog;
    UPROPERTY(Replicated) bool bSpawnGameplay = true;
    int32 PartIndex=0;
    void BuildLandscape();
    void SpawnGameplay();
    void Part(const TCHAR* Shape, FVector Position, FVector Scale, FLinearColor Color,
              FRotator Rotation = FRotator::ZeroRotator, bool Collision = true);
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};
