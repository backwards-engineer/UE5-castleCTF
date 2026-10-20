#include "RGGameState.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGWorldActors.h"
#include "Sound/SoundBase.h"
void ARGPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARGPlayerState, Team);
    DOREPLIFETIME(ARGPlayerState, Kills);
    DOREPLIFETIME(ARGPlayerState, Deaths);
    DOREPLIFETIME(ARGPlayerState, LastClass);
    DOREPLIFETIME(ARGPlayerState, SpawnName);
}
void ARGGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARGGameState, Red);
    DOREPLIFETIME(ARGGameState, Blue);
    DOREPLIFETIME(ARGGameState, Phase);
    DOREPLIFETIME(ARGGameState, Winner);
    DOREPLIFETIME(ARGGameState, SecondsRemaining);
    DOREPLIFETIME(ARGGameState, CaptureTarget);
    DOREPLIFETIME(ARGGameState, Announcement);
    DOREPLIFETIME(ARGGameState, AnnouncementUntil);
    DOREPLIFETIME(ARGGameState, AnnouncementTeam);
    DOREPLIFETIME(ARGGameState, RedRelic);
    DOREPLIFETIME(ARGGameState, BlueRelic);
}
void ARGGameState::Announce(const FString& Text, ERGTeam Team)
{
    if (!HasAuthority())
        return;
    Announcement = Text;
    AnnouncementTeam = Team;
    AnnouncementUntil = GetServerWorldTimeSeconds() + 4;
    ForceNetUpdate();
    OnRep_Announcement();
    UE_LOG(LogTemp, Display, TEXT("[Crownfall] %s"), *Text);
}
void ARGGameState::OnRep_Announcement()
{
    if (GetNetMode() == NM_DedicatedServer)
        return;
    if (URGClassCatalog* C = LoadObject<URGClassCatalog>(nullptr, TEXT("/Game/Crownfall/Data/DA_Classes.DA_Classes")))
        if (USoundBase* Cue = Announcement.Contains(TEXT("CAPTURE")) || Announcement.Contains(TEXT("VICTORY"))
                                  ? C->CaptureSound.Get()
                              : Announcement.Contains(TEXT("unlocked")) ? C->UpgradeSound.Get()
                                                                        : C->RelicSound.Get())
            UGameplayStatics::PlaySound2D(this, Cue, .5f);
}
bool ARGGameState::BuyUpgrade(ERGTeam Team, ERGClass Class, const FRGClassDefinition& Def)
{
    if (!HasAuthority() || !IsPlaying() || Team == ERGTeam::Neutral || Class < ERGClass::Knight ||
        Class > ERGClass::Archer)
        return false;
    FRGTeamStock& S = Stock(Team);
    if (S.HasUpgrade(Class) || S.Wood < Def.WoodCost || S.Ore < Def.OreCost)
        return false;
    S.Wood -= Def.WoodCost;
    S.Ore -= Def.OreCost;
    S.Upgrades |= 1 << static_cast<uint8>(Class);
    for (TActorIterator<ARGCharacter> It(GetWorld()); It; ++It)
        if (It->Team == Team && It->Classes->ClassID == Class)
            It->Classes->SetClass(Class);
    for (TActorIterator<ARGStation> It(GetWorld()); It; ++It)
        if (It->Team == Team && It->ClassID == Class)
            It->RefreshVisual();
    Announce(RG::TeamName(Team) + TEXT(" unlocked ") + RG::ClassName(Class, true), Team);
    return true;
}
