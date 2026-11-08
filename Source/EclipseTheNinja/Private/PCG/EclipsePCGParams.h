// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UPCGComponent;

/**
 * 전용 PCG 액터의 속성을 그래프 파라미터 덮어쓰기로 넣는 도우미.
 * 값이 이미 같으면 넣지 않는다(불필요한 재생성·더티 방지).
 * 그래프 파라미터 타입에 맞춰 변환한다(정수↔실수, 실수 → 균일 벡터).
 */
namespace EclipsePCGParams
{
    // 숫자 파라미터. 그래프 타입이 int/float/double/bool/vector(균일)이면 맞춰 넣는다
    void SetNumber(UPCGComponent* Component, FName Name, double Value);

    // bool 파라미터
    void SetBool(UPCGComponent* Component, FName Name, bool bValue);

    // 벡터 파라미터
    void SetVector(UPCGComponent* Component, FName Name, const FVector& Value);

    // 에셋 경로 파라미터(SoftObjectPath·SoftObject 타입)
    void SetSoftPath(UPCGComponent* Component, FName Name, const FSoftObjectPath& Value);

    // Owner 클래스에서 PCGParam 메타가 붙은 속성을 모두 넣는다. 메타 값이 있으면 그 이름의 파라미터로 넣는다(에디터 전용)
    void PushReflected(const UObject* Owner, UPCGComponent* Component);
}
