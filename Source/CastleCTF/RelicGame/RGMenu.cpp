#include "RGGameMode.h"
#include "AudioDevice.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "RGArena.h"
#include "RGCharacter.h"
#include "RGComponents.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Misc/Paths.h"

namespace
{
const FLinearColor Gold(.94f,.71f,.32f), Pale(.87f,.91f,.87f), Dark(.025f,.045f,.06f,.96f);
class SCrownfallMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCrownfallMenu) {} SLATE_ARGUMENT(ARGPlayerController*,Player) SLATE_END_ARGS()
    TWeakObjectPtr<ARGPlayerController> Player;
    TSharedPtr<SVerticalBox> Body;
    TSharedPtr<SEditableTextBox> Address;
    int32 Page=0;
    float Volume=.75f, Zoom=1350;
    void Construct(const FArguments& Args)
    {
        Player=Args._Player;
        GConfig->GetFloat(TEXT("Crownfall"),TEXT("MasterVolume"),Volume,GGameUserSettingsIni);
        GConfig->GetFloat(TEXT("Crownfall"),TEXT("CameraDistance"),Zoom,GGameUserSettingsIni);
        if(FParse::Param(FCommandLine::Get(),TEXT("SettingsPreview"))) Page=2;
        if(FParse::Param(FCommandLine::Get(),TEXT("NetworkPreview"))) Page=1;
        ChildSlot[
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.015f,.025f,.035f,.48f)).Padding(48)
            [ SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [ SNew(SBox).WidthOverride(460)
                    [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Dark).Padding(28)
                        [ SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Body,SVerticalBox)] ] ] ]
                +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Bottom).Padding(48,0,0,24)
                [ SNew(STextBlock).Text(FText::FromString(TEXT("THE FRONTIER\n\nTwo castles. Three outposts.\nA different battle beneath every hat.")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold",20)).ColorAndOpacity(Pale).AutoWrapText(true) ]
            ] ];
        Rebuild();
    }
    bool FrontEnd() const { return Player.IsValid() && Player->GetWorld()->GetAuthGameMode<ARGFrontEndGameMode>(); }
    void Text(const FString& Value,int32 Size=14,FLinearColor Color=Pale)
    {
        Body->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Color).AutoWrapText(true)];
    }
    void Button(const FString& Label,TFunction<void()> Action)
    {
        Body->AddSlot().AutoHeight().Padding(0,0,0,9)[SNew(SButton).ContentPadding(FMargin(16,12)).HAlign(HAlign_Left)
            .ButtonColorAndOpacity(FLinearColor(.14f,.21f,.23f)).OnClicked_Lambda([Action](){Action();return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Bold",16)).ColorAndOpacity(Pale)]];
    }
    void Rebuild()
    {
        Body->ClearChildren();
        Text(TEXT("C R O W N F A L L"),30,Gold);
        Text(Page==1?TEXT("MULTIPLAYER"):Page==2?TEXT("SETTINGS"):FrontEnd()?TEXT("CASTLE WARFARE"):TEXT("BATTLE MENU"),12,Gold);
        if (Page==0)
        {
            if (FrontEnd())
            {
                Button(TEXT("Single player  /  battle with bots"),[this](){UGameplayStatics::OpenLevel(Player.Get(),TEXT("/Game/Crownfall/Maps/Crownfall"),true,TEXT("Solo"));});
                Button(TEXT("Multiplayer"),[this](){Page=1;Rebuild();});
            }
            else Button(TEXT("Resume battle"),[this](){Player->CloseMenu();});
            Button(TEXT("Settings"),[this](){Page=2;Rebuild();});
            if (!FrontEnd()) Button(TEXT("Return to start menu"),[this](){Player->SetPause(false);UGameplayStatics::OpenLevel(Player.Get(),TEXT("/Game/Crownfall/Maps/FrontEnd"));});
            Button(TEXT("Quit game"),[this](){UKismetSystemLibrary::QuitGame(Player.Get(),Player.Get(),EQuitPreference::Quit,false);});
            Text(TEXT("Capture the enemy relic three times. Secure outposts to respawn closer to the fight."),13);
        }
        else if (Page==1)
        {
            Button(TEXT("Host LAN match"),[this](){Player->HostMatch();});
            Text(TEXT("Join a host by LAN IP address. For two games on this computer, use 127.0.0.1."),13);
            Body->AddSlot().AutoHeight().Padding(0,0,0,12)[SAssignNew(Address,SEditableTextBox).Text(FText::FromString(TEXT("127.0.0.1"))).HintText(FText::FromString(TEXT("Host address"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",16))];
            Button(TEXT("Join match"),[this](){Player->MenuMessage=TEXT("Connecting to host...");Player->JoinMatch(Address->GetText().ToString().TrimStartAndEnd());});
            Button(TEXT("Back"),[this](){Page=0;Rebuild();});
        }
        else
        {
            auto Settings=GEngine->GetGameUserSettings();
            static const TCHAR* Qualities[]={TEXT("Low"),TEXT("Medium"),TEXT("High"),TEXT("Epic")};
            const int32 Quality=Settings->GetOverallScalabilityLevel();
            Button(FString(TEXT("Graphics quality: "))+(Quality<0?TEXT("Custom"):Qualities[FMath::Clamp(Quality,0,3)]),[this,Settings,Quality](){Settings->SetOverallScalabilityLevel((Quality+1)%4);Rebuild();});
            const FIntPoint Resolution=Settings->GetScreenResolution();
            Button(FString::Printf(TEXT("Resolution: %d x %d"),Resolution.X,Resolution.Y),[this,Settings,Resolution](){Settings->SetScreenResolution(Resolution.X<1600?FIntPoint(1920,1080):Resolution.X<2200?FIntPoint(2560,1440):FIntPoint(1280,720));Rebuild();});
            Body->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(SCheckBox).IsChecked(Settings->GetFullscreenMode()!=EWindowMode::Windowed?ECheckBoxState::Checked:ECheckBoxState::Unchecked)
                .OnCheckStateChanged_Lambda([Settings](ECheckBoxState V){Settings->SetFullscreenMode(V==ECheckBoxState::Checked?EWindowMode::WindowedFullscreen:EWindowMode::Windowed);})
                [SNew(STextBlock).Text(FText::FromString(TEXT("Borderless fullscreen"))).ColorAndOpacity(Pale)]];
            Text(TEXT("Master volume"),13);
            Body->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(SSlider).Value(Volume).OnValueChanged_Lambda([this](float V){Volume=V;if(auto Audio=Player->GetWorld()->GetAudioDevice()) Audio->SetTransientPrimaryVolume(V);})];
            Text(TEXT("Camera distance  /  close to wide"),13);
            Body->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(SSlider).Value((Zoom-1000)/900).OnValueChanged_Lambda([this](float V){Zoom=1000+V*900;})];
            Button(TEXT("Apply and save"),[this,Settings](){Settings->ApplySettings(false);Settings->SaveSettings();GConfig->SetFloat(TEXT("Crownfall"),TEXT("MasterVolume"),Volume,GGameUserSettingsIni);GConfig->SetFloat(TEXT("Crownfall"),TEXT("CameraDistance"),Zoom,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);Player->ApplyPlayerSettings();Page=0;Rebuild();});
            Button(TEXT("Back"),[this](){Page=0;Rebuild();});
        }
        Body->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(Player.IsValid()?Player->MenuMessage:FString());}).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(Gold).AutoWrapText(true)];
    }
};
}
void ARGPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&ARGPlayerController::ToggleMenu).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::M,IE_Pressed,this,&ARGPlayerController::ToggleMap);
}
void ARGPlayerController::ToggleMenu()
{
    if (GetWorld()->GetAuthGameMode<ARGFrontEndGameMode>()) return;
    if (bMenuOpen) CloseMenu(); else ShowMenu();
}
void ARGPlayerController::ShowMenu()
{
    if (!IsLocalController() || !GEngine->GameViewport) return;
    if (MenuWidget.IsValid()) GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
    if (auto P=Cast<ARGCharacter>(GetPawn())) { P->bAttackHeld=false; P->Act(6); }
    bMenuOpen=true; SetIgnoreMoveInput(true); SetIgnoreLookInput(true);
    if (!GetWorld()->GetAuthGameMode<ARGFrontEndGameMode>() && GetNetMode()==NM_Standalone) SetPause(true);
    MenuWidget=SNew(SCrownfallMenu).Player(this);
    GEngine->GameViewport->AddViewportWidgetContent(MenuWidget.ToSharedRef(),100);
    FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(MenuWidget); Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
    bShowMouseCursor=true; DefaultMouseCursor=EMouseCursor::Default;
}
void ARGPlayerController::CloseMenu()
{
    if (MenuWidget.IsValid() && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
    MenuWidget.Reset(); bMenuOpen=false; ResetIgnoreMoveInput(); ResetIgnoreLookInput();
    if (GetNetMode()==NM_Standalone) SetPause(false);
    FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);SetInputMode(Mode);
    DefaultMouseCursor=EMouseCursor::Crosshairs;
    FSlateApplication::Get().SetAllUserFocusToGameViewport();
}
void ARGPlayerController::ApplyPlayerSettings()
{
    float Volume=.75f,Zoom=1350;
    GConfig->GetFloat(TEXT("Crownfall"),TEXT("MasterVolume"),Volume,GGameUserSettingsIni);
    GConfig->GetFloat(TEXT("Crownfall"),TEXT("CameraDistance"),Zoom,GGameUserSettingsIni);
    if(auto Audio=GetWorld()->GetAudioDevice()) Audio->SetTransientPrimaryVolume(Volume);
    if(auto P=Cast<ARGCharacter>(GetPawn())) P->Boom->TargetArmLength=Zoom;
}
void ARGPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if(GEngine) { GEngine->OnNetworkFailure().Remove(NetworkFailureHandle); if(MenuWidget.IsValid() && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef()); }
    MenuWidget.Reset(); Super::EndPlay(Reason);
}
ARGFrontEndGameMode::ARGFrontEndGameMode()
{
    PlayerControllerClass=ARGPlayerController::StaticClass(); DefaultPawnClass=nullptr;
}
void ARGFrontEndGameMode::BeginPlay()
{
    Super::BeginPlay();
    auto Arena=GetWorld()->SpawnActorDeferred<ARGArena>(ARGArena::StaticClass(),FTransform::Identity);
    Arena->bSpawnGameplay=false;UGameplayStatics::FinishSpawningActor(Arena,FTransform::Identity);
    auto Camera=GetWorld()->SpawnActor<ACameraActor>(FVector(-6600,2450,3000),FRotator(-34,-32,0));
    Camera->GetCameraComponent()->FieldOfView=65;
    if(auto PC=GetWorld()->GetFirstPlayerController()) PC->SetViewTarget(Camera);
    if(FParse::Param(FCommandLine::Get(),TEXT("MenuPreview")))
    {
        FTimerHandle Shot,Exit;
        GetWorldTimerManager().SetTimer(Shot,FTimerDelegate::CreateWeakLambda(this,[](){
            const FString Name=FParse::Param(FCommandLine::Get(),TEXT("SettingsPreview"))?TEXT("Settings.png"):FParse::Param(FCommandLine::Get(),TEXT("NetworkPreview"))?TEXT("MultiplayerMenu.png"):TEXT("StartMenu.png");
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Validation")/Name,true,false);
        }),8,false);
        GetWorldTimerManager().SetTimer(Exit,FTimerDelegate::CreateWeakLambda(this,[](){FPlatformMisc::RequestExit(false);}),11,false);
    }
}
