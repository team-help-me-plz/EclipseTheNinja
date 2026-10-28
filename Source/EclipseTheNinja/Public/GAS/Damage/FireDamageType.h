#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "FireDamageType.generated.h"

/**
 * ASC가 없는 액터(덩굴 등)에 불속성 공격을 전달할 때 쓰는 엔진 데미지 타입.
 * ASC가 있는 대상은 GE로, 없는 대상은 ApplyDamage + 이 타입으로 처리한다.
 */
UCLASS()
class ECLIPSETHENINJA_API UFireDamageType : public UDamageType
{
	GENERATED_BODY()

public:
	UFireDamageType();
};
