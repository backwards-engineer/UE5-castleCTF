#include "RGGameMode.h"
#include "AIController.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGGameState.h"
#include "RGBotController.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void ARGGameMode::RunMotionTest()
{
    ARGBotController* Bot = GetWorld()->SpawnActor<ARGBotController>();
    ARGCharacter* Pawn = SpawnFighter(Bot, ERGTeam::Red);
    GetWorldTimerManager().ClearTimer(Bot->BrainTimer);
    GetGameState<ARGGameState>()->Phase = ERGPhase::Playing;
    GetGameState<ARGGameState>()->SecondsRemaining = 900;
    struct FRun
    {
        int32 Step = 0;
        bool Moving = false;
        FVector Direction = FVector::ForwardVector;
        FQuat Leg, Arm;
        float AnimationTime = 0;
        TArray<FString> Results;
        FTimerHandle Sample;
    };
    const TSharedRef<FRun> Run = MakeShared<FRun>();
    const bool Preview = FParse::Param(FCommandLine::Get(), TEXT("MotionPreview"));
    if (Preview && GetWorld()->GetFirstPlayerController())
    {
        GetWorld()->GetFirstPlayerController()->SetViewTarget(Pawn);
        Pawn->Boom->TargetArmLength = 480;
        Pawn->Boom->SetRelativeRotation(FRotator(-25, -55, 0));
        Pawn->Boom->bEnableCameraLag = false;
    }
    GetWorldTimerManager().SetTimer(Run->Sample, FTimerDelegate::CreateWeakLambda(this, [this, Pawn, Bot, Run, Preview]() {
        const int32 Case = Run->Step / 7;
        const int32 Phase = Run->Step % 7;
        if (Case == 11)
        {
            FFileHelper::SaveStringArrayToFile(Run->Results, *(FPaths::ProjectSavedDir() / TEXT("Validation/Motion.txt")));
            const bool Failed = Run->Results.ContainsByPredicate([](const FString& Line) { return Line.StartsWith(TEXT("FAIL")); });
            GetWorldTimerManager().ClearTimer(Run->Sample);
            FPlatformMisc::RequestExitWithStatus(false, Failed ? 1 : 0);
            return;
        }
        auto Check = [&](const TCHAR* Name, bool Pass) {
            Run->Results.Add(FString::Printf(TEXT("%s %s: %s"), Pass ? TEXT("PASS") : TEXT("FAIL"),
                *RG::ClassName(Pawn->Classes->ClassID, Pawn->Classes->bUpgraded), Name));
            UE_LOG(LogTemp, Display, TEXT("[MotionTest] %s"), *Run->Results.Last());
        };
        auto Shot = [&](const TCHAR* Name) {
            if (Preview && Case == 2)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Validation") / Name, true, false);
        };
        USkeletalMeshComponent* Mesh = Pawn->GetMesh();
        if (Phase == 0)
        {
            const ERGClass Class = static_cast<ERGClass>(Case < 7 ? Case : Case - 5);
            GetGameState<ARGGameState>()->Red.Upgrades = Case < 7 ? 0 : 1 << static_cast<uint8>(Class);
            Pawn->Classes->SetClass(Class);
            Pawn->SetActorLocation(FVector(-700, 0, 85));
            Pawn->AttackVisualUntil = 0;
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            const FVector Directions[] = {FVector(1, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0), FVector(0, -1, 0)};
            Run->Direction = Directions[Case % 4];
            Pawn->AimDirection = -Run->Direction;
            Run->Moving = true;
            Bot->Destination = Pawn->GetActorLocation() + Run->Direction * 1000;
        }
        else if (Phase == 1)
        {
            Run->Leg = Mesh->GetSocketTransform(TEXT("thigh_l"), RTS_Component).GetRotation();
            Run->AnimationTime = Mesh->GetSingleNodeInstance() ? Mesh->GetSingleNodeInstance()->GetCurrentTime() : -1;
            Shot(TEXT("Motion_Run_A.png"));
        }
        else if (Phase == 2)
        {
            const FQuat Leg = Mesh->GetSocketTransform(TEXT("thigh_l"), RTS_Component).GetRotation();
            Check(TEXT("run playback advances and leg pose changes"), Pawn->CurrentAnimation == Pawn->Classes->Definition().RunAnimation &&
                Mesh->GetSingleNodeInstance() && Mesh->GetSingleNodeInstance()->GetCurrentTime() != Run->AnimationTime &&
                Leg.AngularDistance(Run->Leg) > .05f);
            Check(TEXT("moving pawn and visible model face travel direction"),
                Pawn->GetVelocity().Size2D() > 100 && FVector::DotProduct(Pawn->GetActorForwardVector(), Run->Direction) > .95f &&
                FVector::DotProduct(Mesh->GetRightVector(), Run->Direction) > .95f);
            TArray<UStaticMeshComponent*> Parts;
            Pawn->GetComponents(Parts);
            Check(TEXT("no disk component under player"), !Parts.ContainsByPredicate([](const UStaticMeshComponent* Part) {
                return Part->GetName() == TEXT("TeamMarker");
            }));
            Shot(TEXT("Motion_Run_B.png"));
        }
        else if (Phase == 3)
        {
            Run->Moving = false;
            Bot->Destination = Pawn->GetActorLocation();
            Pawn->GetCharacterMovement()->StopMovementImmediately();
        }
        else if (Phase == 4)
        {
            Check(TEXT("stopping returns to idle"), Pawn->CurrentAnimation == Pawn->Classes->Definition().IdleAnimation);
            Pawn->OnRep_Attack();
        }
        else if (Phase == 5)
        {
            Run->Arm = Mesh->GetSocketTransform(TEXT("upperarm_r"), RTS_Component).GetRotation();
            Shot(TEXT("Motion_Attack.png"));
        }
        else if (Phase == 6)
        {
            // The supplied bow clip is intentionally a held aiming pose.
            if (Pawn->Classes->ClassID != ERGClass::Archer || Pawn->Classes->bUpgraded)
                Check(TEXT("attack changes arm pose during playback"),
                    Mesh->GetSocketTransform(TEXT("upperarm_r"), RTS_Component).GetRotation().AngularDistance(Run->Arm) > .001f);
        }
        ++Run->Step;
    }), .15f, true);
}
