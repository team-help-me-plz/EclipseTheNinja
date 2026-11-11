#include "Character/CeilingEnemy.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectExtension.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GACeilingAmbush.h"
#include "UObject/ConstructorHelpers.h"

ACeilingEnemy::ACeilingEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// 기본 애니메이션. 블루프린트에서 다른 에셋으로 바꿀 수 있다.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> HangAnimFinder(
		TEXT("/Game/Anims/Anim_Seq/CeilingHide/Ninja_CeilingHide_from_Floor_StandingDrawn")
	);
	if (HangAnimFinder.Succeeded())
	{
		HangAnimation = HangAnimFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> AmbushAnimFinder(
		TEXT("/Game/Anims/Anim_Seq/FromAbove/Ninja_fromAbove_Attack_StabDown_Attacker")
	);
	if (AmbushAnimFinder.Succeeded())
	{
		AmbushAnimation = AmbushAnimFinder.Object;
	}

	// 기습 어빌리티를 기본으로 부여한다. 기본 공격 항목은 기존 적과 같이 블루프린트에서 지정한다.
	FAbilityInfo& AmbushInfo = AbilityClassArray.AddDefaulted_GetRef();
	AmbushInfo.AbilityClass = UGACeilingAmbush::StaticClass();
	AmbushInfo.InputID = EAbilityInputID::CeilingAmbush;
}

void ACeilingEnemy::PostInitializeComponents()
{
	// 배치된 적은 Super에서 AI 컨트롤러가 빙의하고 BT가 시작될 수 있다.
	// 그 전에 단계를 정해 두어야 BT의 Is On Ceiling이 처음부터 천장 분기를 고른다.
	Phase = bStartOnCeiling
		? ECeilingEnemyPhase::Hanging
		: ECeilingEnemyPhase::Grounded;

	Super::PostInitializeComponents();
}

void ACeilingEnemy::BeginPlay()
{
	Super::BeginPlay();

	if (bStartOnCeiling && !IsDead())
	{
		// 기습 순간에 멈칫하지 않도록 하강 구간 분석을 미리 해 둔다.
		EnsureAmbushProfile();
		EnterHanging();
		return;
	}

	Phase = ECeilingEnemyPhase::Grounded;
}

void ACeilingEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsDead())
	{
		return;
	}

	// 낙하가 시작되면 매달림 메시 오프셋을 매달림 몽타주가 빠지는 가중치만큼 되돌린다.
	// 자세 블렌드와 같은 비율로 움직여야 몸이 위아래로 튀지 않는다.
	if (Phase != ECeilingEnemyPhase::Hanging && HangMeshOffsetAlpha > 0.0f)
	{
		float HangWeight = 0.0f;
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

		if (AnimInstance && HangMontage)
		{
			if (const FAnimMontageInstance* HangInstance = AnimInstance->GetInstanceForMontage(HangMontage))
			{
				HangWeight = HangInstance->GetWeight();
			}
		}

		HangMeshOffsetAlpha = FMath::Min(HangMeshOffsetAlpha, HangWeight);
		ApplyHangMeshOffset(HangMeshOffsetAlpha);
	}

	// 찍기 자세에서 몸이 캡슐 축을 벗어나는 만큼(좌우·깊이) 메시를 반대로 옮긴다.
	// 기습 몽타주 가중치만큼만 적용해, 들어올 때와 빠질 때 모두 몸이 캡슐 축 위에 머문다.
	FVector NewAmbushBodyCorrection = FVector::ZeroVector;

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (AmbushAnimation && AmbushLockedBodySamples.Num() > 0)
		{
			if (const FAnimMontageInstance* AmbushInstance = AnimInstance->GetInstanceForMontage(GetAmbushMontage()))
			{
				FVector BodyOffset = GetBodyOffsetActorSpace(AmbushInstance->GetPosition());
				BodyOffset.Z = 0.0f;
				NewAmbushBodyCorrection = -BodyOffset * AmbushInstance->GetWeight();
			}
		}
	}

	if (!NewAmbushBodyCorrection.Equals(AmbushBodyCorrection, 0.01f))
	{
		AmbushBodyCorrection = NewAmbushBodyCorrection;
		ApplyHangMeshOffset(HangMeshOffsetAlpha);
	}

	if (Phase == ECeilingEnemyPhase::Grounded)
	{
		return;
	}

	if (Phase == ECeilingEnemyPhase::Hanging)
	{
		UpdateHangLoop(DeltaTime);

		// 매달림 포즈가 몇 프레임 평가된 뒤 메시 높이를 천장에 맞춘다.
		if (!bHangPoseAligned)
		{
			++HangTicksSinceStart;

			UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
			if (HangTicksSinceStart >= 3 && AnimInstance && HangMontage && AnimInstance->Montage_IsPlaying(HangMontage))
			{
				AlignHangPoseToCeiling();
			}
		}

		// 대상 감지와 기습 시작은 BT(Wait For Ambush Target → Ceiling Ambush)가 맡는다.
		return;
	}

	// 기습 시작 때 정한 방향으로 남은 회전을 이어 간다.
	// 가로로 뻗은 매달림 자세가 다 빠진 뒤(몸이 세로로 모인 찍기 자세)에 돌아야 몸 끝이 벽을 쓸지 않는다.
	if (!FMath::IsNearlyZero(AmbushTurnRemaining) && HangMeshOffsetAlpha <= 0.05f)
	{
		const float TurnSpeed = 180.0f / FMath::Max(0.01f, AmbushTurnTime);
		const float Step = FMath::Sign(AmbushTurnRemaining) * FMath::Min(FMath::Abs(AmbushTurnRemaining), TurnSpeed * DeltaTime);

		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + Step, 0.0f));
		AmbushTurnRemaining -= Step;
	}

	// 기습 애니메이션의 하강 궤적을 따라 내려온다.
	if (Phase == ECeilingEnemyPhase::Ambushing && bUsingProfiledDescent && !bDescentReleased)
	{
		UpdateProfiledDescent();
	}

	// 바닥이 없어 착지하지 못하는 경우에도 계속 멈춰 있지 않도록 한다.
	if (!bLandedDuringAmbush && GetWorld()->GetTimeSeconds() - DescentStartTime >= DescentTimeout)
	{
		FinishDescent();
		return;
	}

	// 착지 후 기습 몽타주가 끝나면 일반 근접 적으로 전환한다.
	if (Phase == ECeilingEnemyPhase::Ambushing && bLandedDuringAmbush && !IsAmbushAbilityActive())
	{
		FinishDescent();
	}
}

void ACeilingEnemy::EnterHanging()
{
	Phase = ECeilingEnemyPhase::Hanging;
	AmbushTarget.Reset();
	PendingCombatActor.Reset();
	bLandedDuringAmbush = false;
	bUsingProfiledDescent = false;
	bDescentReleased = false;
	AmbushTurnRemaining = 0.0f;

	// 비행 모드로 두면 중력을 받지 않고 제자리에 머문다.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);

	// 배치 회전값과 관계없이 정해진 방향으로 매달린다. 기울어진 회전도 함께 없앤다.
	SetActorRotation(FRotator(0.0f, HangYaw, 0.0f));

	SnapToCeiling();

	// 매달린 동안에는 감지(발각 범위 UI 포함)를 멈춘다. 이동은 BT의 천장 분기가 하지 않는다.
	SetBehaviorState(EEnemyBehaviorState::Idle);
	EnemyDetectComponent->StopDetection();

	// 포즈가 바뀌면 다시 높이를 맞춘다.
	bHangPoseAligned = false;
	HangTicksSinceStart = 0;
	AutoHangMeshOffset = FVector::ZeroVector;

	HangMeshOffsetAlpha = 1.0f;
	ApplyHangMeshOffset(HangMeshOffsetAlpha);

	HangLoopElapsed = 0.0f;
	PlayHangLoop();
}

void ACeilingEnemy::SnapToCeiling()
{
	bHasCeiling = false;

	if (CeilingSearchDistance <= 0.0f)
	{
		return;
	}

	// 캡슐 바닥 근처에서 위로 선을 쏴 처음 막히는 면을 천장으로 본다.
	// 캡슐을 쓸어 올리지 않으므로 천장에 겹치게 배치해도 천장 아랫면을 찾을 수 있다.
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = GetActorLocation();
	const FVector Start = Location - FVector(0.0f, 0.0f, HalfHeight - 5.0f);
	const FVector End = Location + FVector(0.0f, 0.0f, CeilingSearchDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CeilingSnap), false, this);
	FCollisionResponseParams ResponseParams;
	Capsule->InitSweepCollisionParams(Params, ResponseParams);

	// 위에 있는 다른 캐릭터를 천장으로 잘못 보지 않도록 Pawn은 무시한다.
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		Capsule->GetCollisionObjectType(),
		Params,
		ResponseParams
	);

	if (!bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 위쪽 %.0fcm 안에서 천장을 찾지 못해 배치한 높이에 매달립니다."),
			*GetName(),
			CeilingSearchDistance);
		return;
	}

	bHasCeiling = true;
	CeilingZ = Hit.ImpactPoint.Z;

	// 캡슐 윗면이 천장 아랫면보다 1cm 아래에 오도록 옮겨 겹침 판정을 피한다.
	const FVector Snapped(Location.X, Location.Y, CeilingZ - HalfHeight - 1.0f);
	SetActorLocation(Snapped, false, nullptr, ETeleportType::TeleportPhysics);
}

void ACeilingEnemy::AlignHangPoseToCeiling()
{
	bHangPoseAligned = true;

	if (!bHasCeiling)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	const TArray<FTransform>& BoneTransforms = MeshComp->GetComponentSpaceTransforms();
	if (BoneTransforms.Num() == 0)
	{
		return;
	}

	// 현재 매달림 포즈에서 가장 높은 뼈의 월드 높이를 찾는다.
	const FTransform& ComponentTransform = MeshComp->GetComponentTransform();
	float TopZ = -TNumericLimits<float>::Max();

	for (const FTransform& BoneTransform : BoneTransforms)
	{
		TopZ = FMath::Max(TopZ, static_cast<float>(ComponentTransform.TransformPosition(BoneTransform.GetLocation()).Z));
	}

	// 그 뼈가 천장에 닿도록 메시를 올리거나 내린다.
	FVector WorldDelta(0.0f, 0.0f, CeilingZ - TopZ);

	// 몸통 뼈가 캡슐(배치 위치) 바로 위에 오도록 좌우도 맞춘다. 방향을 돌려도 몸이 옆으로 튀지 않는다.
	const int32 BodyBoneIndex = MeshComp->GetBoneIndex(AmbushBodyBoneName);
	if (BoneTransforms.IsValidIndex(BodyBoneIndex))
	{
		const FVector BodyLocation = ComponentTransform.TransformPosition(BoneTransforms[BodyBoneIndex].GetLocation());
		WorldDelta.X = GetActorLocation().X - BodyLocation.X;
		WorldDelta.Y = GetActorLocation().Y - BodyLocation.Y;
	}

	AutoHangMeshOffset += GetActorRotation().UnrotateVector(WorldDelta);
	ApplyHangMeshOffset(HangMeshOffsetAlpha);

	UE_LOG(LogTemp, Log, TEXT("%s: 매달림 포즈를 천장에 맞춤 (메시 보정 %s)"), *GetName(), *AutoHangMeshOffset.ToCompactString());
}

APawn* ACeilingEnemy::FindAmbushTarget(float DebugDrawTime) const
{
	UWorld* World = GetWorld();

	// 캡슐 중심에서 아래로 AmbushTriggerDepth만큼 뻗은 상자 구역
	const FVector Origin = GetActorLocation();
	const float HalfDepth = FMath::Max(1.0f, AmbushTriggerDepth) * 0.5f;
	const FVector BoxCenter = Origin - FVector(0.0f, 0.0f, HalfDepth);
	const FVector BoxExtent(FMath::Max(1.0f, AmbushTriggerHalfWidth), 200.0f, HalfDepth);

	if (bDrawAmbushTriggerDebug)
	{
		DrawDebugBox(World, BoxCenter, BoxExtent, FColor::Purple, false, DebugDrawTime);
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CeilingAmbushTrigger), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		BoxCenter,
		FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeBox(BoxExtent),
		QueryParams
	);

	APawn* BestTarget = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* Candidate = Cast<APawn>(Overlap.GetActor());

		// 살아 있는 플레이어/분신이 적보다 아래에 있어야 한다.
		if (!IsValidAITarget(Candidate) || Candidate->GetActorLocation().Z >= Origin.Z)
		{
			continue;
		}

		// 바닥이나 벽 너머의 대상은 기습하지 않는다.
		FCollisionQueryParams SightParams(SCENE_QUERY_STAT(CeilingAmbushSight), false, this);
		SightParams.AddIgnoredActor(Candidate);

		if (World->LineTraceTestByChannel(Origin, Candidate->GetActorLocation(), ECC_Visibility, SightParams))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Origin, Candidate->GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

void ACeilingEnemy::StartAmbush(APawn* Target)
{
	Phase = ECeilingEnemyPhase::Ambushing;
	AmbushTarget = Target;
	PendingCombatActor = Target;
	bLandedDuringAmbush = false;
	DescentStartTime = GetWorld()->GetTimeSeconds();

	EnsureAmbushProfile();

	// 대상 쪽으로 방향을 돌린다. 매달린 몸은 천장을 따라 가로로 뻗어 있어 한 번에 뒤집으면 순간이동처럼 보이므로,
	// AmbushTurnTime 동안 카메라 쪽(뒤쪽 벽 반대편)을 거쳐 천천히 돈다.
	const FVector Start = GetActorLocation();
	StartAmbushTurn(Target->GetActorLocation().X - Start.X);

	// 하강은 애니메이션 궤적을 직접 따라가므로 ABP의 루트 모션은 무시한다.
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (!bRootMotionOverridden)
		{
			SavedRootMotionMode = AnimInstance->RootMotionMode;
			bRootMotionOverridden = true;
		}

		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}

	// 실제 바닥까지 내려갈 높이. 애니메이션의 하강 높이를 이 값에 맞춰 늘리거나 줄인다.
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CeilingAmbushFloor), false, this);
	FCollisionResponseParams ResponseParams;
	Capsule->InitSweepCollisionParams(Params, ResponseParams);

	// 바로 아래 서 있는 대상의 머리를 바닥으로 잘못 보지 않도록 Pawn은 무시한다.
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);

	FHitResult FloorHit;
	const bool bFoundFloor = GetWorld()->SweepSingleByChannel(
		FloorHit,
		Start,
		Start - FVector(0.0f, 0.0f, AmbushTriggerDepth + 1000.0f),
		FQuat::Identity,
		Capsule->GetCollisionObjectType(),
		Capsule->GetCollisionShape(),
		Params,
		ResponseParams
	);

	DescentStartLocation = Start;
	DescentHeight = bFoundFloor
		? Start.Z - FloorHit.Location.Z
		: AmbushRootDrop;
	bDescentReleased = false;
	bUsingProfiledDescent = bAmbushProfileValid && DescentHeight > 1.0f;

	// 몸 궤적의 하강량을 실제 높이에 맞추는 배율을 구한다.
	// 착지 시각에 캡슐이 정확히 DescentHeight만큼 내려가도록, 루트 고정으로 생기는 몸 높이 변화분을 뺀 나머지를 몸 하강량으로 나눈다.
	DescentAnimStartTime = GetAmbushStartTime();
	DescentBodyScale = 1.0f;

	if (bUsingProfiledDescent)
	{
		const float BodyStart = SampleAmbushCurve(AmbushBodyZSamples, DescentAnimStartTime);
		const float BodyEnd = SampleAmbushCurve(AmbushBodyZSamples, AmbushDescentEndTime);
		const float LocalStart = BodyStart - SampleAmbushCurve(AmbushRootZSamples, DescentAnimStartTime);
		const float LocalEnd = BodyEnd - SampleAmbushCurve(AmbushRootZSamples, AmbushDescentEndTime);
		const float BodyDrop = BodyStart - BodyEnd;

		if (BodyDrop > 1.0f)
		{
			DescentBodyScale = FMath::Clamp((DescentHeight - (LocalEnd - LocalStart)) / BodyDrop, 0.1f, 10.0f);
		}

		UE_LOG(LogTemp, Log, TEXT("%s: 기습 하강 시작 %.2f초, 착지 %.2f초, 실제 높이 %.0fcm, 몸 하강 %.0fcm, 배율 %.2f"),
			*GetName(),
			DescentAnimStartTime,
			AmbushDescentEndTime,
			DescentHeight,
			BodyDrop,
			DescentBodyScale);
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();

	if (bUsingProfiledDescent)
	{
		// 중력 없이 비행 모드에서 궤적대로 위치를 옮긴다.
		Movement->SetMovementMode(MOVE_Flying);
	}
	else
	{
		// 하강 구간을 찾지 못한 애니메이션이면 빠른 낙하로 대신한다.
		Movement->SetMovementMode(MOVE_Falling);
		Movement->Velocity = FVector(0.0f, 0.0f, -FMath::Max(0.0f, AmbushDropSpeed));
	}

	// 기습 몽타주가 같은 슬롯의 매달림 루프를 블렌드 인 시간 동안 교체한다.
	if (!TryActivateAbilityByInputID(EAbilityInputID::CeilingAmbush))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 기습 어빌리티를 시작하지 못해 공격 없이 떨어집니다."), *GetName());
		ReleaseHangPose(AmbushBlendInTime);
		ReleaseDescentToFalling();
	}
}

void ACeilingEnemy::StartAmbushTurn(float DirectionX)
{
	AmbushTurnRemaining = 0.0f;

	if (FMath::IsNearlyZero(DirectionX))
	{
		return;
	}

	const float CurrentYaw = GetActorRotation().Yaw;
	float Delta = FRotator::NormalizeAxis((DirectionX > 0.0f ? 0.0f : 180.0f) - CurrentYaw);

	if (FMath::Abs(Delta) < 1.0f)
	{
		return;
	}

	// 반 바퀴를 돌 때는 몸 앞쪽이 카메라를 향하는 쪽으로 돈다. 뒤쪽 벽을 통과해 보이지 않게 한다.
	if (FMath::Abs(FMath::Abs(Delta) - 180.0f) < 1.0f)
	{
		if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector ToCamera = -CameraManager->GetCameraRotation().Vector();
			const FVector MidForward = FRotator(0.0f, CurrentYaw + 90.0f, 0.0f).Vector();
			Delta = FVector::DotProduct(MidForward, ToCamera) >= 0.0f ? 180.0f : -180.0f;
		}
	}

	if (AmbushTurnTime <= KINDA_SMALL_NUMBER)
	{
		SetActorRotation(FRotator(0.0f, CurrentYaw + Delta, 0.0f));
		return;
	}

	AmbushTurnRemaining = Delta;
}

void ACeilingEnemy::UpdateProfiledDescent()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	UAnimMontage* AmbushMontage = GetAmbushMontage();

	// 피격 등으로 기습 몽타주가 끊기면 그 자리에서 떨어진다.
	if (!AnimInstance || !AmbushMontage || !AnimInstance->Montage_IsPlaying(AmbushMontage))
	{
		ReleaseDescentToFalling();
		return;
	}

	// 화면에 보이는 몸이 원본 애니메이션의 몸 궤적대로 내려오도록 캡슐 높이를 정한다.
	const float Position = AnimInstance->Montage_GetPosition(AmbushMontage);
	const float Time = FMath::Clamp(Position, DescentAnimStartTime, AmbushDescentEndTime);
	const float Drop = GetDescentDropAt(Time);

	// 캡슐은 수직으로만 내린다. 몸이 캡슐 축을 벗어나는 만큼은 Tick에서 메시를 반대로 옮겨 맞춘다.
	const FVector Desired = DescentStartLocation - FVector(0.0f, 0.0f, Drop);

	FHitResult Hit;
	SetActorLocation(Desired, true, &Hit);

	// 착지 시각이 지났거나 바닥에 먼저 닿으면 내려앉는다.
	if (Position >= AmbushDescentEndTime || (Hit.bBlockingHit && Hit.ImpactNormal.Z > 0.7f))
	{
		ReleaseDescentToFalling();
	}
}

void ACeilingEnemy::ReleaseDescentToFalling()
{
	if (bDescentReleased)
	{
		return;
	}

	bDescentReleased = true;

	// 바로 아래 바닥에 닿아 Landed가 호출되도록 짧게 떨어뜨린다.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Falling);
	Movement->Velocity = FVector(0.0f, 0.0f, -300.0f);
}

void ACeilingEnemy::StartDropWithoutAttack(AActor* DamageInstigator)
{
	Phase = ECeilingEnemyPhase::Dropping;
	AmbushTarget.Reset();
	PendingCombatActor = DamageInstigator;
	DescentStartTime = GetWorld()->GetTimeSeconds();

	ReleaseHangPose(DropBlendOutTime);

	// 공격 없이 중력으로만 떨어진다.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Falling);
}

void ACeilingEnemy::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (Phase == ECeilingEnemyPhase::Ambushing)
	{
		bLandedDuringAmbush = true;
		bDescentReleased = true;

		// 착지 후 남은 동작은 원래 속도로 재생한다.
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			if (UAnimMontage* AmbushMontage = GetAmbushMontage())
			{
				AnimInstance->Montage_SetPlayRate(AmbushMontage, FMath::Max(0.01f, AmbushPlayRate));
			}
		}

		// 기습이 진행 중이면 착지 지점에서 타격 판정을 한다. 이미 끝났다면 바로 일반 상태로 돌아간다.
		if (IsAmbushAbilityActive())
		{
			FGameplayEventData Payload;
			Payload.EventTag = UGABaseAttack::GetHitEventTag();
			Payload.Instigator = this;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, Payload.EventTag, Payload);
		}
		else
		{
			FinishDescent();
		}

		return;
	}

	if (Phase == ECeilingEnemyPhase::Dropping)
	{
		FinishDescent();
	}
}

void ACeilingEnemy::FinishDescent()
{
	if (Phase == ECeilingEnemyPhase::Grounded)
	{
		return;
	}

	const bool bWasAmbush = Phase == ECeilingEnemyPhase::Ambushing;

	// 다 돌지 못하고 착지했다면 남은 회전을 바로 마친다.
	if (!FMath::IsNearlyZero(AmbushTurnRemaining))
	{
		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + AmbushTurnRemaining, 0.0f));
		AmbushTurnRemaining = 0.0f;
	}
	Phase = ECeilingEnemyPhase::Grounded;
	bUsingProfiledDescent = false;

	// 타임아웃으로 끝난 경우 남아 있는 기습 어빌리티를 정리한다.
	if (const FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(EAbilityInputID::CeilingAmbush))
	{
		if (Spec->IsActive())
		{
			ASC->CancelAbilityHandle(Spec->Handle);
		}
	}

	if (bRootMotionOverridden)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(SavedRootMotionMode);
		}

		bRootMotionOverridden = false;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement->MovementMode == MOVE_Flying)
	{
		Movement->SetDefaultMovementMode();
	}

	AActor* CombatActor = PendingCombatActor.Get();
	AmbushTarget.Reset();
	PendingCombatActor.Reset();

	// 감지를 다시 켜고, 기습 대상(또는 공격자)과 바로 전투에 들어간다.
	// BT의 Ceiling Ambush는 단계가 Grounded로 바뀐 것을 보고 끝나며, 이후 일반 전투 분기(BT_Enemy)로 넘어간다.
	EnemyDetectComponent->ResetDetection();
	EnemyDetectComponent->NotifyDamaged(CombatActor);

	// 기습을 공격 1회로 계산해, 착지 직후 기본 공격이 바로 이어지지 않게 한다.
	if (bWasAmbush)
	{
		NextAttackTime = FMath::Max(
			NextAttackTime,
			GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, GetAIAttackCooldown())
		);
	}
}

void ACeilingEnemy::ReleaseHangPose(float BlendOutTime)
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (HangMontage && AnimInstance->Montage_IsPlaying(HangMontage))
		{
			AnimInstance->Montage_Stop(BlendOutTime, HangMontage);
		}
	}

	// 블렌드 아웃이 있으면 Tick에서 매달림 몽타주 가중치를 따라 오프셋을 푼다.
	if (BlendOutTime <= KINDA_SMALL_NUMBER)
	{
		HangMeshOffsetAlpha = 0.0f;
		ApplyHangMeshOffset(HangMeshOffsetAlpha);
	}
}

void ACeilingEnemy::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// 기습 전에 맞으면 공격하지 않고 떨어진다. 피격 몽타주와 감지 갱신은 기본 처리에서 이어진다.
	if (Phase == ECeilingEnemyPhase::Hanging && !IsDead() && Data.NewValue < Data.OldValue)
	{
		if (Data.NewValue > 0.0f)
		{
			AActor* DamageInstigator = nullptr;

			if (Data.GEModData)
			{
				const FGameplayEffectContextHandle Context = Data.GEModData->EffectSpec.GetContext();
				DamageInstigator = IsValid(Context.GetInstigator())
					? Context.GetInstigator()
					: Context.GetEffectCauser();
			}

			StartDropWithoutAttack(DamageInstigator);
		}
		else
		{
			// 매달린 채 죽으면 매달림 자세만 풀고 기본 사망 처리를 따른다.
			ReleaseHangPose(0.0f);
			Phase = ECeilingEnemyPhase::Grounded;
		}
	}

	Super::OnHealthChanged(Data);
}

void ACeilingEnemy::ResetToInitial_Implementation()
{
	if (bRootMotionOverridden)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(SavedRootMotionMode);
		}

		bRootMotionOverridden = false;
	}

	// 기본 초기화가 배치 위치, 메시 위치, 이동 모드, 감지 상태를 되돌린다.
	Super::ResetToInitial_Implementation();

	Phase = ECeilingEnemyPhase::Grounded;
	HangMeshOffsetAlpha = 0.0f;
	AutoHangMeshOffset = FVector::ZeroVector;
	AmbushBodyCorrection = FVector::ZeroVector;
	AmbushTurnRemaining = 0.0f;
	bUsingProfiledDescent = false;

	if (bStartOnCeiling)
	{
		EnterHanging();
	}
}

UAnimMontage* ACeilingEnemy::GetAmbushMontage()
{
	if (!AmbushAnimation)
	{
		return nullptr;
	}

	// 몽타주 에셋을 지정했다면 그 설정(블렌드, 노티파이)을 그대로 쓴다.
	if (UAnimMontage* AmbushMontageAsset = Cast<UAnimMontage>(AmbushAnimation))
	{
		return AmbushMontageAsset;
	}

	if (!AmbushDynamicMontage)
	{
		FAlphaBlendArgs BlendInArgs(FMath::Max(0.0f, AmbushBlendInTime));
		BlendInArgs.BlendOption = AmbushBlendOption;

		FAlphaBlendArgs BlendOutArgs(FMath::Max(0.0f, AmbushBlendOutTime));
		BlendOutArgs.BlendOption = AmbushBlendOption;

		AmbushDynamicMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage_WithBlendSettings(
			AmbushAnimation,
			MontageSlotName,
			FMontageBlendSettings(BlendInArgs),
			FMontageBlendSettings(BlendOutArgs)
		);
	}

	return AmbushDynamicMontage;
}

float ACeilingEnemy::GetAmbushStartTime() const
{
	const float MinStartTime = FMath::Max(0.0f, AmbushStartTime);

	if (!bAmbushProfileValid)
	{
		return MinStartTime;
	}

	// 하강 직전 짧은 준비 동작만 남기고 앞부분은 건너뛴다.
	return FMath::Max(MinStartTime, AmbushDescentBeginTime - FMath::Max(0.0f, AmbushMaxWindupTime));
}

void ACeilingEnemy::EnsureAmbushProfile()
{
	if (ProfiledAmbushAnimation == AmbushAnimation)
	{
		return;
	}

	ProfiledAmbushAnimation = AmbushAnimation;
	AmbushRootZSamples.Reset();
	AmbushBodyZSamples.Reset();
	AmbushLockedBodySamples.Reset();
	bAmbushProfileValid = false;

	// 몽타주를 지정했다면 첫 구간의 시퀀스를 기준으로 삼는다.
	const UAnimSequence* Sequence = Cast<UAnimSequence>(AmbushAnimation);

	if (!Sequence)
	{
		const UAnimMontage* Montage = Cast<UAnimMontage>(AmbushAnimation);
		if (Montage && Montage->SlotAnimTracks.Num() > 0 && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0)
		{
			Sequence = Cast<UAnimSequence>(Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference());
		}
	}

	if (!Sequence)
	{
		return;
	}

	// 몸통 뼈에서 루트까지의 부모 체인. 찾지 못하면 몸 높이를 루트 높이로 대신한다.
	TArray<int32> BodyBoneChain;

	if (const USkeleton* Skeleton = Sequence->GetSkeleton())
	{
		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		int32 BoneIndex = RefSkeleton.FindBoneIndex(AmbushBodyBoneName);

		while (BoneIndex != INDEX_NONE)
		{
			BodyBoneChain.Add(BoneIndex);
			BoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
		}
	}

	if (BodyBoneChain.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 기습 애니메이션에서 %s 뼈를 찾지 못해 루트 궤적으로 내려옵니다."),
			*GetName(),
			*AmbushBodyBoneName.ToString());
	}

	// 루트 높이와 몸통 뼈의 컴포넌트 공간 높이를 일정 간격으로 뽑아 둔다.
	const float Length = Sequence->GetPlayLength();
	const int32 NumSamples = FMath::FloorToInt(Length * AmbushSampleRate) + 1;

	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		const double Time = FMath::Min(static_cast<double>(Index) / AmbushSampleRate, static_cast<double>(Length));
		const FAnimExtractContext Context(Time);

		const FTransform RootTransform = Sequence->ExtractRootTrackTransform(Context, nullptr);
		AmbushRootZSamples.Add(static_cast<float>(RootTransform.GetLocation().Z));

		// 자식 → 부모 순서로 로컬 변환을 곱해 컴포넌트 공간 위치를 구한다.
		// 게임에서는 루트가 기준 포즈로 고정되므로, 루트만 기준 포즈로 바꾼 위치도 함께 구한다.
		FTransform BodyTransform = FTransform::Identity;
		FTransform LockedBodyTransform = FTransform::Identity;

		for (const int32 ChainBoneIndex : BodyBoneChain)
		{
			FTransform LocalTransform;
			Sequence->GetBoneTransform(LocalTransform, FSkeletonPoseBoneIndex(ChainBoneIndex), Context, false);
			BodyTransform = BodyTransform * LocalTransform;

			const bool bIsRoot = Sequence->GetSkeleton()->GetReferenceSkeleton().GetParentIndex(ChainBoneIndex) == INDEX_NONE;
			LockedBodyTransform = LockedBodyTransform * (bIsRoot
				? Sequence->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[ChainBoneIndex]
				: LocalTransform);
		}

		AmbushBodyZSamples.Add(BodyBoneChain.Num() > 0
			? static_cast<float>(BodyTransform.GetLocation().Z)
			: AmbushRootZSamples.Last());

		AmbushLockedBodySamples.Add(LockedBodyTransform.GetLocation());
	}

	// 시작 위치 이후에서 가장 낮은 지점(착지)과, 그 전의 가장 높은 지점(하강 시작)을 찾는다.
	const int32 StartIndex = FMath::Clamp(
		FMath::FloorToInt(FMath::Max(0.0f, AmbushStartTime) * AmbushSampleRate),
		0,
		NumSamples - 1
	);

	int32 LowIndex = StartIndex;
	for (int32 Index = StartIndex; Index < NumSamples; ++Index)
	{
		if (AmbushRootZSamples[Index] < AmbushRootZSamples[LowIndex])
		{
			LowIndex = Index;
		}
	}

	int32 TopIndex = StartIndex;
	for (int32 Index = StartIndex; Index <= LowIndex; ++Index)
	{
		if (AmbushRootZSamples[Index] > AmbushRootZSamples[TopIndex])
		{
			TopIndex = Index;
		}
	}

	AmbushRootTopZ = AmbushRootZSamples[TopIndex];
	AmbushRootDrop = AmbushRootTopZ - AmbushRootZSamples[LowIndex];

	if (AmbushRootDrop < 20.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 기습 애니메이션 루트에 하강 구간이 없어(%.1fcm) 빠른 낙하로 대신합니다."),
			*GetName(),
			AmbushRootDrop);
		return;
	}

	// 꼭대기에서 1cm 넘게 내려가기 시작한 지점과 바닥에서 1cm 안으로 들어온 지점을 하강 구간으로 본다.
	int32 BeginIndex = TopIndex;
	while (BeginIndex + 1 <= LowIndex && AmbushRootZSamples[BeginIndex + 1] >= AmbushRootTopZ - 1.0f)
	{
		++BeginIndex;
	}

	int32 EndIndex = BeginIndex;
	while (EndIndex < LowIndex && AmbushRootZSamples[EndIndex] > AmbushRootZSamples[LowIndex] + 1.0f)
	{
		++EndIndex;
	}

	AmbushDescentBeginTime = BeginIndex / AmbushSampleRate;
	AmbushDescentEndTime = FMath::Max(AmbushDescentBeginTime + KINDA_SMALL_NUMBER, EndIndex / AmbushSampleRate);
	bAmbushProfileValid = true;

	UE_LOG(LogTemp, Log, TEXT("%s: 기습 애니메이션 하강 구간 %.2f~%.2f초, 루트 하강 %.0fcm"),
		*GetName(),
		AmbushDescentBeginTime,
		AmbushDescentEndTime,
		AmbushRootDrop);

	// 루트와 몸 궤적 비교용 (0.05초 간격, 시간:루트/몸)
	FString CurveText;
	for (float Time = FMath::Max(0.0f, AmbushDescentBeginTime - 0.2f); Time <= AmbushDescentEndTime + 0.3f; Time += 0.05f)
	{
		CurveText += FString::Printf(TEXT(" %.2f:%.0f/%.0f"),
			Time,
			SampleAmbushCurve(AmbushRootZSamples, Time),
			SampleAmbushCurve(AmbushBodyZSamples, Time));
	}

	UE_LOG(LogTemp, Verbose, TEXT("%s: 기습 궤적(루트/몸)%s"), *GetName(), *CurveText);
}

float ACeilingEnemy::SampleAmbushCurve(const TArray<float>& Samples, float Time) const
{
	if (Samples.Num() == 0)
	{
		return 0.0f;
	}

	const float SamplePosition = FMath::Max(0.0f, Time) * AmbushSampleRate;
	const int32 LastIndex = Samples.Num() - 1;
	const int32 Index0 = FMath::Clamp(FMath::FloorToInt(SamplePosition), 0, LastIndex);
	const int32 Index1 = FMath::Min(Index0 + 1, LastIndex);

	return FMath::Lerp(
		Samples[Index0],
		Samples[Index1],
		FMath::Clamp(SamplePosition - Index0, 0.0f, 1.0f)
	);
}

FVector ACeilingEnemy::GetBodyOffsetActorSpace(float Time) const
{
	if (AmbushLockedBodySamples.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	// 샘플 사이를 보간한 컴포넌트 공간 몸 위치
	const float SamplePosition = FMath::Max(0.0f, Time) * AmbushSampleRate;
	const int32 LastIndex = AmbushLockedBodySamples.Num() - 1;
	const int32 Index0 = FMath::Clamp(FMath::FloorToInt(SamplePosition), 0, LastIndex);
	const int32 Index1 = FMath::Min(Index0 + 1, LastIndex);
	const FVector BodyComponentSpace = FMath::Lerp(
		AmbushLockedBodySamples[Index0],
		AmbushLockedBodySamples[Index1],
		FMath::Clamp(SamplePosition - Index0, 0.0f, 1.0f)
	);

	// 메시 회전·스케일을 거쳐 액터 공간 오프셋으로 바꾼다. 메시 위치 보정은 별도로 다룬다.
	const FTransform MeshRelative(GetMesh()->GetRelativeRotation(), FVector::ZeroVector, GetMesh()->GetRelativeScale3D());
	return MeshRelative.TransformPosition(BodyComponentSpace);
}

float ACeilingEnemy::GetDescentDropAt(float Time) const
{
	// 루트가 고정된 포즈에서 보이는 몸 높이 = 캡슐 + (몸 - 루트).
	// 이 값이 원본 몸 궤적을 배율만큼 따르려면, 캡슐 하강량 = 배율 × 몸 하강량 + (몸 - 루트) 변화량.
	const float BodyStart = SampleAmbushCurve(AmbushBodyZSamples, DescentAnimStartTime);
	const float Body = SampleAmbushCurve(AmbushBodyZSamples, Time);
	const float LocalStart = BodyStart - SampleAmbushCurve(AmbushRootZSamples, DescentAnimStartTime);
	const float Local = Body - SampleAmbushCurve(AmbushRootZSamples, Time);

	return DescentBodyScale * (BodyStart - Body) + (Local - LocalStart);
}

void ACeilingEnemy::PlayHangLoop()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance || !HangAnimation)
	{
		return;
	}

	if (!HangMontage)
	{
		HangMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
			HangAnimation,
			MontageSlotName,
			0.0f,
			FMath::Max(0.0f, DropBlendOutTime)
		);

		if (!HangMontage)
		{
			return;
		}

		// 원본 끝에 닿아도 자동으로 빠지지 않게 한다. 재생 위치는 UpdateHangLoop에서 직접 정한다.
		HangMontage->bEnableAutoBlendOut = false;
	}

	float LoopStart = 0.0f;
	float LoopEnd = 0.0f;
	GetHangLoopRange(LoopStart, LoopEnd);

	AnimInstance->Montage_Play(
		HangMontage,
		1.0f,
		EMontagePlayReturnType::MontageLength,
		LoopStart,
		true
	);

	// 스스로 진행하지 않도록 멈춰 두고, 위치만 직접 옮긴다.
	AnimInstance->Montage_SetPlayRate(HangMontage, 0.0f);
}

void ACeilingEnemy::UpdateHangLoop(float DeltaTime)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance || !HangMontage)
	{
		return;
	}

	// 피격 등 다른 몽타주에 밀려났다면 다시 매달린다.
	if (!AnimInstance->Montage_IsPlaying(HangMontage))
	{
		PlayHangLoop();
		return;
	}

	float LoopStart = 0.0f;
	float LoopEnd = 0.0f;
	GetHangLoopRange(LoopStart, LoopEnd);

	// 구간이 몇 프레임뿐이라 되감기를 반복하면 떨린다. 시작↔끝을 사인 곡선으로 천천히 오간다.
	const float Period = FMath::Max(0.1f, HangLoopPeriod);
	HangLoopElapsed = FMath::Fmod(HangLoopElapsed + DeltaTime, Period);

	const float Phase01 = 0.5f - 0.5f * FMath::Cos(2.0f * PI * HangLoopElapsed / Period);
	AnimInstance->Montage_SetPosition(HangMontage, FMath::Lerp(LoopStart, LoopEnd, Phase01));
}

void ACeilingEnemy::GetHangLoopRange(float& OutStart, float& OutEnd) const
{
	OutStart = 0.0f;
	OutEnd = 0.0f;

	if (!HangAnimation)
	{
		return;
	}

	// 프레임 번호를 원본 애니메이션의 샘플링 프레임 레이트로 초 단위로 바꾼다.
	const FFrameRate FrameRate = HangAnimation->GetSamplingFrameRate();
	const float PlayLength = HangAnimation->GetPlayLength();
	const int32 StartFrame = FMath::Min(HangLoopStartFrame, HangLoopEndFrame);
	const int32 EndFrame = FMath::Max(HangLoopStartFrame, HangLoopEndFrame);

	OutStart = FMath::Clamp(static_cast<float>(FrameRate.AsSeconds(FFrameTime(StartFrame))), 0.0f, PlayLength);
	OutEnd = FMath::Clamp(static_cast<float>(FrameRate.AsSeconds(FFrameTime(EndFrame))), OutStart, PlayLength);
}

void ACeilingEnemy::ApplyHangMeshOffset(float Alpha)
{
	// 매달림 보정(매달림 몽타주 가중치)과 찍기 자세 보정(기습 몽타주 가중치)을 함께 적용한다.
	const FVector HangOffset = HangMeshOffset + AutoHangMeshOffset;
	GetMesh()->SetRelativeLocation(InitialMeshRelativeLocation + HangOffset * Alpha + AmbushBodyCorrection + AppliedMeshYOffset);
}

bool ACeilingEnemy::IsAmbushAbilityActive() const
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(EAbilityInputID::CeilingAmbush);
	return Spec && Spec->IsActive();
}
