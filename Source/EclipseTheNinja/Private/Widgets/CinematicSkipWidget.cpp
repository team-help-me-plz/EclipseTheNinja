#include "Widgets/CinematicSkipWidget.h"

#include "Character/PlayerCharacterController.h"

#include "Components/Image.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

UCinematicSkipWidget::UCinematicSkipWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UInputAction> SkipActionFinder(
        TEXT("/Game/Input/IA_SkipCinematic.IA_SkipCinematic"));
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> SkipContextFinder(
        TEXT("/Game/Input/IMC_SkipCinematic.IMC_SkipCinematic"));
    if (SkipActionFinder.Succeeded())
    {
        this->SkipAction = SkipActionFinder.Object;
    }
    if (SkipContextFinder.Succeeded())
    {
        this->SkipMappingContext = SkipContextFinder.Object;
    }
}

void UCinematicSkipWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!this->bSkipAvailable)
    {
        HidePrompt();
    }
}

void UCinematicSkipWidget::NativeDestruct()
{
    SetSkipAvailable(false);
    Super::NativeDestruct();
}

void UCinematicSkipWidget::SetSkipAvailable(bool bAvailable)
{
    if (this->bSkipAvailable == bAvailable)
    {
        return;
    }
    this->bSkipAvailable = bAvailable;
    this->bHolding = false;
    this->bSkipRequested = false;

    APlayerCharacterController* CharacterController = Cast<APlayerCharacterController>(GetOwningPlayer());
    if (bAvailable)
    {
        BindSkipInput();
        // ESC는 일시정지 메뉴도 여는 키라서, 건너뛸 수 있는 동안에는 메뉴를 막는다
        if (CharacterController)
        {
            CharacterController->PushPauseMenuBlock();
        }
    }
    else
    {
        UnbindSkipInput();
        if (CharacterController)
        {
            CharacterController->PopPauseMenuBlock();
        }
    }

    // 켜도 키를 누르기 전까지는 화면에 띄우지 않는다
    HidePrompt();
}

void UCinematicSkipWidget::BindSkipInput()
{
    APlayerController* PlayerController = GetOwningPlayer();
    if (!IsValid(PlayerController) || !IsValid(this->SkipAction) || !IsValid(this->SkipMappingContext))
    {
        UE_LOG(LogTemp, Warning, TEXT("[UCinematicSkipWidget::BindSkipInput()] %s : 컨트롤러·SkipAction·SkipMappingContext 중 비어 있는 것이 있어 건너뛰기 입력을 받을 수 없음"), *GetName());
        return;
    }

    if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            InputSubsystem->AddMappingContext(this->SkipMappingContext, this->SkipMappingPriority);
        }
    }

    // 트리거가 없는 매핑은 키가 눌려 있는 동안 매 프레임 Triggered가 온다. 누른 시간은 FInputActionInstance가 잰다
    this->SkipInputComponent = NewObject<UEnhancedInputComponent>(this, NAME_None, RF_Transient);
    this->SkipInputComponent->BindAction(this->SkipAction, ETriggerEvent::Triggered, this, &UCinematicSkipWidget::HandleSkipHeld);
    this->SkipInputComponent->BindAction(this->SkipAction, ETriggerEvent::Completed, this, &UCinematicSkipWidget::HandleSkipReleased);
    this->SkipInputComponent->BindAction(this->SkipAction, ETriggerEvent::Canceled, this, &UCinematicSkipWidget::HandleSkipReleased);

    // 아무 키나 눌러도 잠깐 띄운다. 다른 입력(QTE 등)을 막지 않도록 키를 소비하지 않는다
    // UEnhancedInputComponent는 BindKey를 막아 두었으므로 컨트롤러의 ESC 바인딩처럼 기본 클래스로 키를 묶는다
    UInputComponent* KeyInput = this->SkipInputComponent;
    FInputKeyBinding& AnyKeyBinding = KeyInput->BindKey(EKeys::AnyKey, IE_Pressed, this, &UCinematicSkipWidget::HandleAnyKey);
    AnyKeyBinding.bConsumeInput = false;

    PlayerController->PushInputComponent(this->SkipInputComponent);
}

void UCinematicSkipWidget::UnbindSkipInput()
{
    // 건너뛰기 키 처리 도중(끝까지 눌러 요청한 직후)에도 불리므로 바인딩은 건드리지 않고 입력 스택에서만 내린다
    APlayerController* PlayerController = GetOwningPlayer();
    if (IsValid(this->SkipInputComponent) && IsValid(PlayerController))
    {
        PlayerController->PopInputComponent(this->SkipInputComponent);
    }
    this->SkipInputComponent = nullptr;

    if (IsValid(PlayerController) && IsValid(this->SkipMappingContext))
    {
        if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                InputSubsystem->RemoveMappingContext(this->SkipMappingContext);
            }
        }
    }
}

void UCinematicSkipWidget::HandleSkipHeld(const FInputActionInstance& Instance)
{
    if (!this->bSkipAvailable || this->bSkipRequested)
    {
        return;
    }

    this->bHolding = true;
    ShowPrompt();

    const float Ratio = Instance.GetElapsedTime() / FMath::Max(this->SkipHoldTime, KINDA_SMALL_NUMBER);
    SetProgress(Ratio);

    if (Ratio >= 1.0f)
    {
        this->bSkipRequested = true;
        if (APlayerCharacterController* CharacterController = Cast<APlayerCharacterController>(GetOwningPlayer()))
        {
            CharacterController->RequestCinematicSkip();
        }
    }
}

void UCinematicSkipWidget::HandleSkipReleased()
{
    // 중간에 떼면 안내는 그대로 두고 테두리만 비운다
    this->bHolding = false;
    if (this->bSkipAvailable && !this->bSkipRequested)
    {
        SetProgress(0.0f);
    }
}

void UCinematicSkipWidget::HandleAnyKey()
{
    // 아무 키나 한 번 누르면 건너뛸 수 있는 동안 계속 띄워 둔다
    if (!this->bSkipAvailable || this->bSkipRequested)
    {
        return;
    }
    ShowPrompt();
}

void UCinematicSkipWidget::ShowPrompt()
{
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCinematicSkipWidget::HidePrompt()
{
    SetProgress(0.0f);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UCinematicSkipWidget::SetProgress(float Progress)
{
    if (!IsValid(this->ProgressMaterial) && IsValid(this->BorderImage))
    {
        this->ProgressMaterial = this->BorderImage->GetDynamicMaterial();
    }
    if (IsValid(this->ProgressMaterial))
    {
        this->ProgressMaterial->SetScalarParameterValue(this->ProgressParameterName, FMath::Clamp(Progress, 0.0f, 1.0f));
    }
}
