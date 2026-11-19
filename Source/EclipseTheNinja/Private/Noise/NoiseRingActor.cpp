

#include "Noise/NoiseRingActor.h"
#include "Components/WidgetComponent.h"
#include "Widgets/NoiseRingWidget.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Noise/NoiseTypes.h"
#include "Character/Enemy.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"

namespace
{
	// SpawnNoiseWave로 띄운 파동 컴포넌트. 컷신·암살이 시작될 때 이 목록으로 찾아 바로 끈다
	TArray<TWeakObjectPtr<UNiagaraComponent>>	GActiveNoiseWaves;
}

// Sets default values
ANoiseRingActor::ANoiseRingActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	this->NoiseWidgetComponent = CreateDefaultSubobject<UWidgetComponent>("NoiseWidgetComponent");
	SetRootComponent(this->NoiseWidgetComponent);

	// 화면 공간에 그리고, 크기는 Play에서 반경을 투영해 Draw Size로 맞춘다.
	this->NoiseWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	this->NoiseWidgetComponent->SetDrawSize(FVector2D(this->WidgetDrawSize));
	this->NoiseWidgetComponent->SetDrawAtDesiredSize(false);

	// 카메라 밖에서도 애니메이션이 끝나야 풀로 반환된다.
	this->NoiseWidgetComponent->SetTickWhenOffscreen(true);
	this->NoiseWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	this->NoiseWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	this->NoiseSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("NoiseSphereComponent"));
	this->NoiseSphereComponent->SetupAttachment(GetRootComponent());
	this->NoiseSphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->NoiseSphereComponent->SetCollisionObjectType(ECC_Noise);
	this->NoiseSphereComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	this->NoiseSphereComponent->SetGenerateOverlapEvents(false);
}

// Called when the game starts or when spawned
void ANoiseRingActor::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void ANoiseRingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ANoiseRingActor::OnSpawnFromPool_Implementation()
{
	this->bIsPlaying = false;
	this->EnsureWidget();

	// 꺼낸 직후에는 Play가 호출될 때까지 보이지 않게 한다.
	this->NoiseWidgetComponent->SetVisibility(false);
}

void ANoiseRingActor::OnReturnToPool_Implementation()
{
	// 아래 StopAllAnimations가 OnAnimationFinished를 다시 호출하므로 먼저 내린다.
	this->bIsPlaying = false;

	// 처음 생성되어 풀에 들어올 때(BeginPlay 직후)는 위젯이 아직 없을 수 있다.
	if (this->NoiseRingWidget.IsValid())
	{
		this->NoiseRingWidget->StopAllAnimations();
	}
	this->NoiseWidgetComponent->SetVisibility(false);
}

void ANoiseRingActor::Play(const FVector& InLocation, const float InRadius, const float InStrength)
{
	this->SetActorLocation(InLocation);
	this->NoiseSphereComponent->SetSphereRadius(InRadius, false);

	// 표시에 실패해도 판정은 해야 하므로 위젯 확인보다 먼저 알린다.
	this->NotifyEnemies(InLocation, InRadius, InStrength);

	// 파동 이펙트는 위젯과 별개로 자체 수명으로 끝나므로 위젯 확인 전에 띄운다.
	this->SpawnNoiseWave(InLocation, InRadius);

	// 위젯을 쓰지 않으면 기다릴 애니메이션이 없으므로 다음 틱에 바로 반환한다.
	if (!this->bShowRingWidget)
	{
		this->NoiseWidgetComponent->SetVisibility(false);
		GetWorldTimerManager().SetTimerForNextTick(this, &ANoiseRingActor::ReleaseToPool);
		return ;
	}

	// 풀이 등록되지 않아 일반 스폰된 경우 OnSpawnFromPool이 호출되지 않으므로 여기서도 확인한다.
	if (!this->EnsureWidget())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ANoiseRingActor::Play] 알 수 없는 위젯이 컴포넌트에 설정됐습니다")
		);
		UObjectPoolSubsystem::Release(this);
		return ;
	}

	// 화면 공간 위젯은 액터 스케일을 무시하므로 Draw Size(위젯 단위)로 반경을 맞춘다.
	// 투영은 위젯이 실제로 그려지는 플레이어(지정이 없으면 첫 로컬 플레이어) 기준으로 한다.
	const ULocalPlayer*	OwnerPlayer = this->NoiseWidgetComponent->GetOwnerPlayer();
	APlayerController*	PC = OwnerPlayer ? OwnerPlayer->GetPlayerController(GetWorld()) : nullptr;
	float Diameter = 0.f;
	if (CalcScreenDiameter(PC, InLocation, InRadius, Diameter))
	{
		this->NoiseWidgetComponent->SetDrawSize(FVector2D(Diameter));
	}

	this->NoiseRingWidget->SetStrength(InStrength);
	this->NoiseWidgetComponent->SetVisibility(true);
	this->bIsPlaying = true;
	this->NoiseRingWidget->PlayAnim();
}

void ANoiseRingActor::NotifyEnemies(const FVector& InLocation, const float InRadius, const float InStrength)
{
	// 나올 때 오버랩 되는 대상 있는지 확인
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, InLocation, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Noise), this->NoiseSphereComponent->GetCollisionShape());

	// 혹시나 여러번 겹칠 수 있으니 확인용
	TSet<AEnemy*> Notified;
	// 순회하며 소음발생
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AEnemy* Enemy = Cast<AEnemy>(Overlap.GetActor());
		if (!Enemy || Notified.Contains(Enemy))
		{
			continue;
		}
		Notified.Add(Enemy);
		Enemy->NotifyNoise(InLocation, InRadius, InStrength);
	}
}

void ANoiseRingActor::SpawnNoiseWave(const FVector& InLocation, const float InRadius)
{
	if (!this->NoiseWaveSystem)
	{
		return ;
	}

	// 링 액터는 풀로 재사용되므로 이펙트는 붙이지 않고 위치에 따로 띄워 끝나면 자동 반환시킨다.
	// 반경을 넣은 뒤 재생해야 첫 프레임부터 크기가 맞으므로 자동 활성화는 끈다.
	UNiagaraComponent* WaveComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		this->NoiseWaveSystem,
		InLocation,
		FRotator::ZeroRotator,
		FVector(1.f),
		true,	// bAutoDestroy
		false,	// bAutoActivate
		ENCPoolMethod::AutoRelease,
		true	// bPreCullCheck
	);

	if (!WaveComponent)
	{
		return ;
	}

	WaveComponent->SetVariableFloat(this->NoiseRadiusParamName, InRadius);
	WaveComponent->Activate(true);

	// 끝난(무효) 항목은 정리하면서 새 파동을 기록한다
	GActiveNoiseWaves.RemoveAll([](const TWeakObjectPtr<UNiagaraComponent>& InWave) { return (!InWave.IsValid()); });
	GActiveNoiseWaves.AddUnique(WaveComponent);
}

bool ANoiseRingActor::CalcScreenDiameter(APlayerController* InPC, const FVector& InLocation, const float InRadius, float& OutDiameter)
{
	if (!InPC)
	{
		return (false);
	}

	// 횡스크롤 화면의 가로축은 월드 X축이다. 중심과 반경 끝을 같은 방식(위젯 단위)으로 투영해 비교한다.
	FVector2D Center;
	FVector2D Edge;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(InPC, InLocation, Center, false)
		|| !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(InPC, InLocation + FVector(InRadius, 0.f, 0.f), Edge, false))
	{
		return (false);
	}

	OutDiameter = FVector2D::Distance(Center, Edge) * 2.f;
	return (OutDiameter > 0.f);
}

bool ANoiseRingActor::EnsureWidget()
{
	if (this->NoiseRingWidget.IsValid())
	{
		return (true);
	}

	if (!this->NoiseWidgetComponent->GetUserWidgetObject())
	{
		this->NoiseWidgetComponent->InitWidget();
	}
	if (UNoiseRingWidget* NoiseRing = Cast<UNoiseRingWidget>(this->NoiseWidgetComponent->GetUserWidgetObject()))
	{
		this->NoiseRingWidget = NoiseRing;
		this->NoiseRingWidget->OnNoiseWaveFinished.AddUObject(this, &ANoiseRingActor::OnNoiseEnd);
		return (true);
	}
	return (false);
}

void ANoiseRingActor::StopAllActive(const UObject* WorldContextObject)
{
	UWorld*	World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return ;
	}

	// 링: 재생 중인 것만 풀로 돌려보낸다 (반환 시 애니메이션을 멈추고 위젯을 숨긴다)
	TSet<UNiagaraSystem*>	WaveSystems;
	int32	RingCount = 0;
	int32	RingTotal = 0;
	for (TActorIterator<ANoiseRingActor> It(World); It; ++It)
	{
		ANoiseRingActor*	Ring = *It;
		++RingTotal;
		if (Ring->NoiseWaveSystem)
		{
			WaveSystems.Add(Ring->NoiseWaveSystem);
		}
		if (Ring->bIsPlaying || Ring->NoiseWidgetComponent->IsVisible())
		{
			Ring->bIsPlaying = false;
			Ring->NoiseWidgetComponent->SetVisibility(false);
			if (Ring->NoiseRingWidget.IsValid())
			{
				Ring->NoiseRingWidget->StopAllAnimations();
			}
			UObjectPoolSubsystem::Release(Ring);
			++RingCount;
		}
	}

	// 파동 이펙트: 띄울 때 기록해 둔 컴포넌트를 활성 여부와 상관없이 끄고 숨긴다.
	// (방출이 끝나 IsActive가 false여도 남은 입자가 화면에 남을 수 있다)
	int32	WaveCount = 0;
	int32	WaveActive = 0;
	for (const TWeakObjectPtr<UNiagaraComponent>& WeakWave : GActiveNoiseWaves)
	{
		UNiagaraComponent*	Wave = WeakWave.Get();
		if (!IsValid(Wave) || Wave->GetWorld() != World)
		{
			continue;
		}
		WaveActive += Wave->IsActive() ? 1 : 0;
		Wave->DeactivateImmediate();
		++WaveCount;
	}
	GActiveNoiseWaves.Reset();

	// 기록 밖(다른 경로로 띄운 같은 파동)도 같은 시스템이면 끈다
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent*	Wave = *It;
		if (IsValid(Wave) && Wave->GetWorld() == World && Wave->IsActive() && WaveSystems.Contains(Wave->GetAsset()))
		{
			Wave->DeactivateImmediate();
			++WaveCount;
		}
	}

	if (RingCount > 0 || WaveCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[ANoiseRingActor::StopAllActive] 링 %d개(전체 %d), 파동 %d개(그중 재생 중 %d) 정리"),
			RingCount, RingTotal, WaveCount, WaveActive);
	}
}

void ANoiseRingActor::OnNoiseEnd()
{
	if (!this->bIsPlaying)
	{
		return ;
	}
	this->bIsPlaying = false;
	this->NoiseWidgetComponent->SetVisibility(false);

	// 위젯이 애니메이션을 틱하는 도중이므로, 반환(StopAllAnimations 포함)은 다음 틱에 한다.
	GetWorldTimerManager().SetTimerForNextTick(this, &ANoiseRingActor::ReleaseToPool);
}

void ANoiseRingActor::ReleaseToPool()
{
	UObjectPoolSubsystem::Release(this);
}
