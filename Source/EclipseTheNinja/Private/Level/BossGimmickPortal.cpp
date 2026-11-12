#include "Level/BossGimmickPortal.h"
#include "Level/BossGimmickStage.h"

#include "Camera/PlayerCameraManager.h"
#include "Character/BossEnemy.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Character/PlayerCharacter.h"
#include "Component/BossEnemyGimmickComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

ABossGimmickPortal::ABossGimmickPortal()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// 플레이어 캡슐만 감지한다. 포탈 중심 높이에 맞춰 바닥에서 띄운다.
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(SceneRoot);
	Trigger->SetBoxExtent(FVector(60.0f, 100.0f, 100.0f));
	Trigger->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));

	// 기본은 포탈 오른쪽에 도착한다. 레벨에서 인스턴스마다 옮긴다.
	ArrivalPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ArrivalPoint"));
	ArrivalPoint->SetupAttachment(SceneRoot);
	ArrivalPoint->SetRelativeLocation(FVector(200.0f, 0.0f, 100.0f));

	PortalVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PortalVFX"));
	PortalVFX->SetupAttachment(SceneRoot);
	PortalVFX->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	// 원본 이펙트는 포탈 트리거보다 커서 줄인다. 로컬 스페이스 이미터라 컴포넌트 스케일을 따른다.
	PortalVFX->SetRelativeScale3D(FVector(0.3f));
	PortalVFX->SetAutoActivate(false);

	// 포탈과 레이저 에셋은 Blueprint 기본값 또는 레벨 인스턴스에서 지정한다.
}

void ABossGimmickPortal::BeginPlay()
{
	Super::BeginPlay();

	// 벽 붙기 트레이스(WallAttach)가 트리거 상자에 막히지 않게 한다. Trigger 프로필에는 WallAttach 채널이 없어 기본값 Block이 적용되므로 런타임에 지정한다.
	Trigger->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ABossGimmickPortal::OnTriggerBeginOverlap);

	if (UNiagaraSystem* System = PortalSystem.LoadSynchronous())
	{
		PortalVFX->SetAsset(System);
	}

	// 기믹 스테이지 레벨이 링크가 켜진 뒤에 로드될 수도 있으므로 지금 상태를 직접 확인한다.
	const UBossEnemyGimmickComponent* Gimmick = UBossEnemyGimmickComponent::FindByLinkId(this, LinkId);
	SetLinkActive(Gimmick && Gimmick->IsLinkActive(LinkId));
}

void ABossGimmickPortal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TeleportTimer);

	Super::EndPlay(EndPlayReason);
}

void ABossGimmickPortal::SetLinkActive(bool bActive)
{
	bLinkActive = bActive;

	RefreshPortalVisual();
	RefreshStageLaser();
}

bool ABossGimmickPortal::IsPortalOpen() const
{
	// 기믹 방에서는 링크를 해제한 뒤에도 돌아갈 수 있어야 한다.
	if (PortalSide == EBossGimmickPortalSide::GimmickStage) return true;
	if (!bLinkActive) return false;

	if (const ABossGimmickStage* Stage = ABossGimmickStage::FindByLinkId(GetWorld(), LinkId))
	{
		return Stage->IsReadyForEntry();
	}

	return true;
}

void ABossGimmickPortal::RefreshBossStageEntry()
{
	RefreshPortalVisual();
	if (!IsPortalOpen()) return;
	if (GetWorldTimerManager().IsTimerActive(TeleportTimer)) return;
	if (GetWorld()->GetTimeSeconds() < EntryBlockedUntil) return;

	TArray<AActor*> Players;
	Trigger->GetOverlappingActors(Players, APlayerCharacter::StaticClass());
	for (AActor* Actor : Players)
	{
		APlayerCharacter* Player = CastChecked<APlayerCharacter>(Actor);
		if (Player->IsPlayerControlled())
		{
			BeginTeleport(Player);
			return;
		}
	}
}

FTransform ABossGimmickPortal::GetArrivalTransform() const
{
	return ArrivalPoint->GetComponentTransform();
}

void ABossGimmickPortal::BlockEntry(float Duration)
{
	EntryBlockedUntil = GetWorld()->GetTimeSeconds() + Duration;
}

void ABossGimmickPortal::RestoreCheckpointBeforeGimmick()
{
	if (!bHasCheckpointBeforeGimmick)
	{
		return;
	}

	UCheckpointSubsystem* CP = GetWorld() ? GetWorld()->GetSubsystem<UCheckpointSubsystem>() : nullptr;
	if (!CP)
	{
		return;
	}

	CP->RestoreCheckpoint(CheckpointBeforeGimmick);
	bHasCheckpointBeforeGimmick = false;
}

void ABossGimmickPortal::ToggleActivate_Implementation()
{
	UBossEnemyGimmickComponent* Gimmick = UBossEnemyGimmickComponent::FindByLinkId(this, LinkId);
	if (!Gimmick || !Gimmick->RequestBreakLink(LinkId))
	{
		UE_LOG(LogTemp, Log, TEXT("%s: 기믹 %s의 링크가 켜져 있지 않아 레버를 무시합니다."), *GetName(), *LinkId.ToString());
		return;
	}

	// 링크를 풀었으면 시작점으로 돌아가는 지름길을 연다.
	for (AActor* Target : ActivateOnLinkBroken)
	{
		if (IsValid(Target) && Target->Implements<UActivatable>())
		{
			IActivatable::Execute_ToggleActivate(Target);
		}
	}
}

void ABossGimmickPortal::OnTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	// 분신이나 적은 들여보내지 않는다.
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player || !Player->IsPlayerControlled())
	{
		return;
	}

	if (!IsPortalOpen() || GetWorld()->GetTimeSeconds() < EntryBlockedUntil)
	{
		return;
	}

	if (GetWorldTimerManager().IsTimerActive(TeleportTimer))
	{
		return;
	}

	BeginTeleport(Player);
}

ABossGimmickPortal* ABossGimmickPortal::FindDestination() const
{
	for (TActorIterator<ABossGimmickPortal> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->LinkId == LinkId && It->PortalSide != PortalSide && It->HasActorBegunPlay())
		{
			// 출구 전용 포탈은 보스 스테이지에서 들어갈 도착지가 아니다
			if (It->bExitOnly)
			{
				continue;
			}
			return *It;
		}
	}

	return nullptr;
}

void ABossGimmickPortal::BeginTeleport(APawn* Player)
{
	ABossGimmickPortal* Destination = FindDestination();
	if (!Destination)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 기믹 %s의 반대쪽 포탈을 찾지 못했습니다."), *GetName(), *LinkId.ToString());
		return;
	}

	TeleportingPlayer = Player;
	TeleportDestination = Destination;

	// 이동하는 동안 입력을 막고 화면을 어둡게 한다.
	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (PC)
	{
		Player->DisableInput(PC);
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(
				0.0f,
				1.0f,
				FadeTime,
				FLinearColor::Black,
				false,
				true
			);
		}
	}

	GetWorldTimerManager().SetTimer(
		TeleportTimer,
		this,
		&ABossGimmickPortal::FinishTeleport,
		FMath::Max(FadeTime, KINDA_SMALL_NUMBER),
		false
	);
}

void ABossGimmickPortal::FinishTeleport()
{
	APawn* Player = TeleportingPlayer.Get();
	ABossGimmickPortal* Destination = TeleportDestination.Get();
	TeleportingPlayer.Reset();
	TeleportDestination.Reset();

	if (!IsValid(Player))
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Player->GetController());

	if (!IsPortalOpen())
	{
		Destination = nullptr;
	}

	if (IsValid(Destination))
	{
		// 도착한 포탈에 바로 다시 들어가지 않게 막는다.
		Destination->BlockEntry(ArrivalBlockTime);

		const FTransform Arrival = Destination->GetArrivalTransform();
		if (ACharacter* Character = Cast<ACharacter>(Player))
		{
			Character->GetCharacterMovement()->StopMovementImmediately();
		}

		// 카메라 보정(SmoothTeleport)은 쓰지 않는다. 먼 거리라 화면이 끌려가므로 어두운 동안 바로 따라붙게 둔다.
		const bool bTeleported = Player->SetActorLocationAndRotation(
			Arrival.GetLocation(),
			Arrival.GetRotation(),
			false,
			nullptr,
			ETeleportType::ResetPhysics
		);

		if (!bTeleported)
		{
			Destination = nullptr;
		}
		else if (PC)
		{
			PC->SetControlRotation(Arrival.Rotator());
		}
	}

	if (PC)
	{
		Player->EnableInput(PC);
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(
				1.0f,
				0.0f,
				FadeTime,
				FLinearColor::Black,
				false,
				false
			);
		}
	}

	if (IsValid(Destination))
	{
		HandleStageChange(Destination);
		Destination->OnPlayerArrived.Broadcast(Destination, Player);
	}
}

void ABossGimmickPortal::HandleStageChange(ABossGimmickPortal* Destination)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(Destination))
	{
		return;
	}

	const bool bEnteringGimmick = Destination->GetPortalSide() == EBossGimmickPortalSide::GimmickStage;
	ABossGimmickPortal* RoomPortal = bEnteringGimmick ? Destination : this;
	if (ABossGimmickStage* Stage = ABossGimmickStage::FindOwningStage(RoomPortal))
	{
		Stage->SetPlayerInside(bEnteringGimmick);
	}

	// 링크 제한시간은 기믹 방에 있는 동안에도 계속 흐른다. 기믹 방 공략 시간은 보스의 EnergyLinkTimeLimit로 맞춘다.
	if (UBossEnemyGimmickComponent* Gimmick = UBossEnemyGimmickComponent::FindByLinkId(this, LinkId))
	{
		if (ABossEnemy* Boss = Cast<ABossEnemy>(Gimmick->GetOwner()))
		{
			Boss->SetPlayerInGimmickStage(bEnteringGimmick);
		}
	}

	// 기믹 방 체크포인트는 그 방 안에서만 쓴다. 보스 스테이지로 돌아오면 들어가기 전 체크포인트로 되돌린다
	UCheckpointSubsystem* CP = World->GetSubsystem<UCheckpointSubsystem>();
	if (!CP)
	{
		return;
	}

	ABossGimmickPortal* BossSide = bEnteringGimmick ? this : Destination;
	if (bEnteringGimmick)
	{
		BossSide->CheckpointBeforeGimmick = CP->GetCurrentCheckpoint();
		BossSide->bHasCheckpointBeforeGimmick = true;
	}
	else
	{
		BossSide->RestoreCheckpointBeforeGimmick();
	}
}

void ABossGimmickPortal::RefreshPortalVisual()
{
	if (IsPortalOpen())
	{
		if (!PortalVFX->IsActive())
		{
			PortalVFX->Activate(true);
		}
	}
	else
	{
		PortalVFX->Deactivate();
	}
}

void ABossGimmickPortal::RefreshStageLaser()
{
	const bool bShowLaser = bLinkActive
		&& PortalSide == EBossGimmickPortalSide::GimmickStage
		&& IsValid(StageLever);

	if (!bShowLaser)
	{
		if (LaserComp)
		{
			LaserComp->Deactivate();
		}
		return;
	}

	if (!LaserComp)
	{
		UNiagaraSystem* System = LaserSystem.LoadSynchronous();
		if (!System)
		{
			return;
		}

		LaserComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			System,
			SceneRoot,
			NAME_None,
			BeamStartOffset,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false,
			false,
			ENCPoolMethod::None,
			false
		);
		if (!LaserComp)
		{
			return;
		}
	}

	// 빔 끝점은 월드 좌표(Absolute Beam End)로 쓰이므로 레버 위치를 그대로 넣는다.
	LaserComp->SetVariableVec3(BeamEndParameter, StageLever->GetActorLocation() + BeamTargetOffset);
	LaserComp->Activate(true);
}
