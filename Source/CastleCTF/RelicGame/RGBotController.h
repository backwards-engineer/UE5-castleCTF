#pragma once
#include "AIController.h"
#include "CoreMinimal.h"
#include "RGTypes.h"
#include "RGBotController.generated.h"
class ARGCharacter;
UCLASS()
class CASTLECTF_API ARGBotController : public AAIController
{
    GENERATED_BODY()
  public:
    ARGBotController();
    UPROPERTY(EditAnywhere) ERGBotRole BotRole = ERGBotRole::Attacker;
    UPROPERTY(EditAnywhere) ERGTeam Team = ERGTeam::Red;
    UPROPERTY(EditAnywhere) ERGClass PreferredClass = ERGClass::Knight;
    FTimerHandle BrainTimer;
    FVector Destination = FVector::ZeroVector;
    FVector Detour = FVector::ZeroVector;
    bool bDetouring = false;
    float NextPathCheck = 0;
    void Think();
    void GoTo(FVector Target);
    virtual void OnPossess(APawn* Pawn) override;
    virtual void Tick(float Delta) override;
};
