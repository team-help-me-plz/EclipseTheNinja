#include "Save/GameSaveSubsystem.h"

#include "Character/PlayerCharacter.h"
#include "Character/PlayerCharacterController.h"
#include "Camera/PlayerCameraManager.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "GAS/StatAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Minimap/MinimapSubsystem.h"

namespace GameSave
{
	constexpr int32 User = 0;
}

namespace
{
	// 에디터 실행(PIE)은 이미 연 맵을 가리키는 경로에 "UEDPIE_0_" 접두어를 붙인다.
	// 세이브에는 접두어 없는 원래 패키지 경로만 남겨, 같은 맵을 다른 맵으로 착각하지 않게 한다
	TSoftObjectPtr<UWorld> WithoutPIEPrefix(const TSoftObjectPtr<UWorld>& Level)
	{
		return TSoftObjectPtr<UWorld>(FSoftObjectPath(UWorld::RemovePIEPrefix(Level.ToSoftObjectPath().ToString())));
	}

	// 플레이어를 Transform 위치로 순간이동시키고 시점 회전을 맞춘다
	void TeleportPlayer(APlayerCharacter* Player, const FTransform& Transform)
	{
		Player->GetCharacterMovement()->StopMovementImmediately();
		Player->GetCharacterMovement()->SetPlaneConstraintOrigin(Transform.GetLocation());
		Player->SetActorLocationAndRotation(
			Transform.GetLocation(),
			Transform.Rotator(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		if (AController* Controller = Player->GetController())
		{
			Controller->SetControlRotation(Transform.Rotator());
		}
	}

	// 도착 지점: Id 체크포인트의 "위치"로만 옮긴다. 체크포인트를 활성화하지 않아 현재 체크포인트·리스폰 규칙은 그대로다
	bool MoveToArrival(APlayerCharacter* Player, ECheckpointId Id, FTransform& OutTransform)
	{
		const UCheckpointSubsystem* CP = Player->GetWorld() ? Player->GetWorld()->GetSubsystem<UCheckpointSubsystem>() : nullptr;
		if (!CP || !CP->GetCheckpointTransformById(Id, OutTransform))
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameSave] 도착 지점 %s 체크포인트를 이 맵에서 찾지 못함"), *UEnum::GetValueAsString(Id));
			return false;
		}

		TeleportPlayer(Player, OutTransform);
		UE_LOG(LogTemp, Log, TEXT("[GameSave] 도착 지점 %s 위치에서 시작 (체크포인트는 활성화하지 않음)"), *UEnum::GetValueAsString(Id));
		return true;
	}

	// 세이브 진행 없이 다른 맵에서 넘어왔을 때: URL의 "?Arrival=<체크포인트 이름>"으로 위치를 잡는다
	void MoveToUrlArrivalCheckpoint(APlayerCharacter* Player)
	{
		UWorld* World = Player->GetWorld();
		const TCHAR* Arrival = World ? World->URL.GetOption(TEXT("Arrival="), nullptr) : nullptr;
		if (!Arrival || !*Arrival)
		{
			return;
		}

		const int64 Value = StaticEnum<ECheckpointId>()->GetValueByNameString(Arrival);
		if (Value == INDEX_NONE || Value == static_cast<int64>(ECheckpointId::None))
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameSave] 알 수 없는 도착 체크포인트: %s"), Arrival);
			return;
		}

		FTransform Transform;
		MoveToArrival(Player, static_cast<ECheckpointId>(Value), Transform);
	}
}

UGameSaveSubsystem* UGameSaveSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UGameSaveSubsystem>() : nullptr;
}

bool UGameSaveSubsystem::HasSaveFile() const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, GameSave::User);
}

UGameSave* UGameSaveSubsystem::ReadSave() const
{
	// 파일을 읽을 수 있어도 현재 코드가 복원할 수 있는 데이터인지 별도로 확인한다.
	UGameSave* Save = Cast<UGameSave>(UGameplayStatics::LoadGameFromSlot(SlotName, GameSave::User));
	if (!Save || Save->Version != 1 || Save->Level.IsNull()
		|| !FPackageName::DoesPackageExist(Save->Level.ToSoftObjectPath().GetLongPackageName())
		|| (Save->bHasHealth && (!FMath::IsFinite(Save->Health) || Save->Health <= 0.f)))
	{
		return nullptr;
	}

	return Save;
}

bool UGameSaveSubsystem::CanContinue() const
{
	return HasSaveFile() && ReadSave() != nullptr;
}

void UGameSaveSubsystem::PrepareTravel()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		// 메뉴의 UIOnly 설정을 바꾼다. UI가 처리하지 않은 입력은 게임에도 전달한다.
		FInputModeGameAndUI Mode;

		// 게임 중에도 커서로 조준하므로, 커서를 화면 안에 가두거나 클릭할 때 숨기지 않는다.
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
}

bool UGameSaveSubsystem::StartNewGame(TSoftObjectPtr<UWorld> StartLevel)
{
	// 시작 맵을 확인한 뒤 초기 저장부터 완료한다. 실패한 상태로 맵을 이동하지 않는다.
	if (StartLevel.IsNull() || !FPackageName::DoesPackageExist(StartLevel.ToSoftObjectPath().GetLongPackageName()))
	{
		return false;
	}

	UGameSave* Previous = Progress;
	Progress = NewObject<UGameSave>(this);
	Progress->Level = StartLevel;

	if (!WriteSave())
	{
		// 초기 저장이 실패하면 메모리의 진행도 되돌리고 메뉴에 머문다.
		Progress = Previous;
		return false;
	}

	CurrentPlayer.Reset();
	bPendingRestore = true;
	bStageTravel = false;
	PrepareTravel();
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, StartLevel);
	return true;
}

bool UGameSaveSubsystem::ContinueGame()
{
	// 메뉴를 연 이후 파일이 바뀌었을 수 있으므로 버튼을 누를 때 다시 읽는다.
	UGameSave* Loaded = HasSaveFile() ? ReadSave() : nullptr;
	if (!Loaded)
	{
		return false;
	}

	Progress = Loaded;
	CurrentPlayer.Reset();
	bPendingRestore = true;
	bStageTravel = false;
	PrepareTravel();
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Progress->Level);
	return true;
}

void UGameSaveSubsystem::PlayerReady(APlayerCharacter* Player)
{
	// 기본 스탯이 저장 체력을 덮지 않도록 캐릭터 초기화가 끝난 뒤 복원한다.
	if (!IsValid(Player) || !Player->IsPlayerControlled())
	{
		return;
	}

	CurrentPlayer = Player;

	// 게임 맵을 에디터에서 직접 실행한 경우에는 기존 캠페인 파일에 저장하지 않는다.
	// 대신 이전 맵의 출구가 "?Arrival=" 옵션으로 도착 체크포인트를 넘겼으면 그 위치에서 시작한다.
	if (!Progress)
	{
		MoveToUrlArrivalCheckpoint(Player);
		return;
	}

	if (!IsProgressLevel(Player))
	{
		CurrentPlayer.Reset();
		return;
	}

	// 기본 체력을 저장된 체력으로 바꿀 때도 체력 변경 이벤트가 발생한다.
	// 지금은 피해를 받은 것이 아니므로, 이벤트를 받은 플레이어가 피격 모션과 자동 저장을 건너뛰게 한다.
	bApplyingRestore = true;
	Player->ApplyUnlockedAbilities(Progress->UnlockedAbilities);

	if (bPendingRestore)
	{
		FTransform Transform = Progress->RespawnTransform;
		bool bMoveToTransform = Progress->bHasRespawnTransform;

		if (Progress->Checkpoint != ECheckpointId::None)
		{
			if (UCheckpointSubsystem* CP = Player->GetWorld()->GetSubsystem<UCheckpointSubsystem>())
			{
				CP->RestoreCheckpoint(Progress->Checkpoint);
			}
		}

		if (bMoveToTransform)
		{
			TeleportPlayer(Player, Transform);
		}

		if (Progress->bHasHealth)
		{
			Player->GetAbilitySystemComponent()->SetNumericAttributeBase(
				UStatAttributeSet::GetHealthAttribute(),
				FMath::Clamp(Progress->Health, 0.f, Player->GetStatAttribute()->GetMaxHealth())
			);
		}
	}

	// 앞 맵의 출구가 도착 지점을 지정했으면 그 체크포인트의 위치로만 옮긴다 (체크포인트는 활성화하지 않는다)
	FTransform ArrivalTransform;
	const bool bArrived = Progress->ArrivalCheckpoint != ECheckpointId::None
		&& MoveToArrival(Player, Progress->ArrivalCheckpoint, ArrivalTransform);
	Progress->ArrivalCheckpoint = ECheckpointId::None;

	// 죽어서 이전 맵으로 돌아왔으면 검은 화면에서 시작해 체크포인트 위치에서 서서히 밝힌다
	if (bFadeInOnArrival)
	{
		bFadeInOnArrival = false;
		if (APlayerCharacterController* PC = Cast<APlayerCharacterController>(Player->GetController()))
		{
			if (IsValid(PC->PlayerCameraManager))
			{
				PC->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false);
			}
			PC->FadeInAfterRespawn();
		}
	}

	bApplyingRestore = false;
	bPendingRestore = false;
	bStageTravel = false;

	// 플레이어보다 먼저 BeginPlay에서 활성화된 시작 체크포인트도 저장에 반영한다.
	if (UCheckpointSubsystem* CP = Player->GetWorld()->GetSubsystem<UCheckpointSubsystem>())
	{
		FTransform Transform;
		if (CP->GetCheckpointTransform(Transform))
		{
			// 새로 켜진 시작 체크포인트일 때만 체력을 기록한다. 리스폰·이어하기는 같은 체크포인트라 저장 체력을 유지한다
			if (CP->GetCurrentCheckpoint() != Progress->Checkpoint || !Progress->bHasHealth)
			{
				CaptureHealth();
			}

			Progress->Checkpoint = CP->GetCurrentCheckpoint();
			Progress->RespawnTransform = Transform;
			Progress->bHasRespawnTransform = true;
		}
	}

	// 도착 직후 게임을 끄고 이어하면 맵 처음이 아니라 도착 지점에서 시작한다. 다음 체크포인트에 닿으면 덮어쓴다
	if (bArrived)
	{
		Progress->RespawnTransform = ArrivalTransform;
		Progress->bHasRespawnTransform = true;
	}

	SaveProgress();
}

bool UGameSaveSubsystem::WriteSave()
{
	// 저장량이 작은 단일 슬롯이므로 호출이 끝나면 디스크 기록 결과를 바로 알 수 있다.
	const bool bSuccess = Progress && UGameplayStatics::SaveGameToSlot(Progress, SlotName, GameSave::User);
	UE_LOG(LogTemp, Log, TEXT("[GameSave] Save %s: %s"), *SlotName, bSuccess ? TEXT("OK") : TEXT("FAILED"));
	OnSaveFinished.Broadcast(bSuccess);

	return bSuccess;
}

bool UGameSaveSubsystem::SaveProgress()
{
	// 사망한 체력이나 복원 도중의 임시 값으로 정상 진행을 덮어쓰지 않는다.
	if (!Progress || bPendingRestore || bApplyingRestore || bStageTravel || !CurrentPlayer.IsValid())
	{
		return false;
	}

	// 체력은 체크포인트 갱신 때만 기록하므로 여기서는 사망 여부만 확인한다
	const float Health = CurrentPlayer->GetStatAttribute()->GetHealth();
	if (!FMath::IsFinite(Health) || Health <= 0.f)
	{
		return false;
	}

	return WriteSave();
}

bool UGameSaveSubsystem::CaptureHealth()
{
	if (!Progress || !CurrentPlayer.IsValid())
	{
		return false;
	}

	const float Health = CurrentPlayer->GetStatAttribute()->GetHealth();
	if (!FMath::IsFinite(Health) || Health <= 0.f)
	{
		return false;
	}

	Progress->Health = Health;
	Progress->bHasHealth = true;
	return true;
}

void UGameSaveSubsystem::SaveCheckpoint(ECheckpointId Id, const FTransform& Transform)
{
	// 현재 위치가 아니라 체크포인트의 RespawnPoint를 재개 위치로 기록한다.
	if (!Progress || bPendingRestore || bApplyingRestore || bStageTravel)
	{
		return;
	}

	Progress->Checkpoint = Id;
	Progress->RespawnTransform = Transform;
	Progress->bHasRespawnTransform = true;

	// 이어하기는 체크포인트에 닿은 순간의 체력으로 시작한다
	CaptureHealth();
	SaveProgress();
}

bool UGameSaveSubsystem::SaveStageClear(TSoftObjectPtr<UWorld> NextLevel, bool bFinalStage, ECheckpointId ArrivalCheckpoint, bool bMarkCleared)
{
	// 다음 스테이지의 첫 체크포인트에 닿기 전에 종료해도 다음 맵에서 이어가게 한다.
	if (!Progress || bPendingRestore || bStageTravel || !CurrentPlayer.IsValid())
	{
		return false;
	}

	const float Health = CurrentPlayer->GetStatAttribute()->GetHealth();
	if (Health <= 0.f)
	{
		return false;
	}

	// 진행 맵이 바뀌면 이 맵의 미니맵 기록을 더 받지 않으므로, 마지막 주기 기록 이후 밝힌 칸을 먼저 남긴다
	if (UMinimapSubsystem* Minimap = UMinimapSubsystem::Get(CurrentPlayer.Get()))
	{
		Minimap->FlushReveal();
	}

	Progress->Health = Health;
	Progress->bHasHealth = true;
	if (bMarkCleared)
	{
		Progress->ClearedLevels.AddUnique(GetProgressLevelKey());
	}
	Progress->bGameCompleted |= bFinalStage;
	if (!bFinalStage && !NextLevel.IsNull())
	{
		// 다음 맵에서 죽으면 돌아올 수 있게, 떠나는 맵과 그 맵의 마지막 체크포인트를 남긴다
		Progress->ReturnLevel = WithoutPIEPrefix(Progress->Level);
		Progress->ReturnCheckpoint = Progress->Checkpoint;
		Progress->ReturnRespawnTransform = Progress->RespawnTransform;
		Progress->bHasReturnRespawnTransform = Progress->bHasRespawnTransform;

		Progress->Level = WithoutPIEPrefix(NextLevel);
		// 다음 맵은 체크포인트 없이 연다(시작 체크포인트 규칙을 따른다). 도착 지점은 PlayerReady에서 위치로만 쓴다
		Progress->Checkpoint = ECheckpointId::None;
		Progress->bHasRespawnTransform = false;
		Progress->ArrivalCheckpoint = ArrivalCheckpoint;
	}

	const bool bSaved = WriteSave();

	// 이전 맵의 체력 변경이 클리어 데이터를 덮지 않게 막고, 다음 맵에서 체력을 복원한다.
	bStageTravel = true;
	bPendingRestore = !bFinalStage && !NextLevel.IsNull();

	return bSaved;
}

bool UGameSaveSubsystem::ReturnToPreviousStage(const UObject* WorldContext)
{
	// 에디터에서 맵을 바로 실행했거나 진행 맵이 아니면 이 맵에서 부활하게 둔다
	if (!Progress || bStageTravel || !IsProgressLevel(WorldContext))
	{
		return false;
	}

	const TSoftObjectPtr<UWorld> ReturnLevel = Progress->ReturnLevel;
	if (ReturnLevel.IsNull() || !FPackageName::DoesPackageExist(ReturnLevel.ToSoftObjectPath().GetLongPackageName()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameSave] 돌아갈 이전 맵 기록이 없어 이 맵에서 부활합니다."));
		return false;
	}

	if (UMinimapSubsystem* Minimap = UMinimapSubsystem::Get(WorldContext))
	{
		Minimap->FlushReveal();
	}

	// 이어하기와 같은 방식으로 이전 맵의 마지막 체크포인트·재시작 위치를 복원한다. 체력은 기본값(가득)으로 시작한다
	Progress->Level = ReturnLevel;
	Progress->Checkpoint = Progress->ReturnCheckpoint;
	Progress->RespawnTransform = Progress->ReturnRespawnTransform;
	Progress->bHasRespawnTransform = Progress->bHasReturnRespawnTransform;
	Progress->ArrivalCheckpoint = ECheckpointId::None;
	Progress->bHasHealth = false;

	// 이전 맵에 도착하면 그 맵이 현재 맵이므로 복귀 기록을 비운다. 다시 출구를 지나면 새로 기록한다
	Progress->ReturnLevel.Reset();
	Progress->ReturnCheckpoint = ECheckpointId::None;
	Progress->bHasReturnRespawnTransform = false;

	WriteSave();

	UE_LOG(LogTemp, Log, TEXT("[GameSave] 사망: 이전 맵 %s의 체크포인트 %s에서 부활합니다."),
		*ReturnLevel.ToSoftObjectPath().GetLongPackageName(), *UEnum::GetValueAsString(Progress->Checkpoint));

	// 맵을 여는 동안 죽은 플레이어의 값이 저장되지 않게 막고, 새 맵에서 플레이어가 준비되면 복원한다
	CurrentPlayer.Reset();
	bStageTravel = true;
	bPendingRestore = true;
	bFadeInOnArrival = true;
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, ReturnLevel);
	return true;
}

bool UGameSaveSubsystem::UnlockAbility(EAbilityInputID Ability)
{
	// 해금 ID와 실제 능력 클래스의 연결은 플레이어 Blueprint 설정을 따른다.
	if (!Progress || Ability == EAbilityInputID::None || !CurrentPlayer.IsValid())
	{
		return false;
	}

	Progress->UnlockedAbilities.AddUnique(Ability);
	CurrentPlayer->ApplyUnlockedAbilities(Progress->UnlockedAbilities);
	return SaveProgress();
}

bool UGameSaveSubsystem::IsAbilityUnlocked(EAbilityInputID Ability) const
{
	return Progress && Progress->UnlockedAbilities.Contains(Ability);
}

bool UGameSaveSubsystem::IsProgressLevel(const UObject* WorldContext) const
{
	if (!Progress)
	{
		return false;
	}

	// GetCurrentLevelName은 PIE 접두어를 뺀 짧은 맵 이름을 돌려준다.
	// 예전 세이브에 PIE 접두어가 붙은 경로가 남아 있어도 같은 맵으로 본다
	const FString SavedLevel = UWorld::RemovePIEPrefix(Progress->Level.ToSoftObjectPath().GetLongPackageName());
	const FString CurrentLevel = UGameplayStatics::GetCurrentLevelName(WorldContext, true);

	return SavedLevel == CurrentLevel || FPackageName::GetShortName(SavedLevel) == CurrentLevel;
}

FName UGameSaveSubsystem::GetProgressLevelKey() const
{
	if (!Progress)
	{
		return NAME_None;
	}

	return FName(*UWorld::RemovePIEPrefix(Progress->Level.ToSoftObjectPath().GetLongPackageName()));
}

bool UGameSaveSubsystem::TakeActorState(const AActor* Actor, FSavedActorState& OutState) const
{
	if (!IsValid(Actor) || !IsProgressLevel(Actor))
	{
		return false;
	}

	// 체크포인트에 묶인 퍼즐은 이어하기에서 항상 처음 상태로 시작한다.
	if (IsCheckpointBound(Actor))
	{
		return false;
	}

	const FSavedLevelState* LevelState = Progress->LevelStates.Find(GetProgressLevelKey());
	const FSavedActorState* Found = LevelState
		? LevelState->ActorStates.Find(Actor->GetFName())
		: nullptr;

	if (!Found)
	{
		return false;
	}

	OutState = *Found;
	return true;
}

void UGameSaveSubsystem::RecordActorState(const AActor* Actor, const FSavedActorState& State)
{
	// 저장값을 적용하는 중이거나 다음 맵으로 이동 대기 중이면 기록하지 않는다.
	if (!IsValid(Actor) || bApplyingRestore || bStageTravel || !IsProgressLevel(Actor))
	{
		return;
	}

	// 체크포인트에 묶인 퍼즐은 체크포인트 리셋이 상태를 관리하므로 세이브에 남기지 않는다.
	if (IsCheckpointBound(Actor))
	{
		return;
	}

	// 액터 이름은 맵마다 따로 붙으므로 맵별로 나눠 기록한다
	FSavedLevelState& LevelState = Progress->LevelStates.FindOrAdd(GetProgressLevelKey());
	LevelState.ActorStates.Add(Actor->GetFName(), State);

	// 플레이어 준비 전이라 파일 저장이 거절돼도 기록은 남고, PlayerReady의 저장에 함께 들어간다.
	SaveProgress();
}

bool UGameSaveSubsystem::IsCheckpointBound(const AActor* Actor)
{
	const UCheckpointBoundComponent* Bound = Actor
		? Actor->FindComponentByClass<UCheckpointBoundComponent>()
		: nullptr;

	// 항상 초기화되는 액터도 리스폰이 상태를 관리하므로 세이브 대상에서 뺀다.
	return Bound
		&& (Bound->GetBoundCheckpoint() != ECheckpointId::None || Bound->IsAlwaysResetOnRespawn());
}

bool UGameSaveSubsystem::TakeMinimapReveal(const UObject* WorldContext, int32 GridHash, TArray<uint8>& OutBits) const
{
	if (!IsProgressLevel(WorldContext))
	{
		return false;
	}

	const FSavedLevelState* LevelState = Progress->LevelStates.Find(GetProgressLevelKey());
	if (!LevelState || LevelState->MinimapGridHash != GridHash || LevelState->MinimapRevealed.IsEmpty())
	{
		return false;
	}

	OutBits = LevelState->MinimapRevealed;
	return true;
}

void UGameSaveSubsystem::RecordMinimapReveal(const UObject* WorldContext, int32 GridHash, const TArray<uint8>& Bits)
{
	// 다음 맵으로 이동 대기 중이면 Progress의 맵이 이미 바뀌었으므로 기록하지 않는다
	if (bStageTravel || !IsProgressLevel(WorldContext))
	{
		return;
	}

	FSavedLevelState& LevelState = Progress->LevelStates.FindOrAdd(GetProgressLevelKey());
	LevelState.MinimapGridHash = GridHash;
	LevelState.MinimapRevealed = Bits;
}

bool UGameSaveSubsystem::HasSeenSequence(FName SequenceKey) const
{
	if (SequenceKey.IsNone())
	{
		return false;
	}
	return Progress ? Progress->SeenSequences.Contains(SequenceKey) : SessionSeenSequences.Contains(SequenceKey);
}

void UGameSaveSubsystem::MarkSequenceSeen(FName SequenceKey)
{
	if (SequenceKey.IsNone())
	{
		return;
	}
	if (!Progress)
	{
		SessionSeenSequences.Add(SequenceKey);
		return;
	}
	if (Progress->SeenSequences.Contains(SequenceKey))
	{
		return;
	}
	Progress->SeenSequences.Add(SequenceKey);
	SaveProgress();
}
