# 무기 애니메이션 직접 편집하기

콘텐츠 브라우저의 `BBSB/AnimationAuthoring` 폴더를 사용합니다.

## 주인공의 서 있는 모션부터 편집하기

1. `L_WeaponAnimationStudio` 레벨을 열고 `Character/LS_Player_Idle`을 더블클릭합니다.
2. 주인공 모델이 보이며, 시퀀서의 `Preview_Character → CharacterBody → CR_Mannequin_Body`에 편집 가능한 리그 키가 있습니다.
3. 애니메이션 아웃라이너에서 몸·머리·팔·다리 컨트롤을 선택하고 뷰포트에서 이동하거나 회전합니다. 원하는 프레임에 키를 추가합니다.
4. 시퀀서 툴바의 **저장**을 누릅니다. 연결된 `A_Player_Idle`로 모션이 베이크되어 게임의 Idle 상태에서 재생됩니다. 원본 템플릿의 애니메이션은 별도로 유지합니다.
5. 반복 모션은 시작과 끝 포즈를 맞추세요. 한 자세를 유지하게 바꾸려면 관련 컨트롤의 기존 중간 키를 정리하고 시작·끝에 같은 포즈를 키로 지정합니다.

같은 `Character` 폴더에 걷기/달리기 8방향, 점프, 낙하 반복, 착지까지 총 20개의 `LS_Player_*` 시퀀스가 있습니다. `ABP_PlayerEditable`과 `BS_Player_Idle_Walk_Run`이 이 편집용 애니메이션들을 사용하고, 주인공도 해당 애니메이션 블루프린트에 연결되어 있습니다.

## 주인공의 공격 모션

무기별 `LS_Hammer_Shake`, `LS_Spear_Beat` 등의 시퀀스 안에도 `CharacterBody → CR_Mannequin_Body` 트랙이 있습니다. 몸과 무기 궤적을 한 타임라인에서 편집할 수 있습니다. 레파라톤은 손 소켓에 붙어 몸 동작을 따라갑니다.

공격 시퀀스의 저장 버튼을 누르면 연결된 `A_무기_동작`에 베이크됩니다. 이 Animation Sequence는 해당 무기의 `DA_* → Clips → Character Animation`에 연결되어 게임에서도 사용합니다.

## 처음 시작: 해머 변형

1. `L_WeaponAnimationStudio` 레벨을 엽니다. 실제 게임 맵과 분리된 작업 공간입니다.
2. `Hammer/LS_Hammer_Form`을 더블클릭합니다.
3. 시퀀서에서 `Preview_Hammer` 아래의 `Upper`, `Lower`와 `Transform` 트랙을 펼칩니다.
4. 재생 헤드를 움직여 포즈를 확인합니다. 0프레임은 봉의 조립 위치, 60프레임은 해머 형태입니다.
5. 해당 부품을 선택하고 뷰포트의 이동·회전 도구로 포즈를 만든 뒤 Transform 트랙에 키를 추가합니다. 기존 키를 수정하거나 중간 키를 추가해 변형 경로를 바꿀 수 있습니다.
6. 시퀀스와 레벨을 저장합니다. 이후 게임에서 같은 시퀀스의 곡선을 사용합니다.

게임에서 변형에 걸리는 시간은 `DA_Hammer`의 `Form Transition Seconds`로 조절합니다. `Keep Form Equipped`가 켜져 있으면 가만히 있어도 해머 형태를 유지합니다.

## 휘두르기·찌르기·날리기

`LS_Hammer_Shake`, `LS_Hammer_Flick`, `LS_Spear_Beat` 같은 시퀀스를 엽니다. `WeaponMotion → Transform`의 키를 편집하면 무기 궤적이 바뀝니다.

- 시퀀스의 0~60프레임은 동작 전체의 0~100%에 해당합니다. 실제 재생 시간은 `DA_무기이름 → Clips → Duration`입니다.
- `WeaponMotion`은 명중 대상 위치를 원점으로 사용하는 상대 좌표입니다. 보통 X는 대상을 향하는 방향, Y는 좌우, Z는 위아래입니다.
- 창 Flick은 `Use Impulse Frame`이 켜져 있어 X가 Flick으로 날리는 방향입니다.
- 단검 Beat는 실제 비행 궤적입니다. X=-250에서 0으로 이동하는 기준 궤적을 실제 발사 거리로 늘여 사용합니다. Y/Z로 옆이나 위로 휘는 궤적을 만들고 회전 키로 회전을 편집합니다. 첫 X=-250, 마지막 위치=(0,0,0)을 유지하면 출발점과 목적지가 맞습니다. 실제 비행 시간은 거리와 `DA_Dagger → Projectile Speed`로 정하며 발사 후 목표는 고정됩니다.
- `Active Start`, `Active End`는 실제 타격 검사 구간입니다. 예를 들어 0.2~0.64면 전체 동작의 20~64% 구간입니다. 해당 구간에서 보이는 궤적을 따라 충돌을 검사합니다.
- `Blend In`은 현재 무기 위치에서 편집한 궤적으로 들어가는 연결 구간입니다.
- 창의 박힘·대상 고정·회피 취소는 게임 로직이 관리합니다. 애니메이션 키로 대상을 바꿀 필요가 없습니다.

## 캐릭터의 팔·몸 동작

`DA_무기이름 → Clips → Character Animation`에는 편집 시퀀스에서 베이크한 Animation Sequence를 연결해 두었습니다. 다른 호환 Animation Sequence로 교체할 수도 있습니다. 비워 두면 기본 캐릭터 동작을 사용합니다.

## 미리보기와 저장

작업용 레벨의 `Preview_무기이름` 액터에는 `Animation Set`, `Clip Index`, `Preview Time`과 `Preview Form`, `Preview Action` 버튼이 있습니다. Preview Time은 0~1입니다. 시퀀서를 열지 않고도 지정 시점의 포즈를 확인할 수 있습니다.

게임 확인은 `ThirdPerson/Lvl_ThirdPerson` 레벨로 돌아가 Play를 누릅니다. 숫자 1~6으로 무기를 선택합니다.

## 편집할 때 유지할 것

무기 부품의 기존 `Upper`, `Lower`, `WeaponMotion` 바인딩 이름을 유지하세요. 게임은 이 이름으로 트랙을 찾습니다. 무기 부품은 일반 Transform 트랙의 위치·회전·크기 키와 보간 곡선을 읽습니다. 주인공 Control Rig는 Animation Sequence로 베이크한 결과를 재생합니다. 시퀀서 이벤트나 추가 제약을 게임에서 직접 실행하는 구조는 아닙니다.

새 동작이 필요하면 시퀀스를 복제하고, `DA_무기이름 → Clips`에 연결합니다. `Gesture`, `Mouse Channel`(-1은 양쪽), `Combo Step`(0은 공통)으로 사용할 입력을 선택합니다. 현재 무기 로직에서 지원하는 액션에 연결해야 합니다.
