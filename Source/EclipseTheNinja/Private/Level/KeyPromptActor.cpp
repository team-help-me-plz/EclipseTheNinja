// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/KeyPromptActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/ArrowComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystem/InputGlyphSubsystem.h"
#include "Widgets/InputPromptWidget.h"

AKeyPromptActor::AKeyPromptActor()
{
    // 카메라 향하기·거리 페이드·떠다니기 중 하나라도 켜져 있을 때만 BeginPlay에서 틱을 켠다
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    this->SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(this->SceneRoot);

    // 월드 공간 위젯. 바탕을 투명하게 두고 반투명 블렌드로 그려 키 이미지의 투명 영역이 그대로 비치게 한다
    this->KeyWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("KeyWidget"));
    this->KeyWidget->SetupAttachment(this->SceneRoot);
    this->KeyWidget->SetWidgetSpace(EWidgetSpace::World);
    this->KeyWidget->SetWidgetClass(UKeyPromptWidget::StaticClass());
    this->KeyWidget->SetDrawAtDesiredSize(true);
    this->KeyWidget->SetPivot(FVector2D(0.5f, 0.5f));
    this->KeyWidget->SetBlendMode(EWidgetBlendMode::Transparent);
    this->KeyWidget->SetBackgroundColor(FLinearColor::Transparent);
    this->KeyWidget->SetTwoSided(false);

    // 안내판은 보기만 하는 장식이라 충돌·그림자·내비에 관여하지 않게 한다
    this->KeyWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    this->KeyWidget->SetGenerateOverlapEvents(false);
    this->KeyWidget->SetCastShadow(false);
    this->KeyWidget->SetCanEverAffectNavigation(false);

#if WITH_EDITORONLY_DATA
    this->FacingArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("FacingArrow"));
    if (this->FacingArrow)
    {
        this->FacingArrow->SetupAttachment(this->KeyWidget);
        this->FacingArrow->ArrowColor = FColor::Cyan;
        this->FacingArrow->ArrowSize = 0.5f;
        this->FacingArrow->bIsScreenSizeScaled = true;
    }
#endif
}

void AKeyPromptActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 에디터 뷰포트에서 바로 미리 보이도록 위젯을 만들고 설정을 적용한다
    const UWorld* World = GetWorld();
    if (!World || World->IsGameWorld())
    {
        return;
    }

    this->KeyWidget->InitWidget();
    RefreshPrompt();
    ApplyTint(1.0f);
}

void AKeyPromptActor::BeginPlay()
{
    Super::BeginPlay();

    this->BaseWidgetLocation = this->KeyWidget->GetRelativeLocation();

    // 입력 장치(키보드 ↔ 게임패드)가 바뀌거나 키 설정이 바뀌면 이미지를 다시 고른다
    const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    const ULocalPlayer* LocalPlayer = PlayerController
        ? PlayerController->GetLocalPlayer()
        : nullptr;

    if (LocalPlayer)
    {
        this->DeviceSubsystem = ULocalPlayer::GetSubsystem<UInputGlyphSubsystem>(LocalPlayer);
        if (this->DeviceSubsystem.IsValid())
        {
            this->DeviceChangedHandle = this->DeviceSubsystem->OnDeviceChanged.AddUObject(this, &AKeyPromptActor::HandleDeviceChanged);
        }

        if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
        {
            EnhancedInput->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &AKeyPromptActor::HandleMappingsRebuilt);
        }
    }

    RefreshPrompt();

    // 거리 페이드를 쓰면 처음에는 숨겨 두고 플레이어가 다가올 때 떠오르게 한다
    this->CurrentAlpha = this->ShowDistance > 0.0f
        ? CalcDistanceAlpha()
        : 1.0f;

    ApplyTint(this->CurrentAlpha);

    const bool bNeedsTick = this->bFaceCamera || this->ShowDistance > 0.0f || this->BobHeight > 0.0f;
    SetActorTickEnabled(bNeedsTick);
}

void AKeyPromptActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (this->DeviceSubsystem.IsValid())
    {
        this->DeviceSubsystem->OnDeviceChanged.Remove(this->DeviceChangedHandle);
    }

    this->DeviceChangedHandle.Reset();
    this->DeviceSubsystem.Reset();

    const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    const ULocalPlayer* LocalPlayer = PlayerController
        ? PlayerController->GetLocalPlayer()
        : nullptr;

    if (LocalPlayer)
    {
        if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
        {
            EnhancedInput->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &AKeyPromptActor::HandleMappingsRebuilt);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void AKeyPromptActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (this->bFaceCamera)
    {
        FaceCamera();
    }

    // 거리 페이드: 목표 불투명도로 부드럽게 따라간다
    if (this->ShowDistance > 0.0f)
    {
        const float TargetAlpha = CalcDistanceAlpha();
        const float NewAlpha = FMath::FInterpTo(this->CurrentAlpha, TargetAlpha, DeltaSeconds, this->FadeSpeed);

        if (!FMath::IsNearlyEqual(NewAlpha, this->CurrentAlpha, 0.001f))
        {
            this->CurrentAlpha = NewAlpha;
            ApplyTint(this->CurrentAlpha);
        }
    }

    // 떠다니기: 배치 위치를 중심으로 위아래로 천천히 움직인다
    if (this->BobHeight > 0.0f)
    {
        this->BobTime += DeltaSeconds;

        const float Phase = this->BobTime / this->BobPeriod * UE_TWO_PI;
        const FVector Offset(0.0f, 0.0f, FMath::Sin(Phase) * this->BobHeight);

        this->KeyWidget->SetRelativeLocation(this->BaseWidgetLocation + Offset);
    }
}

void AKeyPromptActor::SetKeyboardKey(const FKey& InKey)
{
    this->KeyboardKey = InKey;
    RefreshPrompt();
}

void AKeyPromptActor::RefreshPrompt()
{
    // 속도를 기록하는 재질로 그려 카메라가 움직여도 TSR·모션 블러에 번지지 않게 한다
    if (this->WidgetMaterial && this->KeyWidget->GetMaterial(0) != this->WidgetMaterial)
    {
        this->KeyWidget->SetMaterial(0, this->WidgetMaterial);
    }

    // RenderScale배 크게 그린 위젯을 그만큼 줄여 표시해 크기는 그대로 두고 선명도만 높인다
    const float SafeRenderScale = FMath::Max(this->RenderScale, 1.0f);
    this->KeyWidget->SetRelativeScale3D(FVector(1.0f / SafeRenderScale));

    UKeyPromptWidget* PromptWidget = Cast<UKeyPromptWidget>(this->KeyWidget->GetUserWidgetObject());
    if (!PromptWidget)
    {
        return;
    }

    if (!this->LoadedGlyphSet)
    {
        this->LoadedGlyphSet = this->GlyphSet.LoadSynchronous();
    }

    PromptWidget->ApplyStyle(this->Style, SafeRenderScale);

    // 이미지 세트에 없는 키는 Unknown 이미지 대신 글자 키캡으로 보여 준다
    auto FindGlyph = [this](const FKey& Key, EInputGlyphDevice Device) -> UTexture2D*
    {
        if (!this->LoadedGlyphSet || !Key.IsValid())
        {
            return nullptr;
        }

        UTexture2D* Texture = this->LoadedGlyphSet->FindTexture(Key, Device);
        return Texture != this->LoadedGlyphSet->Unknown
            ? Texture
            : nullptr;
    };

    // 게임패드: 버튼 이미지를 우선 보여 주고, 지정된 버튼이 없으면 키보드 표시로 넘어간다
    const EInputGlyphDevice Device = GetDisplayDevice();
    if (Device != EInputGlyphDevice::KeyboardMouse)
    {
        // 직접 지정한 게임패드 이미지가 있으면 키 이미지 세트보다 우선한다(PS 전용 이미지 > 공용 이미지)
        UTexture2D* OverrideImage = (Device == EInputGlyphDevice::PlayStation && this->PlayStationImage)
            ? this->PlayStationImage.Get()
            : this->GamepadImage.Get();

        if (OverrideImage)
        {
            PromptWidget->ShowImage(OverrideImage);
            return;
        }

        const FKey PadKey = ResolveKey(true);
        if (PadKey.IsValid())
        {
            if (UTexture2D* PadGlyph = FindGlyph(PadKey, Device))
            {
                PromptWidget->ShowImage(PadGlyph);
            }
            else
            {
                PromptWidget->ShowLabel(UInputPromptWidget::FormatKey(PadKey));
            }

            return;
        }
    }

    // 키보드: 직접 쓴 글자 > 키 이미지 > 키 이름 글자 순서
    if (!this->CustomText.IsEmpty())
    {
        PromptWidget->ShowLabel(this->CustomText);
        return;
    }

    const FKey Key = ResolveKey(false);
    if (UTexture2D* KeyGlyph = FindGlyph(Key, EInputGlyphDevice::KeyboardMouse))
    {
        PromptWidget->ShowImage(KeyGlyph);
        return;
    }

    const FText KeyLabel = Key.IsValid()
        ? UInputPromptWidget::FormatKey(Key)
        : FText::GetEmpty();

    PromptWidget->ShowLabel(KeyLabel);
}

EInputGlyphDevice AKeyPromptActor::GetDisplayDevice() const
{
    const UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return this->PreviewDevice;
    }

    return this->DeviceSubsystem.IsValid()
        ? this->DeviceSubsystem->GetDevice()
        : EInputGlyphDevice::KeyboardMouse;
}

FKey AKeyPromptActor::ResolveKey(bool bGamepad) const
{
    // InputAction이 있으면 현재 키 설정에서 장치에 맞는 키를 찾는다(에디터에서는 플레이어가 없어 아래 키로 넘어간다)
    if (this->InputAction)
    {
        const UWorld* World = GetWorld();
        if (World && World->IsGameWorld())
        {
            APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
            const FKey ActionKey = UInputPromptWidget::ResolveActionKey(
                this,
                PlayerController,
                this->InputAction,
                nullptr,
                FText::GetEmpty(),
                bGamepad
            );

            if (ActionKey.IsValid())
            {
                return ActionKey;
            }
        }
    }

    return bGamepad
        ? this->GamepadKey
        : this->KeyboardKey;
}

float AKeyPromptActor::CalcDistanceAlpha() const
{
    const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!IsValid(PlayerPawn))
    {
        return 0.0f;
    }

    // ShowDistance 안은 1, 그 밖으로 FadeDistance만큼 가면 0
    const float Distance = FVector::Dist(PlayerPawn->GetActorLocation(), this->KeyWidget->GetComponentLocation());
    const float FadeRatio = (Distance - this->ShowDistance) / FMath::Max(this->FadeDistance, 1.0f);

    return 1.0f - FMath::Clamp(FadeRatio, 0.0f, 1.0f);
}

void AKeyPromptActor::ApplyTint(float Alpha)
{
    const FLinearColor Tint(
        this->Brightness,
        this->Brightness,
        this->Brightness,
        this->Opacity * Alpha
    );

    this->KeyWidget->SetTintColorAndOpacity(Tint);

    // 완전히 사라졌을 때는 그리지 않는다
    this->KeyWidget->SetVisibility(Alpha > 0.01f);
}

void AKeyPromptActor::FaceCamera()
{
    const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController || !PlayerController->PlayerCameraManager)
    {
        return;
    }

    // 위젯은 로컬 +X 쪽에서 보이므로, +X가 카메라 시선 반대 방향을 향하게 돌린다
    const FVector CameraForward = PlayerController->PlayerCameraManager->GetCameraRotation().Vector();
    const float Yaw = (-CameraForward).Rotation().Yaw;

    this->KeyWidget->SetWorldRotation(FRotator(0.0f, Yaw, 0.0f));
}

void AKeyPromptActor::HandleDeviceChanged()
{
    RefreshPrompt();
}

void AKeyPromptActor::HandleMappingsRebuilt()
{
    RefreshPrompt();
}
