# PlayerCharacter 분신술 통합

`BP_CharacterTest_Character`가 기존 전투 ASC에서 `GA_BasicAttack`과
`GA_CloneTech`를 함께 사용한다. 능력과 Input Action은 Blueprint 기본값에 지정되어 있다.

- R: 분신술 시작 / 분신 생성 후 재입력으로 위치 교환
- 게임패드 오른쪽 숄더: 분신술 (기존 왼쪽 스틱 아래의 발판 내려가기 유지)
- E 또는 F: 상호작용
- 기존 이동, 점프, 공격, 우클릭 카메라 탐색 입력 유지

`PlayerCharacter`에 `IInteractor`와 상호작용 박스를 추가했다. 기존
`ISideScrollingInteractable` 상호작용도 유지한다. 안전지점은 원점도 허용하며
복귀할 때 이동 속도와 이동 평면을 갱신한다.

`BP_CharacterTestController`와 `PC_CloneTech`의 부모는
`PlayerCharacterController`로 통합했다. 재생성 캐릭터, 입력 매핑,
카메라 매니저는 현재 PlayerCharacter 설정을 사용한다.
`GM_CloneTech`도 `BP_CharacterTest_Character`를 생성한다.
기존 `GM_CharacterTest`의 카메라 테스트 컨트롤러 연결은 그대로 유효하다.

기존 테스트 캐릭터와 에셋은 다른 테스트 레벨의 참조를 보존하기 위해 남겼다.
분신 생성·명령·타게팅·쿨다운·위치 교환 구현은 기존 능력을 재사용한다.
`GA_CloneTech`의 Blueprint 인스턴싱 정책은 재입력을 받도록
`InstancedPerActor`로 수정했다.

## 검증

- UE 5.8 Development Editor C++ 빌드 통과.
- 저장한 Blueprint를 별도 프로세스에서 재로드·컴파일하고 설정 검증 통과.
- `Lvl_CharacterTest`, `Lv_CloneTech` PIE에서 통합 PlayerCharacter 생성 확인.
- 두 레벨에서 같은 ASC에 기본 공격과 분신술이 각각 한 번 부여됨을 확인.
- 입력 매핑에 기존 11개 매핑과 추가 R / 오른쪽 숄더 / E 매핑 확인.
- 타게팅 입력의 소비 설정과 높은 매핑 우선순위 확인.
- 마우스로 목적지 지정, 취소, 이동/퍼즐 명령 완료, 위치 교환 및 사망 후
  재생성 전체 시나리오는 별도의 수동 플레이 검증이 필요하다.

`validate_clone_tech_player.py`는 에디터 Python 또는 PythonScript commandlet에서
실행할 수 있다. `merge_clone_tech_player.py`는 동일 설정을 다시 적용하는
에디터 마이그레이션 스크립트다. 실행 중인 다른 에디터가 같은 에셋을 로드한
상태에서는 commandlet 저장이 잠길 수 있으므로 한 에디터에서 적용한다.
원본 에셋 백업은 `Saved/CloneMerge/AssetBackup`에 있다.
