#include "Level/GuideSign.h"

#include "Character/PlayerCharacter.h"
#include "Component/InteractHighlightComponent.h"
#include "Level/GuideSignData.h"
#include "Widgets/GuideSignPopupWidget.h"

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "Sound/SoundBase.h"

AGuideSign::AGuideSign()
{
	PrimaryActorTick.bCanEverTick = false;

	this->SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(this->SceneRoot);

	// 플레이어 캡슐(Pawn)과의 겹침만 감지한다. 적의 바닥 검사(Monster 채널)에 걸리지 않도록 나머지는 모두 무시한다
	this->PopupTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("PopupTrigger"));
	this->PopupTrigger->SetupAttachment(this->SceneRoot);
	this->PopupTrigger->InitBoxExtent(FVector(60.f, 60.f, 100.f));
	this->PopupTrigger->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	this->PopupTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	this->PopupTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	this->PopupTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	this->PopupTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	this->PopupTrigger->SetGenerateOverlapEvents(true);
	this->PopupTrigger->CanCharacterStepUpOn = ECB_No;

	// 플레이어의 InteractBox(모든 채널 Overlap)에 잡히도록 겹침만 켠다. Block이 없어 바닥 검사에는 걸리지 않는다
	this->InteractTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractTrigger"));
	this->InteractTrigger->SetupAttachment(this->SceneRoot);
	this->InteractTrigger->InitBoxExtent(FVector(60.f, 60.f, 100.f));
	this->InteractTrigger->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	this->InteractTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	this->InteractTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	this->InteractTrigger->SetCollisionResponseToAllChannels(ECR_Overlap);
	this->InteractTrigger->SetGenerateOverlapEvents(true);
	this->InteractTrigger->CanCharacterStepUpOn = ECB_No;

	// 동영상을 미리 열 범위. 팝업 영역보다 넉넉히 커야 닿기 전에 디코더가 준비된다
	this->PreloadRange = CreateDefaultSubobject<USphereComponent>(TEXT("PreloadRange"));
	this->PreloadRange->SetupAttachment(this->SceneRoot);
	this->PreloadRange->InitSphereRadius(1500.f);
	this->PreloadRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	this->PreloadRange->SetCollisionObjectType(ECC_WorldDynamic);
	this->PreloadRange->SetCollisionResponseToAllChannels(ECR_Ignore);
	this->PreloadRange->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	this->PreloadRange->SetGenerateOverlapEvents(true);
	this->PreloadRange->CanCharacterStepUpOn = ECB_No;

	this->InteractHighlight = CreateDefaultSubobject<UInteractHighlightComponent>(TEXT("InteractHighlight"));

	this->InteractKeyWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractKeyWidget"));
	this->InteractKeyWidget->SetupAttachment(this->SceneRoot);
	this->InteractKeyWidget->SetWidgetSpace(EWidgetSpace::Screen);
	this->InteractKeyWidget->SetDrawAtDesiredSize(true);
	this->InteractKeyWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->InteractKeyWidget->SetVisibility(false);
}

void AGuideSign::BeginPlay()
{
	Super::BeginPlay();

	if (!this->SignDataList.ContainsByPredicate([](const TObjectPtr<UGuideSignData>& Data) { return IsValid(Data); }))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: SignDataList에 유효한 데이터가 없습니다."), *GetName());
	}

	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetKeyWidget(this->InteractKeyWidget);
	}

	this->RefreshHighlight();
	this->CreateVideoObjects();

	this->PopupTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AGuideSign::OnPopupTriggerBeginOverlap);
	this->PreloadRange->OnComponentBeginOverlap.AddUniqueDynamic(this, &AGuideSign::OnPreloadRangeBeginOverlap);
	this->PreloadRange->OnComponentEndOverlap.AddUniqueDynamic(this, &AGuideSign::OnPreloadRangeEndOverlap);

	// 플레이어가 처음부터 범위 안에 있으면 BeginOverlap이 오지 않을 수 있으므로 직접 확인한다
	if (this->IsPlayerInPreloadRange())
	{
		this->PrepareVideos();
	}
}

void AGuideSign::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 팝업이 열린 채 사라지면 알림을 더 받지 않도록 바인딩을 푼다
	if (this->bPopupShowing)
	{
		this->UnbindFromPopup();
		this->ShownPages.Reset();
		this->bPopupShowing = false;
	}

	this->CloseVideos();

	Super::EndPlay(EndPlayReason);
}

bool AGuideSign::IsPlayer(const AActor* InActor)
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(InActor);
	return (IsValid(Player) && Player->IsPlayerControlled());
}

bool AGuideSign::IsPlayerInPreloadRange() const
{
	TArray<AActor*> OverlappingActors;
	this->PreloadRange->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	for (const AActor* Actor : OverlappingActors)
	{
		if (IsPlayer(Actor))
		{
			return (true);
		}
	}

	return (false);
}

bool AGuideSign::CanShowPopup() const
{
	return (!this->bPopupShowing && !this->SignDataList.IsEmpty() && this->PopupClass);
}

void AGuideSign::OnPopupTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	// 한 번 본 표지판은 닿아도 띄우지 않는다 (상호작용으로만 다시 본다)
	if (this->bHasBeenViewed || !IsPlayer(OtherActor) || !this->CanShowPopup())
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
	if (!IsValid(Controller))
	{
		return;
	}

	this->ShowPopup(Controller);
}

void AGuideSign::Interact_Implementation(AActor* InActor)
{
	if (!IsPlayer(InActor) || !this->CanShowPopup())
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(InActor);
	APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
	if (!IsValid(Controller))
	{
		return;
	}

	this->ShowPopup(Controller);
}

void AGuideSign::ShowPopup(APlayerController* InController)
{
	// 같은 프레임에 다른 표지판이 이미 팝업을 열었으면 그 팝업에 페이지만 이어 붙인다
	UGuideSignPopupWidget* Popup = UGuideSignPopupWidget::FindOpenPopup(this);
	const bool bNewPopup = !IsValid(Popup);
	if (bNewPopup)
	{
		Popup = CreateWidget<UGuideSignPopupWidget>(InController, this->PopupClass);
	}

	if (!IsValid(Popup))
	{
		return;
	}

	// 처음 보는 표지판은 마지막 페이지까지 넘겨야 닫을 수 있다
	const bool bFirstView = !this->bHasBeenViewed;

	// 목록 순서대로 페이지를 붙인다. 비어 있는 칸은 건너뛴다
	this->ShownPages.Reset();
	for (int32 Index = 0; Index < this->SignDataList.Num(); ++Index)
	{
		const UGuideSignData* Data = this->SignDataList[Index];
		if (!IsValid(Data))
		{
			continue;
		}

		UMediaTexture* PageTexture = this->GetVideoSource(Index)
			? this->VideoTextures[Index].Get()
			: nullptr;
		const int32 PageIndex = Popup->AddPage(Data, PageTexture, bFirstView);
		if (PageIndex != INDEX_NONE)
		{
			this->ShownPages.Add(PageIndex, Index);
		}
	}

	if (this->ShownPages.IsEmpty())
	{
		return;
	}

	this->bPopupShowing = true;
	this->ActivePopup = Popup;
	Popup->OnPopupClosed.AddUniqueDynamic(this, &AGuideSign::HandlePopupClosed);
	Popup->OnPageChanged.AddUniqueDynamic(this, &AGuideSign::HandlePageChanged);

	// 여러 표지판이 동시에 띄울 때 효과음이 겹쳐 커지지 않도록 팝업을 새로 연 표지판만 재생한다.
	// 일시정지 전에 UI 사운드(PlaySound2D)로 재생해 멈춘 동안에도 끊기지 않는다.
	// 새 팝업은 열면서 첫 페이지를 알리므로 그때 첫 페이지 동영상이 재생된다
	if (bNewPopup)
	{
		if (IsValid(this->PopupSound))
		{
			UGameplayStatics::PlaySound2D(this, this->PopupSound);
		}

		Popup->OpenPopup(InController);
	}

	this->BP_OnPopupShown(bFirstView);
}

void AGuideSign::HandlePageChanged(int32 InPageIndex)
{
	// 보이는 페이지의 동영상만 재생하고, 이 표지판의 다른 페이지 동영상은 멈춘다
	const int32* ShownIndex = this->ShownPages.Find(InPageIndex);

	for (const TPair<int32, int32>& Pair : this->ShownPages)
	{
		if (ShownIndex && Pair.Value == *ShownIndex)
		{
			continue;
		}

		this->StopVideo(Pair.Value);
	}

	if (ShownIndex)
	{
		this->PlayVideo(*ShownIndex);
	}
}

void AGuideSign::HandlePopupClosed()
{
	if (!this->bPopupShowing)
	{
		return;
	}

	this->bPopupShowing = false;
	this->ShownPages.Reset();
	this->UnbindFromPopup();

	// 끝까지 본 뒤에만 닫히므로 닫혔다면 본 것으로 친다. 이후로는 상호작용으로만 다시 띄운다
	this->bHasBeenViewed = true;

	this->StopVideos();

	// 플레이어가 아직 범위 안이면 다시 볼 때를 위해 열어 두고, 아니면 닫는다
	if (!this->IsPlayerInPreloadRange())
	{
		this->CloseVideos();
	}

	this->BP_OnPopupClosed();
}

void AGuideSign::UnbindFromPopup()
{
	UGuideSignPopupWidget* Popup = this->ActivePopup.Get();
	this->ActivePopup.Reset();

	if (!IsValid(Popup))
	{
		return;
	}

	Popup->OnPopupClosed.RemoveDynamic(this, &AGuideSign::HandlePopupClosed);
	Popup->OnPageChanged.RemoveDynamic(this, &AGuideSign::HandlePageChanged);
}

void AGuideSign::ShowInteractionIndicator_Implementation()
{
	this->SelectCount++;
	this->RefreshHighlight();
}

void AGuideSign::HideInteractionIndicator_Implementation()
{
	this->SelectCount = FMath::Max(0, this->SelectCount - 1);
	this->RefreshHighlight();
}

bool AGuideSign::bIsInteractReady_Implementation()
{
	// 처음 보기 전에도 상호작용으로 열 수 있다 (그때도 끝까지 넘겨야 닫힌다)
	return (true);
}

EInteractState AGuideSign::GetInteractState_Implementation()
{
	return (EInteractState::Ready);
}

void AGuideSign::RefreshHighlight()
{
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetHighlight(EInteractState::Ready, (this->SelectCount > 0));
	}
}

void AGuideSign::OnPreloadRangeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (IsPlayer(OtherActor))
	{
		this->PrepareVideos();
	}
}

void AGuideSign::OnPreloadRangeEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	// 팝업이 떠 있는 동안에는 재생 중이므로 닫지 않는다 (닫힐 때 다시 판단한다)
	if (IsPlayer(OtherActor) && !this->bPopupShowing)
	{
		this->CloseVideos();
	}
}

UMediaSource* AGuideSign::GetVideoSource(int32 Index) const
{
	if (!this->SignDataList.IsValidIndex(Index) || !this->VideoPlayers.IsValidIndex(Index))
	{
		return (nullptr);
	}

	const UGuideSignData* Data = this->SignDataList[Index];
	if (!IsValid(Data) || !IsValid(this->VideoPlayers[Index]))
	{
		return (nullptr);
	}

	return (Data->Video);
}

void AGuideSign::CreateVideoObjects()
{
	// 페이지마다 따로 만들어 페이지를 넘겨도 다시 열 필요 없이 바로 재생할 수 있게 한다.
	// SignDataList와 순서를 맞추려고 동영상이 없는 칸도 nullptr로 채운다
	this->VideoPlayers.Init(nullptr, this->SignDataList.Num());
	this->VideoTextures.Init(nullptr, this->SignDataList.Num());

	for (int32 Index = 0; Index < this->SignDataList.Num(); ++Index)
	{
		const UGuideSignData* Data = this->SignDataList[Index];
		if (!IsValid(Data) || !IsValid(Data->Video))
		{
			continue;
		}

		UMediaPlayer* Player = NewObject<UMediaPlayer>(this);
		Player->PlayOnOpen = false;
		Player->SetLooping(true);

		// 닫혀 있을 때는 검은 화면으로 비운다
		UMediaTexture* Texture = NewObject<UMediaTexture>(this);
		Texture->AutoClear = true;
		Texture->ClearColor = FLinearColor::Black;
		Texture->SetDefaultMediaPlayer(Player);
		Texture->UpdateResource();

		this->VideoPlayers[Index] = Player;
		this->VideoTextures[Index] = Texture;
	}
}

void AGuideSign::PrepareVideos()
{
	for (int32 Index = 0; Index < this->VideoPlayers.Num(); ++Index)
	{
		UMediaSource* Source = this->GetVideoSource(Index);
		if (!Source)
		{
			continue;
		}

		UMediaPlayer* Player = this->VideoPlayers[Index];
		if (Player->IsPreparing() || Player->IsReady())
		{
			continue;
		}

		// 파일 열기와 디코더 준비를 미리 끝내 둔다. 재생은 그 페이지가 보일 때 한다
		Player->PlayOnOpen = false;
		Player->OpenSource(Source);
	}
}

void AGuideSign::PlayVideo(int32 Index)
{
	UMediaSource* Source = this->GetVideoSource(Index);
	if (!Source)
	{
		return;
	}

	// 미리 열어 둔 상태면 바로 재생한다
	UMediaPlayer* Player = this->VideoPlayers[Index];
	if (Player->IsReady())
	{
		Player->Rewind();
		Player->Play();
		return;
	}

	// 아직 여는 중이거나 열지 않았으면 열리자마자 재생하게 한다
	Player->PlayOnOpen = true;
	if (!Player->IsPreparing())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: %d번 동영상을 미리 열지 못해 페이지를 띄울 때 엽니다. PreloadRange 크기를 확인하세요."),
			*GetName(),
			Index
		);

		Player->OpenSource(Source);
	}
}

void AGuideSign::StopVideos()
{
	for (int32 Index = 0; Index < this->VideoPlayers.Num(); ++Index)
	{
		this->StopVideo(Index);
	}
}

void AGuideSign::StopVideo(int32 Index)
{
	if (!this->VideoPlayers.IsValidIndex(Index))
	{
		return;
	}

	UMediaPlayer* Player = this->VideoPlayers[Index];
	if (!IsValid(Player))
	{
		return;
	}

	Player->PlayOnOpen = false;

	if (Player->IsReady())
	{
		Player->Pause();
		Player->Rewind();
	}
}

void AGuideSign::CloseVideos()
{
	for (UMediaPlayer* Player : this->VideoPlayers)
	{
		if (!IsValid(Player))
		{
			continue;
		}

		Player->PlayOnOpen = false;
		Player->Close();
	}
}
