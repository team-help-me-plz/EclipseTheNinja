#include "Item/HealPickup.h"
#include "Character/PlayerCharacter.h"
#include "GAS/StatAttributeSet.h"
#include "ObjectPool/ObjectPoolSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

namespace HealPickup
{
	// 커브 키를 지정한 기울기의 부드러운 보간으로 추가한다. 극값 키는 기울기 0으로 두어 값이 넘치지 않게 한다.
	void AddSmoothKey(FRichCurve& Curve, float Time, float Value, float Tangent = 0.0f)
	{
		const FKeyHandle KeyHandle = Curve.AddKey(Time, Value);

		FRichCurveKey& Key = Curve.GetKey(KeyHandle);
		Key.InterpMode = RCIM_Cubic;
		Key.TangentMode = RCTM_User;
		Key.ArriveTangent = Tangent;
		Key.LeaveTangent = Tangent;
	}
}

AHealPickup::AHealPickup()
{
	// 떠다니기/흡수 연출을 매 프레임 갱신한다. 풀 대기 중에는 풀이 틱을 끈다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 플레이어 캡슐(Pawn)과의 겹침만 감지한다.
	// 충돌은 획득 대기 시간이 지난 뒤 EnablePickup에서 켠다.
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(40.0f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->CanCharacterStepUpOn = ECB_No;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(VisualRoot);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);

	IdleEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("IdleEffectComp"));
	IdleEffectComponent->SetupAttachment(VisualRoot);
	IdleEffectComponent->bAutoActivate = true;

	// 트레일은 흡수가 시작될 때만 켠다.
	AbsorbTrailEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AbsorbTrailEffectComp"));
	AbsorbTrailEffectComponent->SetupAttachment(VisualRoot);
	AbsorbTrailEffectComponent->bAutoActivate = false;

	// 기본 거리 커브: 플레이어 반대 방향으로 살짝 밀려났다가 점점 빨라지며 플레이어 위치로 빨려 들어간다.
	FRichCurve* DistanceCurve = AbsorbDistanceCurve.GetRichCurve();
	HealPickup::AddSmoothKey(*DistanceCurve, 0.0f, 0.0f);
	HealPickup::AddSmoothKey(*DistanceCurve, 0.3f, -0.3f);
	HealPickup::AddSmoothKey(*DistanceCurve, 1.0f, 1.0f, 3.0f);

	// 기본 크기 커브: 살짝 커졌다가 캐릭터에 가려질 만큼 작아진다.
	FRichCurve* ScaleCurve = AbsorbScaleCurve.GetRichCurve();
	HealPickup::AddSmoothKey(*ScaleCurve, 0.0f, 1.0f);
	HealPickup::AddSmoothKey(*ScaleCurve, 0.25f, 1.3f);
	HealPickup::AddSmoothKey(*ScaleCurve, 1.0f, 0.05f);
}

void AHealPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 지정한 이펙트 에셋을 컴포넌트에 반영한다.
	IdleEffectComponent->SetAsset(IdleEffect);
	AbsorbTrailEffectComponent->SetAsset(AbsorbEffect);
}

void AHealPickup::BeginPlay()
{
	Super::BeginPlay();

	Collision->OnComponentBeginOverlap.AddDynamic(this, &AHealPickup::OnCollisionBeginOverlap);

	// 풀 없이 스폰된 경우를 위해 바로 시작한다. 풀 액터는 직후 OnReturnToPool에서 정리된다.
	StartIdle();
}

void AHealPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PickupEnableTimer);
	GetWorldTimerManager().ClearTimer(LifetimeTimer);
	AbsorbingPlayer.Reset();

	Super::EndPlay(EndPlayReason);
}

void AHealPickup::OnSpawnFromPool_Implementation()
{
	StartIdle();
}

void AHealPickup::OnReturnToPool_Implementation()
{
	// 공통 비활성화(대기 위치 이동) 전에 트레일을 즉시 꺼서 이동 궤적이 남지 않게 한다.
	ClearPickupState();
}

void AHealPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (PickupState)
	{
	case EHealPickupState::Idle:
		TickIdle(DeltaSeconds);
		break;
	case EHealPickupState::Absorbing:
		TickAbsorb(DeltaSeconds);
		break;
	case EHealPickupState::Expiring:
		TickExpire(DeltaSeconds);
		break;
	default:
		break;
	}
}

void AHealPickup::StartIdle()
{
	ClearPickupState();

	PickupState = EHealPickupState::Idle;
	IdleEffectComponent->Activate(true);

	// 획득 대기 시간이 지나야 플레이어가 흡수할 수 있다.
	FTimerManager& TimerManager = GetWorldTimerManager();
	if (PickupEnableDelay > 0.0f)
	{
		TimerManager.SetTimer(PickupEnableTimer, this, &AHealPickup::EnablePickup, PickupEnableDelay, false);
	}
	else
	{
		EnablePickup();
	}

	// 흡수되지 않으면 수명이 다한 뒤 사라진다.
	if (PickupState == EHealPickupState::Idle)
	{
		TimerManager.SetTimer(LifetimeTimer, this, &AHealPickup::StartExpire, FMath::Max(0.1f, Lifetime), false);
	}
}

void AHealPickup::EnablePickup()
{
	if (PickupState != EHealPickupState::Idle)
	{
		return;
	}

	// 충돌 활성화
	// 충돌을 활성화하면 엔진이 겹침을 갱신하므로, 이미 겹쳐 있던 플레이어도 OnCollisionBeginOverlap으로 처리됨
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AHealPickup::OnCollisionBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (PickupState != EHealPickupState::Idle)
	{
		return;
	}

	if (APlayerCharacter* Player = FindAbsorbingPlayer(OtherActor, OtherComp))
	{
		StartAbsorb(Player);
	}
}

APlayerCharacter* AHealPickup::FindAbsorbingPlayer(AActor* OtherActor, UPrimitiveComponent* OtherComp) const
{
	// 분신이나 플레이어의 다른 컴포넌트가 아닌 플레이어 캡슐만 흡수 대상으로 본다.
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!IsValid(Player) || OtherComp != Player->GetCapsuleComponent())
	{
		return nullptr;
	}

	// 사망한 플레이어는 흡수하지 않는다.
	const UStatAttributeSet* StatAttributeSet = Player->GetStatAttribute();
	if (!StatAttributeSet || StatAttributeSet->GetHealth() <= 0.0f)
	{
		return nullptr;
	}

	return Player;
}

void AHealPickup::StartAbsorb(APlayerCharacter* Player)
{
	GetWorldTimerManager().ClearTimer(PickupEnableTimer);
	GetWorldTimerManager().ClearTimer(LifetimeTimer);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PickupState = EHealPickupState::Absorbing;
	AbsorbingPlayer = Player;
	StateElapsed = 0.0f;
	AbsorbStartLocation = GetActorLocation();
	AbsorbStartFloatingOffset = VisualRoot->GetRelativeLocation().Z;

	// 평상시 이펙트는 남은 파티클이 자연스럽게 사라지도록 Deactivate하고 트레일을 켠다.
	IdleEffectComponent->Deactivate();
	AbsorbTrailEffectComponent->Activate(true);
}

void AHealPickup::StartExpire()
{
	if (PickupState != EHealPickupState::Idle)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(PickupEnableTimer);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IdleEffectComponent->Deactivate();

	if (ExpireShrinkDuration <= 0.0f)
	{
		ReleasePickup();
		return;
	}

	PickupState = EHealPickupState::Expiring;
	StateElapsed = 0.0f;
}

void AHealPickup::TickIdle(float DeltaSeconds)
{
	UpdateFloatingMotion(DeltaSeconds);
}

void AHealPickup::TickAbsorb(float DeltaSeconds)
{
	// 흡수 도중 플레이어가 사라지면(사망 후 재생성 등) 회복 없이 정리한다.
	APlayerCharacter* Player = AbsorbingPlayer.Get();
	if (!IsValid(Player))
	{
		ReleasePickup();
		return;
	}

	StateElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(StateElapsed / FMath::Max(0.05f, AbsorbDuration), 0.0f, 1.0f);

	// 거리 커브로 시작 위치와 플레이어 위치 사이를 보간한다. 음수 구간에서는 플레이어 반대 방향으로 밀려난다.
	const float DistanceRatio = EvaluateCurve(AbsorbDistanceCurve, Alpha, Alpha);
	const FVector TargetLocation = GetAbsorbTargetLocation(Player);
	SetActorLocation(FMath::Lerp(AbsorbStartLocation, TargetLocation, DistanceRatio));

	// 크기 커브를 적용하고, 떠다니던 높이는 흡수 동안 점점 0으로 모은다.
	const float Scale = FMath::Max(0.0f, EvaluateCurve(AbsorbScaleCurve, Alpha, 1.0f - Alpha));
	VisualRoot->SetRelativeScale3D(FVector(Scale));
	VisualRoot->SetRelativeLocation(FVector(0.0f, 0.0f, AbsorbStartFloatingOffset * (1.0f - Alpha)));
	VisualRoot->AddLocalRotation(RotationRate * DeltaSeconds);

	if (Alpha >= 1.0f)
	{
		CompleteAbsorb();
	}
}

void AHealPickup::TickExpire(float DeltaSeconds)
{
	UpdateFloatingMotion(DeltaSeconds);

	StateElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(StateElapsed / ExpireShrinkDuration, 0.0f, 1.0f);
	VisualRoot->SetRelativeScale3D(FVector(1.0f - Alpha));

	if (Alpha >= 1.0f)
	{
		ReleasePickup();
	}
}

void AHealPickup::UpdateFloatingMotion(float DeltaSeconds)
{
	FloatingElapsed += DeltaSeconds;

	// 스폰 위치를 중심으로 FloatingPeriod 주기의 사인파로 오르내린다.
	const float FloatingPhase = FloatingElapsed / FMath::Max(0.05f, FloatingPeriod) * UE_TWO_PI;
	const float FloatingOffset = FMath::Sin(FloatingPhase) * FloatingHeight;

	VisualRoot->SetRelativeLocation(FVector(0.0f, 0.0f, FloatingOffset));
	VisualRoot->AddLocalRotation(RotationRate * DeltaSeconds);
}

void AHealPickup::CompleteAbsorb()
{
	APlayerCharacter* Player = AbsorbingPlayer.Get();
	if (IsValid(Player))
	{
		ApplyHeal(Player);

		// 아이템은 바로 풀로 돌아가므로 완료 이펙트는 플레이어에 붙여 따로 재생한다.
		if (HealCompleteEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				HealCompleteEffect,
				Player->GetRootComponent(),
				NAME_None,
				HealCompleteEffectOffset,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				true,
				true,
				ENCPoolMethod::AutoRelease
			);
		}
	}

	ReleasePickup();
}

void AHealPickup::ApplyHeal(APlayerCharacter* Player)
{
	if (!HealEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHealPickup::ApplyHeal()] %s 에 HealEffectClass가 지정되지 않았습니다."),
			   *GetClass()->GetName());
		return;
	}

	UAbilitySystemComponent* TargetASC = Player->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		return;
	}

	// 흡수 도중 사망한 플레이어는 회복으로 되살리지 않는다.
	const UStatAttributeSet* StatAttributeSet = Player->GetStatAttribute();
	if (!StatAttributeSet || StatAttributeSet->GetHealth() <= 0.0f)
	{
		return;
	}

	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddInstigator(Player, this);

	const FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(HealEffectClass, 1.0f, Context);
	if (!Spec.IsValid())
	{
		return;
	}

	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

FVector AHealPickup::GetAbsorbTargetLocation(const APlayerCharacter* Player) const
{
	return Player->GetActorLocation() + AbsorbTargetOffset;
}

float AHealPickup::EvaluateCurve(const FRuntimeFloatCurve& Curve, float Time, float DefaultValue)
{
	const FRichCurve* RichCurve = Curve.GetRichCurveConst();
	if (!RichCurve || RichCurve->GetNumKeys() == 0)
	{
		return DefaultValue;
	}

	return RichCurve->Eval(Time);
}

void AHealPickup::ClearPickupState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PickupEnableTimer);
		World->GetTimerManager().ClearTimer(LifetimeTimer);
	}

	PickupState = EHealPickupState::Inactive;
	AbsorbingPlayer.Reset();
	StateElapsed = 0.0f;
	FloatingElapsed = 0.0f;
	AbsorbStartFloatingOffset = 0.0f;

	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	VisualRoot->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	VisualRoot->SetRelativeScale3D(FVector::OneVector);

	IdleEffectComponent->DeactivateImmediate();
	AbsorbTrailEffectComponent->DeactivateImmediate();
}

void AHealPickup::ReleasePickup()
{
	UObjectPoolSubsystem::Release(this);
}
