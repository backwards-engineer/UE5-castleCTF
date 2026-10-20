#include "RGCharacter.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RGComponents.h"
#include "RGGameMode.h"
#include "RGGameState.h"
#include "RGWorldActors.h"
#include "RGOutpost.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

ARGCharacter::ARGCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    GetCapsuleComponent()->InitCapsuleSize(32, 65);
    GetCharacterMovement()->MaxAcceleration = 5000;
    GetCharacterMovement()->BrakingDecelerationWalking = 4500;
    GetCharacterMovement()->GroundFriction = 10;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->JumpZVelocity = 400;
    bUseControllerRotationYaw = false;
    Health = CreateDefaultSubobject<URGHealthComponent>(TEXT("Health"));
    Classes = CreateDefaultSubobject<URGClassComponent>(TEXT("Classes"));
    Combat = CreateDefaultSubobject<URGCombatComponent>(TEXT("Combat"));
    Carry = CreateDefaultSubobject<URGCarryComponent>(TEXT("Carry"));
    Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("BattlefieldArm"));
    Boom->SetupAttachment(GetRootComponent());
    Boom->TargetArmLength = 1350;
    Boom->SetUsingAbsoluteRotation(true);
    Boom->SetRelativeRotation(FRotator(-42, -90, 0));
    Boom->bDoCollisionTest = false;
    Boom->bEnableCameraLag = true;
    Boom->CameraLagSpeed = 12;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("BattlefieldCamera"));
    Camera->SetupAttachment(Boom);
    Camera->FieldOfView = 55;
    Hat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClassHat"));
    Hat->SetupAttachment(GetMesh());
    Hat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Weapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClassWeapon"));
    Weapon->SetupAttachment(GetMesh());
    Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}
void ARGCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (!Classes->Catalog)
        Classes->Catalog = LoadObject<URGClassCatalog>(nullptr, TEXT("/Game/Crownfall/Data/DA_Classes.DA_Classes"));
    RefreshAppearance();
    RefreshSpeed();
    RefreshGateAccess();
}
void ARGCharacter::OnRep_Team()
{
    RefreshAppearance();
    RefreshGateAccess();
}
void ARGCharacter::RefreshGateAccess()
{
    for (TActorIterator<ARGStation> It(GetWorld());It;++It)
        if (It->Kind==ERGStationType::Gate) GetCapsuleComponent()->IgnoreActorWhenMoving(*It,It->Team==Team);
}
void ARGCharacter::OnRep_Attack()
{
    AttackVisualUntil = GetWorld()->GetTimeSeconds() + .4f;
    CurrentAnimation = nullptr;
    if (GetNetMode() != NM_DedicatedServer)
        if (USoundBase* Cue = Classes->Catalog ? Classes->Catalog->SwingSound.Get() : nullptr)
            UGameplayStatics::PlaySoundAtLocation(this, Cue, GetActorLocation(), .25f);
}
bool ARGCharacter::IsAlive() const
{
    return Health && !Health->bDead;
}
void ARGCharacter::RefreshAppearance()
{
    if (!Classes || !Hat)
        return;
    const FRGClassDefinition& D = Classes->Definition();
    USkeletalMesh* Body = Classes->bUpgraded && D.UpgradeBodyMesh ? D.UpgradeBodyMesh.Get() : D.BodyMesh.Get();
    if (Body)
    {
        GetMesh()->SetSkeletalMesh(Body);
        GetMesh()->SetRelativeTransform(D.BodyTransform);
        CurrentAnimation = nullptr;
        if (D.AnimationClass)
            GetMesh()->SetAnimInstanceClass(D.AnimationClass);
    }
    UStaticMesh* H = Classes->bUpgraded && D.UpgradeHatMesh ? D.UpgradeHatMesh.Get() : D.HatMesh.Get();
    UStaticMesh* W = Classes->bUpgraded && D.UpgradeWeaponMesh ? D.UpgradeWeaponMesh.Get() : D.WeaponMesh.Get();
    if (H)
    {
        Hat->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, D.HatSocket);
        Hat->SetRelativeTransform(D.HatTransform);
    }
    Hat->SetStaticMesh(H);
    if (W)
    {
        Weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, D.WeaponSocket);
        Weapon->SetRelativeTransform(D.WeaponTransform);
    }
    Weapon->SetStaticMesh(W);
    GetMesh()->SetVectorParameterValueOnMaterials(TEXT("TeamColor"), FVector(RG::Color(Team)));
    if (H)
        RGVisual::Tint(Hat, RG::Color(Team));
}
void ARGCharacter::RefreshSpeed()
{
    const float Now = GetWorld()->GetGameState<ARGGameState>()
                          ? GetWorld()->GetGameState<ARGGameState>()->GetServerWorldTimeSeconds()
                          : GetWorld()->GetTimeSeconds();
    GetCharacterMovement()->MaxWalkSpeed = Classes->Definition().MoveSpeed * Carry->SpeedModifier() *
                                           (Combat->SlowUntil > Now ? .55f : 1.f) * (Combat->bBlocking ? .6f : 1.f);
}
void ARGCharacter::Tick(float Delta)
{
    Super::Tick(Delta);
    RefreshSpeed();
    const FRGClassDefinition& D = Classes->Definition();
    if (IsAlive() && !D.AnimationClass)
    {
        UAnimSequence* A = GetVelocity().Size2D() > 30 ? D.RunAnimation.Get() : D.IdleAnimation.Get();
        bool Loop = true;
        if (Carry->Carried)
            A = D.CarryAnimation;
        else if (Combat->bBlocking)
            A = D.BlockAnimation;
        else if (AttackVisualUntil > GetWorld()->GetTimeSeconds())
        {
            A = Classes->bUpgraded && D.UpgradeAttackAnimation ? D.UpgradeAttackAnimation.Get()
                                                               : D.AttackAnimation.Get();
            Loop = false;
        }
        if (A && A != CurrentAnimation)
        {
            GetMesh()->PlayAnimation(A, Loop);
            CurrentAnimation = A;
        }
        if (A && GetMesh()->GetSingleNodeInstance())
        {
            const float Rate = !Loop ? A->GetPlayLength() / .4f
                                    : A == D.RunAnimation ? FMath::Clamp(GetVelocity().Size2D() / 280.f, .7f, 2.2f)
                                                          : 1.f;
            GetMesh()->GetSingleNodeInstance()->SetPlayRate(Rate);
        }
    }
    if (IsLocallyControlled() && !bBot && IsAlive())
    {
        if (APlayerController* PC = Cast<APlayerController>(Controller))
        {
            if(!bAppliedLocalSettings)
            {
                if(auto Local=Cast<ARGPlayerController>(PC)) Local->ApplyPlayerSettings();
                bAppliedLocalSettings=true;
            }
            FVector Origin, Ray;
            if (PC->DeprojectMousePositionToWorld(Origin, Ray) && FMath::Abs(Ray.Z) > .01f)
            {
                const float T = (GetActorLocation().Z - Origin.Z) / Ray.Z;
                FVector Dir = (Origin + Ray * T - GetActorLocation()).GetSafeNormal2D();
                if (!Dir.IsNearlyZero())
                {
                    AimDirection = Dir;
                    if (GetWorld()->GetTimeSeconds() - LastAimSend > .06f)
                    {
                        ServerAim(Dir);
                        LastAimSend = GetWorld()->GetTimeSeconds();
                    }
                }
            }
        }
        if (bAttackHeld && Classes->ClassID != ERGClass::Mage && GetWorld()->GetTimeSeconds() > LastAttackRequest + .12f)
        {
            Act(0);
            LastAttackRequest = GetWorld()->GetTimeSeconds();
        }
    }
    if (Combat->bCharging && GetNetMode()!=NM_DedicatedServer && GetWorld()->GetTimeSeconds()>LastChargeVisual+.3f)
    {
        LastChargeVisual=GetWorld()->GetTimeSeconds();
        if (auto FX=GetWorld()->SpawnActor<ARGAreaEffect>(GetActorLocation()-FVector(0,0,55),FRotator::ZeroRotator))
            FX->Init(D.AreaRadius*(1+Combat->ChargeFraction()*.35f),Classes->bUpgraded ? FLinearColor(.2f,.6f,1) : FLinearColor(1,.45f,.12f));
    }
    if (IsAlive() && (HasAuthority() || IsLocallyControlled()))
    {
        // Face travel during locomotion; mouse/target aiming takes over during combat.
        const bool bAimFacing = Combat->bBlocking || Combat->bCharging || AttackVisualUntil > GetWorld()->GetTimeSeconds() || bAttackHeld;
        const FVector Travel = GetVelocity().GetSafeNormal2D();
        const FVector Facing = !bAimFacing && !Travel.IsNearlyZero() ? Travel : AimDirection;
        if (!Facing.IsNearlyZero())
            SetActorRotation(Facing.Rotation());
    }
    if (Weapon->GetStaticMesh())
    {
        const float Phase = FMath::Clamp((AttackVisualUntil - GetWorld()->GetTimeSeconds()) / .23f, 0.f, 1.f);
        Weapon->SetRelativeRotation(Classes->Definition().WeaponTransform.Rotator() +
                                    FRotator(0, FMath::Sin(Phase * PI) * 100, 0));
    }
    if (HasAuthority() && GetActorLocation().Z < -700 && IsAlive())
    {
        Carry->Drop();
        SetActorLocation(FVector(RG::Side(Team) * RG::SpawnX, 0, 120));
    }
}
void ARGCharacter::SetupPlayerInputComponent(UInputComponent* I)
{
    Super::SetupPlayerInputComponent(I);
    I->BindAxis(TEXT("RGForward"), this, &ARGCharacter::MoveForward);
    I->BindAxis(TEXT("RGRight"), this, &ARGCharacter::MoveRight);
    I->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ARGCharacter::PrimaryPressed);
    I->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ARGCharacter::PrimaryReleased);
    I->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ARGCharacter::SecondaryPressed);
    I->BindKey(EKeys::RightMouseButton, IE_Released, this, &ARGCharacter::SecondaryReleased);
    I->BindKey(EKeys::E, IE_Pressed, this, &ARGCharacter::InteractPressed);
    I->BindKey(EKeys::Q, IE_Pressed, this, &ARGCharacter::DropPressed);
    I->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ARGCharacter::DodgePressed);
    I->BindKey(EKeys::X, IE_Pressed, this, &ARGCharacter::RemoveHatPressed);
}
void ARGCharacter::MoveForward(float V)
{
    if (IsAlive())
        AddMovementInput(FVector(0, -1, 0), V);
}
void ARGCharacter::MoveRight(float V)
{
    if (IsAlive())
        AddMovementInput(FVector(1, 0, 0), V);
}
void ARGCharacter::PrimaryPressed()
{
    bAttackHeld = true;
    Act(0);
}
void ARGCharacter::PrimaryReleased()
{
    bAttackHeld = false;
    Act(0,false);
}
void ARGCharacter::SecondaryPressed()
{
    Act(1);
}
void ARGCharacter::SecondaryReleased()
{
    Act(1, false);
}
void ARGCharacter::InteractPressed()
{
    Act(2);
}
void ARGCharacter::DropPressed()
{
    Act(3);
}
void ARGCharacter::DodgePressed()
{
    Act(4);
}
void ARGCharacter::RemoveHatPressed()
{
    Act(5);
}
void ARGCharacter::Act(uint8 A, bool B)
{
    if (auto PC=Cast<ARGPlayerController>(Controller); PC && PC->bMenuOpen && A!=6)
    {
        bAttackHeld=false;
        return;
    }
    if (HasAuthority())
        ServerAction_Implementation(A, B);
    else
        ServerAction(A, B);
}
void ARGCharacter::ServerAim_Implementation(FVector_NetQuantizeNormal Direction)
{
    if (!IsAlive() || Direction.ContainsNaN() || Direction.IsNearlyZero())
        return;
    AimDirection = FVector(Direction).GetSafeNormal2D();
}
void ARGCharacter::ServerAction_Implementation(uint8 Action, bool bPressed)
{
    if (!IsAlive())
        return;
    switch (Action)
    {
    case 0:
        if (Classes->ClassID==ERGClass::Mage)
        {
            if (bPressed) Combat->BeginCharge(); else Combat->ReleaseCharge();
        }
        else if (bPressed) Combat->Primary();
        break;
    case 1:
        Combat->Secondary(bPressed);
        break;
    case 2:
        Interact();
        break;
    case 3:
        Carry->Drop();
        break;
    case 4:
        Combat->Dodge();
        break;
    case 5:
        if (!Carry->Carried)
            Classes->SetClass(ERGClass::Peasant);
        break;
    case 6:
        Combat->CancelCharge();
        Combat->bBlocking=false;
        break;
    default:
        break;
    }
}
void ARGCharacter::Interact()
{
    if (!HasAuthority() || !IsAlive() || GetWorld()->GetTimeSeconds() < LastInteract + .3f)
        return;
    LastInteract = GetWorld()->GetTimeSeconds();
    AActor* Best = nullptr;
    float Distance = 220;
    for (TActorIterator<ARGCarryable> It(GetWorld()); It; ++It)
    {
        if (It->Carrier || (It->bRelic && It->Team == Team && It->State == ERGRelicState::Safe))
            continue;
        float D = FVector::Dist(GetActorLocation(), It->GetActorLocation());
        if (D < Distance)
        {
            Best = *It;
            Distance = D;
        }
    }
    for (TActorIterator<ARGStation> It(GetWorld()); It; ++It)
    {
        float D = FVector::Dist2D(GetActorLocation(), It->GetActorLocation());
        if (D < Distance && !It->Prompt(this).IsEmpty())
        {
            Best = *It;
            Distance = D;
        }
    }
    if (ARGCarryable* Item = Cast<ARGCarryable>(Best))
    {
        if (Item->Team == Team && Item->bRelic && Item->State == ERGRelicState::Dropped)
        {
            Item->ReturnHome();
            if (ARGGameState* S = GetWorld()->GetGameState<ARGGameState>())
                S->Announce(RG::TeamName(Team) + TEXT(" relic returned"), Team);
        }
        else
            Carry->Pickup(Item);
    }
    else if (ARGStation* Station = Cast<ARGStation>(Best))
        Station->Interact(this);
}
void ARGCharacter::HandleDeath()
{
    Combat->CancelCharge();
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 85));
    Hat->SetVisibility(false);
    Weapon->SetVisibility(false);
    if (!HasAuthority())
        return;
    Carry->Drop();
    Combat->bBlocking = false;
    if (PackWood + PackOre > 0)
    {
        ARGCarryable* Bundle = GetWorld()->SpawnActor<ARGCarryable>(GetActorLocation(), FRotator::ZeroRotator);
        Bundle->bRelic = false;
        Bundle->Wood = PackWood;
        Bundle->Ore = PackOre;
        Bundle->OnDropped();
        PackWood = PackOre = 0;
    }
    AController* Previous = Controller;
    if (ARGGameMode* GM = GetWorld()->GetAuthGameMode<ARGGameMode>())
        GM->Respawn(Previous, Team);
    SetLifeSpan(4.2f);
}
void ARGCharacter::MulticastArea_Implementation(float Radius,FLinearColor Color)
{
    if (GetNetMode()==NM_DedicatedServer) return;
    if (auto FX=GetWorld()->SpawnActor<ARGAreaEffect>(GetActorLocation()-FVector(0,0,55),FRotator::ZeroRotator)) FX->Init(Radius,Color);
}
void ARGCharacter::MulticastFeedback_Implementation(FVector Location, FLinearColor Color, float Amount)
{
    if (GetNetMode() == NM_DedicatedServer)
        return;
    if (ARGEffect* FX = GetWorld()->SpawnActor<ARGEffect>(Location, FRotator::ZeroRotator))
        FX->Init(Color, Amount);
    if (Amount > 0 && FVector::DistSquared(Location, GetActorLocation() + FVector(0, 0, 95)) < 10000)
    {
        HitVisualUntil = GetWorld()->GetTimeSeconds() + .15f;
        if (UNiagaraSystem* FX = Classes->Catalog ? Classes->Catalog->HitEffect.Get() : nullptr)
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FX, Location, FRotator::ZeroRotator, FVector(.5f));
        if (IsLocallyControlled())
            if (APlayerController* PC = Cast<APlayerController>(Controller))
                if (Classes->Catalog && Classes->Catalog->HitShake)
                    PC->ClientStartCameraShake(Classes->Catalog->HitShake, .25f);
    }
    if (Classes->Catalog)
        if (USoundBase* Cue = Amount > 0 ? Classes->Catalog->HitSound.Get() : Classes->Catalog->PickupSound.Get())
            UGameplayStatics::PlaySoundAtLocation(this, Cue, Location, .4f);
}
void ARGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARGCharacter, Team);
    DOREPLIFETIME(ARGCharacter, ProtectedUntil);
    DOREPLIFETIME(ARGCharacter, PackWood);
    DOREPLIFETIME(ARGCharacter, PackOre);
    DOREPLIFETIME(ARGCharacter, AttackSequence);
    DOREPLIFETIME(ARGCharacter, bBot);
}
