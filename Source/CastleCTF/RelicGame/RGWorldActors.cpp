#include "RGWorldActors.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGGameState.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"

void RGVisual::Shape(UStaticMeshComponent* M, const TCHAR* Name)
{
    M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name)));
}
void RGVisual::Tint(UStaticMeshComponent* M, FLinearColor C)
{
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Crownfall/Art/M_Palette.M_Palette"));
    if (!Base)
        Base =
            LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    UMaterialInstanceDynamic* Existing=Cast<UMaterialInstanceDynamic>(M->GetMaterial(0));
    if (UMaterialInstanceDynamic* D = Existing && Existing->Parent==Base ? Existing : UMaterialInstanceDynamic::Create(Base, M))
    {
        D->SetVectorParameterValue(TEXT("Color"), C);
        M->SetMaterial(0, D);
    }
}
ARGCarryable::ARGCarryable()
{
    bReplicates = true;
    SetReplicateMovement(true);
    bAlwaysRelevant = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelicBase"));
    SetRootComponent(Mesh);
    RGVisual::Shape(Mesh, TEXT("Cylinder"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetRelativeScale3D(FVector(.7f, .7f, .18f));
    Jewel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelicGem"));
    Jewel->SetupAttachment(Mesh);
    RGVisual::Shape(Jewel, TEXT("Sphere"));
    Jewel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Jewel->SetRelativeLocation(FVector(0, 0, 150));
    Jewel->SetRelativeScale3D(FVector(.55f, .55f, 3.2f));
    for(int32 Index=0;Index<6;++Index)
    {
        UStaticMeshComponent* Point=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("CrownPoint%d"),Index));
        Point->SetupAttachment(Mesh); RGVisual::Shape(Point,TEXT("Cone")); Point->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const float Angle=Index*PI/3;
        Point->SetRelativeLocation(FVector(FMath::Cos(Angle)*36,FMath::Sin(Angle)*36,120));
        Point->SetRelativeScale3D(FVector(.2f,.2f,2.7f)); CrownPoints.Add(Point);
    }
}
void ARGCarryable::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
        Home = GetActorLocation();
    OnRep_State();
}
void ARGCarryable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARGCarryable, Carrier);
    DOREPLIFETIME(ARGCarryable, Team);
    DOREPLIFETIME(ARGCarryable, bRelic);
    DOREPLIFETIME(ARGCarryable, State);
    DOREPLIFETIME(ARGCarryable, Home);
    DOREPLIFETIME(ARGCarryable, Wood);
    DOREPLIFETIME(ARGCarryable, Ore);
}
bool ARGCarryable::CanPickup(ARGCharacter* P) const
{
    ARGGameState* S = GetWorld()->GetGameState<ARGGameState>();
    return P && P->IsAlive() && S && S->IsPlaying() && !Carrier && (!bRelic || P->Team != Team) &&
           FVector::DistSquared(P->GetActorLocation(), GetActorLocation()) < FMath::Square(220.f);
}
void ARGCarryable::OnPickedUp(ARGCharacter* P)
{
    if (!HasAuthority())
        return;
    GetWorldTimerManager().ClearTimer(ReturnTimer);
    Carrier = P;
    SetState(ERGRelicState::Stolen);
    OnRep_Carrier();
    if (bRelic)
        if (ARGGameState* S = GetWorld()->GetGameState<ARGGameState>())
            S->Announce(RG::TeamName(Team) + TEXT(" RELIC STOLEN"), P->Team);
}
void ARGCarryable::OnRep_Carrier()
{
    if (Carrier)
    {
        AttachToComponent(Carrier->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
        SetActorRelativeLocation(FVector(0, 0, 155));
        SetActorRelativeRotation(FRotator::ZeroRotator);
    }
    else
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}
void ARGCarryable::OnDropped()
{
    if (!HasAuthority())
        return;
    FVector Where = Carrier ? Carrier->GetActorLocation() + Carrier->GetActorForwardVector() * 90 : GetActorLocation();
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Carrier = nullptr;
    FHitResult Hit;
    FCollisionQueryParams Q;
    Q.AddIgnoredActor(this);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Where + FVector(0, 0, 100), Where - FVector(0, 0, 1000),
                                             ECC_WorldStatic, Q))
        Where.Z = Hit.ImpactPoint.Z + 50;
    SetActorLocation(Where);
    SetState(ERGRelicState::Dropped);
    if (bRelic)
        GetWorldTimerManager().SetTimer(ReturnTimer, this, &ARGCarryable::ReturnHome, AutoReturnSeconds, false);
    else
        SetLifeSpan(30);
}
void ARGCarryable::ReturnHome()
{
    if (!HasAuthority())
        return;
    GetWorldTimerManager().ClearTimer(ReturnTimer);
    if (Carrier)
    {
        Carrier->Carry->Carried = nullptr;
        Carrier->Carry->OnRep_Carried();
    }
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Carrier = nullptr;
    SetActorLocation(Home);
    SetState(ERGRelicState::Safe);
    ForceNetUpdate();
}
void ARGCarryable::SetState(ERGRelicState NewState)
{
    State = NewState;
    OnRep_State();
    if (bRelic)
        if (ARGGameState* S = GetWorld()->GetGameState<ARGGameState>())
        {
            if (Team == ERGTeam::Red)
                S->RedRelic = State;
            else
                S->BlueRelic = State;
        }
    ForceNetUpdate();
}
void ARGCarryable::OnRep_State()
{
    for(UStaticMeshComponent* Point:CrownPoints) { Point->SetVisibility(bRelic); RGVisual::Tint(Point,FLinearColor(.95f,.65f,.12f)); }
    RGVisual::Tint(Mesh, bRelic ? FLinearColor(.95f, .65f, .12f) : FLinearColor(.42f, .22f, .08f));
    RGVisual::Tint(Jewel, bRelic ? RG::Color(Team) : FLinearColor(.7f, .8f, .9f));
}

ARGStation::ARGStation()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
    SetRootComponent(Mesh);
    RGVisual::Shape(Mesh, TEXT("Cylinder"));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Accent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Accent"));
    Accent->SetupAttachment(Mesh);
    RGVisual::Shape(Accent, TEXT("Cone"));
    Accent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(Mesh);
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(22);
    Label->SetRelativeRotation(FRotator(42, 90, 0));
    Label->SetTextRenderColor(FColor(245, 228, 178));
}
void ARGStation::BeginPlay()
{
    Super::BeginPlay();
    RefreshVisual();
    if (!Catalog)
        Catalog = LoadObject<URGClassCatalog>(nullptr, TEXT("/Game/Crownfall/Data/DA_Classes.DA_Classes"));
    RefreshVisual();
}
void ARGStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARGStation, Kind);
    DOREPLIFETIME(ARGStation, Team);
    DOREPLIFETIME(ARGStation, ClassID);
    DOREPLIFETIME(ARGStation, Health);
    DOREPLIFETIME(ARGStation, bDepleted);
    DOREPLIFETIME(ARGStation, bUpgraded);
}
void ARGStation::RefreshVisual()
{
    if (HasAuthority())
        if (ARGGameState* S = GetWorld()->GetGameState<ARGGameState>())
            bUpgraded = S->Stock(Team).HasUpgrade(ClassID);
    FLinearColor C = RG::Color(Team);
    FString T;
    if (Kind == ERGStationType::Gate)
    {
        RGVisual::Shape(Mesh, TEXT("Cube"));
        Mesh->SetWorldScale3D(FVector(1.2f, 6.6f, 3.6f));
        RGVisual::Tint(Mesh, RG::Color(Team)*.28f+FLinearColor(.18f,.12f,.07f));
        Mesh->SetCollisionEnabled(Health > 0 ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Mesh->SetVisibility(Health > 0);
        Accent->SetVisibility(false);
        for (TActorIterator<ARGCharacter> It(GetWorld());It;++It)
            It->GetCapsuleComponent()->IgnoreActorWhenMoving(this,It->Team==Team);
        T = Health > 0 ? FString::Printf(TEXT("GATE  %d%%"), FMath::RoundToInt(100 * Health / MaxHealth))
                       : TEXT("GATE BREACHED");
    }
    else if (Kind == ERGStationType::Wood || Kind == ERGStationType::Ore)
    {
        bool W = Kind == ERGStationType::Wood;
        RGVisual::Shape(Mesh, W ? TEXT("Cylinder") : TEXT("Sphere"));
        Mesh->SetWorldScale3D(W ? FVector(.65f, .65f, 1.9f) : FVector(1.6f, 1.25f, 1));
        RGVisual::Tint(Mesh, W ? FLinearColor(.23f, .12f, .06f) : FLinearColor(.19f, .24f, .32f));
        Accent->SetRelativeLocation(FVector(0, 0, W ? 75 : 30));
        Accent->SetRelativeScale3D(W ? FVector(4, 4, 1.1f) : FVector(.55f, .55f, 1.2f));
        RGVisual::Tint(Accent, W ? FLinearColor(.09f, .32f, .19f) : FLinearColor(.18f, .65f, .85f));
        Accent->SetVisibility(!bDepleted);
        T = bDepleted ? TEXT("REGROWING") : W ? TEXT("WOOD") : TEXT("ORE");
    }
    else
    {
        Mesh->SetWorldScale3D(FVector(1, 1, .25f));
        RGVisual::Tint(Mesh, FLinearColor(.14f, .19f, .25f));
        Accent->SetRelativeLocation(FVector(0, 0, 155));
        Accent->SetRelativeScale3D(FVector(.65f, .65f, 2.5f));
        RGVisual::Tint(Accent, bUpgraded ? FLinearColor(1, .7f, .13f) : C);
        T = Kind == ERGStationType::Deposit   ? TEXT("STOCKPILE")
            : Kind == ERGStationType::Upgrade ? TEXT("FORGE  ") + RG::ClassName(ClassID, bUpgraded)
                                              : RG::ClassName(ClassID, bUpgraded).ToUpper();
        if (Kind == ERGStationType::Hat && Catalog)
        {
            const FRGClassDefinition& D = Catalog->Get(ClassID);
            UStaticMesh* H =
                bUpgraded && D.UpgradeStationHatMesh ? D.UpgradeStationHatMesh.Get() : D.StationHatMesh.Get();
            if (H)
            {
                Accent->SetStaticMesh(H);
                Accent->SetWorldScale3D(FVector(1.25f));
                Accent->SetWorldLocation(GetActorLocation() + FVector(0, 0, 100) - H->GetBounds().Origin * 1.25f);
                for (int32 I = 0; I < H->GetStaticMaterials().Num(); ++I)
                    Accent->SetMaterial(I, H->GetStaticMaterials()[I].MaterialInterface);
                Accent->SetVectorParameterValueOnMaterials(TEXT("TeamColor"), FVector(RG::Color(Team)));
            }
        }
    }
    Label->SetWorldScale3D(FVector(1));
    Label->SetWorldLocation(GetActorLocation() + FVector(0, 0, Kind == ERGStationType::Gate ? 190 : 155));
    Label->SetText(FText::FromString(T));
}
FString ARGStation::Prompt(ARGCharacter* P) const
{
    if (!P)
        return FString();
    if (Kind == ERGStationType::Wood || Kind == ERGStationType::Ore)
        return bDepleted ? FString(TEXT("Resource replenishing"))
                         : FString(TEXT("LMB  Harvest ")) + (Kind == ERGStationType::Wood ? TEXT("wood") : TEXT("ore"));
    if (P->Team != Team)
        return Kind == ERGStationType::Gate ? TEXT("LMB  Break enemy gate") : FString();
    if (Kind == ERGStationType::Hat)
        return TEXT("E  Wear ") + RG::ClassName(ClassID, bUpgraded);
    if (Kind == ERGStationType::Deposit)
        return TEXT("E  Deposit resources");
    if (Kind == ERGStationType::Gate)
        return P->Classes->ClassID==ERGClass::Worker ? TEXT("Friendly gate / E repair for 2 wood") : TEXT("Friendly gate / walk through");
    const FRGClassDefinition& D = (Catalog ? Catalog.Get() : GetDefault<URGClassCatalog>())->Get(ClassID);
    return bUpgraded ? TEXT("Team upgrade active")
                     : FString::Printf(TEXT("E  Unlock %s  /  %d wood + %d ore"), *RG::ClassName(ClassID, true),
                                       D.WoodCost, D.OreCost);
}
void ARGStation::Interact(ARGCharacter* P)
{
    ARGGameState* S = GetWorld()->GetGameState<ARGGameState>();
    if (!HasAuthority() || !P || !P->IsAlive() || !S || !S->IsPlaying() ||
        FVector::Dist2D(P->GetActorLocation(), GetActorLocation()) > 250)
        return;
    if (Kind == ERGStationType::Wood || Kind == ERGStationType::Ore)
    {
        Hit(P, 10);
        return;
    }
    if (P->Team != Team)
        return;
    if (Kind == ERGStationType::Hat && !P->Carry->Carried)
        P->Classes->SetClass(ClassID);
    else if (Kind == ERGStationType::Upgrade)
        S->BuyUpgrade(Team, ClassID, (Catalog ? Catalog.Get() : GetDefault<URGClassCatalog>())->Get(ClassID));
    else if (Kind == ERGStationType::Deposit)
    {
        if (P->Carry->Carried && !P->Carry->Carried->bRelic)
        {
            P->PackWood += P->Carry->Carried->Wood;
            P->PackOre += P->Carry->Carried->Ore;
            P->Carry->Carried->Destroy();
            P->Carry->Carried = nullptr;
            P->RefreshSpeed();
        }
        int32 Total = P->PackWood + P->PackOre;
        S->Stock(Team).Wood += P->PackWood;
        S->Stock(Team).Ore += P->PackOre;
        P->PackWood = 0;
        P->PackOre = 0;
        if (Total)
            P->MulticastFeedback(GetActorLocation() + FVector(0, 0, 90), FLinearColor(1, .75f, .2f), -Total);
    }
    else if (Kind == ERGStationType::Gate && P->Classes->ClassID == ERGClass::Worker && Health < MaxHealth &&
             S->Stock(Team).Wood >= RepairWoodCost)
    {
        S->Stock(Team).Wood -= RepairWoodCost;
        Health = FMath::Min(MaxHealth, Health + 65);
        RefreshVisual();
    }
}
void ARGStation::Hit(ARGCharacter* P, float Damage)
{
    if (!HasAuthority() || !P || !P->IsAlive())
        return;
    if (Kind == ERGStationType::Gate && Team != P->Team && Health > 0)
    {
        Health = FMath::Max(0.f, Health - Damage);
        P->MulticastFeedback(GetActorLocation(), FLinearColor(.9f, .6f, .2f), Damage);
        RefreshVisual();
    }
    else if ((Kind == ERGStationType::Wood || Kind == ERGStationType::Ore) && !bDepleted)
    {
        int32 Amount = P->Classes->ClassID == ERGClass::Worker ? Yield : 1;
        if (P->PackWood + P->PackOre >= 30)
            return;
        Amount = FMath::Min(Amount, 30 - P->PackWood - P->PackOre);
        if (Kind == ERGStationType::Wood)
            P->PackWood += Amount;
        else
            P->PackOre += Amount;
        Health -= P->Classes->ClassID == ERGClass::Worker ? 100 : 25;
        P->MulticastFeedback(GetActorLocation() + FVector(0, 0, 110), FLinearColor(.85f, .7f, .25f), -Amount);
        if (Health <= 0)
        {
            bDepleted = true;
            RefreshVisual();
            GetWorldTimerManager().SetTimer(RegrowTimer,
                                            FTimerDelegate::CreateWeakLambda(this,
                                                                             [this]() {
                                                                                 Health = MaxHealth;
                                                                                 bDepleted = false;
                                                                                 RefreshVisual();
                                                                             }),
                                            RegrowSeconds, false);
        }
    }
}

ARGProjectile::ARGProjectile()
{
    bReplicates = true;
    SetReplicateMovement(true);
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(14);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Collision->SetCollisionObjectType(ECC_GameTraceChannel1);
    Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Ignore);
    Collision->SetNotifyRigidBodyCollision(true);
    Collision->OnComponentHit.AddDynamic(this, &ARGProjectile::Impact);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bolt"));
    Mesh->SetupAttachment(Collision);
    RGVisual::Shape(Mesh, TEXT("Sphere"));
    Mesh->SetRelativeScale3D(FVector(.4f, .15f, .15f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->UpdatedComponent = Collision;
    Movement->InitialSpeed = 1800;
    Movement->MaxSpeed = 1800;
    Movement->ProjectileGravityScale = 0;
    Movement->bRotationFollowsVelocity = true;
    InitialLifeSpan = 1.2f;
}
void ARGProjectile::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner())
        Collision->IgnoreActorWhenMoving(GetOwner(), true);
    RefreshVisual();
}
void ARGProjectile::RefreshVisual()
{
    RGVisual::Tint(Mesh, ClassID == ERGClass::Mage ? (bIce ? FLinearColor(.1f, .8f, 1) : FLinearColor(1, .25f, .02f))
                         : ClassID == ERGClass::Priest ? FLinearColor(.7f, .3f, 1)
                                                       : FLinearColor(1, .85f, .4f));
}
void ARGProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARGProjectile, Team);
    DOREPLIFETIME(ARGProjectile, ClassID);
    DOREPLIFETIME(ARGProjectile, bIce);
}
void ARGProjectile::Impact(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
    if (!HasAuthority() || Other == Shooter)
        return;
    if (ARGCharacter* P = Cast<ARGCharacter>(Other))
    {
        if (P->Team != Team && IsValid(Shooter))
        {
            P->Health->Damage(Damage, Shooter, GetActorForwardVector());
            if (bIce && ClassID == ERGClass::Mage)
                P->Combat->SlowUntil = GetWorld()->GetTimeSeconds() + 2.5f;
            if (bIce && ClassID == ERGClass::Priest)
                Shooter->Health->Heal(Damage * .45f);
            P->LaunchCharacter(GetActorForwardVector() * Knockback, false, false);
        }
    }
    else if (ARGStation* S = Cast<ARGStation>(Other))
    {
        if (IsValid(Shooter))
            S->Hit(Shooter, Damage);
    }
    if (IsValid(Shooter))
        Shooter->MulticastFeedback(Hit.ImpactPoint, RG::Color(Team), 0);
    Destroy();
}
ARGEffect::ARGEffect()
{
    PrimaryActorTick.bCanEverTick = true;
    InitialLifeSpan = .55f;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pulse"));
    SetRootComponent(Mesh);
    RGVisual::Shape(Mesh, TEXT("Sphere"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Amount"));
    Text->SetupAttachment(Mesh);
    Text->SetWorldSize(28);
    Text->SetHorizontalAlignment(EHTA_Center);
    Text->SetRelativeRotation(FRotator(42, 90, 0));
}
void ARGEffect::Init(FLinearColor Color, float Amount)
{
    RGVisual::Tint(Mesh, Color);
    Text->SetTextRenderColor(Color.ToFColor(true));
    Text->SetText(Amount == 0 ? FText::GetEmpty()
                              : FText::FromString(FString::Printf(TEXT("%s%.0f"), Amount < 0 ? TEXT("+") : TEXT(""),
                                                                  FMath::Abs(Amount))));
}
void ARGEffect::Tick(float Delta)
{
    Super::Tick(Delta);
    Age += Delta;
    Mesh->SetWorldScale3D(FVector(.15f + Age * .9f));
    Text->SetWorldScale3D(FVector(1));
    AddActorWorldOffset(FVector(0, 0, Delta * 120));
}
