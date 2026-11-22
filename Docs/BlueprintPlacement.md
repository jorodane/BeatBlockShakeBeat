# 배치 가능한 게임플레이 블루프린트

배치용 에셋은 `/Game/BBSB/Placeables`에 모았다. 공통 동작은 C++ 기반 클래스에서 유지하고, 종류별 기본값·메시·재질·로컬 위치·연결 대상은 블루프린트와 레벨 인스턴스에서 설정한다. 블루프린트의 자식 클래스를 만들면 같은 공통 동작을 재사용하면서 외형과 수치를 바꿀 수 있다.

## 바로 배치하기

콘텐츠 브라우저에서 아래 에셋을 레벨로 끌어 놓는다. 기존 레벨의 교체된 액터도 같은 방식으로 이동·회전·복제할 수 있다.

| 용도 | 폴더 / 대표 에셋 |
| --- | --- |
| 공통 사물 | `Interactions/BP_Interactable` |
| 미닫이문·승강기·경로를 따라 움직이는 사물 | `Interactions/BP_Draggable` |
| 사다리 | `Interactions/BP_Climbable` |
| 버튼·홀드 장치·압력판 | `Interactions/BP_ConditionSwitch` |
| 운반할 사물·드롭 아이템 | `Interactions/BP_Grabbable`, `Props/BP_StepCrate`, `BP_BreakableProp`, `BP_Pickup` |
| 1스테이지 퀘스트 장치 | `Devices/BP_Stage01Device`와 인스턴스의 `Kind` |
| NPC | `Characters/BP_NPC`, 1스테이지 상인 `BP_UtilityMerchant`, `BP_ArtMerchant` |
| 적 | `Enemies/BP_Enemy_*`, 1스테이지 노래 버니아 `BP_Bunniar_*` |
| 퍼즐 예제 | `Puzzles/BP_Puzzle_*` 20종 |
| 전투 구역·체크포인트 | `World/BP_SequentialEncounter`, `BP_Checkpoint` |
| 진행 연결 | `World/BP_Stage01Director` |

신규 상호작용은 `Interactions`에 있는 공통 BP로 배치한다. `Legacy/Stage01Presets`에는 기존 참조를 위한 이전 프리셋이 있다. 1스테이지의 기존 장치는 하나의 `BP_Stage01Device`에서 역할과 메시를 설정한다.

## 상호작용과 조건 연결

`Draggable`, `Grabbable`, `Climbable`, `Condition Switch` 컴포넌트는 `Interactable`에서 공통 입력과 조건을 상속한다. 배치 BP에서는 `Interaction` 또는 `Grabbable` 컴포넌트를 선택해 설정한다. 직접 만든 BP에서는 상호작용을 담당할 컴포넌트를 하나 추가하고 에임에 닿는 메시가 `Visibility`를 막도록 한다. 물리 운반 사물은 기존 물리 운반 부모가 들어 있는 `BP_Grabbable`을 사용한다.

- `Interact Type`을 고르면 `DA_InteractTypes`에 지정한 동작 문구와 아이콘을 F 표시에 사용한다. 새 타입은 이 데이터 에셋에 행을 추가한다. 개별 개체의 `Prompt Override`로 문구나 아이콘만 바꿀 수 있다.
- `Required Conditions`에는 확인할 액터, 조건 이름, 최소값을 추가한다. 액터 칸이 비면 자기 `Condition Receiver`의 값을 읽는다. 블루프린트의 `Can Interact`를 재정의해 추가 조건을 만들 수 있다. F와 버니아 클릭이 같은 조건을 확인한다.
- `OnInteractEnter`, `OnInteractStay`, `OnInteractEnd`는 `Interaction` 컴포넌트에서 바인딩한다. 즉시 사용은 Enter와 End를 한 번씩 보낸다. 누르고 있는 조작은 유지 중 Stay를 보내고 입력 해제나 부착 해제 시 End를 보낸다.
- `BP_Draggable`의 `Constraint`는 `Axis`, `Area`, `Spline` 중 선택한다. 축의 이동량과 영역 상·하한은 초기 부품 위치 기준 로컬 값이다. 스플라인은 `Drag Path` 컴포넌트의 점을 뷰포트에서 편집한다. 시작점 대비 이동량을 메시 위치에 적용한다. 움직일 메시가 루트 아래에 있고 `Movable`인지 확인한다. `OnDragEnter/Stay/End`에 실제 이동 위치, 진행률과 막힘 여부가 전달된다.
- `BP_Grabbable`의 `OnGrabEnter/Stay/End`는 실제로 들어 올린 뒤부터 놓거나 던질 때까지 발생한다. 메시, 크기, 무게, 따라가는 속도, 던지는 속도는 인스턴스에서 변경한다.
- `BP_ConditionSwitch`의 `Mode`는 `Set On`, `Toggle`, `Hold`, `Pressure`, `Pressure Toggle` 중 선택한다. `Hold Fill Seconds`가 양수면 누르는 동안 0에서 1로 차오른다. 압력 모드는 `Pressure Area`를 판 윗면에 맞추고 최소 무게를 설정한다. 압력판의 안내는 `Unavailable Text`에 `밟아서 작동`처럼 쓸 수 있다. `Targets`에는 액터와 보낼 조건 이름을 지정한다. 여러 장치의 신호는 최댓값으로 결합하며, 각 장치는 자신이 보낸 값만 해제한다.
- 버튼으로 문을 움직이려면 `BP_Draggable`의 `Drive Condition`에 같은 이름을 쓰고, 버튼 `Targets`에 그 문을 연결한다. 값 0부터 1까지가 닫힌 곳부터 열린 곳이다. 조건은 `OnConditionChanged`로 받아 다른 BP 연출에도 사용할 수 있다.

`BP_Climbable`의 `Local Frame`에서 시작·위 끝·도착점과 바깥 방향을 정한다. `Animations`에서 잡기, 한 칸 오르기, 뛰어오르기, 내려오기, 꼭대기 올라서기를 골라 쓸 수 있다. 비워 둔 슬롯은 캐릭터의 기본 모션을 사용한다. `OnClimbEnter/Stay/End`는 잡기부터 놓기 또는 꼭대기 도달까지 전달된다. `Generate Rungs`를 끄면 직접 지정한 메시를 사용할 수 있다.

## 사다리와 움직이는 사물

1스테이지의 장치는 진행 역할에 맞는 기존 이동을 사용한다. `BP_Stage01Device`에서 `Open Offset`, `Ladder Bottom`, `Ladder Top`, `Destination`을 조정한다. 축·영역·스플라인 방식의 새 장치는 `BP_Draggable`에서 구성한다.

- 기존 `BP_Stage01Device`에서 `Placement / Local Placement`가 켜져 있으면 끝점과 이동량은 **액터 기준 로컬 좌표**다. 사다리 액터를 이동·회전하면 발판, 잡기 선, 꼭대기 도착점이 함께 따라간다.
- `Ladder Bottom`, `Ladder Top`은 **캐릭터 캡슐 중심**이 이동하는 선의 양 끝이다. 메시의 발끝 좌표가 아니다. `Destination`은 꼭대기에서 넘어갈 캡슐 중심 위치다. 두 끝점과 도착점은 뷰포트의 편집 위젯 또는 Details에서 조정한다.
- `Ladder Outward`는 사다리를 바라보는 캐릭터 쪽 방향이다. `Rung Spacing`으로 칸 간격을 바꾼다. 사다리 높이를 바꿀 때는 스케일 대신 위 끝점과 도착점을 조정한다.
- 문·발판·도구의 `Placement / Geometry / Surface Parts`는 메시, 재질, 로컬 Transform, 표시 여부, 이동 충돌을 가진 배열이다. 별도의 월드 메시 액터를 찾아다닐 필요가 없다. `Open Offset`도 액터 기준 이동량이다.
- 값 변경 뒤 표시를 새로 만들려면 Details의 `Rebuild Physical Presentation`을 실행한다. 일반적인 에디터 이동과 속성 편집 때는 Construction에서 다시 만든다. 플레이 도중 전체 표시 재생성은 초기 배치용으로만 사용한다.
- 기존 `Targets`는 구버전 레벨 호환용이다. 교체 과정에서 정적 메시를 `Surface Parts`로 흡수하고 원본은 숨김·충돌 해제 상태로 보관했다. 이 복구용 액터는 `BBSBLegacyPlacementReference` 태그로 구분한다.

## 진행, 저장, 복제

독립 사다리는 Stage01 Director 없이도 쓸 수 있다. 1스테이지의 상인·진행 잠금·보상·단계별 전투를 사용하려면 해당 레벨에 `BP_Stage01Director`를 하나 두고 필요한 레벨 참조를 연결한다. Director를 지정하지 않은 Stage01 사물과 가수는 BeginPlay에서 같은 월드의 Director를 찾아 등록한다. 일반 NPC, 운반 사물, 일반 적과 퍼즐 예제는 해당 공통 시스템을 사용한다.

기존 진행용 `Node Id`, `Singer Id`, `Point Id`는 교체 전 값을 유지했다. 새 사물은 자동 ID를 쓰고 에디터 복제 시에는 별도의 ID를 부여한다. 명시적 진행 ID를 직접 지정할 때는 같은 맵에서 중복시키지 않는다. 기존 액터를 삭제하고 새로 만들거나 이름을 바꾸면 저장된 개체의 식별 방식도 달라질 수 있다.

노래 버니아의 기존 운반 본체는 에디터에서 가수의 자식으로 연결돼 같이 이동한다. 플레이가 시작되면 분리하고 이전 운반·물리 로직을 사용한다. 새로 놓은 가수는 자신의 운반 본체를 생성한다. `Carrier Class`와 `Carrier Offset`으로 프리셋을 바꿀 수 있다. 1스테이지 가수 프리셋은 진행 단계에 맞춰 노래하는 적이다. 다른 맵에서 자유 전투만 하려면 일반 `BP_Enemy_*`를 사용한다.

Director의 시작·옥상·트리오·상단장 좌표도 로컬 좌표다. 독립적인 연출/구역 중심이 필요하면 해당 Anchor 액터를 연결한다. 문이나 사다리만 옮기는 데 Director를 함께 옮길 필요는 없다.

## 순차 등장 전투 구역

`BP_SequentialEncounter`의 `SpawnPath`로 경로를 만든다. 월드 아웃라이너에서 전투 구역 액터를 선택하고 디테일의 `Monster Spawning`에서 웨이브를 순서대로 추가한다.

- 각 **웨이브**에는 등장 항목 목록, 웨이브 딜레이, 스플라인 이동 거리가 있다. 첫 웨이브는 경로의 `첫 등장 위치`에서 시작한다. 이후 웨이브는 이전 웨이브의 등장과 전투가 모두 끝나면 자신의 이동 거리만큼 경로를 이동하고 딜레이를 적용한다.
- 각 **등장 항목**에는 몬스터 클래스, 한 번에 등장하는 마릿수, 반복 횟수, 등장 딜레이, 등장 확률이 있다. 반복 횟수는 첫 실행을 포함한다. 첫 실행과 매 반복 전에 등장 딜레이를 적용하며, 확률은 반복마다 묶음 전체를 한 번 판정한다. 예를 들어 마릿수 3, 반복 횟수 2, 확률 1은 3마리 묶음 두 번을 뜻한다.
- **웨이브 무한 반복**을 켜면 마지막 웨이브가 끝난 뒤 첫 웨이브부터 이어진다. 기본 설정은 마지막 웨이브를 정리하면 완료된다.

닫힌 경로는 설정한 방향으로 돌고 열린 경로는 끝에서 방향을 바꾼다. 각 묶음의 스폰 좌표는 해당 웨이브의 위치를 기준으로 스플라인에서 찾는다. 지형과 플레이어 또는 다른 적 때문에 공간이 부족하면 남은 몬스터는 안전한 위치가 생길 때 등장한다.

## 런타임 생성과 앞으로의 추가 기준

`DA_PlacementClasses`는 네이티브 기본 클래스를 배치용 블루프린트에 연결한다. 장비 버니아, 적/연습 구역, 드롭·저장 복원 아이템, 운반 본체, 시작 연출과 구버전 상단 생성 코드가 이 연결을 사용한다. 이미 명시한 자식 블루프린트 클래스는 그대로 사용한다. 구버전 장치의 키는 `BunniarDevice_Shutter` 같은 형태다.

새 게임플레이 개체도 공통 기반 클래스와 실제 배치할 블루프린트를 함께 제공한다. 레벨 이름·액터 이름·절대 월드 좌표를 공통 동작에 박지 않는다. 개체가 소유하는 메시/충돌/상호작용 위치는 자기 루트와 함께 움직여야 한다. 외부 목표가 필요한 경우 명시적 액터 참조 또는 Anchor로 연결한다. 런타임 스포너에는 편집 가능한 클래스 참조 또는 이 레지스트리를 사용한다.

변경 검증에는 에디터 이동·회전·복제와 실제 PIE 상호작용을 포함한다. 저장 ID와 기존 퀘스트 연결도 확인한다. C++ 변경은 에디터 저장·정상 종료·일반 빌드·재실행 순서로 반영한다.
