#pragma once

#include "CoreMinimal.h"
#include "Character/Enemy.h"
#include "KunaiEnemy.generated.h"

/**
 * 공통 Enemy의 감지·피격·사망·풀링 흐름을 상속하고, 표창 전투 정책을 제공하는 원거리 적.
 *
 * BT는 AEnemy의 공통 가상 함수를 호출한다. 이 클래스가 공격 준비 여부, 쿨타임 중 접근/후퇴,
 * 전투 시야와 공격 중 이동 고정을 재정의하므로 BT에 적 종류를 검사하는 분기를 추가할 필요가 없다.
 *
 * 공격 흐름: 쿨타임 동안 목표 거리 확보 -> 쿨타임이 끝나면 현재 위치에서 투척 -> 몽타주 완료 후 이동 재개.
 * 실제 몽타주·투사체·피해 처리는 GAS 어빌리티가 담당한다. BP_Enemy_Kunai의 AbilityClassArray에
 * GA_Enemy_ThrowKunai를 ThrowKunai 입력 ID로 지정하며, 프로젝트 에셋 참조는 Blueprint에서 연결한다.
 */
UCLASS()
class ECLIPSETHENINJA_API AKunaiEnemy : public AEnemy
{
	GENERATED_BODY()

public:
	/**
	 * 대상이 쿨타임 중 정지할 목표 거리 구간 안에 있는지 검사한다.
	 * 횡스크롤 이동에 맞춰 X축 거리만 사용하며, 목표 거리 ± 허용 오차 안이면 true를 반환한다.
	 * UpdateCombatMovement에서 이동을 멈출 기준으로 사용한다. 공격 시작 조건은 CanStartAIAttack에서 판단한다.
	 */
	virtual bool IsWithinAttackRange(AActor* Target) const override;

	/**
	 * BT가 거리 확보를 끝내고 투척을 시도해도 되는지 검사한다. 어빌리티를 직접 활성화하지 않는다.
	 * 유효한 플레이어/분신, 피격·공격 중이 아닌 상태, NextAttackTime 경과, 전투 시야 반경 내 3차원 거리를 확인한다.
	 * 목표 거리 구간 밖이어도 준비되면 true를 반환해 접근/후퇴를 중단시킨다.
	 * 장애물에 가려졌는지는 감지 컴포넌트와 BT가, 지상 여부·몽타주·투사체 설정 등은 GAS가 추가로 검사한다.
	 */
	virtual bool CanStartAIAttack(AActor* Target) const override;

	/**
	 * Chase 태스크가 공격을 시작할 수 없는 동안 호출하는 거리 조절 처리.
	 * 대상이 너무 가까우면 후퇴하고 멀면 접근한다. 이동 방향과 관계없이 몸은 대상을 바라본다.
	 * 목표 거리 구간에 도착했거나 피격/공격 중이면 수평 이동을 멈춘다.
	 * 쿨타임이 끝났을 때의 공격 전환은 BT가 CanStartAIAttack 결과를 보고 결정한다.
	 */
	virtual void UpdateCombatMovement(AActor* Target, float DeltaSeconds) override;

	/**
	 * 전투에 들어간 후 대상을 계속 추적할 반경을 반환한다.
	 * 감지 컴포넌트의 바깥 감지 원과 현재 경계 단계 배율을 사용한다. 기본 전투 반경은 600 × 1.5 = 900cm.
	 * 최초 발각 게이지·안쪽 즉시 발각 범위·장애물 시야 검사는 기존 감지 컴포넌트 규칙을 따른다.
	 */
	virtual float GetCombatSightRadius() const override;

	/**
	 * 시작한 투척을 제자리에서 완료하도록 BT에 요청한다.
	 * Attack 태스크는 몽타주 완료까지 거리/대상 변경 검사를 보류하고, 감지 컴포넌트는 관련 Blackboard 키를
	 * 유지해 거리·시야 Decorator가 공격을 중단하지 않도록 한다. 피격·사망·태스크 타임아웃에 의한 중단은 유지한다.
	 */
	virtual bool ShouldLockMovementDuringAttack() const override { return true; }

protected:
	/** 활성화·실행 중 판정·취소에 공통으로 사용할 GAS 입력 ID. BP의 AbilityClassArray 항목과 일치해야 한다. */
	virtual EAbilityInputID GetAIAttackInputID() const override { return EAbilityInputID::ThrowKunai; }

	/** 공통 공격 실행부가 성공한 투척 시작 시각을 기준으로 다음 공격 가능 시각을 계산할 때 사용한다. */
	virtual float GetAIAttackCooldown() const override { return KunaiThrowCooldown; }

	/**
	 * ThrowKunai 입력 ID로 이미 부여된 GAS 어빌리티의 활성화를 시도한다.
	 * 반환값은 활성화 성공 여부이며, 몽타주 재생·실제 발사 완료 여부는 어빌리티가 비동기로 처리한다.
	 */
	virtual bool ActivateAIAttack() override;

	/**
	 * DirectionX(+1/-1) 방향으로 KunaiRetreatProbeDistance만큼 후퇴할 수 있는지 검사한다.
	 * 활동 영역(CombatAreaRadius) 밖으로 더 멀어지거나, 벽에 막히거나, 앞에 걸을 수 있는 바닥이 없으면 false다.
	 */
	bool CanRetreatAlongX(float DirectionX) const;

	/** 후퇴 전에 벽과 낭떠러지를 미리 확인할 거리(cm). 막혀 있으면 후퇴를 멈추고 제자리에서 쿨타임을 기다린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Kunai", meta = (ClampMin = "1.0", Units = "cm"))
	float KunaiRetreatProbeDistance = 50.0f;

	/**
	 * 쿨타임 동안 확보할 대상과의 X축 거리(cm). Blueprint 기본값과 배치 인스턴스에서 조절할 수 있다.
	 * 기본 600cm이며 실행 시 최소 100cm로 보정한다. 공격 준비가 끝나면 이 거리 도달 여부와 관계없이 투척한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Kunai", meta = (ClampMin = "100.0", Units = "cm"))
	float KunaiKeepDistance = 600.0f;

	/**
	 * 목표 거리의 앞뒤로 허용할 오차(cm). 기본값이면 500~700cm 구간에서 이동을 멈춘다.
	 * 작은 거리 변화마다 접근/후퇴를 반복하는 것을 줄인다. 실행 시 10cm~목표 거리의 절반 범위로 보정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Kunai", meta = (ClampMin = "10.0", Units = "cm"))
	float KunaiDistanceTolerance = 100.0f;

	/**
	 * 성공한 투척 시작 사이의 최소 간격(초). 기본 4초이며 실행 시 최소 0.1초로 보정한다.
	 * 쿨타임은 몽타주 시작 시부터 진행된다. 몽타주가 더 길면 완료 후 다음 투척을 시작한다.
	 * GA_Enemy_ThrowKunai의 추가 쿨타임 GE는 비워 두어 이 값으로 간격을 제어한다.
	 * 다음 공격 시각은 AEnemy의 NextAttackTime에 저장하며, 기존 체크포인트/풀 초기화 경로에서 함께 초기화한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Kunai", meta = (ClampMin = "0.1", Units = "s"))
	float KunaiThrowCooldown = 4.0f;
};
