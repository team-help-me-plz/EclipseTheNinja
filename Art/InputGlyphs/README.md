# Eclipse 입력 키 이미지

키보드 146개, Xbox 28개, PlayStation 30개로 구성된 투명 배경 PNG 이미지 204개다. 이미지의 글자는 영문만 사용한다. 같은 이름의 SVG 원본과 전체 미리보기도 포함한다. 남청색 바탕, 금색 테두리, 밝은 글자는 기존 Eclipse 메뉴 색상을 따른다.

## 파일 구성

- `Keyboard/`에는 한국어 풀사이즈 배열의 106키와 확장 키 40개가 있다. 문자, 숫자, F1–F24, 좌우 보조키, 탐색키, 방향키, 숫자패드, 한/영, 한자, Fn, 미디어·시스템 키를 포함한다. 한/영 키는 `Lang`, 한자 키는 `Hanja`로 표기한다.
- `Xbox/`에는 ABXY, LB/RB, LT/RT, 스틱과 누르기, 방향 입력, 방향패드, View/Menu/Share/Home이 있다.
- `PlayStation/`에는 네 도형 버튼, L1/R1, L2/R2, L3/R3, 스틱과 방향 입력, 방향패드, 터치패드, Create/Options/Home, 마이크 음소거가 있다.
- `Preview/`에서 전체 키보드, 확장 키, 게임패드 및 플랫폼별 미리보기를 볼 수 있다.
- `manifest.json`의 `lookup`은 플랫폼과 UE `FKey` 이름을 개별 PNG 경로에 연결한다. `entries`에는 모든 이미지의 이름, 크기, 표시 내용과 SVG 경로가 있다.

## HUD에서 사용

1. 필요한 PNG를 Unreal의 콘텐츠 브라우저로 가져온다. 텍스처 그룹은 `UI`, 압축 설정은 `UserInterface2D (RGBA)`로 지정하고 알파와 sRGB를 사용한다. 밉맵은 `NoMipmaps`로 설정한다.
2. Enhanced Input 사용자 프로필에서 액션에 현재 지정된 키를 조회한다. `FKey::GetFName()` 문자열로 `lookup.Keyboard` 또는 게임패드 플랫폼의 대응표를 조회한다.
3. 대응하는 텍스처를 UMG Image의 브러시에 지정한다. 키가 변경되거나 입력 장치가 바뀌면 현재 키를 다시 조회해 이미지를 갱신한다.
4. 모든 이미지의 원본 높이는 256px이다. 가로가 긴 키는 비율을 유지해 표시한다. HUD에서는 32–48px 높이를 기준으로 조정할 수 있다.

예를 들어 `J`는 `Keyboard/J.png`, `SpaceBar`는 `Keyboard/SpaceBar.png`로 연결된다. `Gamepad_FaceButton_Bottom`은 Xbox에서는 A, PlayStation에서는 Cross 이미지로 연결된다. `entries`의 파일 경로는 이 폴더를 기준으로 한 원본 파일 경로다. 프로젝트에 가져온 후에는 대응하는 텍스처 에셋 참조를 사용한다.

한/영, 한자, Print Screen, 컨텍스트 메뉴, Fn, F13–F24 및 일부 시스템 버튼은 UE 5.8의 기본 Windows 키 목록에 독립된 이름이 없어 `engine_key`가 `null`이다. 이미지는 제공하며 해당 입력을 지원하는 플랫폼 코드에서 명시적으로 연결한다. 숫자패드 Enter는 기본 UE Windows 입력에서 일반 Enter와 같은 키로 전달되므로 별도 PNG를 제공하면서 기본 대응표는 일반 Enter를 사용한다. 게임패드 시스템 버튼 전달은 플랫폼과 드라이버에 따라 달라지므로 실제 입력 이벤트 이름에 맞춰 연결한다.

대응표에 없는 키에는 `Keyboard/Unknown.png`를 사용할 수 있다. 이 변경의 범위는 이미지와 대응표 제작이다. HUD의 이미지 자동 교체 코드는 별도로 연결해야 한다.

## 다시 생성

Node.js와 `sharp`가 설치된 환경에서 다음 명령을 실행한다.

```sh
node Art/InputGlyphs/generate.cjs
```

글자와 도형을 코드로 작성한 UI 이미지이며 PNG는 SVG 원본에서 렌더링했다. 글꼴은 Segoe UI와 Malgun Gothic을 사용한다. 재생성할 때 해당 글꼴이 설치되어 있어야 같은 모양을 얻는다.
