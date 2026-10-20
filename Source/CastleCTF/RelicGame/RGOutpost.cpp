#include "RGOutpost.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "RGCharacter.h"
#include "RGGameState.h"
#include "RGWorldActors.h"
#include "TimerManager.h"

ARGOutpost::ARGOutpost()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CaptureCourt"));
    Platform->SetupAttachment(RootComponent);
    RGVisual::Shape(Platform, TEXT("Cylinder"));
    Platform->SetRelativeScale3D(FVector(7.2f, 7.2f, .12f));
    Platform->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto Pole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flagpole"));
    Pole->SetupAttachment(RootComponent);
    RGVisual::Shape(Pole, TEXT("Cylinder"));
    Pole->SetRelativeLocation(FVector(0,0,150));
    Pole->SetRelativeScale3D(FVector(.1f,.1f,3));
    Pole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Flag = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Banner"));
    Flag->SetupAttachment(RootComponent);
    RGVisual::Shape(Flag, TEXT("Cube"));
    Flag->SetRelativeLocation(FVector(0,60,260));
    Flag->SetRelativeScale3D(FVector(.035f,1.2f,.7f));
    Flag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Name"));
    Label->SetupAttachment(RootComponent);
    Label->SetRelativeLocation(FVector(0,0,345));
    Label->SetRelativeRotation(FRotator(42,90,0));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(30);
}
void ARGOutpost::BeginPlay()
{
    Super::BeginPlay();
    RefreshVisual();
    if (HasAuthority()) GetWorldTimerManager().SetTimer(CaptureTimer,this,&ARGOutpost::CaptureStep,.2f,true);
}
void ARGOutpost::CaptureStep()
{
    const auto State = GetWorld()->GetGameState<ARGGameState>();
    if (!HasAuthority() || !State || !State->IsPlaying()) return;
    int32 Red=0, Blue=0;
    for (TActorIterator<ARGCharacter> It(GetWorld()); It; ++It)
        if (It->IsAlive() && FVector::Dist2D(It->GetActorLocation(),GetActorLocation()) < CaptureRadius)
        {
            if (It->Team == ERGTeam::Red) ++Red;
            if (It->Team == ERGTeam::Blue) ++Blue;
        }
    bContested = Red > 0 && Blue > 0;
    if (!bContested && (Red || Blue))
    {
        const float Direction = Red ? -1.f : 1.f;
        Control = FMath::Clamp(Control + Direction * .2f / CaptureSeconds * FMath::Min(2,Red+Blue),-1.f,1.f);
        const ERGTeam Before = Team;
        if ((Team == ERGTeam::Red && Control >= 0) || (Team == ERGTeam::Blue && Control <= 0)) Team=ERGTeam::Neutral;
        if (Control <= -.999f) Team=ERGTeam::Red;
        if (Control >= .999f) Team=ERGTeam::Blue;
        if (Before != Team && Team != ERGTeam::Neutral)
            State->Announce(RG::TeamName(Team) + TEXT(" secured ") + Name,Team);
    }
    RefreshVisual();
}
bool ARGOutpost::CanSpawn(ERGTeam ForTeam) const
{
    if (Team != ForTeam || bContested) return false;
    for (TActorIterator<ARGCharacter> It(GetWorld()); It; ++It)
        if (It->IsAlive() && It->Team != ForTeam && FVector::Dist2D(It->GetActorLocation(),GetActorLocation())<650) return false;
    return true;
}
FVector ARGOutpost::SpawnLocation(ERGTeam ForTeam) const
{
    return GetActorLocation() + FVector(RG::Side(ForTeam)*240,0,95);
}
void ARGOutpost::RefreshVisual()
{
    RGVisual::Tint(Flag, RG::Color(Team));
    RGVisual::Tint(Platform, RG::Color(Team)*.4f + FLinearColor(.12f,.14f,.12f));
    Label->SetText(FText::FromString(Name.ToUpper() + (bContested ? TEXT(" / CONTESTED") : FString::Printf(TEXT(" / %d%%"),FMath::RoundToInt(FMath::Abs(Control)*100)))));
    Label->SetTextRenderColor(RG::Color(Team).ToFColor(true));
}
void ARGOutpost::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARGOutpost,Team); DOREPLIFETIME(ARGOutpost,Name);
    DOREPLIFETIME(ARGOutpost,Control); DOREPLIFETIME(ARGOutpost,bContested);
}
ARGAreaEffect::ARGAreaEffect()
{
    PrimaryActorTick.bCanEverTick=true;
    InitialLifeSpan=.65f;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    for (int32 I=0;I<48;++I)
    {
        auto Segment=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Arc%d"),I));
        Segment->SetupAttachment(RootComponent);
        RGVisual::Shape(Segment,TEXT("Cube"));
        Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Segment->SetCastShadow(false);
        Segments.Add(Segment);
    }
}
void ARGAreaEffect::Init(float InRadius,FLinearColor Color)
{
    Radius=InRadius;
    for (int32 I=0;I<Segments.Num();++I)
    {
        const float A=2*PI*I/Segments.Num();
        Segments[I]->SetRelativeLocation(FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,0));
        Segments[I]->SetRelativeRotation(FRotator(0,FMath::RadiansToDegrees(A)+90,0));
        Segments[I]->SetRelativeScale3D(FVector(Radius*.00135f,.065f,.055f));
        RGVisual::Tint(Segments[I],Color);
    }
}
void ARGAreaEffect::Tick(float Delta)
{
    Super::Tick(Delta); Age+=Delta;
    SetActorScale3D(FVector(1+Age*.08f,1+Age*.08f,1));
}
