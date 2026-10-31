#include "Character/BossAttackTelegraph.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

// 엔진 기본 평면 메시는 XY 평면 위 100cm 정사각형이다.
static constexpr float TelegraphPlaneSize = 100.0f;

ABossAttackTelegraph::ABossAttackTelegraph()
{
	PrimaryActorTick.bCanEverTick = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		Mesh->SetStaticMesh(PlaneMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterial(
		TEXT("/Game/VFX/BossTelegraph/M_BossAttackTelegraph.M_BossAttackTelegraph")
	);
	if (DefaultMaterial.Succeeded())
	{
		TelegraphMaterial = DefaultMaterial.Object;
	}
}

void ABossAttackTelegraph::BeginPlay()
{
	Super::BeginPlay();

	if (TelegraphMaterial)
	{
		TelegraphMID = Mesh->CreateDynamicMaterialInstance(0, TelegraphMaterial);
	}

	if (TelegraphMID)
	{
		TelegraphMID->SetVectorParameterValue(TEXT("Color"), Color);
	}

	if (MaxLifeTime > 0.0f)
	{
		SetLifeSpan(MaxLifeTime);
	}

	Alpha = FadeInTime > 0.0f ? 0.0f : 1.0f;
	ApplyAlpha();
}

void ABossAttackTelegraph::Show(AActor* Boss, const FBossTelegraphArea& Area, bool bFollowBoss)
{
	if (!Boss)
	{
		return;
	}

	// 액터는 보스와 같은 변환(스케일 포함)에 두고, 사각형은 보스 기준 상대 위치로 놓는다.
	// 그래서 판정 영역처럼 위치와 크기 모두 보스 스케일을 따른다.
	if (bFollowBoss)
	{
		AttachToActor(Boss, FAttachmentTransformRules::KeepRelativeTransform);
		SetActorRelativeTransform(FTransform::Identity);
	}
	else
	{
		SetActorTransform(Boss->GetActorTransform());
	}

	// 평면 메시를 X-Z 평면으로 세운 뒤(Roll 90) 정면 기준으로 기울인다.
	const FQuat StandUp = FRotator(0.0f, 0.0f, 90.0f).Quaternion();
	const FQuat Tilt = FRotator(Area.PitchDegrees, 0.0f, 0.0f).Quaternion();

	const FVector Scale(
		FMath::Max(Area.HalfSize.X, 1.0f) * 2.0f / TelegraphPlaneSize,
		FMath::Max(Area.HalfSize.Y, 1.0f) * 2.0f / TelegraphPlaneSize,
		1.0f
	);

	Mesh->SetRelativeTransform(FTransform(Tilt * StandUp, Area.Center, Scale));
}

void ABossAttackTelegraph::Hide()
{
	if (bHiding)
	{
		return;
	}

	bHiding = true;

	if (FadeOutTime <= 0.0f)
	{
		Destroy();
	}
}

void ABossAttackTelegraph::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 나타날 때는 FadeInTime, 사라질 때는 FadeOutTime에 걸쳐 투명도를 바꾼다.
	if (bHiding)
	{
		Alpha -= DeltaSeconds / FMath::Max(FadeOutTime, KINDA_SMALL_NUMBER);
	}
	else if (Alpha < 1.0f)
	{
		Alpha += DeltaSeconds / FMath::Max(FadeInTime, KINDA_SMALL_NUMBER);
	}

	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	ApplyAlpha();

	if (bHiding && Alpha <= 0.0f)
	{
		Destroy();
	}
}

void ABossAttackTelegraph::ApplyAlpha()
{
	if (!TelegraphMID)
	{
		return;
	}

	TelegraphMID->SetScalarParameterValue(TEXT("Opacity"), FillOpacity * Alpha);
	TelegraphMID->SetScalarParameterValue(TEXT("EdgeOpacity"), EdgeOpacity * Alpha);
}
