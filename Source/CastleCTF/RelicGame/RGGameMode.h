#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "RGTypes.h"
#include "RGGameMode.generated.h"
class ARGCharacter;

UCLASS(Config = Game)
class CASTLECTF_API ARGGameMode : public AGameModeBase
{
    GENERATED_BODY()
  public:
    ARGGameMode();
    UPROPERTY(EditAnywhere, Config) int32 CapturesToWin = 3;
    UPROPERTY(EditAnywhere, Config) int32 CountdownSeconds = 10;
    UPROPERTY(EditAnywhere, Config) int32 VictorySeconds = 15;
    UPROPERTY(EditAnywhere, Config) int32 MatchSeconds = 900;
    UPROPERTY(EditAnywhere, Config) int32 BotCount = 18;
    UPROPERTY(EditAnywhere, Config) float RespawnSeconds = 4;
    UPROPERTY(EditAnywhere) TObjectPtr<URGClassCatalog> ClassCatalog;
    FTimerHandle MatchTimer;
    FTimerHandle CaptureTimer;
    int32 NextBot = 0;
    ERGTeam BalancedTeam() const;
    ARGCharacter* SpawnFighter(AController* Controller, ERGTeam Team, bool bForwardSpawn = false);
    class ARGOutpost* ForwardOutpost(ERGTeam Team) const;
    void Respawn(AController* Controller, ERGTeam Team);
    void MatchStep();
    void CheckCaptures();
    void StartBots();
    void Win(ERGTeam Team);
    void RunSelfTest();
    void ReportBotSoak();
    void RunAbilityTest();
    void RunMotionTest();
    void RunFrontierTest();
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};

UCLASS()
class CASTLECTF_API ARGPlayerController : public APlayerController
{
    GENERATED_BODY()
  public:
    ARGPlayerController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupInputComponent() override;
    TSharedPtr<class SWidget> MenuWidget;
    bool bMenuOpen = false;
    bool bLargeMap = false;
    void ToggleMap() { if (!bMenuOpen) bLargeMap=!bLargeMap; }
    FString MenuMessage;
    FDelegateHandle NetworkFailureHandle;
    void ToggleMenu();
    void ShowMenu();
    void CloseMenu();
    void ApplyPlayerSettings();
    UFUNCTION(Exec) void HostMatch();
    UFUNCTION(Exec) void JoinMatch(const FString& Address);
};

UCLASS()
class CASTLECTF_API ARGFrontEndGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ARGFrontEndGameMode();
    virtual void BeginPlay() override;
};
