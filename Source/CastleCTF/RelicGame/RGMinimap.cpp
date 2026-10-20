#include "RGHUD.h"
#include "EngineUtils.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "RGOutpost.h"
#include "RGWorldActors.h"

void ARGHUD::DrawBattleMap(float X,float Y,float Width,float Height)
{
    const float Pad=14, Top=34;
    Panel(X,Y,Width,Height,FLinearColor(.025f,.045f,.065f,.96f));
    Text(TEXT("THE FRONTIER   /   M map"),X+Pad,Y+10,.8f,FLinearColor(.95f,.75f,.36f));
    const float W=Width-2*Pad,H=Height-Top-24;
    auto Point=[&](FVector P){return FVector2D(X+Pad+W*(.5f+P.X/(2*RG::MapHalfX)),Y+Top+H*(.5f+P.Y/(2*RG::MapHalfY)));};
    const FLinearColor Route(.25f,.28f,.22f),Stone(.5f,.54f,.5f),Gold(1,.8f,.3f);
    for (float Row : {0.f,-1700.f,1700.f})
    {
        const auto A=Point(FVector(-4000,Row,0)),B=Point(FVector(4000,Row,0));
        Line(A.X,A.Y,B.X,B.Y,Route,3);
    }
    const auto River=Point(FVector(0,-3100,0)),End=Point(FVector(0,3100,0));
    Line(River.X,River.Y,End.X,End.Y,FLinearColor(.13f,.39f,.48f),4);
    for (ERGTeam T : {ERGTeam::Red,ERGTeam::Blue})
    {
        const auto A=Point(FVector(RG::Side(T)*4000,-1400,0)),B=Point(FVector(RG::Side(T)*5600,1400,0));
        const float L=FMath::Min(A.X,B.X),R=FMath::Max(A.X,B.X);
        Panel(L,A.Y,R-L,B.Y-A.Y,RG::Color(T)*.4f);
        Line(L,A.Y,R,A.Y,Stone,1);Line(L,B.Y,R,B.Y,Stone,1);
        Line(L,A.Y,L,B.Y,Stone,1);Line(R,A.Y,R,B.Y,Stone,1);
    }
    ARGCharacter* Player=Cast<ARGCharacter>(GetOwningPawn());
    ARGOutpost* Forward=nullptr;float Progress=-MAX_flt;
    for (TActorIterator<ARGOutpost> It(GetWorld());It;++It)
    {
        if(Player && It->CanSpawn(Player->Team) && -RG::Side(Player->Team)*It->GetActorLocation().X>Progress)
        {Forward=*It;Progress=-RG::Side(Player->Team)*It->GetActorLocation().X;}
    }
    for (TActorIterator<ARGOutpost> It(GetWorld());It;++It)
    {
        const auto P=Point(It->GetActorLocation());
        const auto C=It->bContested?Gold:RG::Color(It->Team);
        if(*It==Forward) Panel(P.X-7,P.Y-7,14,14,Gold);
        Panel(P.X-5,P.Y-5,10,10,C);
        Text(Width>400?It->Name:It->Name.Left(1),P.X-(Width>400?20:3),P.Y+8,.8f,C);
    }
    for (TActorIterator<ARGStation> It(GetWorld());It;++It)
        if (It->Kind==ERGStationType::Gate)
        {
            const auto P=Point(It->GetActorLocation());
            Panel(P.X-2,P.Y-5,4,10,It->Health>0?RG::Color(It->Team):FLinearColor(.3f,.3f,.3f));
        }
    for (TActorIterator<ARGCharacter> It(GetWorld());It;++It)
        if (It->IsAlive())
        {
            const auto P=Point(It->GetActorLocation());
            if (*It==Player)
            {
                Panel(P.X-3,P.Y-3,6,6,FLinearColor::White);
                const FVector F=It->GetActorForwardVector();Line(P.X,P.Y,P.X+F.X*9,P.Y+F.Y*9,FLinearColor::White,2);
            }
            else Panel(P.X-1.5f,P.Y-1.5f,3,3,RG::Color(It->Team));
        }
    for (TActorIterator<ARGCarryable> It(GetWorld());It;++It)
        if (It->bRelic) {const auto P=Point(It->GetActorLocation());Panel(P.X-3,P.Y-3,6,6,Gold);}
    Text(Forward?TEXT("NEXT SPAWN  ")+Forward->Name:TEXT("NEXT SPAWN  Castle"),X+Pad,Y+Height-18,.72f,Gold);
}
