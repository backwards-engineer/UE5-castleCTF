#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "RGTypes.h"
#include "RGGameState.generated.h"

UCLASS()
class CASTLECTF_API ARGPlayerState : public APlayerState
{
    GENERATED_BODY()
  public:
    UPROPERTY(Replicated, BlueprintReadOnly) ERGTeam Team = ERGTeam::Neutral;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 Kills = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 Deaths = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) ERGClass LastClass = ERGClass::Peasant;
    UPROPERTY(Replicated, BlueprintReadOnly) FString SpawnName = TEXT("Castle");
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class CASTLECTF_API ARGGameState : public AGameStateBase
{
    GENERATED_BODY()
  public:
    UPROPERTY(Replicated, BlueprintReadOnly) FRGTeamStock Red;
    UPROPERTY(Replicated, BlueprintReadOnly) FRGTeamStock Blue;
    UPROPERTY(Replicated, BlueprintReadOnly) ERGPhase Phase = ERGPhase::WaitingForPlayers;
    UPROPERTY(Replicated, BlueprintReadOnly) ERGTeam Winner = ERGTeam::Neutral;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 SecondsRemaining = 10;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 CaptureTarget = 3;
    UPROPERTY(ReplicatedUsing = OnRep_Announcement, BlueprintReadOnly) FString Announcement;
    UFUNCTION() void OnRep_Announcement();
    UPROPERTY(Replicated, BlueprintReadOnly) float AnnouncementUntil = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) ERGTeam AnnouncementTeam = ERGTeam::Neutral;
    UPROPERTY(Replicated, BlueprintReadOnly) ERGRelicState RedRelic = ERGRelicState::Safe;
    UPROPERTY(Replicated, BlueprintReadOnly) ERGRelicState BlueRelic = ERGRelicState::Safe;
    FRGTeamStock& Stock(ERGTeam Team)
    {
        return Team == ERGTeam::Red ? Red : Blue;
    }
    const FRGTeamStock& Stock(ERGTeam Team) const
    {
        return Team == ERGTeam::Red ? Red : Blue;
    }
    bool IsPlaying() const
    {
        return Phase == ERGPhase::Playing;
    }
    void Announce(const FString& Text, ERGTeam Team = ERGTeam::Neutral);
    bool BuyUpgrade(ERGTeam Team, ERGClass Class, const FRGClassDefinition& Def);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
