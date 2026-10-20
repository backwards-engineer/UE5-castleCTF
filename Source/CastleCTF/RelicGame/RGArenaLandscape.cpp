#include "RGArena.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"

void ARGArena::BuildLandscape()
{
    const FLinearColor Grass(.16f,.29f,.18f), Stone(.25f,.31f,.34f), Trim(.43f,.47f,.43f), Road(.46f,.37f,.23f), Wood(.24f,.13f,.06f);
    Part(TEXT("Cube"),FVector(0,0,-60),FVector(128,66,1),Grass);
    for (float Y : {0.f,-1700.f,1700.f})
        Part(TEXT("Cube"),FVector(0,Y,-4),FVector(84,Y==0?6.f:3.4f,.08f),Road);
    Part(TEXT("Cube"),FVector(0,0,-1),FVector(6,62,.06f),FLinearColor(.09f,.36f,.42f),FRotator::ZeroRotator,false);
    for (float Y : {0.f,-1700.f,1700.f})
    {
        Part(TEXT("Cube"),FVector(0,Y,4),FVector(8.4f,5,.15f),Trim);
        for (int I=-5;I<=5;++I)
            Part(TEXT("Cube"),FVector(I*72,Y,17),FVector(.65f,4.6f,.12f),Wood);
        for (float Edge : {-240.f,240.f})
        {
            Part(TEXT("Cube"),FVector(0,Y+Edge,75),FVector(8.4f,.12f,.16f),Wood);
            for (float X : {-390.f,0.f,390.f}) Part(TEXT("Cube"),FVector(X,Y+Edge,50),FVector(.15f,.15f,1),Wood);
        }
    }
    for (ERGTeam Team : {ERGTeam::Red,ERGTeam::Blue})
    {
        const float S=RG::Side(Team);
        Part(TEXT("Cube"),FVector(S*4800,0,-3),FVector(17,29,.14f),FLinearColor(.31f,.34f,.31f));
        // Continuous overlapping perimeter. Only the 600cm gate opening is traversable.
        for (float Y : {-875.f,875.f})
        {
            Part(TEXT("Cube"),FVector(S*RG::CastleGateX,Y,170),FVector(1.6f,11.5f,3.6f),Stone);
            Part(TEXT("Cube"),FVector(S*RG::CastleGateX,Y,345),FVector(1.85f,11.5f,.25f),Trim);
        }
        Part(TEXT("Cube"),FVector(S*RG::CastleBackX,0,170),FVector(1.6f,30,3.6f),Stone);
        for (float Y : {-RG::CastleHalfWidth,RG::CastleHalfWidth})
        {
            Part(TEXT("Cube"),FVector(S*4800,Y,170),FVector(17.6f,1.6f,3.6f),Stone);
            Part(TEXT("Cube"),FVector(S*4800,Y,345),FVector(17.8f,1.9f,.25f),Trim);
            for (int I=0;I<17;++I) Part(TEXT("Cube"),FVector(S*(4000+I*100),Y,395),FVector(.58f,1.8f,.75f),Trim);
        }
        for (float X : {RG::CastleGateX,RG::CastleBackX})
            for (float Y : {-RG::CastleHalfWidth,RG::CastleHalfWidth})
            {
                Part(TEXT("Cylinder"),FVector(S*X,Y,210),FVector(3.8f,3.8f,4.4f),Trim);
                Part(TEXT("Cylinder"),FVector(S*X,Y,425),FVector(4.1f,4.1f,.4f),Stone);
                Part(TEXT("Cone"),FVector(S*X,Y,565),FVector(4.5f,4.5f,2.6f),RG::Color(Team)*.65f);
                Part(TEXT("Cylinder"),FVector(S*X,Y,730),FVector(.11f,.11f,1.2f),Wood);
                Part(TEXT("Cube"),FVector(S*X,Y+65,760),FVector(.04f,1.3f,.65f),RG::Color(Team),FRotator::ZeroRotator,false);
            }
        for (int I=-12;I<=12;++I)
            if (FMath::Abs(I)>3) Part(TEXT("Cube"),FVector(S*4000,I*110,395),FVector(1.8f,.58f,.75f),Trim);
        // Gatehouse lintel, stone jambs and timber braces frame the destructible door.
        Part(TEXT("Cube"),FVector(S*4000,0,375),FVector(2.1f,7.5f,.65f),Trim);
        if(!bSpawnGameplay) Part(TEXT("Cube"),FVector(S*4000,0,175),FVector(1.2f,6.6f,3.6f),RG::Color(Team)*.28f+FLinearColor(.18f,.12f,.07f));
        for (float Y : {-350.f,350.f})
        {
            Part(TEXT("Cube"),FVector(S*4000,Y,175),FVector(2.1f,.9f,3.5f),Trim);
            Part(TEXT("Sphere"),FVector(S*3880,Y,190),FVector(.3f,.3f,.5f),FLinearColor(1,.65f,.12f),FRotator::ZeroRotator,false);
        }
        // Courtyard paving, class pavilion, relic shrine, and a sheltered spawn court.
        Part(TEXT("Cube"),FVector(S*4780,0,3),FVector(15.2f,3.2f,.07f),Road);
        Part(TEXT("Cube"),FVector(S*4770,-580,4),FVector(11.3f,6.6f,.08f),Trim);
        for (float X : {4240.f,5300.f})
            Part(TEXT("Cylinder"),FVector(S*X,-940,115),FVector(.55f,.55f,2.4f),Trim);
        Part(TEXT("Cube"),FVector(S*4770,-940,255),FVector(11.6f,.75f,.55f),Wood);
        Part(TEXT("Cylinder"),FVector(S*RG::RelicX,530,12),FVector(4.7f,4.7f,.22f),Trim);
        Part(TEXT("Cylinder"),FVector(S*RG::RelicX,530,27),FVector(3.7f,3.7f,.08f),FLinearColor(.75f,.57f,.19f));
        for (float Y : {220.f,850.f})
        {
            Part(TEXT("Cylinder"),FVector(S*5450,Y,180),FVector(.18f,.18f,3.6f),Wood);
            Part(TEXT("Cube"),FVector(S*5450,Y+65,285),FVector(.03f,1.4f,1.2f),RG::Color(Team),FRotator::ZeroRotator,false);
        }
        // Field ruins provide landmarks and cover without obstructing the three lanes.
        for (float X : {1200.f,3100.f})
            for (float Y : {-900.f,900.f})
            {
                Part(TEXT("Cube"),FVector(S*X,Y,55),FVector(3.6f,.65f,1.2f),Stone);
                for (float Offset : {-160.f,160.f}) Part(TEXT("Cube"),FVector(S*X+Offset,Y,125),FVector(.6f,.8f,2.5f),Trim);
            }
    }
    // Capture courts connect the main road to flank lanes.
    for (FVector P : {FVector(-2250,-650,0),FVector(0,0,20),FVector(2250,-650,0)})
    {
        Part(TEXT("Cube"),P*.5f+FVector(P.X*.5f,0,-3),FVector(3.8f,FMath::Abs(P.Y)/100+4,.07f),Road);
        for (float X : {-420.f,420.f})
        {
            Part(TEXT("Cylinder"),P+FVector(X,0,130),FVector(.7f,.7f,2.6f),Trim);
            Part(TEXT("Cone"),P+FVector(X,0,310),FVector(1.3f,1.3f,1.2f),FLinearColor(.3f,.34f,.27f));
        }
    }
    FRandomStream Random(6142);
    for (int I=0;I<54;++I)
    {
        const float X=Random.FRandRange(-5800,5800), Y=(I%2?1.f:-1.f)*Random.FRandRange(2250,3150);
        const float Size=Random.FRandRange(.7f,1.3f);
        Part(TEXT("Sphere"),FVector(X,Y,20),FVector(3.3f,2.8f,1.6f)*Size,FLinearColor(.24f,.31f,.24f));
        Part(TEXT("Cylinder"),FVector(X+80,Y,140*Size),FVector(.45f,.45f,2.8f)*Size,Wood);
        for (int L=0;L<3;++L) Part(TEXT("Cone"),FVector(X+80,Y,(240+L*75)*Size),FVector(3.6f-L*.6f,3.6f-L*.6f,2.4f)*Size,FLinearColor(.055f+L*.015f,.19f+L*.025f,.12f));
    }
    for (float Y : {-3300.f,3300.f}) Part(TEXT("Cube"),FVector(0,Y,150),FVector(128,.6f,4),FLinearColor(.17f,.24f,.19f));
    for (float X : {-6400.f,6400.f}) Part(TEXT("Cube"),FVector(X,0,150),FVector(.6f,66,4),FLinearColor(.17f,.24f,.19f));
    auto Sun=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,1000),FRotator(-55,-35,0));
    Sun->GetLightComponent()->SetIntensity(4);
    CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent())->SetForwardShadingPriority(1);
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1,.91f,.77f));
    auto Fill=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,1000),FRotator(-55,140,0));
    Fill->GetLightComponent()->SetIntensity(2.2f); Fill->GetLightComponent()->SetCastShadows(false);
    Fill->GetLightComponent()->SetLightColor(FLinearColor(.6f,.75f,1));
    auto Sky=GetWorld()->SpawnActor<ASkyLight>(); Sky->GetLightComponent()->SetIntensity(1);
    Sky->GetLightComponent()->bRealTimeCapture=true;
    auto PP=GetWorld()->SpawnActor<APostProcessVolume>(); PP->bUnbound=true;
    PP->Settings.bOverride_AutoExposureMethod=true; PP->Settings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    PP->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; PP->Settings.AutoExposureApplyPhysicalCameraExposure=false;
    PP->Settings.bOverride_AutoExposureBias=true; PP->Settings.AutoExposureBias=0;
    PP->Settings.bOverride_MotionBlurAmount=true; PP->Settings.MotionBlurAmount=.12f;
}
