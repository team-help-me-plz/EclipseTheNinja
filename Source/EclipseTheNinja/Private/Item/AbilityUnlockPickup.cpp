#include "Item/AbilityUnlockPickup.h"

#include "Character/PlayerCharacter.h"
#include "Component/InteractHighlightComponent.h"
#include "Interface/Interactor.h"
#include "Save/GameSaveSubsystem.h"
#include "Widgets/AbilityUnlockNoticeWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

AAbilityUnlockPickup::AAbilityUnlockPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	// 플레이어의 InteractBox(모든 채널 Overlap)에 잡히도록 겹침 이벤트를 켠다
	this->InteractTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("InteractTrigger"));
	this->InteractTrigger->InitSphereRadius(60.f);
	this->InteractTrigger->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	this->InteractTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	this->InteractTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	this->InteractTrigger->SetCollisionResponseToAllChannels(ECR_Overlap);
	this->InteractTrigger->SetGenerateOverlapEvents(true);
	SetRootComponent(this->InteractTrigger);

	this->Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	this->Mesh->SetupAttachment(GetRootComponent());
	this->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->Mesh->SetGenerateOverlapEvents(false);

	this->InteractHighlight = CreateDefaultSubobject<UInteractHighlightComponent>(TEXT("InteractHighlight"));

	this->InteractKeyWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractKeyWidget"));
	this->InteractKeyWidget->SetupAttachment(GetRootComponent());
	this->InteractKeyWidget->SetWidgetSpace(EWidgetSpace::Screen);
	this->InteractKeyWidget->SetDrawAtDesiredSize(true);
	this->InteractKeyWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->InteractKeyWidget->SetVisibility(false);
}

void AAbilityUnlockPickup::BeginPlay()
{
	Super::BeginPlay();

	// 에디터(OnConstruction)에서 만들면 다이내믹 인스턴스가 맵에 저장되어 머티리얼 오버라이드를 덮으므로 플레이 시작 때만 적용한다
	this->ApplySkillTexture();

	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetKeyWidget(this->InteractKeyWidget);
	}

	if (this->AbilityToUnlock == EAbilityInputID::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: AbilityToUnlock이 None입니다."), *GetName());
	}

	// 이미 해금한 세이브라면 처음부터 사용된 상태로 둔다
	const UGameInstance* GameInstance = GetGameInstance();
	const UGameSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UGameSaveSubsystem>() : nullptr;
	if (Save && Save->IsAbilityUnlocked(this->AbilityToUnlock))
	{
		this->RemoveAsObtained();
		return;
	}
	this->RefreshHighlight();
}

void AAbilityUnlockPickup::RemoveAsObtained()
{
	// 제단처럼 남겨 두는 픽업은 상호작용만 막고, 나머지는 레벨에서 없앤다
	if (!this->bHideAfterUnlock)
	{
		this->Consume();
		return;
	}

	Destroy();
}

void AAbilityUnlockPickup::RemoveOtherPickups()
{
	// 방금 얻은 능력을 주는 다른 픽업은 더 이상 의미가 없으므로 함께 없앤다
	for (TActorIterator<AAbilityUnlockPickup> It(GetWorld()); It; ++It)
	{
		AAbilityUnlockPickup* Other = *It;
		if (Other != this && Other->AbilityToUnlock == this->AbilityToUnlock)
		{
			Other->RemoveAsObtained();
		}
	}
}

void AAbilityUnlockPickup::Interact_Implementation(AActor* InActor)
{
	if (this->InteractState != EInteractState::Ready || !IsValid(InActor) || !InActor->Implements<UInteractor>())
	{
		return;
	}
	APlayerCharacter* Player = Cast<APlayerCharacter>(InActor);
	if (!IsValid(Player))
	{
		return;
	}

	// 이미 가진 능력이면 다시 부여하거나 안내를 띄우지 않고 픽업만 없앤다 (시작 능력이거나 세이브 없이 실행한 경우)
	if (this->AbilityToUnlock != EAbilityInputID::None
		&& Player->FindAbilitySpecByInputID(this->AbilityToUnlock))
	{
		this->RemoveAsObtained();
		return;
	}

	if (!this->GrantAbility(Player))
	{
		return;
	}

	this->UnlockedPlayer = Player;
	IInteractor::Execute_PlayInteractMontage(InActor, this, nullptr);
	this->Consume();
	this->RemoveOtherPickups();
	this->BP_OnUnlocked(Player);

	if (!this->NoticeWidgetClass)
	{
		return;
	}
	if (this->NoticeOpenDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(this->NoticeTimer, this, &AAbilityUnlockPickup::OpenNotice, this->NoticeOpenDelay, false);
	}
	else
	{
		this->OpenNotice();
	}
}

bool AAbilityUnlockPickup::GrantAbility(APlayerCharacter* InPlayer) const
{
	if (this->AbilityToUnlock == EAbilityInputID::None)
	{
		return (false);
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (UGameSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UGameSaveSubsystem>() : nullptr)
	{
		// 세이브 세션이 있으면 해금 기록, 부여, 저장까지 한다 (저장 자체가 건너뛰어져도 부여는 된다)
		Save->UnlockAbility(this->AbilityToUnlock);
	}
	// 에디터에서 맵을 직접 실행해 세이브 세션이 없을 때도 테스트할 수 있게 직접 부여한다. 이미 있으면 아무것도 하지 않는다.
	InPlayer->ApplyUnlockedAbilities({ this->AbilityToUnlock });

	if (!InPlayer->FindAbilitySpecByInputID(this->AbilityToUnlock))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: %s를 부여하지 못했습니다. 플레이어 BP의 UnlockableAbilities에 같은 InputID 항목이 있는지 확인하세요."),
			*GetName(), *UEnum::GetValueAsString(this->AbilityToUnlock));
		return (false);
	}
	return (true);
}

void AAbilityUnlockPickup::Consume()
{
	this->InteractState = EInteractState::Done;
	this->RefreshHighlight();

	// 콜리전을 끄면 InteractBox의 EndOverlap이 불려 선택 목록에서 빠진다
	this->InteractTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (this->bHideAfterUnlock)
	{
		SetActorHiddenInGame(true);
	}
}

void AAbilityUnlockPickup::OpenNotice()
{
	APlayerCharacter* Player = this->UnlockedPlayer.Get();
	APlayerController* Controller = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (!IsValid(Controller) || !this->NoticeWidgetClass)
	{
		return;
	}
	UAbilityUnlockNoticeWidget* Notice = CreateWidget<UAbilityUnlockNoticeWidget>(Controller, this->NoticeWidgetClass);
	if (!IsValid(Notice))
	{
		return;
	}
	UTexture2D* Icon = IsValid(this->NoticeIcon) ? this->NoticeIcon.Get() : this->SkillTexture.Get();
	Notice->SetupNotice(this->NoticeTitle, this->NoticeDescription, Icon);
	Notice->OpenNotice(Controller);
}

void AAbilityUnlockPickup::ShowInteractionIndicator_Implementation()
{
	this->SelectCount++;
	this->RefreshHighlight();
}

void AAbilityUnlockPickup::HideInteractionIndicator_Implementation()
{
	this->SelectCount = FMath::Max(0, this->SelectCount - 1);
	this->RefreshHighlight();
}

bool AAbilityUnlockPickup::bIsInteractReady_Implementation()
{
	return (this->InteractState == EInteractState::Ready);
}

EInteractState AAbilityUnlockPickup::GetInteractState_Implementation()
{
	return (this->InteractState);
}

void AAbilityUnlockPickup::ApplySkillTexture()
{
	if (!IsValid(this->Mesh) || !IsValid(this->SkillTexture) || this->SkillTextureParameterName.IsNone())
	{
		return;
	}
	if (!this->Mesh->GetStaticMesh() || this->SkillTextureMaterialIndex >= this->Mesh->GetNumMaterials())
	{
		return;
	}

	// 이미 다이내믹 인스턴스면 재사용하고, 아니면 현재 머티리얼을 부모로 새로 만든다
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(this->Mesh->GetMaterial(this->SkillTextureMaterialIndex));
	if (!MID)
	{
		MID = this->Mesh->CreateDynamicMaterialInstance(this->SkillTextureMaterialIndex);
	}
	if (MID)
	{
		MID->SetTextureParameterValue(this->SkillTextureParameterName, this->SkillTexture);
	}
}

void AAbilityUnlockPickup::RefreshHighlight()
{
	if (IsValid(this->InteractHighlight))
	{
		this->InteractHighlight->SetHighlight(this->InteractState, (this->SelectCount > 0));
	}
}
