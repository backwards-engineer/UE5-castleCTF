#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RGHUD.generated.h"
UCLASS()
class CASTLECTF_API ARGHUD : public AHUD
{
    GENERATED_BODY()
  public:
    float UIScale = 1;
    virtual void DrawHUD() override;
    void Text(const FString& Value, float X, float Y, float Scale = 1, FLinearColor Color = FLinearColor::White);
    void Panel(float X, float Y, float W, float H, FLinearColor Color);
    void DrawBattleMap(float X,float Y,float Width,float Height);
    void Line(float X1,float Y1,float X2,float Y2,FLinearColor Color,float Thickness=1);
};
