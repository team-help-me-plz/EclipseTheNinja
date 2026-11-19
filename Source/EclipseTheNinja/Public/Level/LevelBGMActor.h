#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelBGMActor.generated.h"

class UAudioComponent;
class USoundBase;

// 맵에 하나 배치해 두면 레벨이 시작될 때 BGM을 2D로 재생한다. 맵이 바뀌어도 새 맵에 이 액터를 놓고 Music만 지정하면 된다
UCLASS()
class ECLIPSETHENINJA_API ALevelBGMActor : public AActor
{
	GENERATED_BODY()

public:
	ALevelBGMActor();

	// BGM을 서서히 멈춘다 (보스전 등 다른 음악으로 바꿀 때 사용)
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void	StopMusic(float FadeOutTime = 1.0f);

protected:
	virtual void	BeginPlay() override;
	virtual void	EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 재생할 BGM. 루프로 설정되지 않은 사운드도 끝나면 처음부터 다시 재생한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase>	Music = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BGM", meta = (ClampMin = "0"))
	float	Volume = 0.5f;

	// 레벨 시작 시 소리가 커지는 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BGM", meta = (ClampMin = "0"))
	float	FadeInTime = 2.0f;

private:
	// 루프가 아닌 사운드가 끝나면 다시 재생한다
	UFUNCTION()
	void	HandleMusicFinished();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent>	MusicComponent = nullptr;

	// StopMusic 이후에는 다시 재생하지 않는다
	bool	bIsStopped = false;
};
