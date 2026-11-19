# 금색 입체형 방향패드 원본

사용자가 세 가지 샘플 중 첫 번째 금색 입체형을 선택했다. `Left.png`는 선택한 샘플이며 나머지 방향은 같은 이미지를 참조해 내장 `image_gen`으로 편집했다. 각 방향은 선택 팔 부분만 따뜻한 금색으로 강조한다. 모든 파일은 투명 배경 PNG다.

## 제작 프롬프트

왼쪽 원본에는 다음 디자인 지시를 사용했다.

> Straight-on circular dark midnight navy D-pad HUD icon with restrained warm brass perimeter. Tactile connected cross-shaped rocker, softly chamfered edges, matte material. Only the left rectangular arm is physically pressed down, with a narrow occlusion gap, contact shadow and warm ivory/brass reflection. Other arms remain raised dark graphite/navy. No arrows, text or face symbols. Transparent background.

상하우 변형은 왼쪽 원본을 편집 대상으로 다음 지시를 사용했다. `DIRECTION`은 각각 UP, BOTTOM, RIGHT다.

> Produce the DIRECTION-pressed version. Preserve the exact straight-on circular navy/gunmetal D-pad, matte texture, brass outer ring, cross shape, canvas dimensions, position and top-left illumination. Move only the depressed warm-gold active face from the LEFT arm to the DIRECTION arm. Return LEFT to dark raised navy. Only DIRECTION is gold. Maintain geometry and material without rotating the image or lighting. Clean alpha transparency, no backdrop, text or arrows.

`generate.cjs`는 이 원본을 읽어 Xbox와 PlayStation의 `Gamepad_DPad_*.png`를 생성하고 대응표 및 미리보기를 갱신한다.
