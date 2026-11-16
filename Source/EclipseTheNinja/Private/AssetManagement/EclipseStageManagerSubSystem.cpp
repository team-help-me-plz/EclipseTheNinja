#include "AssetManagement/EclipseStageManagerSubsystem.h"

#include "AssetManagement/EclipseAssetManager.h"
#include "AssetManagement/EclipseAssetSet.h"
#include "AssetManagement/EclipseGameData.h"
#include "AssetManagement/EclipseStageDefinition.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Level/EclipseWorldSettings.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Styling/CoreStyle.h"
#include "Util/NiagaraWarmup.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// 프리웜 이펙트를 놓을 위치: 카메라 앞 이만큼 (cm)
	constexpr float WarmupDistance = 300.0f;
	// 이펙트를 최소 이만큼은 그린다 (첫 렌더링·셰이더 준비)
	constexpr double WarmupMinSeconds = 0.25;
	// 텍스처가 다 올라오지 않아도 이 시간이 지나면 끝낸다
	constexpr double WarmupMaxSeconds = 10.0;
	// 플레이어 카메라가 이 시간 안에 생기지 않으면 원점에서 프리웜한다
	constexpr double PlayerWaitSeconds = 5.0;
	constexpr float WarmupTickInterval = 0.05f;
	// 로딩 화면은 다른 UI보다 위에 그린다
	constexpr int32 LoadingScreenZOrder = 10000;
	// 로딩 화면이 사라지는 시간
	constexpr double LoadingFadeSeconds = 0.3;
	constexpr float LoadingFadeTickInterval = 0.016f;

	// 프리웜 이펙트가 쓰는 텍스처를 모으고, 새로 찾은 텍스처는 밉을 모두 올리도록 요청한다
	void GatherWarmupTextures(const TArray<TObjectPtr<UNiagaraComponent>>& Components, TArray<TObjectPtr<UTexture2D>>& OutTextures)
	{
		for (const UNiagaraComponent* Component : Components)
		{
			if (!IsValid(Component))
			{
				continue;
			}
			TArray<UMaterialInterface*> Materials;
			Component->GetUsedMaterials(Materials);
			for (const UMaterialInterface* Material : Materials)
			{
				if (!Material)
				{
					continue;
				}
				TArray<UTexture*> Textures;
				Material->GetUsedTextures(Textures);
				for (UTexture* Texture : Textures)
				{
					UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
					if (Texture2D && !OutTextures.Contains(Texture2D))
					{
						Texture2D->SetForceMipLevelsToBeResident(static_cast<float>(WarmupMaxSeconds) + 2.0f);
						OutTextures.Add(Texture2D);
					}
				}
			}
		}
	}
}

void UEclipseStageManagerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	// 액터의 BeginPlay보다 먼저 호출
	Super::OnWorldBeginPlay(InWorld);

	const AEclipseWorldSettings* WorldSettings = Cast<AEclipseWorldSettings>(InWorld.GetWorldSettings());
	const TSoftObjectPtr<UEclipseStageDefinition> Definition = WorldSettings ? WorldSettings->StageDefinition : nullptr;

	if (Definition.IsNull())
	{
		// 메인 메뉴·테스트 맵처럼 정의가 없는 맵은 기다리지 않는다.
		// GameData 이펙트만 보이지 않게 작게 프리웜한다 (텍스처 밉까지는 올리지 않는다)
		LoadState = EEclipseStageLoadState::Loaded;
		for (UNiagaraSystem* System : CollectEffects())
		{
			EclipseVFX::WarmupNiagaraSystem(this, System, FVector::ZeroVector);
		}
		// 이 서브시스템보다 먼저 OnWorldBeginPlay가 불린 다른 서브시스템이 등록해 둔 것도 알린다
		OnStageLoaded.Broadcast(nullptr);
		OnStageLoaded.Clear();
		return;
	}

	LoadState = EEclipseStageLoadState::Loading;
	ShowLoadingScreen();
	StartStageLoad(Definition);
}

void UEclipseStageManagerSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarmupTimer);
		World->GetTimerManager().ClearTimer(LoadingFadeTimer);
	}
	ClearWarmupEffects();
	HideLoadingScreen();

	// 맵이 닫힐 때 이 맵에서 불러온 것을 놓는다. 실제 해제는 맵 이동 시의 GC에서 일어난다
	if (!LoadedIds.IsEmpty() && UAssetManager::IsInitialized())
	{
		UAssetManager::Get().UnloadPrimaryAssets(LoadedIds);
	}
	LoadedIds.Reset();
	OnStageLoaded.Clear();
	Super::Deinitialize();
}

void UEclipseStageManagerSubsystem::CallOrRegister_OnStageLoaded(FOnEclipseStageLoaded::FDelegate&& Delegate)
{
	if (IsStageLoaded())
	{
		Delegate.Execute(StageDefinition);
	}
	else
	{
		OnStageLoaded.Add(MoveTemp(Delegate));
	}
}

void UEclipseStageManagerSubsystem::StartStageLoad(const TSoftObjectPtr<UEclipseStageDefinition>& Definition)
{
	UEclipseAssetManager& AssetManager = UEclipseAssetManager::Get();
	LoadStartTime = FPlatformTime::Seconds();

	// 정의 에셋 자체는 작아서 바로 불러온다. 그 안의 에셋들은 아래에서 번들로 비동기 로드한다
	StageDefinition = Definition.LoadSynchronous();
	const FPrimaryAssetId DefinitionId = StageDefinition ? StageDefinition->GetPrimaryAssetId() : FPrimaryAssetId();
	if (!StageDefinition || !AssetManager.GetPrimaryAssetPath(DefinitionId).IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage] %s 정의를 찾지 못했거나 에셋 매니저에 등록되지 않아 프리로드 없이 시작합니다."),
			*Definition.ToString());
		StageDefinition = nullptr;
		OnStageAssetsLoaded();
		return;
	}

	LoadedIds.Add(DefinitionId);
	for (const TObjectPtr<UEclipseAssetSet>& AssetSet : StageDefinition->AssetSets)
	{
		if (AssetSet)
		{
			LoadedIds.AddUnique(AssetSet->GetPrimaryAssetId());
		}
	}

	TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAssets(
		LoadedIds,
		{ UEclipseAssetManager::BundleGame },
		FStreamableDelegate(),
		FStreamableManager::AsyncLoadHighPriority);

	FStreamableDelegate OnLoaded = FStreamableDelegate::CreateUObject(this, &UEclipseStageManagerSubsystem::OnStageAssetsLoaded);

	// 이미 다 메모리에 있으면 핸들이 없거나 완료 상태로 돌아온다
	if (!Handle.IsValid() || Handle->HasLoadCompleted())
	{
		OnLoaded.ExecuteIfBound();
	}
	else
	{
		Handle->BindCompleteDelegate(OnLoaded);
		// 취소돼도 게임이 멈춰 있지 않도록 완료로 처리한다
		Handle->BindCancelDelegate(FStreamableDelegate::CreateWeakLambda(this, [OnLoaded]() { OnLoaded.ExecuteIfBound(); }));
	}
}

void UEclipseStageManagerSubsystem::OnStageAssetsLoaded()
{
	// 완료와 취소가 둘 다 오거나, 이미 프리웜을 시작한 경우
	if (LoadState != EEclipseStageLoadState::Loading || WarmupBeginTime > 0.0)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Stage] %s 로드 완료 (데이터 %d개, %.2f초)"),
		*GetNameSafe(StageDefinition), LoadedIds.Num(), FPlatformTime::Seconds() - LoadStartTime);

	BeginWarmup();
}

TArray<UNiagaraSystem*> UEclipseStageManagerSubsystem::CollectEffects() const
{
	// 여러 목록에 같은 이펙트가 있어도 한 번만 넣는다
	TArray<UNiagaraSystem*> Systems;
	auto Collect = [&Systems](const FEclipseAssetList& List)
	{
		for (const TSoftObjectPtr<UNiagaraSystem>& Effect : List.Effects)
		{
			if (UNiagaraSystem* System = Effect.Get())
			{
				Systems.AddUnique(System);
			}
		}
	};

	if (const UEclipseGameData* GameData = UEclipseAssetManager::Get().GetGameData())
	{
		Collect(GameData->GlobalAssets);
	}
	if (StageDefinition)
	{
		Collect(StageDefinition->Assets);
		for (const TObjectPtr<UEclipseAssetSet>& AssetSet : StageDefinition->AssetSets)
		{
			if (AssetSet)
			{
				Collect(AssetSet->Assetes);
			}
		}
	}
	return Systems;
}

void UEclipseStageManagerSubsystem::BeginWarmup()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		FinishWarmup(TEXT("월드 없음"));
		return;
	}

	WarmupBeginTime = FPlatformTime::Seconds();
	WarmupSpawnTime = 0.0;
	World->GetTimerManager().SetTimer(WarmupTimer,
		FTimerDelegate::CreateUObject(this, &UEclipseStageManagerSubsystem::TickWarmup), WarmupTickInterval, true);
	// 이미 플레이어가 있으면 다음 틱을 기다리지 않는다
	TickWarmup();
}

void UEclipseStageManagerSubsystem::TickWarmup()
{
	const double Now = FPlatformTime::Seconds();

	// 1) 플레이어 카메라를 기다렸다가 그 앞에서 재생한다. 텍스처 스트리밍은 카메라에서 보이는 크기로 해상도를 고르므로
	//    원점처럼 먼 곳이나 아주 작은 크기로 재생하면 고해상도 밉이 올라오지 않는다
	if (WarmupSpawnTime <= 0.0)
	{
		const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
		if (Camera && PC->GetPawn() && !Camera->GetCameraLocation().IsNearlyZero())
		{
			const FRotator CameraRotation = Camera->GetCameraRotation();
			SpawnWarmupEffects(Camera->GetCameraLocation() + CameraRotation.Vector() * WarmupDistance, CameraRotation);
		}
		else if (Now - WarmupBeginTime > PlayerWaitSeconds)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Stage] 플레이어 카메라가 없어 원점에서 프리웜합니다."));
			SpawnWarmupEffects(FVector::ZeroVector, FRotator::ZeroRotator);
		}
		return;
	}

	// 2) 방금 만든 이펙트는 이미터가 아직 준비되지 않아 머티리얼이 다 잡히지 않으므로, 기다리는 동안 텍스처를 계속 모은다
	GatherWarmupTextures(WarmupComponents, WarmupTextures);

	// 3) 최소 시간만큼 그린 뒤, 텍스처 밉이 다 올라오면(또는 시간 제한) 끝낸다
	const double Elapsed = Now - WarmupSpawnTime;
	TArray<FString> NotStreamed;
	for (UTexture2D* Texture : WarmupTextures)
	{
		if (Texture && !Texture->IsFullyStreamedIn())
		{
			NotStreamed.Add(Texture->GetName());
		}
	}

	if (Elapsed >= WarmupMinSeconds && NotStreamed.IsEmpty())
	{
		FinishWarmup(TEXT("텍스처 준비 완료"));
	}
	else if (Elapsed >= WarmupMaxSeconds)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage] 시간 안에 다 올라오지 않은 텍스처 %d장: %s"),
			NotStreamed.Num(), *FString::Join(NotStreamed, TEXT(", ")));
		FinishWarmup(TEXT("시간 제한"));
	}
}

void UEclipseStageManagerSubsystem::SpawnWarmupEffects(const FVector& Location, const FRotator& Rotation)
{
	WarmupSpawnTime = FPlatformTime::Seconds();

	const TArray<UNiagaraSystem*> Systems = CollectEffects();
	WarmupEffectCount = Systems.Num();

	for (UNiagaraSystem* System : Systems)
	{
		// 실제 크기로 재생한다. 로딩 화면이 가리므로 보이지 않는다.
		// 풀을 쓰지 않는다: 끝나서 풀로 돌아간 컴포넌트를 실제 게임이 다시 쓰는 것을 프리웜이 지우면 안 되므로 직접 만들고 직접 지운다
		UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, System, Location, Rotation, FVector::OneVector,
			/*bAutoDestroy*/ false, /*bAutoActivate*/ true, ENCPoolMethod::None, /*bPreCullCheck*/ false);
		if (!Component)
		{
			continue;
		}
		WarmupComponents.Add(Component);
	}

	// 이 이펙트들이 쓰는 텍스처의 밉을 모두 올리도록 요청한다 (기다리는 동안 TickWarmup에서도 계속 모은다)
	GatherWarmupTextures(WarmupComponents, WarmupTextures);
}

void UEclipseStageManagerSubsystem::FinishWarmup(const TCHAR* Reason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarmupTimer);
	}

	const double Now = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Log, TEXT("[Stage] 이펙트 프리웜 %d개, 텍스처 %d장 (%.2f초, %s)"),
		WarmupEffectCount, WarmupTextures.Num(), WarmupBeginTime > 0.0 ? Now - WarmupBeginTime : 0.0, Reason);

	ClearWarmupEffects();
	WarmupTextures.Reset();
	FadeOutLoadingScreen();

	LoadState = EEclipseStageLoadState::Loaded;

	// 로딩 중 막아 둔 플레이어 입력을 돌려준다(PlayerCharacter::BeginPlay). 알림보다 먼저 해야
	// 알림을 받고 시작하는 컷신이 입력을 다시 막은 상태가 유지된다
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->EnableInput(PC);
		}
	}

	OnStageLoaded.Broadcast(StageDefinition);
	OnStageLoaded.Clear();
}

void UEclipseStageManagerSubsystem::ClearWarmupEffects()
{
	for (UNiagaraComponent* Component : WarmupComponents)
	{
		if (IsValid(Component))
		{
			Component->DeactivateImmediate();
			Component->DestroyComponent();
		}
	}
	WarmupComponents.Reset();
}

void UEclipseStageManagerSubsystem::ShowLoadingScreen()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || LoadingScreen.IsValid())
	{
		return;
	}

	// 검은 화면 + 오른쪽 아래 "불러오는 중"과 회전 표시. 뒤에서 프리웜 이펙트를 그려도 보이지 않게 화면 전체를 덮는다
	LoadingScreen = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::Black)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(48.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 16.0f, 0.0f))
			[
				SNew(SCircularThrobber)
				.Radius(14.0f)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("불러오는 중")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 20))
				.ColorAndOpacity(FLinearColor::White)
			]
		];

	Viewport->AddViewportWidgetContent(LoadingScreen.ToSharedRef(), LoadingScreenZOrder);
}

void UEclipseStageManagerSubsystem::FadeOutLoadingScreen()
{
	UWorld* World = GetWorld();
	if (!LoadingScreen.IsValid() || !World)
	{
		HideLoadingScreen();
		return;
	}

	LoadingFadeStartTime = FPlatformTime::Seconds();
	World->GetTimerManager().SetTimer(LoadingFadeTimer,
		FTimerDelegate::CreateUObject(this, &UEclipseStageManagerSubsystem::TickLoadingFade), LoadingFadeTickInterval, true);
}

void UEclipseStageManagerSubsystem::TickLoadingFade()
{
	const double Alpha = 1.0 - (FPlatformTime::Seconds() - LoadingFadeStartTime) / LoadingFadeSeconds;
	if (Alpha <= 0.0 || !LoadingScreen.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(LoadingFadeTimer);
		}
		HideLoadingScreen();
		return;
	}
	LoadingScreen->SetRenderOpacity(static_cast<float>(Alpha));
}

void UEclipseStageManagerSubsystem::HideLoadingScreen()
{
	if (!LoadingScreen.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(LoadingScreen.ToSharedRef());
	}
	LoadingScreen.Reset();
}
