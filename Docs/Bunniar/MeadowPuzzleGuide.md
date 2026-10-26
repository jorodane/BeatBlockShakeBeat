# 버니아 초원: 20개 입력 퍼즐 테스트 필드

맵: `/Game/BBSB/PuzzleField/L_BunniarMeadow`.
기존 상단 스테이지와 원본 캐릭터·무기·머티리얼은 보존한 별도 실험 필드다.

253 × 253 버텍스, 63쿼드 × 16컴포넌트의 실제 Landscape다. 외곽 언덕과 평탄한 장치 터, 잔디 텍스처, 굽이진 흙길 마스크가 있다. 자연물은 별도 메시 6종과 `PCG_BunniarMeadowNature`로 구성한다. 나무 130개, 자작나무 95개, 관목 270개, 바위 85개, 풀 1,900개, 꽃 300개로 총 2,780개를 배치했다. 퍼즐 주변은 빈터로 남기고 나무·바위에는 충돌을 적용했다.

## 조작

- WASD: 기본 달리기. Shift: 입력 방향으로 대시. 이동 입력이 없으면 바라보는 방향으로 대시한다.
- Space: 점프. 공중 첫 사용은 2단 점프와 버니아 활강이다. 활강 중 다시 누르면 해제해 낙하하고, 낙하 중에는 추가 상승 없이 활강만 다시 펼칠 수 있다. 상승 효과는 착지 전 한 번이다.
- 활강 중 Shift: 지상 대시처럼 카메라 기준 WASD 방향으로 버니아가 회전하며 먼저 나가고 레파라도가 뒤따른다. 입력이 없으면 캐릭터가 바라보는 방향으로 이동한다. 이동이 끝나거나 벽에 막히면 활강을 계속 유지한다. Space를 누르면 활강을 해제한다.
- 활강 버니아는 평소 장착 크기를 유지하고 왼팔을 길게 뻗어 잡는다. 오른팔과 다리는 아래로 늘어뜨리고 오른손에는 레파라톤을 든다. 돌진 중에는 몸이 뒤로 기울어 따라가고 활강 자세로 부드럽게 돌아온다. 활강 중 좌클릭하면 활강을 풀고 버니아를 장착해 누른 표면에 부착한다. 사거리 밖이나 빈 공중을 클릭하면 사거리 끝의 공중 지점에서 대기하며, 놓으면 회수한다.
- 장비 전환은 활강 해제 또는 착지 뒤 적용된다. Shift 돌진 뒤에도 활강을 이어가므로 대기 중인 장비 변경은 유지된다. 피격·조종 해제 시 원래 중력과 동행 상태로 복귀한다.
- 일반 낙하는 꼭대기를 지난 뒤 중력을 기본의 2.4배로 적용한다. 상승 중 중력과 점프 힘은 유지하고, 활강 낙하 속도는 기존 140cm/s 수준으로 유지한다.
- 짧은 Tab: 한손검 ↔ 버니아. 길게 Tab: 기존 10칸 원형 메뉴.
- 버니아로 표면을 왼쪽 클릭한 채 유지하면 그 위치에 부착한다. 마우스를 놓으면 해제한다.
- 버니아는 무기와 동일한 마우스 입력을 쓴다. 부착 때문에 커서나 입력 모드가 바뀌지 않으며, 카메라 회전 제한 중에도 드래그·Shake·Flick을 계속 받는다.
- Beat는 누르는 순간, Hold는 0.35초 유지, Shake는 누른 채 왕복 흔들기다.
- Flick은 놓기 직전 빠른 이동이다. Dive는 빠르게 튕기지 않고 놓는 입력이다.
- R: 가까운 장치 재시도. Backspace: 20개 전체 초기화.
- 필드 경계를 넘으면 잠시 안쪽으로 걸어 돌아온 뒤 조작을 돌려준다. 낙하 복구는 별도로 시작점으로 이동한다.

처음에는 한손검을 장착한다. 시작점에서 Tab으로 버니아를 선택한다. 장치 주변에 가면 상단에 조건·진행도·실패 이유가 표시된다. 숫자는 안내 순서이며 다른 장치부터 자유롭게 시험할 수 있다.

## 실험 목록

| 번호 | 장치 | 시험하는 입력과 조건 |
|---|---|---|
| 01 | Tap shutter | Beat 한 번으로 셔터 열기 |
| 02 | Three-beat chime | 80BPM 초록 신호와 소리에 연속 세 Beat. 타이밍 창 ±0.18초 |
| 03 | Ordered keys | 왼쪽·가운데·오른쪽 패드를 순서대로 Beat |
| 04 | Held gate | Hold 1.25초. 누르고 있어야 문이 열려 있고 놓으면 닫힘 |
| 05 | Hold elevator | Hold로 승강기 상승. 놓으면 진행을 멈추고 다시 누르면 이어감 |
| 06 | Shake loose | 두 번의 인식된 Shake로 고정된 물건 분리 |
| 07 | Shake burst | 2.4초 안에 Shake 세 번. 시간 초과 시 다시 시작 |
| 08 | Flick right | 오른쪽 Flick으로 목표에 물건 보내기. 다른 방향은 빗나감 |
| 09 | Flick upward | 위쪽 Flick으로 높은 목표에 물건 보내기 |
| 10 | Dive on green | 누른 뒤 초록 구간에서 Dive. 매 1.8초 주기의 1.2초 지점 |
| 11 | Stop the pendulum | 움직이는 블록이 가운데를 통과할 때 Dive |
| 12 | Lower ladder | Hold 후 아래로 160px 드래그해 사다리 내리기 |
| 13 | Pull lever | Hold 후 아래로 80px 드래그해 레버 당기기 |
| 14 | Connect the wire | Hold한 채 오른쪽 80px → 위 70px → 오른쪽 70px로 점 잇기 |
| 15 | Gesture phrase | 한 번의 누름 안에서 Beat → Hold → Shake → Dive |
| 16 | Charge and lob | 1.0–1.8초 Hold 후 위 Flick. 부족하거나 과하면 실패 |
| 17 | Shake and launch | Shake 두 번 준비 후 오른쪽 Flick |
| 18 | Carry and mount | Shake 두 번으로 분리 → 놓기 → 분리된 물건을 다시 눌러 운반 → 소켓에서 Dive |
| 19 | Balance delivery | 물건을 접시에 옮겨 0.8초 안정적으로 유지한 뒤 Dive |
| 20 | Five-action relay | Beat → Hold → Shake → 오른쪽 Flick. 다시 누르고 초록 신호에서 Dive |

퍼즐은 완성된 스테이지 미술이나 범용 등반 시스템이 아닌 조작감 검사용 장치다. 운반 장치는 각 장치의 수직 작업 평면 안에서 이동한다. 번호별 성공 기록은 플레이를 끝내면 초기화된다.

## 수정 위치

- 장치의 Details에서 Kind, Number, BPM, Timing Window, Drag Distance를 수정한다. `Rebuild Presentation`으로 저장된 장치 모양을 다시 구성할 수 있다.
- `DA_BunniarAnimation`과 편집용 Bunniar Level Sequence는 기존 작업 공간을 사용한다.
- `/Game/BBSB/PuzzleField/Materials/M_MeadowLandscape`: 월드 좌표 기반 잔디/흙길/경사 재질.
- `PCG_BunniarMeadowNature`: 메시 6종 교체 파라미터와 전체 크기 배율. 배치 시드·빈터·길 제외 영역은 `configure_nature.py`에 있다.
- `C:/BBSB/Source/BBSB/Puzzle/`: 장치별 규칙, 필드 경계, 안내 HUD, Landscape 생성 도우미. 작업 기록의 `source/Puzzle/`에도 사본이 있다.

## 복구와 검증

`before/`에 변경 전 전체 게임 코드와 Config가 있다. 빌드는 Live Coding 없이 에디터 저장·정상 종료·일반 빌드·재실행으로 수행했고 `build-8.log`에서 성공했다.

`verification-final.json`에서 20종 전체와 98개 검사가 통과했다. 첫 전체 실행 뒤 가려진 운반 물건을 앞으로 옮겼고, 영향을 받는 8종을 다시 검사했다. `fresh-play.json`의 11개 검사에서는 새 플레이의 기본 한손검, 동행 버니아 한 마리, 걷기 속도, 조작·시점·시간 복구도 통과했다. 10칸 메뉴의 실제 UI와 종료 후 시간·커서 복구는 `wheel-cleanup.json` 및 네이티브 로그로 확인했다.

자연물 PCG는 여섯 속성 이름을 수정한 뒤 경고·오류 없이 실행됐다(`pcg-execution-fixed.json`). 에디터 10개, 실제 플레이 13개, 맵 재로딩 10개 검사를 모두 통과했다(`nature-verification-editor/play/reload.json`). 세 환경에서 중복 없이 2,780개가 유지됐으며 퍼즐 20개, 나무 주변 빈터, 충돌 설정, 플레이어 시작과 조작도 확인했다. 수정 전 그래프와 맵은 `before-pcg-repair/`, 최종 자산 사본은 `after/Assets/`에 있다.

잔디 원본: `art/T_MeadowGrass.png`. 내장 imagegen으로 생성했으며 최종 프롬프트는 `art/generation.json`에 저장했다. 흙길 마스크와 메시 형상은 프로젝트의 결정적 생성 스크립트 `make_art.py`로 만든다.

2026-10-22 입력 수정: 버니아 부착 때 별도 UI 입력 모드로 전환하며 뷰포트 마우스 캡처를 해제하던 처리를 제거했다. 공통 무기 입력 경로로 실제 뷰포트 이벤트를 보내 45개 검사를 통과했다. 사다리 드래그·물건 운반·Shake·방향성 Flick과 커서/입력 모드 유지, 부착 중 시점 고정, 해제를 확인했다. 콘솔의 `Automation RunTests BBSB.Field.Equipment.BunniarSharedPointer`로 다시 검사할 수 있으며 결과는 `Saved/Automation/BunniarPointer.json`에 저장된다.

활강 자세 조정: 캐릭터 Traversal의 LeftGripOffset / BunniarOffset, RelaxedLimbExtension, PullLeanDegrees, TravelLeanDegrees, GlideSwayDegrees를 조정할 수 있다. GlideBodyPose를 지정하면 기본 팔다리 자세 대신 해당 애니메이션을 사용하며 왼손 접촉과 부드러운 기울기는 유지된다. bUseRelaxedGlidePose를 끄면 기울기와 기본 팔다리 자세 보정도 끌 수 있다.

버니아 조준: 부착, 대상 하이라이트, 공중 대기는 보이는 커서(숨겨졌다면 화면 중앙 조준점)의 같은 광선을 사용한다. 사거리 안의 첫 표면에 부착하고, 사거리 밖에는 그 광선 위 사거리 끝에서 대기한다. 캐릭터와 목적지 사이의 장애물이 다른 대상으로 목적지를 바꾸지 않는다. 운반 중 물체의 실제 충돌은 기존대로 유지된다.


2026-10-24 연출 조정:

- 무기에서 버니아로 전환할 때 0.32초 동안 위치·회전·크기가 이어진다. 공중 대기를 놓으면 0.40초 동안 장착 위치로 돌아온다. 동행 상태로 돌아갈 때도 같은 전환을 사용한다. 버니아 Details의 EquipmentTravelSeconds / ReturnTravelSeconds로 조정한다.
- 공중에 대기 중인 버니아는 위치를 유지한 채 현재 조준 지점을 바라본다. 조준점이 버니아와 겹칠 때는 플레이어 쪽을 바라본다. AirGazeInterpSpeed로 시선 속도를 조정한다.
- 일반 벽·바닥 부착의 지점 록온 중에는 조준점과 부착 미리보기를 숨기고, 붙은 버니아에만 실제 윤곽선을 적용한다. 일반 벽·바닥은 하이라이트하지 않는다. 해제하면 버니아 메시의 기존 Custom Depth와 Stencil 설정을 복원한다.
- 빈 공간은 벌어진 십자선, 사거리 안의 일반 표면은 모인 십자선이다. 버니아 장착 시 일반 표면의 부착 위치에는 반지름 17cm의 작은 원 데칼이 나온다. 상호작용 가능한 대상은 원형 조준점으로 표시하며 NPC는 분홍, 운반 물체는 하늘색, 장치는 민트색, 무기 공격 대상은 주황 계열이다.
- 위 색상과 SurfaceAimRadius / SurfaceDecalColor는 캐릭터의 FieldActions에서 조정한다. 원 재질은 /Game/BBSB/Interaction/Materials/M_BunniarAimRing이며 Radius / LineWidth / BandCenter / Opacity / Tint / Glow / MaskContrast / InkStrength를 편집할 수 있다. 조준 표시는 기존 전투·상단 HUD와 퍼즐 초원 HUD가 공유한다.
- 첫 공중 점프는 약 0.10초간 7cm 내려온 뒤 기존 한 번의 상승력을 적용한다. 공중 대시도 짧게 내려온 뒤 14cm 위쪽으로 호를 그리며 진행하고 시작 높이보다 조금 아래에서 활강으로 이어진다. 반복 대시로 높이를 누적하지 않는다. Traversal의 AirAnticipationSeconds / AirAnticipationDrop / AirDashRise로 조정한다. Space·부착으로 중단하면 예약된 상승도 함께 취소한다.

버니아 전용 윤곽은 /Game/BBSB/Interaction/Materials/M_PP_BunniarOutline의 별도 Stencil 4 마스크로 렌더링한다. 다른 강조 대상과 윤곽이 합쳐지지 않도록 카메라에서 록온 동안만 적용하며, 기존 M_PP_FocusOutline과 월드의 후처리 설정은 수정하지 않는다.

2026-10-24 표시 보완: 부착 원은 크기를 유지하고 선 두께를 약 1.6배로 높였다. Opacity 1, MaskContrast 3으로 중심 선을 불투명하게 하고 InkStrength .25 / Glow 0으로 진한 잉크 색을 사용한다. 버니아 전용 윤곽의 CaptureColor는 흰색(1,1,1,1)이다.
