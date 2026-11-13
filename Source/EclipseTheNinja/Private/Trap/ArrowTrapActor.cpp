


#include "Trap/ArrowTrapActor.h"
#include "Trap/TrapArrowActor.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Component/ActivateHighlightComponent.h"
#include "Save/GameSaveSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

// Sets default values
AArrowTrapActor::AArrowTrapActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->MeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("MeshAssembly"));
	this->MeshAssembly->SetupAttachment(GetRootComponent());

	this->ArrowSpawnPos = CreateDefaultSubobject<USceneComponent>(TEXT("ArrowSpawnPos"));
	this->ArrowSpawnPos->SetupAttachment(GetRootComponent());

	this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
	this->ActivateHighlight = CreateDefaultSubobject<UActivateHighlightComponent>(TEXT("ActivateHighlight"));

	// 발사 사운드(FireSound)는 BP에서 등록한다
}

void AArrowTrapActor::ResetToInitial_Implementation()
{
	GetWorldTimerManager().ClearTimer(this->ActivateCooldownTimer);
	this->bIsActivateCooldown = false;
}

// Called when the game starts or when spawned
void AArrowTrapActor::BeginPlay()
{
	Super::BeginPlay();

	this->bIsFire = this->bIsStartFire;

	// 이어하기라면 저장된 발사 상태에서 시작한다. 단발 함정은 bIsFire가 의미 없으므로 자동 발사 함정만 복원한다
	if (this->bIsAutoFire)
	{
		UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this);
		FSavedActorState State;

		if (Save && Save->TakeActorState(this, State))
		{
			this->bIsFire = State.bIsOn;
		}
	}

	// 이 함정이 쓸 화살을 미리 생성하도록 요청 (화살 풀이 등록되어 있을 때만 적용, 함정 수만큼 합산됨)
	if (UObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>())
	{
		PoolSubsystem->RequestWarmupByClass(this->ArrowClass, this->ArrowWarmupCount);
	}
	if (this->bIsFire)
	{
		GetWorldTimerManager().SetTimer(
			this->AutoFireTimer,
			this,
			&AArrowTrapActor::SpawnArrow,
			this->FireInterval,
			true,
			0.f
		);
	}
}

// Called every frame
void AArrowTrapActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AArrowTrapActor::ToggleActivate_Implementation()
{
	if (this->bIsActivateCooldown)
	{
		return ;
	}
	this->bIsActivateCooldown = true;
	GetWorldTimerManager().SetTimer(
		ActivateCooldownTimer,
		this,
		&AArrowTrapActor::ResetActivateCooldown,
		this->ActivateInterval,
		false
	);
	if (this->bIsAutoFire)
	{
		if (this->bIsFire)
		{
			GetWorldTimerManager().ClearTimer(this->AutoFireTimer);
		}
		else
		{
			GetWorldTimerManager().SetTimer(
				this->AutoFireTimer,
				this,
				&AArrowTrapActor::SpawnArrow,
				this->FireInterval,
				true,
				0.f
			);
		}
	}
	else
	{
		this->SpawnArrow();
	}
	this->bIsFire = !this->bIsFire;

	// 자동 발사 함정의 켜짐/꺼짐을 저장한다. 체크포인트에 묶인 함정은 Subsystem이 기록을 거른다
	if (this->bIsAutoFire)
	{
		if (UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this))
		{
			FSavedActorState State;
			State.bIsOn = this->bIsFire;
			Save->RecordActorState(this, State);
		}
	}
}

void AArrowTrapActor::ShowActivationIndicator_Implementation()
{
	this->ActivateHighlight->ShowHighlight();
}

void AArrowTrapActor::HideActivationIndicator_Implementation()
{
	this->ActivateHighlight->HideHighlight();
}

void AArrowTrapActor::SpawnArrow()
{
	const FVector	SpawnDir = this->ArrowSpawnPos->GetForwardVector();
	const FVector	SpawnPos = this->ArrowSpawnPos->GetComponentLocation();
	const FRotator	SpawnRot = SpawnDir.Rotation();

	UObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
	if (!PoolSubsystem)
	{
		return;
	}

	// 풀에 들어 있는 액터를 스폰
	ATrapArrowActor* SpawnedArrow = PoolSubsystem->SpawnActor<ATrapArrowActor>(
		this->ArrowClass,
		FTransform(SpawnRot, SpawnPos),
		this
	);
	if (SpawnedArrow)
	{
		SpawnedArrow->SetLaunch(this->ArrowSpeed, this->TrapDamage);
		if (this->FireSound)
		{
			UGameplayStatics::SpawnSoundAttached(this->FireSound, SpawnedArrow->GetRootComponent());
		}

		// 체크포인트 리스폰 시 날아가던 화살 제거하기 위해
		// 체크포인트 서브시스템에 등록
		if (UCheckpointSubsystem* Subsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
		{
			Subsystem->RegisterRuntimeSpawnedActor(SpawnedArrow);
		}
	}
}

void AArrowTrapActor::ResetActivateCooldown()
{
	this->bIsActivateCooldown = false;
}

