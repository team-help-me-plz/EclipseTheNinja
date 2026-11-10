#include "Level/LevelBGMActor.h"

#include "Components/AudioComponent.h"
#include "Components/BillboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

ALevelBGMActor::ALevelBGMActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RootScene")));

	// 에디터에서 배치 위치를 찾을 수 있도록 아이콘만 표시한다
#if WITH_EDITORONLY_DATA
	UBillboardComponent* Billboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (Billboard)
	{
		Billboard->SetupAttachment(GetRootComponent());
	}
#endif
}

void ALevelBGMActor::BeginPlay()
{
	Super::BeginPlay();

	if (!this->Music)
	{
		return;
	}

	// 위치와 관계없이 들리도록 2D로 만들고, 맵을 떠나면 함께 정리되게 한다. 재생은 아래 FadeIn이 시작한다
	this->MusicComponent = UGameplayStatics::CreateSound2D(
		this,
		this->Music,
		this->Volume,
		1.0f,
		0.0f,
		nullptr,
		false,
		false
	);
	if (!this->MusicComponent)
	{
		return;
	}

	this->MusicComponent->OnAudioFinished.AddDynamic(this, &ALevelBGMActor::HandleMusicFinished);
	// Volume은 컴포넌트 볼륨 배율에 이미 들어 있으므로 페이드 목표는 1이다
	this->MusicComponent->FadeIn(this->FadeInTime, 1.0f);
}

void ALevelBGMActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	this->bIsStopped = true;
	if (IsValid(this->MusicComponent))
	{
		this->MusicComponent->Stop();
	}
	this->MusicComponent = nullptr;

	Super::EndPlay(EndPlayReason);
}

void ALevelBGMActor::StopMusic(float FadeOutTime)
{
	this->bIsStopped = true;
	if (IsValid(this->MusicComponent))
	{
		this->MusicComponent->FadeOut(FadeOutTime, 0.0f);
	}
}

void ALevelBGMActor::HandleMusicFinished()
{
	if (this->bIsStopped || !IsValid(this->MusicComponent))
	{
		return;
	}

	this->MusicComponent->Play();
}
