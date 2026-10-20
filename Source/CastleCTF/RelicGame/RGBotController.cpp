#include "RGBotController.h"
#include "EngineUtils.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGGameState.h"
#include "RGWorldActors.h"
#include "TimerManager.h"
#include "Components/PrimitiveComponent.h"
ARGBotController::ARGBotController()
{
    PrimaryActorTick.bCanEverTick = true;
    bWantsPlayerState = true;
}
void ARGBotController::OnPossess(APawn* P)
{
    Super::OnPossess(P);
    Destination = P->GetActorLocation();
    bDetouring = false;
    NextPathCheck = 0;
    GetWorldTimerManager().SetTimer(BrainTimer, this, &ARGBotController::Think, .25f, true, .3f);
}
void ARGBotController::Tick(float Delta)
{
    Super::Tick(Delta);
    ARGCharacter* P = Cast<ARGCharacter>(GetPawn());
    ARGGameState* S = GetWorld()->GetGameState<ARGGameState>();
    if (!P || !P->IsAlive() || !S || !S->IsPlaying())
        return;
    const FVector Pos=P->GetActorLocation();
    if(bDetouring && (FVector::Dist2D(Pos,Detour)<55 || FVector::Dist2D(Pos,Destination)<85)) bDetouring=false;
    if(!bDetouring && GetWorld()->GetTimeSeconds()>NextPathCheck && FVector::Dist2D(Pos,Destination)>120)
    {
        NextPathCheck=GetWorld()->GetTimeSeconds()+.25f;
        FCollisionQueryParams Query;Query.AddIgnoredActor(P);
        for(TActorIterator<ARGStation> It(GetWorld());It;++It) if(It->Kind==ERGStationType::Gate && It->Team==Team) Query.AddIgnoredActor(*It);
        const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
        auto Clear=[&](FVector A,FVector B,FHitResult& Hit){A.Z=Pos.Z+8;B.Z=Pos.Z+8;return !GetWorld()->SweepSingleByObjectType(Hit,A,B,FQuat::Identity,Objects,FCollisionShape::MakeSphere(36),Query);};
        FHitResult Hit;
        if(!Clear(Pos,Destination,Hit) && Hit.GetComponent())
        {
            const FBox Box=Hit.GetComponent()->Bounds.GetBox().ExpandBy(100);
            float Best=MAX_flt;
            for(float X:{Box.Min.X,Box.Max.X}) for(float Y:{Box.Min.Y,Box.Max.Y})
            {
                FVector Corner(X,Y,Pos.Z);FHitResult Test;
                if(!Clear(Pos,Corner,Test)) continue;
                const float Score=FVector::Dist2D(Pos,Corner)+FVector::Dist2D(Corner,Destination)+(Clear(Corner,Destination,Test)?0:800);
                if(Score<Best) {Best=Score;Detour=Corner;bDetouring=true;}
            }
        }
    }
    FVector Dir = ((bDetouring?Detour:Destination) - Pos).GetSafeNormal2D();
    if (FVector::Dist2D(P->GetActorLocation(), Destination) > 75)
        P->AddMovementInput(Dir, 1);
}
