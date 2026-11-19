



#include "GAS/Ability/GASwiftStrike.h"
#include "Character/PlayerCharacter.h"
#include "NativeGameplayTags.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Ability/GAAssassinateBase.h"
#include "GAS/Ability/GAWallAttach.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "CollisionShape.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/RootMotionSource.h"
#include "Curves/CurveFloat.h"
#include "TimerManager.h"
#include "Character/Enemy.h"
#include "GAS/StatAttributeSet.h"
#include "GAS/Tags/EclipseGameplayTags.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/AssetManager.h"
#include "Util/EclipseCollisionChannels.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SwiftStrike, "Ability.Attack.SwiftStrike");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SwiftStriking, "State.SwiftStriking");
// 기본공격·쿠나이 등 공격 어빌리티의 공통 부모. 땅에서 질풍참이 이들을 끊고 들어간다.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AttackAbility, "Ability.Attack");

// 잔상이 사라진 뒤 그 텍스처를 다시 쓰기까지 두는 여유 시간
static constexpr double AfterimageReuseMargin = 0.1;
// 돌진 시작 후 첫 잔상을 찍기까지의 시간
static constexpr float AfterimageFirstDelay = 0.02f;
// 직교 캡처라 크기에는 영향이 없고, 캡처 위치가 캐릭터를 가리지 않을 만큼 앞에 두기 위한 거리
static constexpr float AfterimageCaptureDistance = 450.f;
// 잔상이 캐릭터 몸을 뚫지 않도록 캡슐 뒤로 더 미는 여유 거리
static constexpr float AfterimageDepthMargin = 5.f;
// 가로 섬광을 켠 직후 미리 돌리는 틱 수와 한 틱의 시간. 생성 틱 + 크기 적용 틱이면 되고, 수명은 거의 줄지 않게 짧게 둔다.
static constexpr int32 DashSlashPrimeTicks = 2;
static constexpr float DashSlashPrimeTickDelta = 0.001f;

UGASwiftStrike::UGASwiftStrike()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_SwiftStrike);
	SetAssetTags(Tags);

	// 돌진하는 동안 기본공격·쿠나이·암살·불속성이 끼어들지 못하게 공격 중 태그를 가진다
	ActivationOwnedTags.AddTag(TAG_SwiftStriking);
	ActivationOwnedTags.AddTag(UGABaseAttack::GetAttackingTag());
	// 무적은 돌진 구간에서만 별도로 부여하고 해제한다.
	ActivationBlockedTags.AddTag(TAG_SwiftStriking);

	// 땅에서는 우선순위가 가장 높다. 공격 중 태그로 막지 않고, 진행 중인 공격을 끊는다 (ActivateAbility 전에 취소된다).
	// 공중에서 공격 중일 때는 CanActivateAbility에서 막는다. 암살은 끊지 않는다.
	CancelAbilitiesWithTag.AddTag(TAG_AttackAbility);
	ActivationBlockedTags.AddTag(UGAAssassinateBase::GetAssassinatingTag());

	// 벽에 붙음·모서리 매달림·오르기 중에는 쓸 수 없다
	ActivationBlockedTags.AddTag(UGAWallAttach::GetWallStateTag());

	// VFX 에셋은 파생 블루프린트의 Class Defaults에서 지정한다.
}

void UGASwiftStrike::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	if (HasAnyFlags(RF_ClassDefaultObject)) return;

	// 시전할 때 쓰는 에셋을 비동기로 불러 둔다. 다 불러오기 전에 시전하면 기존처럼 그 자리에서 불러온다.
	AssetPreloader.Request({
		SwiftStrikeMontage.ToSoftObjectPath(), AirSwiftStrikeMontage.ToSoftObjectPath(),
		DashSlashEffect.ToSoftObjectPath(), AfterimageEffect.ToSoftObjectPath(),
		AfterimageMaterial.ToSoftObjectPath(), ImpactEffect.ToSoftObjectPath() },
		FStreamableDelegate::CreateUObject(this, &UGASwiftStrike::OnAssetsPreloaded));
}

void UGASwiftStrike::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	AssetPreloader.Cancel();
	if (IsValid(AfterimageCapture))
	{
		AfterimageCapture->DestroyComponent();
	}
	AfterimageCapture = nullptr;

	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UGASwiftStrike::OnAssetsPreloaded()
{
	LoadedAfterimageEffect = AfterimageEffect.Get();
	LoadedImpactEffect = ImpactEffect.Get();

	// 잔상 텍스처도 미리 만들어 첫 돌진에서 생성 비용이 들지 않게 한다.
	if (UMaterialInterface* Material = AfterimageMaterial.Get())
	{
		EnsureAfterimagePool(Material);
	}
}

bool UGASwiftStrike::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	if (!ActorInfo) return false;

	const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(Character) || !IsValid(ASC)) return false;

	//// 공중에서는 다른 공격을 끊지 않는다
	//const bool bOnGround = Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsMovingOnGround();
	//if (!bOnGround && ASC->HasMatchingGameplayTag(UGABaseAttack::GetAttackingTag())) return false;

	return true;
}

void UGASwiftStrike::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bDashFinished = false;
	bMontageFinished = true;
	bUsingAirMontage = false;

	FHitResult Hit;

	ACharacter* Player = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!IsValid(Player))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	if (!IsValid(Capsule))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float halfHeight = Capsule->GetScaledCapsuleHalfHeight();

	FVector Start = Player->GetActorLocation();
	FVector Dir = Player->GetActorForwardVector();
	FVector End = Start + Dir * DashDistance;
	FCollisionShape Shape = FCollisionShape::MakeCapsule(FMath::Max(1.f, Radius - 2.f), FMath::Max(1.f, halfHeight - 2.f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SwiftStrike), false, Player);
	// 시작할 때 살짝 겹쳐 있어도 제자리에서 멈추지 않게 한다. 겹침은 이동 중에 캐릭터 무브먼트가 풀어 준다.
	Params.bFindInitialOverlaps = false;

	// 실제 이동과 같은 기준으로 막히는 지점을 찾는다. 투명벽처럼 Visibility는 무시하고 Pawn만 막는 물체도 반영된다.
	const ECollisionChannel MoveChannel = Capsule->GetCollisionObjectType();
	FCollisionResponseParams WorldResponse(Capsule->GetCollisionResponseToChannels());
	WorldResponse.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);

	// 지상에서는 무브먼트가 올라갈 수 있는 턱을 사전 스윕이 벽으로 잘라내지 않게 한다.
	// 캡슐 윗부분은 유지하고 아래쪽만 줄인다. 실제 이동 충돌과 StepUp 검사는 그대로 수행된다.
	FCollisionShape PathShape = Shape;
	FVector PathOffset = FVector::ZeroVector;
	const UCharacterMovementComponent* PathMovement = Player->GetCharacterMovement();
	if (IsValid(PathMovement) && PathMovement->IsMovingOnGround())
	{
		const float StepClearance = FMath::Clamp(PathMovement->MaxStepHeight, 0.0f,
			2.0f * (Shape.GetCapsuleHalfHeight() - Shape.GetCapsuleRadius()));
		PathOffset.Z = StepClearance * 0.5f;
		PathShape = FCollisionShape::MakeCapsule(Shape.GetCapsuleRadius(), Shape.GetCapsuleHalfHeight() - PathOffset.Z);
	}

	if (GetWorld()->SweepSingleByChannel(Hit, Start + PathOffset, End + PathOffset,
		FQuat::Identity, MoveChannel, PathShape, Params, WorldResponse))
	{
		End = Hit.Location - PathOffset;
	}

	End = ResolveDashEnd(Start, End, Dir, Shape, MoveChannel, Params, WorldResponse, Capsule->GetCollisionResponseToChannel(ECC_Pawn));

	const float ActualDistance = FVector::Dist2D(Start, End);
	const float MoveDuration = FMath::Max(0.03f, Duration * ActualDistance / DashDistance);

	AttackTargets.Reset();
	CollectTargets(Player, Start, End, Shape);
	DrawDebugHitbox(Start, End, Shape);

	SavedPawnCollisionResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	bDashCollisionChanged = true;

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(SourceASC))
	{
		SourceASC->AddLooseGameplayTag(EclipseGameplayTags::State_Invincible);
		bDashInvincibilityAdded = true;
	}

	// 짧은 돌진이 먼저 끝나더라도 몽타주 완료를 기다릴 수 있도록 먼저 재생한다.
	TSoftObjectPtr<UAnimMontage> SelectedMontage = SwiftStrikeMontage;
	const UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	if (IsValid(Movement) && Movement->IsFalling())
	{
		if (!AirSwiftStrikeMontage.IsNull())
		{
			SelectedMontage = AirSwiftStrikeMontage;
			bUsingAirMontage = true;
		}
	}

	LoadedSwiftStrikeMontage = SelectedMontage.LoadSynchronous();
	if (IsValid(LoadedSwiftStrikeMontage))
	{
		const FName DashSection(TEXT("Dash"));
		const FName RecoverySection(TEXT("Recovery"));
		const int32 DashSectionIndex = LoadedSwiftStrikeMontage->GetSectionIndex(DashSection);
		FName StartSection = NAME_None;
		float MontagePlayRate = 2.0f;
		if (DashSectionIndex != INDEX_NONE
			&& LoadedSwiftStrikeMontage->GetSectionIndex(RecoverySection) != INDEX_NONE)
		{
			StartSection = DashSection;

			// 돌진 구간을 되감지 않고 실제 이동 시간에 맞춰 한 번만 재생한다.
			const float DashSectionLength = LoadedSwiftStrikeMontage->GetSectionLength(DashSectionIndex);
			const float MontageRateScale = LoadedSwiftStrikeMontage->RateScale;
			if (DashSectionLength > KINDA_SMALL_NUMBER && MontageRateScale > KINDA_SMALL_NUMBER)
			{
				MontagePlayRate = DashSectionLength / (MoveDuration * MontageRateScale);
			}
		}

		bMontageFinished = false;
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, LoadedSwiftStrikeMontage, MontagePlayRate, StartSection, true);
		MontageTask->OnCompleted.AddDynamic(this, &UGASwiftStrike::OnSwiftStrikeMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &UGASwiftStrike::OnSwiftStrikeInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UGASwiftStrike::OnSwiftStrikeInterrupted);
		MontageTask->ReadyForActivation();

		// 에셋에 반복 연결이 있어도 돌진 포즈에서 마무리 포즈로 한 번만 이어진다.
		if (IsActive() && StartSection == DashSection)
		{
			UAnimInstance* Animation = ActorInfo->GetAnimInstance();
			if (IsValid(Animation))
			{
				Animation->Montage_SetNextSection(DashSection, RecoverySection, LoadedSwiftStrikeMontage);
				Animation->Montage_SetNextSection(RecoverySection, NAME_None, LoadedSwiftStrikeMontage);
			}
		}
	}

	// 몽타주 재생 실패로 취소되었다면 이동을 시작하지 않는다.
	if (!IsActive()) return;
	if (const APlayerCharacter* Ninja = Cast<APlayerCharacter>(Player))
	{
		Ninja->PlayActionSound(EPlayerActionSound::SwiftStrike);
	}

	UCharacterMovementComponent* DashMovement = Player->GetCharacterMovement();
	if (!IsValid(DashMovement))
	{
		OnSwiftStrikeFinished();
		return;
	}

	// 등속 대신 감속 곡선으로 이동해 출발 직후 대부분을 가고 끝에서 급하게 멈춘다.
	TSharedPtr<FRootMotionSource_MoveToDynamicForce> DashForce = MakeShared<FRootMotionSource_MoveToDynamicForce>();
	DashForce->InstanceName = TEXT("SwiftStrike");
	DashForce->AccumulateMode = ERootMotionAccumulateMode::Override;
	DashForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
	DashForce->Priority = 900;
	DashForce->StartLocation = Start;
	DashForce->InitialTargetLocation = End;
	DashForce->TargetLocation = End;
	DashForce->Duration = MoveDuration;
	DashForce->bRestrictSpeedToExpected = false;
	DashForce->TimeMappingCurve = GetDashTimeCurve();
	DashForce->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	DashDirection = (End - Start).GetSafeNormal2D();
	DashForce->FinishVelocityParams.SetVelocity = DashDirection * DashEndSpeed;
	DashRootMotionSourceID = DashMovement->ApplyRootMotionSource(DashForce);

	StartDashEffects(Player, MoveDuration);

	GetWorld()->GetTimerManager().SetTimer(DashTimerHandle, this, &UGASwiftStrike::OnSwiftStrikeFinished, MoveDuration, false);
}

void UGASwiftStrike::StartDashEffects(ACharacter* Player, float DashDuration)
{
	StopDashEffects();
	if (!IsValid(Player)) return;
	DashSlashStart = Player->GetActorLocation();

	// 실제 이동 방향이 정해진 뒤 생성해 회전 중간 각도가 이펙트에 남지 않게 한다.
	const FRotator FacingRotation = DashDirection.Rotation();
	const FQuat WindRotation = FacingRotation.Quaternion() * DashWindRotation.Quaternion();
	UNiagaraSystem* WindSystem = DashSlashEffect.LoadSynchronous();
	if (IsValid(WindSystem))
	{
		// 캐릭터에 붙이지 않고 시작점과 현재 위치 사이의 중앙에 배치한다.
		ActiveDashSlash = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Player, WindSystem, DashSlashStart, WindRotation.Rotator(),
			FVector::OneVector, true, false, ENCPoolMethod::None, false);
		if (IsValid(ActiveDashSlash))
		{
			ActiveDashSlash->SetTickBehavior(ENiagaraTickBehavior::ForceTickLast);
			ActiveDashSlash->SetVariableLinearColor(TEXT("User.Color"), DashWindColor);
			ActiveDashSlash->SetVariableVec3(TEXT("User.TintScale"), FVector(DashWindColor.R, DashWindColor.G, DashWindColor.B));
			ActiveDashSlash->SetVariableFloat(TEXT("User.Opacity"), DashWindColor.A);
			ActiveDashSlash->SetVariableFloat(TEXT("User.Lifetime"), DashDuration);
			ActiveDashSlash->SetVariableFloat(TEXT("User.Rotation"), 0.f);
			UpdateDashSlash();
			ActiveDashSlash->Activate(true);
			// Stab 이미터는 생성한 틱에 업데이트 스크립트를 건너뛰어, User.MeshScale이 적용되기 전 원본 크기(약 12m)로 한 번 그려진다.
			// 생성 틱과 크기를 적용하는 틱을 바로 돌려, 처음 그려지는 프레임부터 실제 돌진 길이로 보이게 한다.
			ActiveDashSlash->AdvanceSimulation(DashSlashPrimeTicks, DashSlashPrimeTickDelta);

			// 감속 재생 중에도 끝점이 뒤처지지 않도록 매 프레임 한 번만 갱신한다.
			FTimerManagerTimerParameters UpdateParameters;
			UpdateParameters.bLoop = true;
			UpdateParameters.bMaxOncePerFrame = true;
			GetWorld()->GetTimerManager().SetTimer(DashSlashTimerHandle, this, &UGASwiftStrike::UpdateDashSlash,
				UE_KINDA_SMALL_NUMBER, UpdateParameters);
		}
	}

	if (!IsValid(LoadedImpactEffect))
	{
		LoadedImpactEffect = ImpactEffect.LoadSynchronous();
	}
	StartAfterimages(Player);
}

void UGASwiftStrike::UpdateDashSlash()
{
	if (!IsValid(ActiveDashSlash)) return;

	const AActor* Player = GetAvatarActorFromActorInfo();
	if (!IsValid(Player)) return;

	const FVector Current = Player->GetActorLocation();
	const FVector Offset = DashDirection.Rotation().RotateVector(DashSlashOffset);
	const FVector ForwardOffset = DashDirection.GetSafeNormal2D() * DashSlashForwardOffset;
	const float Length = FVector::Distance(DashSlashStart, Current);

	// 기존 길이와 높이를 유지하고, 선 전체를 돌진 방향 앞쪽으로만 이동한다.
	ActiveDashSlash->SetWorldLocation((DashSlashStart + Current) * 0.5f + Offset + ForwardOffset);
	ActiveDashSlash->SetVariableFloat(TEXT("User.PathLength"), Length);
	// 원본 섬광의 기준 폭 1500cm에 대한 배율이며, 두께는 독립적으로 유지한다.
	ActiveDashSlash->SetVariableVec2(TEXT("User.PathScale"), FVector2D(Length / 1500.f, DashWindScale));

	// OneStab 메시의 원본 Y축 길이(600cm × 초기 배율 2)를 실제 경로 길이에 맞춘다.
	ActiveDashSlash->SetVariableVec3(TEXT("User.MeshScale"), FVector(1.f, Length / 1200.f, DashWindScale));
	ActiveDashSlash->SetVariableVec3(TEXT("User.LocalStart"), FVector(0.f, -Length * 0.5f, 0.f));
	ActiveDashSlash->SetVariableVec3(TEXT("User.LocalEnd"), FVector(0.f, Length * 0.5f, 0.f));
	ActiveDashSlash->SetVariableVec2(TEXT("User.SpriteScale"), FVector2D(DashWindScale, DashWindScale));
	ActiveDashSlash->SetVariableFloat(TEXT("User.Thickness"), 25.f * DashWindScale);
}

int32 UGASwiftStrike::GetAfterimageSlotCount() const
{
	// 한 장은 수명 + 여유 시간 동안 다시 쓸 수 없으므로, 그동안 찍히는 장수에 한 장을 더 둔다.
	const double Interval = FMath::Max(0.03f, AfterimageInterval);
	return FMath::CeilToInt32((AfterimageLifetime + AfterimageReuseMargin) / Interval) + 1;
}

void UGASwiftStrike::EnsureAfterimagePool(UMaterialInterface* Material)
{
	if (!IsValid(Material)) return;

	// 캡처 텍스처는 재사용하되, 아직 보이는 잔상은 덮어쓰지 않는다. 간격·수명이 바뀌어 더 필요해지면 모자란 만큼만 만든다.
	const int32 SlotCount = GetAfterimageSlotCount();
	while (AfterimageTextures.Num() < SlotCount)
	{
		UTextureRenderTarget2D* Texture = NewObject<UTextureRenderTarget2D>(this);
		Texture->RenderTargetFormat = RTF_RGBA16f;
		Texture->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
		Texture->InitAutoFormat(AfterimageResolution, AfterimageResolution);
		Texture->UpdateResourceImmediate(true);
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
		Instance->SetTextureParameterValue(TEXT("PoseTexture"), Texture);
		AfterimageTextures.Add(Texture);
		AfterimageMaterials.Add(Instance);
		AfterimageReuseTimes.Add(0.0);
	}
}

void UGASwiftStrike::EnsureAfterimageCapture(ACharacter* Player)
{
	if (IsValid(AfterimageCapture) && AfterimageCapture->GetOwner() == Player) return;
	if (IsValid(AfterimageCapture))
	{
		AfterimageCapture->DestroyComponent();
	}

	AfterimageCapture = NewObject<USceneCaptureComponent2D>(Player);
	AfterimageCapture->bCaptureEveryFrame = false;
	AfterimageCapture->bCaptureOnMovement = false;
	AfterimageCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	AfterimageCapture->OrthoWidth = AfterimageSize;
	AfterimageCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	AfterimageCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

	// 잔상은 색을 입힌 실루엣이라 그림자·안개·간접광·반사가 필요 없다. 캡처마다 렌더링하지 않도록 끈다.
	FEngineShowFlags& Flags = AfterimageCapture->ShowFlags;
	Flags.SetDynamicShadows(false);
	Flags.SetFog(false);
	Flags.SetVolumetricFog(false);
	Flags.SetAtmosphere(false);
	Flags.SetAmbientOcclusion(false);
	Flags.SetScreenSpaceReflections(false);
	Flags.SetLumenGlobalIllumination(false);
	Flags.SetLumenReflections(false);
	Flags.SetBloom(false);
	Flags.SetMotionBlur(false);
	Flags.SetEyeAdaptation(false);
	Flags.SetDepthOfField(false);

	AfterimageCapture->RegisterComponent();
}

void UGASwiftStrike::StartAfterimages(ACharacter* Player)
{
	// 미리 불러 두지 못했으면 여기서 불러온다.
	if (!IsValid(LoadedAfterimageEffect))
	{
		LoadedAfterimageEffect = AfterimageEffect.LoadSynchronous();
	}
	if (!IsValid(LoadedAfterimageEffect)) return;
	UMaterialInterface* Material = AfterimageMaterial.LoadSynchronous();
	if (!IsValid(Material)) return;

	EnsureAfterimagePool(Material);
	EnsureAfterimageCapture(Player);

	// 무기 교체 등으로 붙은 부품이 바뀔 수 있으므로 캡처 대상은 돌진마다 다시 정한다.
	AfterimageCapture->ClearShowOnlyComponents();
	AfterimageCapture->ShowOnlyComponent(Player->GetMesh());
	// 무기처럼 캐릭터 메시 아래에 붙은 부품도 같은 자세로 캡처한다.
	TArray<USceneComponent*> Children;
	Player->GetMesh()->GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children)
	{
		UMeshComponent* Part = Cast<UMeshComponent>(Child);
		if (!IsValid(Part)) continue;
		AfterimageCapture->ShowOnlyComponent(Part);
	}
	GetWorld()->GetTimerManager().SetTimer(AfterimageTimerHandle, this, &UGASwiftStrike::EmitAfterimage,
		FMath::Max(0.03f, AfterimageInterval), true, AfterimageFirstDelay);
}

void UGASwiftStrike::EmitAfterimage()
{
	if (!IsValid(AfterimageCapture)) return;
	if (!IsValid(LoadedAfterimageEffect)) return;
	ACharacter* Player = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Player)) return;

	// 이 캐릭터를 조종하는 플레이어의 카메라를 기준으로 찍는다.
	const APlayerController* Controller = Cast<APlayerController>(Player->GetController());
	const APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
	if (!IsValid(Camera)) return;
	const double Now = GetWorld()->GetTimeSeconds();
	int32 Slot = INDEX_NONE;
	for (int32 Index = 0; Index < AfterimageReuseTimes.Num(); ++Index)
	{
		if (AfterimageReuseTimes[Index] > Now) continue;
		Slot = Index;
		break;
	}
	if (Slot == INDEX_NONE) return;

	const FVector Center = Player->GetActorLocation();
	const FRotator Rotation = Camera->GetCameraRotation();
	const FVector ViewDirection = Rotation.Vector();
	AfterimageCapture->SetWorldLocationAndRotation(Center - ViewDirection * AfterimageCaptureDistance, Rotation);
	AfterimageCapture->TextureTarget = AfterimageTextures[Slot];
	AfterimageCapture->CaptureScene();

	// 잔상 판이 몸을 뚫고 보이지 않도록 캡슐 뒤까지 민다.
	// 원근 카메라에서 뒤로 밀면 작아지고 화면 중앙 쪽으로 어긋나므로, 카메라에서 캐릭터를 지나는 선 위에 두고 그만큼 키운다.
	const float Depth = Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + AfterimageDepthMargin;
	FVector GhostLocation = Center + ViewDirection * Depth;
	float GhostSize = AfterimageSize;
	const FVector CameraLocation = Camera->GetCameraLocation();
	const float CenterDistance = FVector::DotProduct(Center - CameraLocation, ViewDirection);
	if (!Camera->IsOrthographic() && CenterDistance > KINDA_SMALL_NUMBER)
	{
		const float Scale = (CenterDistance + Depth) / CenterDistance;
		GhostLocation = CameraLocation + (Center - CameraLocation) * Scale;
		GhostSize *= Scale;
	}

	// 횡스크롤 카메라를 향한 자세 스프라이트를 캡처 당시의 월드 위치에 남긴다. 자주 생성하므로 풀에서 재사용한다.
	UNiagaraComponent* Ghost = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Player, LoadedAfterimageEffect,
		GhostLocation, FRotator::ZeroRotator, FVector::OneVector, true, false, ENCPoolMethod::AutoRelease, false);
	if (!IsValid(Ghost)) return;
	Ghost->SetVariableMaterial(TEXT("User.AfterimageMaterial"), AfterimageMaterials[Slot]);
	Ghost->SetVariableFloat(TEXT("User.SnapshotSize"), GhostSize);
	Ghost->SetVariableFloat(TEXT("User.Lifetime"), AfterimageLifetime);
	Ghost->SetVariableFloat(TEXT("User.Opacity"), AfterimageOpacity);
	Ghost->SetVariableLinearColor(TEXT("User.TintColor"), AfterimageColor);
	Ghost->Activate(true);
	AfterimageReuseTimes[Slot] = Now + AfterimageLifetime + AfterimageReuseMargin;
}

void UGASwiftStrike::StopDashEffects()
{
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(AfterimageTimerHandle);
		World->GetTimerManager().ClearTimer(DashSlashTimerHandle);
	}
	// 캡처 컴포넌트는 다음 돌진에서 재사용한다. 자동 캡처가 꺼져 있어 남겨 둬도 렌더링하지 않는다.

	// 가로 섬광은 돌진이 끝나거나 취소된 순간에 제거한다. 잔상은 각자의 수명대로 사라진다.
	if (IsValid(ActiveDashSlash))
	{
		ActiveDashSlash->DestroyComponent();
	}
	ActiveDashSlash = nullptr;
}

void UGASwiftStrike::PlayImpactEffect(const FVector& Location) const
{
	if (!IsValid(LoadedImpactEffect)) return;

	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetAvatarActorFromActorInfo(), LoadedImpactEffect, Location, DashDirection.Rotation(),
		FVector(ImpactScale), true, false, ENCPoolMethod::AutoRelease, false);
	if (!IsValid(Effect)) return;

	Effect->SetVariableLinearColor(TEXT("User.Color"), ImpactColor);
	Effect->SetVariableFloat(TEXT("User.Lifetime"), ImpactLifetime);
	Effect->SetVariableFloat(TEXT("User.Color_Hue_Start"), ImpactHueShiftStart);
	Effect->SetVariableFloat(TEXT("User.Color_Hue_End"), ImpactHueShiftEnd);
	Effect->SetVariableFloat(TEXT("User.Color_Hue_RandomMin"), FMath::Min(ImpactHueRandomMin, ImpactHueRandomMax));
	Effect->SetVariableFloat(TEXT("User.Color_Hue_RandomMax"), FMath::Max(ImpactHueRandomMin, ImpactHueRandomMax));
	Effect->Activate(true);
}

FVector UGASwiftStrike::ResolveDashEnd(const FVector& Start, const FVector& End, const FVector& Dir, const FCollisionShape& Shape,
	ECollisionChannel MoveChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& WorldResponse, ECollisionResponse PawnResponse) const
{
	UWorld* World = GetWorld();
	if (!World || PawnResponse != ECR_Block) return End;

	// 돌진이 끝나 Pawn 충돌이 돌아왔을 때 막히는 적과 겹치는지만 검사한다.
	FCollisionResponseParams PawnOnly(ECR_Ignore);
	PawnOnly.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);
	auto IsBlockedByPawn = [&](const FVector& Location)
	{
		return World->OverlapBlockingTestByChannel(Location, FQuat::Identity, MoveChannel, Shape, Params, PawnOnly);
	};
	if (!IsBlockedByPawn(End)) return End;

	// 적 뒤에 빈 공간이 있으면 관통해서 그 뒤에 선다. 벽에 막히면 그 앞까지만 본다.
	const FVector Forward = Dir.GetSafeNormal2D();
	FVector ForwardLimit = End + Forward * MaxPassThroughDistance;
	FHitResult WallHit;
	if (World->SweepSingleByChannel(WallHit, End, ForwardLimit, FQuat::Identity, MoveChannel, Shape, Params, WorldResponse))
	{
		ForwardLimit = WallHit.Location;
	}
	const float ForwardRoom = FVector::Dist2D(End, ForwardLimit);
	for (float Step = EndSearchStep; Step <= ForwardRoom; Step += EndSearchStep)
	{
		const FVector Candidate = End + Forward * Step;
		if (!IsBlockedByPawn(Candidate)) return Candidate;
	}

	// 빈 곳이 없으면 원래 끝점에서 적 사이에 선다. 겹침은 캐릭터 무브먼트가 밀어서 푼다.
	return End;
}

UCurveFloat* UGASwiftStrike::GetDashTimeCurve()
{
	if (DashTimeCurve) return DashTimeCurve;

	// 지수가 바뀌었을 때만 1 - (1 - t)^n 감속 곡선을 다시 만든다.
	if (!GeneratedDashTimeCurve || !FMath::IsNearlyEqual(GeneratedDashEaseExponent, DashEaseExponent))
	{
		GeneratedDashTimeCurve = NewObject<UCurveFloat>(this);
		constexpr int32 KeyCount = 16;
		for (int32 Index = 0; Index <= KeyCount; ++Index)
		{
			const float Time = float(Index) / KeyCount;
			const FKeyHandle Key = GeneratedDashTimeCurve->FloatCurve.AddKey(Time, 1.f - FMath::Pow(1.f - Time, DashEaseExponent));
			GeneratedDashTimeCurve->FloatCurve.SetKeyInterpMode(Key, RCIM_Cubic);
		}
		GeneratedDashEaseExponent = DashEaseExponent;
	}
	return GeneratedDashTimeCurve;
}

void UGASwiftStrike::StopDashRootMotion()
{
	GetWorld()->GetTimerManager().ClearTimer(DashTimerHandle);
	if (DashRootMotionSourceID == 0) return;

	const ACharacter* Player = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* Movement = IsValid(Player) ? Player->GetCharacterMovement() : nullptr;
	if (IsValid(Movement))
	{
		Movement->RemoveRootMotionSourceByID(DashRootMotionSourceID);
		// 끝 속도를 남겨 미끄러지며 서게 한다. 공중에서는 낙하 속도를 새로 시작한다.
		Movement->Velocity = DashDirection * DashEndSpeed;
	}
	DashRootMotionSourceID = 0;
}

void UGASwiftStrike::StartHitStop()
{
	if (HitStopDuration <= 0.f || AttackTargets.Num() == 0) return;

	EndHitStop();
	HitStopActors.Add(GetAvatarActorFromActorInfo());
	for (const FHitResult& Hit : AttackTargets)
	{
		HitStopActors.Add(Hit.GetActor());
	}
	for (const TWeakObjectPtr<AActor>& Actor : HitStopActors)
	{
		if (Actor.IsValid()) Actor->CustomTimeDilation = HitStopTimeDilation;
	}
	// 월드 시간은 느려지지 않으므로 지정한 실제 시간 뒤에 풀린다.
	// EndAbility가 어빌리티에 묶인 타이머를 모두 지우므로, 히트스톱 중에 어빌리티가 끝나도 풀리도록 플레이어에 묶는다.
	TWeakObjectPtr<UGASwiftStrike> WeakThis(this);
	const FTimerDelegate Restore = FTimerDelegate::CreateWeakLambda(GetAvatarActorFromActorInfo(), [WeakThis]()
	{
		if (WeakThis.IsValid()) WeakThis->EndHitStop();
	});
	GetWorld()->GetTimerManager().SetTimer(HitStopTimerHandle, Restore, HitStopDuration, false);
}

void UGASwiftStrike::EndHitStop()
{
	GetWorld()->GetTimerManager().ClearTimer(HitStopTimerHandle);
	for (const TWeakObjectPtr<AActor>& Actor : HitStopActors)
	{
		if (Actor.IsValid()) Actor->CustomTimeDilation = 1.f;
	}
	HitStopActors.Reset();
}

void UGASwiftStrike::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopDashEffects();
	StopDashRootMotion();
	RestoreDashState();
	AttackTargets.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGASwiftStrike::OnSwiftStrikeFinished()
{
	if (!IsActive()) return;
	if (bDashFinished) return;

	bDashFinished = true;
	StopDashEffects();
	StopDashRootMotion();
	RestoreDashState();

	ApplyHitToTargets();
	StartHitStop();
	AttackTargets.Reset();

	if (!IsActive()) return;
	if (bMontageFinished)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
		return;
	}

	// Recovery가 없는 기존 몽타주는 끝까지 재생하고, 있으면 마무리 구간으로 넘긴다.
	if (!IsValid(LoadedSwiftStrikeMontage)) return;
	const FName RecoverySection(TEXT("Recovery"));
	if (LoadedSwiftStrikeMontage->GetSectionIndex(RecoverySection) == INDEX_NONE) return;

	UAnimInstance* Animation = GetCurrentActorInfo()->GetAnimInstance();
	if (!IsValid(Animation)) return;

	Animation->Montage_SetNextSection(RecoverySection, NAME_None, LoadedSwiftStrikeMontage);
	float RecoveryPlayRate = 2.0f;
	if (bUsingAirMontage)
	{
		// 공중에서는 짧게 포즈를 마무리하고 낙하 애니메이션으로 돌아간다.
		RecoveryPlayRate = 6.0f;
	}
	Animation->Montage_SetPlayRate(LoadedSwiftStrikeMontage, RecoveryPlayRate);
	// 이미 마무리 동작에 들어갔다면 시작 프레임으로 다시 되감지 않는다.
	if (Animation->Montage_GetCurrentSection(LoadedSwiftStrikeMontage) != RecoverySection)
	{
		Animation->Montage_JumpToSection(RecoverySection, LoadedSwiftStrikeMontage);
	}
}

void UGASwiftStrike::OnSwiftStrikeMontageCompleted()
{
	if (!IsActive()) return;

	bMontageFinished = true;
	// 몽타주가 먼저 끝나더라도 아직 진행 중인 돌진을 끊지 않는다.
	if (!bDashFinished) return;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGASwiftStrike::RestoreDashState()
{
	if (bDashInvincibilityAdded)
	{
		bDashInvincibilityAdded = false;
		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		if (IsValid(SourceASC))
		{
			// 이 어빌리티가 더한 개수만 제거해 다른 무적 효과는 유지한다.
			SourceASC->RemoveLooseGameplayTag(EclipseGameplayTags::State_Invincible);
		}

		// 무적 중에 겹친 함정은 다시 들어오기 전까지 시작 오버랩이 오지 않으므로, 끝나는 시점에 메시 오버랩을 새로 잡는다.
		// 함정이 안전 지점으로 옮기며 어빌리티를 취소할 수 있으므로, 종료 처리 중이 아닌 다음 틱에 갱신한다.
		if (ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Avatar, [Avatar]()
			{
				USkeletalMeshComponent* Mesh = Avatar->GetMesh();
				if (!IsValid(Mesh) || !Mesh->GetGenerateOverlapEvents()) return;
				// 오버랩 이벤트를 끈 채 갱신하면 기존 오버랩이 모두 끝나고, 다시 켜서 갱신하면 겹친 대상에 시작 오버랩이 다시 온다.
				Mesh->SetGenerateOverlapEvents(false);
				Mesh->UpdateOverlaps();
				Mesh->SetGenerateOverlapEvents(true);
				Mesh->UpdateOverlaps();
			}));
		}
	}

	if (!bDashCollisionChanged) return;
	bDashCollisionChanged = false;

	ACharacter* Player = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Player)) return;

	UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	if (!IsValid(Capsule)) return;

	Capsule->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnCollisionResponse);
}

void UGASwiftStrike::OnSwiftStrikeInterrupted()
{
	if (!IsActive()) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
}

void UGASwiftStrike::CollectTargets(const ACharacter* Player, const FVector& Start, const FVector& End, const FCollisionShape& Shape)
{
	if (!Player) return;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SwiftStrike), false, Player);
	FCollisionObjectQueryParams ObjectParam(ECC_Pawn);
	// 몬스터 캡슐은 Monster 채널을 쓴다
	ObjectParam.AddObjectTypesToQuery(ECC_Monster);

	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectParam, Shape, Params);

	TSet<const AActor*> Seen;
	for (const FHitResult& Hit : Hits)
	{
		const AEnemy* Enemy = Cast<AEnemy>(Hit.GetActor());
		if (!IsValid(Enemy) || Enemy->IsDead() || Seen.Contains(Enemy)) continue;

		AttackTargets.Add(Hit);
		Seen.Add(Enemy);
	}
}

void UGASwiftStrike::ApplyHitToTargets()
{
	for (const FHitResult Hit : AttackTargets)
	{
		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Hit.GetActor());
		if (!SourceASC || !TargetASC || !TargetASC->GetSet<UStatAttributeSet>()
			|| TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddHitResult(Hit);

		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Context);
		if (Spec.IsValid())
		{
			// 피해 처리로 적이 제거되기 전에 이펙트 위치를 저장한다.
			FVector ImpactLocation = Hit.ImpactPoint;
			if (Hit.bStartPenetrating)
			{
				ImpactLocation = Hit.GetActor()->GetActorLocation();
			}

			Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), Damage);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			PlayImpactEffect(ImpactLocation);
		}
	}

}

void UGASwiftStrike::DrawDebugHitbox(const FVector& Start, const FVector& End, const FCollisionShape& Shape) const
{
	if (!bDrawDebugHitbox) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const float Radius = Shape.GetCapsuleRadius();
	const float HalfHeight = Shape.GetCapsuleHalfHeight();
	// 맞은 적이 있으면 빨강, 없으면 초록
	const FColor Color = AttackTargets.Num() > 0 ? FColor::Red : FColor::Green;

	DrawDebugCapsule(World, Start, HalfHeight, Radius, FQuat::Identity, FColor::White, false, DebugDrawTime, 0, 1.f);
	DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, Color, false, DebugDrawTime, 0, 1.5f);

	// 캡슐이 쓸고 지나간 영역의 좌우·위아래 외곽선
	const FVector Dir = (End - Start).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir) * Radius;
	const FVector Up = FVector::UpVector * HalfHeight;
	DrawDebugLine(World, Start + Side, End + Side, Color, false, DebugDrawTime, 0, 1.5f);
	DrawDebugLine(World, Start - Side, End - Side, Color, false, DebugDrawTime, 0, 1.5f);
	DrawDebugLine(World, Start + Up, End + Up, Color, false, DebugDrawTime, 0, 1.5f);
	DrawDebugLine(World, Start - Up, End - Up, Color, false, DebugDrawTime, 0, 1.5f);

	for (const FHitResult& Hit : AttackTargets)
	{
		DrawDebugPoint(World, Hit.ImpactPoint, 14.f, FColor::Red, false, DebugDrawTime);
		if (const AActor* Target = Hit.GetActor())
		{
			DrawDebugString(World, Target->GetActorLocation() + FVector(0.f, 0.f, HalfHeight + 20.f),
				TEXT("SwiftStrike Hit"), nullptr, FColor::Red, DebugDrawTime, true);
		}
	}
}

FGameplayTag UGASwiftStrike::GetSwiftStrikeAbilityTag()
{
	return TAG_SwiftStrike;
}

FGameplayTag UGASwiftStrike::GetSwiftStrikingTag()
{
	return TAG_SwiftStriking;
}
