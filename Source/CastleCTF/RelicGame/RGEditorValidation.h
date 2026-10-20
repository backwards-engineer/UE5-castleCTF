#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RGEditorValidation.generated.h"
UCLASS()
class CASTLECTF_API URGEditorValidation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
  public:
    UFUNCTION(BlueprintCallable, Category = "Crownfall|Validation") static void StartMultiplayerTest();
};
