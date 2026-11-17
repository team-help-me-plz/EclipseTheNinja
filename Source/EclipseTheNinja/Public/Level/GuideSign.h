#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "GuideSign.generated.h"

class UBoxComponent;
class USphereComponent;
class UWidgetComponent;
class UInteractHighlightComponent;
class UMediaPlayer;
class UMediaTexture;
class UMediaSource;
class USoundBase;
class UGuideSignData;
class UGuideSignPopupWidget;

/**
 * 게임을 멈추고 안내 팝업(제목, 내용, 반복 동영상)을 페이지 넘김 방식으로 보여 주는 표지판.
 *  - 보여 줄 페이지는 SignDataList(UGuideSignData 여러 개)에서 고른다
 *  - 처음에는 플레이어가 닿으면 자동으로 띄우고, 마지막 페이지까지 넘겨야 닫을 수 있다
 *  - 한 번 본 뒤에는 닿아도 띄우지 않고, 상호작용 키로 다시 볼 수 있다 (이때는 바로 닫을 수 있다)
 *  - 여러 표지판에 동시에 닿으면 페이지가 팝업 하나에 이어 붙는다
 *  - 동영상 첫 재생 렉을 없애려고 PreloadRange 안에 들어오면 동영상을 미리 열어 두고, 벗어나면 닫는다
 * 본 기록은 레벨이 열려 있는 동안만 유지된다 (세이브에 남지 않는다).
 * 메시는 BP에서 Static Mesh 컴포넌트를 붙여 조합한다.
 */
UCLASS()
class ECLIPSETHENINJA_API AGuideSign : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AGuideSign();

	virtual void	Interact_Implementation(AActor* InActor) override;
	virtual void	ShowInteractionIndicator_Implementation() override;
	virtual void	HideInteractionIndicator_Implementation() override;
	virtual bool	bIsInteractReady_Implementation() override;
	virtual EInteractState	GetInteractState_Implementation() override;
	/** 분신은 표지판을 대신 읽을 수 없다 */
	virtual bool	CanCloneInteract_Implementation() override { return (false); }

protected:
	virtual void	BeginPlay() override;
	virtual void	EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 팝업에 페이지를 띄운 직후 호출된다. BP에서 연출을 붙인다. bFirstView는 처음 보는 경우 true */
	UFUNCTION(BlueprintImplementableEvent, Category = "GuideSign")
	void	BP_OnPopupShown(bool bFirstView);

	/** 팝업이 닫힌 직후 호출된다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "GuideSign")
	void	BP_OnPopupClosed();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent>	SceneRoot = nullptr;

	/** 처음 보는 표지판이면 플레이어가 닿을 때 팝업을 띄우는 영역 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent>	PopupTrigger = nullptr;

	/** 플레이어의 상호작용 박스에 잡히는 영역 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent>	InteractTrigger = nullptr;

	/** 플레이어가 들어오면 동영상을 미리 여는 영역. PopupTrigger보다 넉넉히 크게 둔다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent>	PreloadRange = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInteractHighlightComponent>	InteractHighlight = nullptr;

	/** 선택되어 상호작용할 수 있을 때만 보이는 키 위젯. 위젯 클래스는 BP에서 지정 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent>	InteractKeyWidget = nullptr;

	/** 팝업에 띄울 페이지. 위에서부터 차례로 넘긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GuideSign")
	TArray<TObjectPtr<UGuideSignData>>	SignDataList;

	/** 팝업 WBP. 이미 열린 팝업이 있으면 그 팝업에 페이지만 이어 붙인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GuideSign")
	TSubclassOf<UGuideSignPopupWidget>	PopupClass;

	/** 팝업을 띄울 때 재생할 효과음. 일시정지 중에도 끊기지 않게 UI 사운드로 재생한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GuideSign")
	TObjectPtr<USoundBase>	PopupSound = nullptr;

private:
	UFUNCTION()
	void	OnPopupTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void	OnPreloadRangeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void	OnPreloadRangeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

	UFUNCTION()
	void	HandlePopupClosed();

	/** 팝업의 보이는 페이지가 바뀌면 그 페이지가 이 표지판의 것일 때만 동영상을 재생하고 나머지는 멈춘다 */
	UFUNCTION()
	void	HandlePageChanged(int32 InPageIndex);

	/** 플레이어가 조종하는 캐릭터인지 (분신·적 제외) */
	static bool	IsPlayer(const AActor* InActor);

	/** 페이지를 띄울 수 있는 상태인지 (데이터와 팝업 클래스가 있고 지금 떠 있지 않은지) */
	bool	CanShowPopup() const;

	/** 열린 팝업에 페이지를 붙이거나 새 팝업을 연다. 처음 보는 표지판이면 끝까지 읽어야 닫히게 한다 */
	void	ShowPopup(APlayerController* InController);

	/** 팝업의 알림 바인딩을 푼다 */
	void	UnbindFromPopup();

	void	RefreshHighlight();

	/** 카드마다 미디어 플레이어와 텍스처를 만든다. 동영상이 없는 칸은 nullptr로 둔다 */
	void	CreateVideoObjects();

	/** 모든 페이지의 동영상을 열어 첫 프레임 직전에 멈춰 둔다. 이미 열린 것은 그대로 둔다 */
	void	PrepareVideos();

	/** Index번 페이지의 동영상을 처음부터 재생한다. 아직 여는 중이면 열리자마자 재생한다 */
	void	PlayVideo(int32 Index);

	/** 모든 페이지의 동영상을 멈추고 처음으로 되돌린다 */
	void	StopVideos();

	/** Index번 페이지의 동영상만 멈추고 처음으로 되돌린다 */
	void	StopVideo(int32 Index);

	/** 모든 페이지의 동영상을 닫아 디코더를 놓는다 */
	void	CloseVideos();

	/** Index번 페이지의 동영상 소스. 동영상이 없거나 플레이어가 없으면 nullptr */
	UMediaSource*	GetVideoSource(int32 Index) const;

	/** 플레이어가 PreloadRange 안에 있는지 */
	bool	IsPlayerInPreloadRange() const;

	/** SignDataList와 같은 순서. 동영상이 없는 페이지는 nullptr */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMediaPlayer>>	VideoPlayers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMediaTexture>>	VideoTextures;

	/** 팝업 페이지 번호 → 이 표지판의 SignDataList 번호 */
	TMap<int32, int32>	ShownPages;

	/** 이 표지판의 페이지가 팝업에 떠 있는 동안 true */
	bool	bPopupShowing = false;

	/** 페이지를 붙이고 알림을 연결한 팝업. 닫힐 때는 이미 뷰포트에서 빠져 있으므로 직접 들고 있다가 바인딩을 푼다 */
	TWeakObjectPtr<UGuideSignPopupWidget>	ActivePopup;

	/** 한 번이라도 끝까지 본 적이 있는지. true면 닿아도 띄우지 않고 상호작용으로만 띄운다 */
	bool	bHasBeenViewed = false;

	int32	SelectCount = 0;
};
