// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VineLockable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UVineLockable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 덩굴에 감겨 상호작용이 막힐 수 있는 액터(레버, 문 등)가 구현하는 인터페이스.
 * 덩굴 여러 개가 같은 대상을 감을 수 있으므로 잠금은 횟수(카운트)로 관리하고, 0이 되면 풀린다.
 */
class ECLIPSETHENINJA_API IVineLockable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// 잠금을 하나 추가하는 함수 (덩굴이 감길 때)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void	AddInteractLock();
	// 잠금을 하나 제거하는 함수 (덩굴이 다 타서 없어질 때)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void	RemoveInteractLock();
	// 덩굴에 의해 잠겨 있는지 확인하는 함수
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	bool	IsInteractLocked();
};
