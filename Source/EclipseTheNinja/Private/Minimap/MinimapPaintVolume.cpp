#include "Minimap/MinimapPaintVolume.h"

#include "Components/BoxComponent.h"

AMinimapPaintVolume::AMinimapPaintVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true;

	this->Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(this->Box);
	this->Box->SetBoxExtent(FVector(200.f, 100.f, 200.f));
	this->Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->Box->SetGenerateOverlapEvents(false);
	this->Box->SetCanEverAffectNavigation(false);
	this->Box->SetLineThickness(3.f);
}

void AMinimapPaintVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 뷰포트에서 모드를 구분할 수 있게 상자 색을 바꾼다
	switch (this->Mode)
	{
	case EMinimapPaintMode::Solid:  this->Box->ShapeColor = FColor(230, 210, 160); break;
	case EMinimapPaintMode::Empty:  this->Box->ShapeColor = FColor(60, 140, 255);  break;
	case EMinimapPaintMode::Hidden: this->Box->ShapeColor = FColor(160, 60, 220);  break;
	case EMinimapPaintMode::Color:  this->Box->ShapeColor = this->Color;           break;
	}
	this->Box->MarkRenderStateDirty();
}
