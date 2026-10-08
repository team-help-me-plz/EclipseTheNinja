// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CloneCommandType.generated.h"

/**
 * 
 */
UENUM(BlueprintType)
enum class ECloneCommandType : uint8
{
	Move,
	Interact,
};

USTRUCT(BlueprintType)
struct ECLIPSETHENINJA_API FCloneCommandType
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Clone|Command")
	ECloneCommandType	Type = ECloneCommandType::Move;

	UPROPERTY(BlueprintReadWrite, Category = "Clone|Command")
	FVector				TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Clone|Command")
	AActor*				TargetActor = nullptr;

public:
	static	FCloneCommandType	MakeMove(const FVector& InLocation)
	{
		FCloneCommandType	Command;

		Command.Type = ECloneCommandType::Move;
		Command.TargetLocation = InLocation;

		return (Command);
	}
	static	FCloneCommandType	MakeInteract(AActor* InTargetActor, const FVector& InLocation)
	{
		FCloneCommandType	Command;

		Command.Type = ECloneCommandType::Interact;
		Command.TargetLocation = InLocation;
		Command.TargetActor = InTargetActor;

		return (Command);
	}

};