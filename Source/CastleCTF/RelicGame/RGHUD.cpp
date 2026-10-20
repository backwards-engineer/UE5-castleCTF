#include "RGHUD.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGGameState.h"
#include "RGWorldActors.h"
#include "RGGameMode.h"
#include "RGOutpost.h"
void ARGHUD::Text(const FString& V, float X, float Y, float Scale, FLinearColor Color)
{
    DrawText(V, Color, X*UIScale, Y*UIScale, GEngine->GetMediumFont(), Scale*UIScale);
}
void ARGHUD::Panel(float X, float Y, float W, float H, FLinearColor C)
{
    DrawRect(C, X*UIScale, Y*UIScale, W*UIScale, H*UIScale);
}
void ARGHUD::Line(float X1,float Y1,float X2,float Y2,FLinearColor Color,float Thickness)
{
    DrawLine(X1*UIScale,Y1*UIScale,X2*UIScale,Y2*UIScale,Color,Thickness*UIScale);
}
void ARGHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas)
        return;
    ARGGameState* S = GetWorld()->GetGameState<ARGGameState>();
    ARGCharacter* P = Cast<ARGCharacter>(GetOwningPawn());
    if (!S)
        return;
    UIScale=FMath::Max(.3f,FMath::Min(Canvas->ClipX/1280.f,Canvas->ClipY/720.f));
    float W = Canvas->ClipX/UIScale, H = Canvas->ClipY/UIScale, CX = W * .5f;
    FLinearColor Ink(.025f, .045f, .065f, .93f), Gold(.95f, .75f, .36f), Muted(.63f, .72f, .75f);
    Panel(CX - 250, 20, 500, 88, Ink);
    Panel(CX - 250, 20, 4, 88, RG::Color(ERGTeam::Red));
    Panel(CX + 246, 20, 4, 88, RG::Color(ERGTeam::Blue));
    Text(TEXT("CRIMSON"), CX - 226, 31, .9f, RG::Color(ERGTeam::Red));
    Text(TEXT("AZURE"), CX + 153, 31, .9f, RG::Color(ERGTeam::Blue));
    Text(FString::FromInt(S->Red.Captures), CX - 160, 52, 2, Gold);
    Text(FString::FromInt(S->Blue.Captures), CX + 136, 52, 2, Gold);
    Text(TEXT("CROWNFALL"), CX - 67, 31, 1, Gold);
    Text(FString::Printf(TEXT("%02d:%02d  /  FIRST TO %d"), S->SecondsRemaining / 60, S->SecondsRemaining % 60,
                         S->CaptureTarget),
         CX - 86, 63, .85f, Muted);
    auto Relic = [](ERGRelicState State) {
        switch (State)
        {
        case ERGRelicState::Stolen:
            return TEXT("STOLEN");
        case ERGRelicState::Dropped:
            return TEXT("DROPPED");
        case ERGRelicState::Returning:
            return TEXT("RETURNING");
        default:
            return TEXT("SAFE");
        }
    };
    Text(FString(TEXT("RELIC  ")) + Relic(S->RedRelic), CX - 225, 88, .67f, Muted);
    Text(FString(TEXT("RELIC  ")) + Relic(S->BlueRelic), CX + 125, 88, .67f, Muted);
    const auto Controller=Cast<ARGPlayerController>(GetOwningPlayerController());
    if (Controller && Controller->bLargeMap) DrawBattleMap(CX-430,H*.5f-230,860,440);
    else DrawBattleMap(W-300,20,276,200);
    if (P)
    {
        const FRGTeamStock& Stock = S->Stock(P->Team);
        Panel(24, 24, 210, 88, Ink);
        Text(RG::TeamName(P->Team) + TEXT(" STOCKPILE"), 40, 36, .85f, RG::Color(P->Team));
        Text(FString::Printf(TEXT("WOOD  %d     ORE  %d"), Stock.Wood, Stock.Ore), 40, 66, 1, Gold);
        Panel(24, H - 144, 330, 116, Ink);
        Panel(24, H - 144, 4, 116, RG::Color(P->Team));
        Text(RG::ClassName(P->Classes->ClassID, P->Classes->bUpgraded).ToUpper(), 42, H - 130, 1.2f, Gold);
        Panel(42, H - 96, 292, 12, FLinearColor(.14f, .19f, .22f));
        Panel(42, H - 96, 292 * FMath::Clamp(P->Health->Health / P->Health->MaxHealth, 0.f, 1.f), 12,
              RG::Color(P->Team));
        Text(FString::Printf(TEXT("%.0f / %.0f HP"), P->Health->Health, P->Health->MaxHealth), 42, H - 76, .8f);
        Text(FString::Printf(TEXT("PACK  %d wood / %d ore"), P->PackWood, P->PackOre), 160, H - 76, .8f, Muted);
        float Cooldown = FMath::Max(0.f, P->Combat->NextAbilityTime - S->GetServerWorldTimeSeconds());
        Text(Cooldown > 0 ? FString::Printf(TEXT("ABILITY  %.1fs"), Cooldown) : TEXT("ABILITY READY"), 42, H - 52, .78f,
             Muted);
        Panel(W - 583, H - 125, 559, 97, Ink);
        Text(TEXT("WASD Move  |  Mouse Aim  |  M Map  |  Esc Menu"), W - 565, H - 82, .82f,
             Muted);
        Text(TEXT("E Interact  |  Q Drop  |  Space Dodge  |  X Remove hat"), W - 565, H - 58, .82f,
             Muted);
        FString AbilityHint=P->Classes->ClassID==ERGClass::Mage ? TEXT("LMB click: burst  /  Hold + release: charged burst") :
            P->Classes->ClassID==ERGClass::Priest ? (P->Classes->bUpgraded?TEXT("LMB Drain circle  /  RMB Heal circle"):TEXT("LMB Heal circle  /  RMB Greater heal")) : TEXT("LMB Attack  /  RMB Ability or block");
        Text(AbilityHint,W-565,H-110,.82f,Gold);
        if(P->Combat->bCharging)
        {
            Panel(CX-140,H-260,280,26,Ink);
            Panel(CX-136,H-256,272*P->Combat->ChargeFraction(),18,Gold);
            Text(TEXT("CHARGING"),CX-36,H-254,.8f,Ink);
        }
        for(TActorIterator<ARGOutpost> It(GetWorld());It;++It)
            if(FVector::Dist2D(It->GetActorLocation(),P->GetActorLocation())<It->CaptureRadius)
            {
                Panel(CX-220,190,440,60,Ink);
                Text(It->Name+(It->bContested?TEXT(" / CONTESTED"):It->Team==P->Team?TEXT(" / SECURED"):TEXT(" / CAPTURING")),CX-200,198,.95f,Gold);
                Panel(CX-200,232,400,6,FLinearColor(.2f,.24f,.26f));
                Panel(CX-200,232,400*FMath::Abs(It->Control),6,RG::Color(It->Control<0?ERGTeam::Red:ERGTeam::Blue));
                break;
            }
        FString Prompt;
        float Best = 240;
        for (TActorIterator<ARGStation> It(GetWorld()); It; ++It)
        {
            float D = FVector::Dist2D(P->GetActorLocation(), It->GetActorLocation());
            if (D < Best)
            {
                FString Str = It->Prompt(P);
                if (!Str.IsEmpty())
                {
                    Best = D;
                    Prompt = Str;
                }
            }
        }
        for (TActorIterator<ARGCarryable> It(GetWorld()); It; ++It)
            if (!It->Carrier && FVector::Dist(P->GetActorLocation(), It->GetActorLocation()) < Best)
            {
                if (It->Team != P->Team)
                    Prompt = TEXT("E  Take enemy relic");
                else if (It->State == ERGRelicState::Dropped)
                    Prompt = TEXT("E  Return your relic");
            }
        if (P->Carry->Carried)
        {
            bool OwnSafe = (P->Team == ERGTeam::Red ? S->RedRelic : S->BlueRelic) == ERGRelicState::Safe;
            Prompt = P->Carry->Carried->bRelic ? (OwnSafe ? TEXT("BRING THE RELIC HOME  /  Q to drop")
                                                          : TEXT("RETURN YOUR OWN RELIC TO ENABLE CAPTURE"))
                                               : TEXT("BRING RESOURCES TO YOUR STOCKPILE");
        }
        if (!Prompt.IsEmpty())
        {
            Panel(CX - 260, H - 205, 520, 44, Ink);
            Text(Prompt, CX - 242, H - 194, .95f, Gold);
        }
        if (!P->IsAlive())
        {
            Panel(CX - 180, H * .45f, 360, 80, Ink);
            Text(TEXT("FALLEN IN BATTLE"), CX - 120, H * .45f + 14, 1.25f, Gold);
            Text(TEXT("Returning to your front line..."), CX - 128, H * .45f + 48, .9f, Muted);
        }
        if (P->HitVisualUntil > GetWorld()->GetTimeSeconds())
            Panel(0, 0, W, H, FLinearColor(1, .05f, .02f, .1f));
        // Overhead health bars and team markers make nearby skirmishes readable.
        for (TActorIterator<ARGCharacter> It(GetWorld()); It; ++It)
        {
            if (!It->IsAlive() || *It == P)
                continue;
            FVector Screen = Project(It->GetActorLocation() + FVector(0, 0, 110));
            Screen.X/=UIScale;Screen.Y/=UIScale;
            if (Screen.Z > 0 && Screen.X > 0 && Screen.X < W && Screen.Y > 110 && Screen.Y < H - 150)
            {
                Panel(Screen.X - 23, Screen.Y, 46, 5, Ink);
                Panel(Screen.X - 23, Screen.Y, 46 * It->Health->Health / It->Health->MaxHealth, 5, RG::Color(It->Team));
            }
        }
    }
    if (S->Phase == ERGPhase::Countdown)
    {
        Panel(CX - 180, H * .26f, 360, 86, Ink);
        Text(TEXT("RAISE YOUR BANNERS"), CX - 132, H * .26f + 15, 1.2f, Gold);
        Text(FString::Printf(TEXT("Battle begins in %d"), S->SecondsRemaining), CX - 96, H * .26f + 49, 1, Muted);
    }
    else if (S->Phase == ERGPhase::Victory)
    {
        Panel(CX - 240, H * .32f, 480, 130, Ink);
        Text(RG::TeamName(S->Winner) + TEXT(" VICTORY"), CX - 165, H * .32f + 24, 1.9f, Gold);
        Text(FString::Printf(TEXT("New battle in %d seconds"), S->SecondsRemaining), CX - 120, H * .32f + 86, 1, Muted);
    }
    else if (S->AnnouncementUntil > S->GetServerWorldTimeSeconds())
    {
        Panel(CX - 300, 130, 600, 48, Ink);
        Text(S->Announcement, CX - 280, 143, 1.1f, Gold);
    }
}
