#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/GameSave.h"
#include "GameSaveSubsystem.generated.h"

class APlayerCharacter;
class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGameSaveResult, bool, bSuccess);

/**
 * 현재 진행 상황을 보관하고, 세이브 파일을 저장하거나 읽는다.
 * 새 게임에서는 진행 상황을 처음부터 만들고, 이어하기에서는 파일에 있던 값을 읽는다.
 * 맵이 열린 뒤 플레이어의 초기 설정이 끝나면 저장된 위치·체력·해금 능력을 적용한다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGameSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 액터나 컴포넌트에서 이 서브시스템을 찾는다. 게임 인스턴스가 없으면 nullptr. */
	static UGameSaveSubsystem* Get(const UObject* WorldContext);

	/** 저장 파일을 읽을 수 있고 저장된 맵도 존재하는지 확인한다. 이어하기 버튼의 활성 여부에 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool CanContinue() const;

	/** 슬롯에 파일이 있는지만 확인한다. 새 게임의 덮어쓰기 경고에 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool HasSaveFile() const;

	/** 기존 진행 대신 새 게임의 시작 데이터를 저장한 뒤 첫 맵을 연다. 기존 파일을 지워도 되는지는 메뉴에서 먼저 확인한다. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool StartNewGame(TSoftObjectPtr<UWorld> StartLevel);

	/** 디스크에서 다시 읽은 데이터로 재개할 맵을 연다. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool ContinueGame();

	/** Progress를 파일로 저장한다. 체력은 갱신하지 않는다(체크포인트 갱신 때만 기록). 저장값을 적용하는 중이거나 스테이지 클리어 후 이동 대기 중, 사망 상태면 저장하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveProgress();

	/** 해금 ID를 기록하고 Blueprint에 지정된 능력을 부여한 뒤 저장한다. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool UnlockAbility(EAbilityInputID Ability);

	/** 현재 저장 세션에서 해당 능력을 해금했는지 확인한다. */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool IsAbilityUnlocked(EAbilityInputID Ability) const;

	/** 파일 저장을 시도한 뒤 호출한다. true면 저장 성공, false면 저장 실패이다. */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FGameSaveResult OnSaveFinished;

	/** 플레이어의 기본 체력과 시작 능력을 설정한 뒤 호출한다. 이어하기라면 이때 저장된 위치·체력·능력을 적용한다. */
	void PlayerReady(APlayerCharacter* Player);

	/** 재개 위치를 체크포인트로 고정하고 현재 체력과 함께 저장한다. */
	void SaveCheckpoint(ECheckpointId Id, const FTransform& Transform);

	/**
	 * 현재 맵을 클리어 목록에 추가하고 다음 맵으로 진행을 옮긴다.
	 * ArrivalCheckpoint가 None이면 다음 맵의 시작 위치에서, 아니면 그 체크포인트의 위치에서 시작한다(체크포인트는 활성화하지 않는다).
	 * bMarkCleared를 끄면 클리어 목록에 넣지 않는다(앞 맵으로 되돌아가는 출구).
	 */
	bool SaveStageClear(TSoftObjectPtr<UWorld> NextLevel, bool bFinalStage, ECheckpointId ArrivalCheckpoint = ECheckpointId::None, bool bMarkCleared = true);

	/** 이어하기로 저장된 체크포인트를 적용할 차례인지 확인한다. true이면 맵 시작 시 자동으로 켜지는 체크포인트가 저장된 진행을 바꾸지 못하게 한다. */
	bool IsRestoring() const { return bPendingRestore && Progress && Progress->Checkpoint != ECheckpointId::None; }

	/**
	 * 파일에서 읽은 값을 플레이어에게 적용하고 있는지 확인한다.
	 * 예를 들어 기본 체력 100을 저장된 체력 60으로 바꿔도 체력 변경 이벤트가 발생한다.
	 * 이때 true를 반환해 피격 모션을 재생하거나 체력을 다시 저장하지 않도록 한다.
	 */
	bool IsApplyingRestore() const { return bApplyingRestore; }

	/** 현재 사용 중인 Progress를 반환한다. 파일을 다시 읽는 함수가 아니므로, 마지막 파일 저장 이후 변경한 값도 들어 있을 수 있다. */
	const UGameSave* GetProgress() const { return Progress; }

	/**
	 * 퍼즐 액터가 BeginPlay 끝에서 저장된 상태를 받아 간다. 적용할 상태가 있으면 true.
	 * 체크포인트에 묶인(BoundCheckpoint가 None이 아닌) 액터는 항상 false로, 처음 상태로 시작한다.
	 */
	bool TakeActorState(const AActor* Actor, FSavedActorState& OutState) const;

	/** 체크포인트에 묶이지 않은 퍼즐 액터의 상태가 바뀌었을 때 기록하고 파일에 저장한다. */
	void RecordActorState(const AActor* Actor, const FSavedActorState& State);

	/** 현재 맵의 미니맵 기록을 받아 간다. 격자 해시가 같고 기록이 있으면 true. */
	bool TakeMinimapReveal(const UObject* WorldContext, int32 GridHash, TArray<uint8>& OutBits) const;

	/** 현재 맵의 미니맵 기록을 진행 데이터에 남긴다. 자주 불리므로 파일에는 쓰지 않고, 다음 저장(체크포인트 등) 때 함께 기록된다. */
	void RecordMinimapReveal(const UObject* WorldContext, int32 GridHash, const TArray<uint8>& Bits);

private:
	// 체크포인트 리셋 대상인지 확인한다. BoundCheckpoint가 None이 아니거나 bAlwaysResetOnRespawn이 켜져 있으면 true.
	static bool IsCheckpointBound(const AActor* Actor);

	// 살아 있는 플레이어의 현재 체력을 Progress에 기록한다. 파일에는 쓰지 않는다. 사망 상태면 false.
	bool CaptureHealth();

	// 액터가 Progress에 저장된 맵에 있는지 확인한다. 에디터에서 다른 맵을 직접 실행했을 때 기록이 섞이지 않게 한다.
	bool IsProgressLevel(const UObject* WorldContext) const;

	// 퍼즐 상태를 맵별로 나눠 둘 때 쓰는 키. 저장된 맵의 패키지 이름이다.
	FName GetProgressLevelKey() const;

	// 저장 파일 이름. 확장자는 슬롯 API가 붙인다.
	const FString SlotName = TEXT("GameSave");

	// Progress의 내용을 파일에 기록한다. 저장이 끝날 때까지 기다린 뒤 성공 여부를 반환한다.
	bool WriteSave();

	// 맵 이동 전에 입력 설정을 바꾸는 함수
	// 메뉴에서는 입력을 UI만 받는다. 게임 맵을 열기 전에 캐릭터도 입력을 받을 수 있게 바꾼다.
	// 마우스 커서는 계속 표시하고, 클릭할 때 숨기거나 게임 화면 안에 가두지 않는다.
	void PrepareTravel();

	// 파일을 읽고 내용을 확인한다. 파일을 못 읽거나 버전·맵·체력 값에 문제가 있으면 nullptr를 반환한다.
	UGameSave* ReadSave() const;

	// 현재 플레이의 맵·체크포인트·체력·해금 목록을 담은 객체이다. 맵이 바뀌어도 계속 사용한다.
	// Transient는 이 서브시스템의 Progress 참조를 자동 저장 대상에서 제외한다는 뜻이다.
	// Progress 안의 실제 진행 값은 WriteSave()가 SaveGameToSlot에 넘겨 .sav 파일로 저장한다.
	UPROPERTY(Transient)
	TObjectPtr<UGameSave> Progress;

	// 현재 체력을 읽어 올 플레이어이다. 맵 이동 등으로 캐릭터가 사라지면 더 이상 사용하지 않는다.
	TWeakObjectPtr<APlayerCharacter> CurrentPlayer;

	// 새 게임·이어하기·다음 스테이지의 맵을 여는 동안 true이다.
	// 플레이어가 준비되면 Progress에 있는 위치와 체력을 적용하고 false로 바꾼다.
	bool bPendingRestore = false;

	// 저장된 값을 플레이어에게 적용하는 동안만 true이다.
	// 체력 변경 이벤트가 발생해도 피격 모션이나 자동 저장을 실행하지 않게 한다.
	bool bApplyingRestore = false;

	// 스테이지 클리어를 저장한 뒤 true로 바꾼다.
	// 이동 대기 중 이전 맵에서 받은 피해 등이 다음 스테이지의 저장값을 바꾸지 못하게 한다.
	bool bStageTravel = false;

};
