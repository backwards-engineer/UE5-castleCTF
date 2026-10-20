#include "RGArena.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "RGWorldActors.h"
#include "RGOutpost.h"
#include "Net/UnrealNetwork.h"

ARGArena::ARGArena()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Static);
}
void ARGArena::Part(const TCHAR* Shape, FVector Pos, FVector Scale, FLinearColor Color, FRotator Rotation,
                    bool Collision)
{
    UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("ArenaPart_%03d"),PartIndex++));
    C->SetNetAddressable();
    C->SetupAttachment(RootComponent);
    C->SetMobility(EComponentMobility::Static);
    RGVisual::Shape(C, Shape);
    C->SetRelativeLocation(Pos);
    C->SetRelativeScale3D(Scale);
    C->SetRelativeRotation(Rotation);
    C->SetCollisionEnabled(Collision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    C->SetCollisionObjectType(ECC_WorldStatic);
    C->SetCollisionResponseToAllChannels(ECR_Block);
    C->RegisterComponent();
    RGVisual::Tint(C, Color);
}
void ARGArena::BeginPlay()
{
    Super::BeginPlay();
    BuildLandscape();
    if (HasAuthority() && bSpawnGameplay)
        SpawnGameplay();
}
void ARGArena::SpawnGameplay()
{
    const FVector Points[]={FVector(-2250,-650,5),FVector(0,0,28),FVector(2250,-650,5)};
    const TCHAR* Names[]={TEXT("Westwatch"),TEXT("Riverkeep"),TEXT("Eastwatch")};
    for (int I=0;I<3;++I)
    {
        auto Post=GetWorld()->SpawnActor<ARGOutpost>(Points[I],FRotator::ZeroRotator);
        Post->Name=Names[I]; Post->RefreshVisual();
    }

    auto Station = [this](ERGStationType Kind, ERGTeam T, ERGClass Class, FVector Location) {
        ARGStation* A = GetWorld()->SpawnActor<ARGStation>(Location, FRotator::ZeroRotator);
        A->Kind = Kind;
        A->Team = T;
        A->ClassID = Class;
        A->Catalog = Catalog;
        A->RefreshVisual();
        return A;
    };
    for (ERGTeam T : {ERGTeam::Red, ERGTeam::Blue})
    {
        const float S = RG::Side(T);
        ARGCarryable* Relic = GetWorld()->SpawnActor<ARGCarryable>(FVector(S * RG::RelicX, 530, 80), FRotator::ZeroRotator);
        Relic->Team = T;
        Relic->Home = Relic->GetActorLocation();
        Relic->SetState(ERGRelicState::Safe);
        Station(ERGStationType::Gate, T, ERGClass::Worker, FVector(S * RG::CastleGateX, 0, 175));
        Station(ERGStationType::Deposit, T, ERGClass::Worker, FVector(S * 4330, 650, 30));
        for (int I = 1; I <= 6; ++I)
            Station(ERGStationType::Hat, T, static_cast<ERGClass>(I),
                    FVector(S * (4380 + (I - 1) % 3 * 330), -300 - (I - 1) / 3 * 280, 25));
        for (int I = 2; I <= 5; ++I)
            Station(ERGStationType::Upgrade, T, static_cast<ERGClass>(I),
                    FVector(S * (4360 + (I - 2) * 290), 1020, 25));
        for (int I = 0; I < 3; ++I)
        {
            Station(ERGStationType::Wood, ERGTeam::Neutral, ERGClass::Worker, FVector(S * (1900 + I * 380), -2050, 85));
            Station(ERGStationType::Ore, ERGTeam::Neutral, ERGClass::Worker, FVector(S * (1900 + I * 380), 2070, 55));
        }
    }
}

void ARGArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARGArena,bSpawnGameplay);
}
