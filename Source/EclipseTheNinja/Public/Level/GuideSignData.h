#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GuideSignData.generated.h"

class UMediaSource;

/**
 * 안내 표지판 팝업에 보여 줄 정보 묶음 (제목, 내용, 반복 동영상).
 * 표지판(AGuideSign)이 이 에셋을 골라 쓰므로, 같은 안내를 여러 곳에 놓아도 문구는 이 에셋 하나만 고치면 된다.
 */
UCLASS(BlueprintType, Const)
class ECLIPSETHENINJA_API UGuideSignData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 팝업 제목 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GuideSign")
	FText	Title;

	/** 팝업 본문 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GuideSign", meta = (MultiLine = "true"))
	FText	Body;

	/**
	 * 게임패드로 조작 중일 때 보여 줄 본문. 비워 두면 Body를 쓴다 (패드와 키보드 문장이 달라야 할 때만 채운다).
	 * Body와 BodyGamepad 모두 중괄호 토큰을 쓸 수 있다.
	 *  {@Jump}   : 키 설정 매핑 칸 Eclipse.Jump(없으면 액션 IA_Jump)에 지금 연결된 키. 키 변경과 입력 장치를 따른다
	 *  {SpaceBar}: 그 키 이름 그대로 (패드 키는 Xbox A / PlayStation ×처럼 패드 종류에 맞는 이름)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GuideSign", meta = (MultiLine = "true"))
	FText	BodyGamepad;

	/**
	 * 반복 재생할 동영상. 비워 두면 동영상 칸을 숨긴다.
	 * 동영상 파일은 Content/Movies 아래에 두고 File Media Source 에셋으로 지정한다.
	 * 미디어 소스는 경로만 담은 작은 에셋이라 직접 참조하고, 실제 파일은 표지판에 다가갈 때 미리 연다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GuideSign")
	TObjectPtr<UMediaSource>	Video = nullptr;
};
