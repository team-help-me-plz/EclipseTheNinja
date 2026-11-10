#include "Component/BossEnemyVFXComponent.h"

#include "AbilitySystemComponent.h"
#include "Character/BossEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GAS/Ability/GABossPatternBase.h"
#include "GAS/StatAttributeSet.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

UBossEnemyVFXComponent::UBossEnemyVFXComponent()
{
	// 트레일 디버그 표시(bDrawDebugTrail)를 켰을 때만 Tick에서 그린다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// 사용할 에셋은 보스 Blueprint의 VFXComp 기본값에서 지정한다.
}

void UBossEnemyVFXComponent::BeginPlay()
{
	Super::BeginPlay();

	Boss = Cast<ABossEnemy>(GetOwner());
	ABossEnemy* Owner = Boss.Get();
	if (!Owner)
	{
		return;
	}

	Owner->OnEnergyLinkActivated.AddDynamic(this, &UBossEnemyVFXComponent::HandleEnergyLinkActivated);
	Owner->OnEnergyLinkBroken.AddDynamic(this, &UBossEnemyVFXComponent::HandleEnergyLinkBroken);
	Owner->OnEnergyLinkFailed.AddDynamic(this, &UBossEnemyVFXComponent::HandleEnergyLinkFailed);

	if (UStatAttributeSet* Stats = Owner->GetStatAttribute())
	{
		Stats->OnDamageReceived.AddUObject(this, &UBossEnemyVFXComponent::HandleDamageReceived);
		Stats->OnDamageBlocked.AddUObject(this, &UBossEnemyVFXComponent::HandleDamageBlocked);
	}

	// 패턴 어빌리티의 시작·끝에 맞춰 칼 트레일을 켜고 끈다.
	if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
	{
		AbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &UBossEnemyVFXComponent::HandleAbilityActivated);
		AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &UBossEnemyVFXComponent::HandleAbilityEnded);
	}

	CreateTrail();
}

void UBossEnemyVFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ABossEnemy* Owner = Boss.Get())
	{
		Owner->OnEnergyLinkActivated.RemoveDynamic(this, &UBossEnemyVFXComponent::HandleEnergyLinkActivated);
		Owner->OnEnergyLinkBroken.RemoveDynamic(this, &UBossEnemyVFXComponent::HandleEnergyLinkBroken);
		Owner->OnEnergyLinkFailed.RemoveDynamic(this, &UBossEnemyVFXComponent::HandleEnergyLinkFailed);

		if (UStatAttributeSet* Stats = Owner->GetStatAttribute())
		{
			Stats->OnDamageReceived.RemoveAll(this);
			Stats->OnDamageBlocked.RemoveAll(this);
		}

		if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
		{
			ASC->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
			ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UBossEnemyVFXComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ABossEnemy* Owner = Boss.Get();
	if (!bDrawDebugTrail || !Owner)
	{
		return;
	}

	// 칼 본의 축. 트레일 Z축(노랑)이 칼날을 따라 놓였는지 비교한다.
	const FTransform BoneTransform = Owner->GetMesh()->GetSocketTransform(TrailSocket);
	const FVector BoneLocation = BoneTransform.GetLocation();
	const float AxisLength = 80.0f;

	DrawDebugLine(GetWorld(), BoneLocation, BoneLocation + BoneTransform.GetUnitAxis(EAxis::X) * AxisLength, FColor::Red, false, -1.0f, SDPG_Foreground, 2.0f);
	DrawDebugLine(GetWorld(), BoneLocation, BoneLocation + BoneTransform.GetUnitAxis(EAxis::Y) * AxisLength, FColor::Green, false, -1.0f, SDPG_Foreground, 2.0f);
	DrawDebugLine(GetWorld(), BoneLocation, BoneLocation + BoneTransform.GetUnitAxis(EAxis::Z) * AxisLength, FColor::Blue, false, -1.0f, SDPG_Foreground, 2.0f);

	if (TrailComp)
	{
		// 리본은 트레일 위치를 중심으로 Z축 방향 TrailWidth만큼 펼쳐진다.
		const FVector Center = TrailComp->GetComponentLocation();
		const FVector HalfSpan = TrailComp->GetUpVector() * TrailWidth * 0.5f;
		DrawDebugLine(GetWorld(), Center - HalfSpan, Center + HalfSpan, FColor::Yellow, false, -1.0f, SDPG_Foreground, 3.0f);
	}
}

void UBossEnemyVFXComponent::CreateTrail()
{
	ABossEnemy* Owner = Boss.Get();
	if (!Owner || TrailComp)
	{
		return;
	}

	UNiagaraSystem* System = TrailSystem.LoadSynchronous();
	if (!System)
	{
		return;
	}

	// 칼 본에 붙여 칼과 똑같이 움직이게 한다.
	TrailComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		System,
		Owner->GetMesh(),
		TrailSocket,
		TrailOffset,
		TrailRotation,
		EAttachLocation::KeepRelativeOffset,
		false,
		false,
		ENCPoolMethod::None,
		false
	);
	if (TrailComp)
	{
		TrailComp->SetVariableFloat(TrailWidthParameter, TrailWidth);

		// 재생을 늦춰 리본이 더 오래 남게 한다. 리본 조각은 매 프레임 만들어지므로 끊기지 않는다.
		TrailComp->SetCustomTimeDilation(1.0f / FMath::Max(0.1f, TrailLifetimeScale));

		// 원본 머티리얼은 그대로 두고, 보스 트레일에만 색을 바꾼 머티리얼을 넣는다.
		if (UMaterialInterface* BaseMaterial = TrailMaterial.LoadSynchronous())
		{
			TrailMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			TrailMID->SetVectorParameterValue(TEXT("Color"), TrailColor);
			TrailMID->SetScalarParameterValue(TEXT("Intensity"), TrailIntensity);
			TrailComp->SetVariableMaterial(TrailMaterialParameter, TrailMID);
		}
	}
}

void UBossEnemyVFXComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
	if (!Ability || !Ability->IsA<UGABossPatternBase>())
	{
		return;
	}

	++ActivePatternCount;

	if (TrailComp && !TrailComp->IsActive())
	{
		TrailComp->SetVariableFloat(TrailWidthParameter, TrailWidth);
		TrailComp->Activate(true);
	}
}

void UBossEnemyVFXComponent::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (!EndedData.AbilityThatEnded || !EndedData.AbilityThatEnded->IsA<UGABossPatternBase>())
	{
		return;
	}

	ActivePatternCount = FMath::Max(0, ActivePatternCount - 1);

	// 남은 리본이 짧게 사라지도록 비활성화만 한다.
	if (ActivePatternCount == 0 && TrailComp)
	{
		TrailComp->Deactivate();
	}
}

void UBossEnemyVFXComponent::ResetVFX()
{
	HandleEnergyLinkBroken();

	ActivePatternCount = 0;
	if (TrailComp)
	{
		TrailComp->DeactivateImmediate();
	}
}

void UBossEnemyVFXComponent::HandleEnergyLinkActivated(int32 LinkIndex)
{
	ABossEnemy* Owner = Boss.Get();
	if (!Owner)
	{
		return;
	}

	if (!ShieldComp)
	{
		UNiagaraSystem* System = ShieldSystem.LoadSynchronous();
		if (!System)
		{
			return;
		}

		// 보스 루트에 붙여 실드가 보스를 따라다니게 한다.
		ShieldComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			System,
			Owner->GetRootComponent(),
			NAME_None,
			ShieldOffset,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false,
			false,
			ENCPoolMethod::None,
			false
		);
		if (!ShieldComp)
		{
			return;
		}

		ShieldComp->SetRelativeScale3D(FVector(ShieldScale));
	}

	ShieldComp->Activate(true);
}

void UBossEnemyVFXComponent::HandleEnergyLinkBroken()
{
	if (ShieldComp)
	{
		ShieldComp->Deactivate();
	}
}

void UBossEnemyVFXComponent::HandleEnergyLinkFailed()
{
	// 실패하면 바로 처형 컷신이 나오므로 실드가 서서히 사라지지 않고 즉시 꺼지게 한다.
	if (ShieldComp)
	{
		ShieldComp->DeactivateImmediate();
	}
}

void UBossEnemyVFXComponent::HandleDamageReceived(const FGameplayEffectContextHandle& Context)
{
	FVector Location;
	FVector Direction;
	GetHitPoint(Context, Location, Direction);

	// 공격 방향을 중심으로 좌우·위아래를 흔들고, 감는 각도는 매번 무작위로 둔다.
	FRotator Rotation = Direction.Rotation();
	Rotation.Yaw += FMath::FRandRange(-BloodYawRange, BloodYawRange);
	Rotation.Pitch += FMath::FRandRange(-BloodPitchRange, BloodPitchRange);
	Rotation.Roll = FMath::FRandRange(0.0f, 360.0f);

	SpawnOneShot(
		BloodSystem,
		Location,
		Rotation,
		BloodScale,
		BloodActiveTime
	);
}

void UBossEnemyVFXComponent::HandleDamageBlocked(const FGameplayEffectContextHandle& Context)
{
	FVector Location;
	FVector Direction;
	GetHitPoint(Context, Location, Direction);
	const ABossEnemy* Owner = Boss.Get();
	if (Owner && Owner->IsEnergyLinkActive() && Owner->GetNetMode() != NM_DedicatedServer && !ShieldHitSounds.IsEmpty())
	{
		if (USoundBase* Sound = ShieldHitSounds[FMath::RandRange(0, ShieldHitSounds.Num() - 1)])
		{
			UGameplayStatics::PlaySoundAtLocation(Owner, Sound, Location);
		}
	}

	// 공격자 쪽을 바라보게 해 실드 표면에서 튕겨 나가는 것처럼 보이게 한다.
	SpawnOneShot(
		ShieldHitSystem,
		Location,
		(-Direction).Rotation(),
		ShieldHitScale,
		0.0f
	);
}

void UBossEnemyVFXComponent::GetHitPoint(
	const FGameplayEffectContextHandle& Context,
	FVector& OutLocation,
	FVector& OutDirection
) const
{
	const ABossEnemy* Owner = Boss.Get();
	if (!Owner)
	{
		OutLocation = FVector::ZeroVector;
		OutDirection = FVector::ForwardVector;
		return;
	}

	const FVector Center = Owner->GetActorLocation();

	// 표창처럼 따로 날아온 공격은 투사체 위치를, 아니면 공격자 위치를 기준으로 방향을 잡는다.
	const AActor* Source = Context.GetEffectCauser();
	if (!IsValid(Source))
	{
		Source = Context.GetInstigator();
	}

	OutDirection = IsValid(Source)
		? (Center - Source->GetActorLocation()).GetSafeNormal2D()
		: FVector::ZeroVector;

	if (OutDirection.IsNearlyZero())
	{
		OutDirection = -Owner->GetActorForwardVector().GetSafeNormal2D();
	}

	// 공격이 들어온 쪽의 캡슐 표면에서 터지게 한다.
	const float Radius = Owner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	OutLocation = Center - OutDirection * Radius + FVector(0.0f, 0.0f, HitHeightOffset);
}

void UBossEnemyVFXComponent::SpawnGaleGust(const FVector& DashStart, const FVector& DashEnd)
{
	const ABossEnemy* Owner = Boss.Get();
	if (!Owner || GustSystem.IsNull())
	{
		return;
	}

	// 먼지는 캡슐 바닥(발밑) 높이에서 피운다.
	const FVector FloorOffset(0.0f, 0.0f, -Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Start = DashStart + FloorOffset;
	const FVector End = DashEnd + FloorOffset;

	// 돌진 경로(선택): 출발점부터 도착점까지 차례로 먼지가 피어올라 지나간 자리에 바람이 남은 것처럼 보인다.
	const float PathLength = FVector::Dist2D(Start, End);
	const bool bPathDust = GustPathSpacing > 0.0f && PathLength > KINDA_SMALL_NUMBER;
	if (bPathDust)
	{
		const int32 PathCount = FMath::Max(1, FMath::FloorToInt(PathLength / GustPathSpacing));
		for (int32 Index = 0; Index < PathCount; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / PathCount;
			SpawnGustPuffDelayed(
				FMath::Lerp(Start, End, Alpha),
				GustPathDuration * Alpha
			);
		}
	}

	// 도착 지점: 발밑 한 점에서 시작해 링이 하나씩 바깥으로 넓어지며 모래 바람이 퍼진다.
	if (GustRingCount <= 0)
	{
		return;
	}

	// 경로 먼지가 없으면 돌진이 끝난 순간 바로 퍼지기 시작한다.
	const float RingStartDelay = bPathDust ? GustPathDuration : 0.0f;
	for (int32 Ring = 0; Ring < GustRingCount; ++Ring)
	{
		const float RingAlpha = GustRingCount > 1
			? static_cast<float>(Ring) / (GustRingCount - 1)
			: 0.0f;
		const float Radius = GustRingRadius * RingAlpha;
		const float Delay = RingStartDelay + GustRingInterval * Ring;

		// 첫 링은 한 점, 바깥 링일수록 반지름에 비례해 먼지가 많다.
		const int32 PuffCount = Ring == 0
			? 1
			: FMath::Max(3, FMath::RoundToInt(GustRingPuffs * RingAlpha));

		// 링마다 시작 각도를 비틀어 먼지가 일렬로 겹치지 않게 한다.
		const float AngleOffset = FMath::FRandRange(0.0f, UE_TWO_PI);
		for (int32 Index = 0; Index < PuffCount; ++Index)
		{
			const float Angle = AngleOffset + UE_TWO_PI * Index / PuffCount;
			const FVector Offset(
				FMath::Cos(Angle) * Radius,
				FMath::Sin(Angle) * Radius * GustRingDepthScale,
				0.0f
			);
			SpawnGustPuffDelayed(End + Offset, Delay);
		}
	}
}

void UBossEnemyVFXComponent::SpawnGustPuffDelayed(const FVector& Location, float Delay)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 먼지마다 방향을 무작위로 돌려 같은 모양이 반복되지 않게 한다.
	const FRotator Rotation(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);

	if (Delay <= 0.0f)
	{
		SpawnGustPuff(Location, Rotation);
		return;
	}

	TWeakObjectPtr<UBossEnemyVFXComponent> WeakThis = this;
	FTimerHandle PuffTimer;
	World->GetTimerManager().SetTimer(
		PuffTimer,
		FTimerDelegate::CreateWeakLambda(this, [WeakThis, Location, Rotation]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->SpawnGustPuff(Location, Rotation);
			}
		}),
		Delay,
		false
	);
}

void UBossEnemyVFXComponent::SpawnGustPuff(const FVector& Location, const FRotator& Rotation)
{
	UNiagaraComponent* Puff = SpawnOneShot(
		GustSystem,
		Location,
		Rotation,
		GustScale,
		GustActiveTime
	);
	if (!Puff)
	{
		return;
	}

	// 성 바닥에 맞게 원본보다 옅은 모래색을 쓴다.
	Puff->SetVariableLinearColor(TEXT("User.Sand Lit Color"), GustLitColor);
	Puff->SetVariableLinearColor(TEXT("User.Sand Shadow Color"), GustShadowColor);
}

UNiagaraComponent* UBossEnemyVFXComponent::SpawnOneShot(
	const TSoftObjectPtr<UNiagaraSystem>& System,
	const FVector& Location,
	const FRotator& Rotation,
	float Scale,
	float ActiveTime
) const
{
	UNiagaraSystem* LoadedSystem = System.LoadSynchronous();
	UWorld* World = GetWorld();
	if (!LoadedSystem || !World)
	{
		return nullptr;
	}

	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		LoadedSystem,
		Location,
		Rotation,
		FVector(Scale),
		true,
		true,
		ENCPoolMethod::None,
		true
	);
	if (!Comp || ActiveTime <= 0.0f)
	{
		return Comp;
	}

	// 루프 에셋은 ActiveTime 뒤 비활성화한다. 남은 입자가 다 사라지면 자동으로 파괴된다.
	TWeakObjectPtr<UNiagaraComponent> WeakComp = Comp;
	FTimerHandle StopTimer;
	World->GetTimerManager().SetTimer(
		StopTimer,
		FTimerDelegate::CreateWeakLambda(Comp, [WeakComp]()
		{
			if (WeakComp.IsValid())
			{
				WeakComp->Deactivate();
			}
		}),
		ActiveTime,
		false
	);

	return Comp;
}
