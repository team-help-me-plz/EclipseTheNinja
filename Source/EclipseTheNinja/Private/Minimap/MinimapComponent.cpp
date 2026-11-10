#include "Minimap/MinimapComponent.h"
#include "Minimap/MinimapSubsystem.h"
#include "Widgets/MinimapWidget.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

// 테스트용: 1이면 Tab 없이 미니맵을 계속 띄운다 (자동 캡처·디버그용)
static TAutoConsoleVariable<int32> CVarMinimapForceShow(
	TEXT("Eclipse.Minimap.ForceShow"), 0,
	TEXT("1이면 Tab 입력 없이 미니맵 위젯을 표시한다."),
	ECVF_Cheat);

// Live Coding으로 다시 패치하면 static 변수가 새로 생겨 등록이 어긋나므로 이름으로 찾아 읽는다
static bool IsForceShowEnabled()
{
	const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Eclipse.Minimap.ForceShow"));
	return Var && Var->GetInt() > 0;
}

namespace MinimapComponentPrivate
{
	// 플레이어 카메라가 플레이 평면(PlaneY)에서 보여 주는 화면의 반폭·반높이(XZ, uu)를 구한다
	static bool GetCameraViewHalfExtent(const APlayerController* PlayerController, float PlaneY, FVector2D& OutHalfExtent)
	{
		const APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager.Get() : nullptr;
		if (!CameraManager)
		{
			return false;
		}

		int32 ViewX = 0;
		int32 ViewY = 0;
		PlayerController->GetViewportSize(ViewX, ViewY);
		if (ViewX <= 0 || ViewY <= 0)
		{
			return false;
		}
		const float InvAspect = static_cast<float>(ViewY) / static_cast<float>(ViewX);

		const FMinimalViewInfo& View = CameraManager->GetCameraCacheView();
		float HalfWidth = 0.f;
		if (View.ProjectionMode == ECameraProjectionMode::Orthographic)
		{
			HalfWidth = View.OrthoWidth * 0.5f;
		}
		else
		{
			// 카메라에서 플레이 평면까지 시선 방향 거리. 카메라가 평면과 거의 나란하면 쓸 수 없다
			const FVector Forward = View.Rotation.Vector();
			if (FMath::Abs(Forward.Y) < 0.1f)
			{
				return false;
			}
			const float Distance = (PlaneY - View.Location.Y) / Forward.Y;
			if (Distance <= 0.f)
			{
				return false;
			}
			HalfWidth = Distance * FMath::Tan(FMath::DegreesToRadians(View.FOV * 0.5f));
		}

		OutHalfExtent = FVector2D(HalfWidth, HalfWidth * InvAspect);
		return HalfWidth > 0.f;
	}
}

UMinimapComponent::UMinimapComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	WidgetClass = UMinimapWidget::StaticClass();
}

void UMinimapComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	// 폰의 BeginPlay는 빙의보다 먼저 올 수 있어 컨트롤러가 바뀔 때 입력과 위젯을 준비한다
	Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &UMinimapComponent::HandleControllerChanged);
	if (APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController()))
	{
		SetupForController(PlayerController);
	}

	GetWorld()->GetTimerManager().SetTimer(this->RevealTimer, this, &UMinimapComponent::TickReveal, this->RevealInterval, true, this->RevealInterval);
}

void UMinimapComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(this->RevealTimer);
	}
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UMinimapComponent::HandleControllerChanged);
	}
	TeardownController();
	Super::EndPlay(EndPlayReason);
}

void UMinimapComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	TeardownController();
	if (APlayerController* PlayerController = Cast<APlayerController>(NewController))
	{
		SetupForController(PlayerController);
	}
}

void UMinimapComponent::SetupForController(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController() || this->BoundController.Get() == PlayerController)
	{
		return;
	}
	TeardownController();
	this->BoundController = PlayerController;

	// 입력 액션과 IMC. 에셋을 지정하지 않았으면 런타임에 만든다
	this->ActiveAction = this->MinimapAction;
	if (!this->ActiveAction)
	{
		this->ActiveAction = NewObject<UInputAction>(this, TEXT("IA_Minimap_Runtime"));
		this->ActiveAction->ValueType = EInputActionValueType::Boolean;
	}
	this->ActiveContext = this->MinimapMappingContext;
	if (!this->ActiveContext)
	{
		this->ActiveContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Minimap_Runtime"));
		this->ActiveContext->MapKey(this->ActiveAction, this->FallbackKey);
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(this->ActiveContext, this->MappingPriority);
	}

	// 컨트롤러의 입력 스택에 따로 넣어 기본 IMC를 빼는 입력 잠금과 무관하게 받는다
	this->MinimapInputComponent = NewObject<UEnhancedInputComponent>(GetOwner(), TEXT("MinimapInput"));
	this->MinimapInputComponent->RegisterComponent();
	this->MinimapInputComponent->bBlockInput = false;
	this->MinimapInputComponent->BindAction(this->ActiveAction, ETriggerEvent::Started, this, &UMinimapComponent::OnMinimapStarted);
	this->MinimapInputComponent->BindAction(this->ActiveAction, ETriggerEvent::Completed, this, &UMinimapComponent::OnMinimapCompleted);
	this->MinimapInputComponent->BindAction(this->ActiveAction, ETriggerEvent::Canceled, this, &UMinimapComponent::OnMinimapCompleted);
	PlayerController->PushInputComponent(this->MinimapInputComponent);

	// 위젯은 미리 만들어 두고 숨긴다
	if (this->WidgetClass)
	{
		this->Widget = CreateWidget<UMinimapWidget>(PlayerController, this->WidgetClass);
		if (this->Widget)
		{
			this->Widget->SetViewer(Cast<APawn>(GetOwner()));
			this->Widget->SetVisibility(ESlateVisibility::Collapsed);
			this->Widget->AddToViewport(this->WidgetZOrder);
		}
	}
	this->bMinimapVisible = false;
}

void UMinimapComponent::TeardownController()
{
	APlayerController* PlayerController = this->BoundController.Get();
	if (PlayerController)
	{
		if (this->MinimapInputComponent)
		{
			PlayerController->PopInputComponent(this->MinimapInputComponent);
		}
		if (this->ActiveContext)
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				InputSubsystem->RemoveMappingContext(this->ActiveContext);
			}
		}
	}

	if (this->MinimapInputComponent)
	{
		this->MinimapInputComponent->ClearActionBindings();
		this->MinimapInputComponent->DestroyComponent();
		this->MinimapInputComponent = nullptr;
	}
	if (this->Widget)
	{
		this->Widget->RemoveFromParent();
		this->Widget = nullptr;
	}

	this->BoundController.Reset();
	this->bMinimapVisible = false;
}

void UMinimapComponent::OnMinimapStarted(const FInputActionValue& Value)
{
	SetMinimapVisible(this->bHoldToShow ? true : !this->bMinimapVisible);
}

void UMinimapComponent::OnMinimapCompleted(const FInputActionValue& Value)
{
	if (this->bHoldToShow)
	{
		SetMinimapVisible(false);
	}
}

void UMinimapComponent::SetMinimapVisible(bool bVisible)
{
	this->bMinimapVisible = bVisible;
	if (this->Widget)
	{
		this->Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// 열자마자 최신 시야가 보이도록 바로 한 번 밝힌다
	if (bVisible)
	{
		this->LastRevealTime = -1000.0;
		TickReveal();
	}
}

FVector UMinimapComponent::GetEyeLocation() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Pawn->GetActorLocation() + FVector(0.f, 0.f, Pawn->BaseEyeHeight) : FVector::ZeroVector;
}

void UMinimapComponent::TickReveal()
{
	UWorld* World = GetWorld();
	UMinimapSubsystem* Minimap = UMinimapSubsystem::Get(this);
	if (!World || !Minimap)
	{
		return;
	}

	if (IsForceShowEnabled() && !this->bMinimapVisible && this->Widget)
	{
		SetMinimapVisible(true);
		return;
	}

	const FVector Eye = GetEyeLocation();
	const double Now = World->GetTimeSeconds();
	const bool bMoved = FVector::DistSquared(Eye, this->LastRevealLocation) >= FMath::Square(this->MoveThreshold);
	if (!bMoved && Now - this->LastRevealTime < this->IdleRevealInterval)
	{
		return;
	}

	this->LastRevealLocation = Eye;
	this->LastRevealTime = Now;
	// 화면에 보이는 크기만큼만 밝힌다. 카메라 정보를 못 얻으면 RevealRadius 원으로 대신한다
	// 카메라 이동(둘러보기)으로 밝힐 범위가 늘지 않도록 사각형은 카메라가 아니라 플레이어를 중심으로 둔다
	FVector2D ViewHalfExtent;
	if (MinimapComponentPrivate::GetCameraViewHalfExtent(this->BoundController.Get(), Eye.Y, ViewHalfExtent))
	{
		Minimap->RevealSightInView(Eye, ViewHalfExtent, this->RayCount, GetOwner());
	}
	else
	{
		Minimap->RevealSight(Eye, this->RevealRadius, this->RayCount, GetOwner());
	}
}
