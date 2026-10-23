# 버니아 스켈레탈 애니메이션과 표면 부착

실제 메시: `/Game/Meshes/Characters/Bunniar/Bunniar_Normal`.
캐릭터와 같은 FK Control Rig → Level Sequence → 일반 AnimSequence 구조다. 게임 코드가 귀 뼈를 직접 덮어쓰지 않는다.

## 애니메이션 수정

1. `/Game/BBSB/AnimationAuthoring/Bunniar/L_BunniarAnimationStudio`를 연다.
2. 같은 폴더의 `LS_Bunniar_Idle`, `LS_Bunniar_Travel`, `LS_Bunniar_Equipped`, `LS_Bunniar_Attached` 중 원하는 시퀀스를 연다.
3. FK Control Rig의 Root·HeadLine·EarInner·EarOuter 컨트롤 키를 수정한다. 60fps, 7개 컨트롤이며 `Attached`가 접힌 귀 포즈다.
4. 시퀀서의 연결된 애니메이션 갱신/내보내기를 사용한다. 일괄 갱신은 언리얼 Python 콘솔에서 다음 파일을 실행하면 된다.

```python
exec(open(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())+'Python/BBSB/bake_bunniar_animations.py',encoding='utf-8').read())
```

이 스크립트는 실제 프로젝트의 `Content/Python/BBSB/`에 들어 있고, 결과는 프로젝트 `Saved/BunniarAnimationBake.json`에 기록하므로 환경을 옮겨도 사용할 수 있다.
`DA_BunniarAnimation`에서 네 런타임 클립, 편집용 원본 참조, 블렌드 시간, 이동 판정 속도, 동행/장착 크기, 등 접점과 표면 간격을 수정할 수 있다. 원본 FBX의 Root 스케일 100은 임포트 좌표계의 일부이므로 그대로 보존했다.

`Attached`는 부착할 때 한 번 재생하고 마지막 포즈를 유지한다. 해제하면 일반 장착 애니메이션으로 돌아가며, 다음 부착에서 다시 시작한다. 현재 부착 시퀀스는 0–60프레임의 1초 클립이며, FK Control Rig 섹션은 내보내기의 마지막 샘플까지 포함하도록 0–61프레임을 덮는다. 끝 샘플이 섹션 밖의 기본 포즈로 돌아가지 않도록 이 범위를 유지한다.

## 부착과 퍼즐 연결

- Tab으로 버니아를 장착하고, 도달 가능한 충돌 표면을 왼쪽 클릭한 채 유지한다.
- 버니아도 무기와 같은 마우스 입력과 Beat / Hold / Shake / Flick / Dive 인식기를 사용한다. 부착·해제는 커서 표시나 게임/UI 입력 모드를 바꾸지 않는다.
- 클릭한 실제 위치와 표면 법선에 등을 맞춘다. 특정 장치의 미리 정해진 접점은 필요 없다.
- 얼굴은 표면의 바깥쪽 노말을 향하고 등은 표면에 닿는다. `DA_BunniarAnimation`의 `Face Direction`은 실제 메시 앞쪽인 `(0, 1, 0)`, `Back Contact Point`는 `(0, -54, 70)`이다. 시각 컴포넌트의 회전·크기·오프셋을 포함해 접점을 계산하므로 모델 원본을 회전시킬 필요가 없다.
- 누르고 있는 동안 귀가 접히고 그 지점에 남는다. 움직이는 표면도 컴포넌트 좌표계로 따라간다.
- 부착할 때는 `Attach Travel Seconds`의 0.24초 동안 조준한 표면으로 날아가며 방향과 크기가 전환된다. 안착한 크기는 별도 장착 크기 대신 `Companion Scale`의 작은 동행 크기를 사용한다. 놓으면 다시 떠 있는 위치로 이동하고, 이동 중 놓아도 제스처 점유를 유지하지 않는다.
- 드래그 입력은 퍼즐에 전달하며, 버니아의 최초 접점을 미끄러뜨리지 않는다. 물건을 옮기는 퍼즐에서는 물건 쪽 컴포넌트가 움직이고 버니아가 그 물건을 따라간다.
- 부착 중에는 카메라 회전을 제한하면서 공통 마우스 이동값을 드래그와 제스처 인식에 함께 전달한다.
- 버튼 해제·회피·사망·장착 해제·창 포커스 상실·플레이 종료 때 부착과 제스처 점유를 해제한다.
- `BBSBBunniarSurface` 인터페이스의 Attached / Dragged / Gesture / Released를 구현하면 장치를 연결할 수 있다. 일반 표면에는 이 인터페이스가 필요 없다.

## 확인 기록

`surface-verification.json`의 실제 PIE 33개 검사 통과. 일반 지면/벽, 움직이는 벽, 접힌 귀의 실제 뼈 포즈, 드래그 중 시점 고정, 장착 예약, 원형 메뉴와 포인터 복구, 기존 크랭크 연동을 포함한다.
`bake-verification.json`에서 네 편집용 시퀀스의 연결과 재내보내기를 확인했다. `before/`와 `after/`에 복구 자료가 있다.

공통 무기 입력 경로의 회귀 검증은 `BBSB.Field.Equipment.BunniarSharedPointer`다. 초원 맵을 연 상태에서 콘솔에 `Automation RunTests BBSB.Field.Equipment.BunniarSharedPointer`를 실행하면 자체 PIE 세션에서 검사한다. Slate 마우스 이벤트가 뷰포트와 PlayerInput을 거쳐 사다리 드래그, 물건 운반, Shake 분리, 방향성 Flick까지 전달되는 45개 검사를 통과했다. 커서·입력 모드 유지, 부착 중 카메라 고정, 버튼 해제도 포함하며 결과는 `Saved/Automation/BunniarPointer.json`에 기록한다.

부착 방향·안정성의 후속 검증은 `work/bunniar-attachment/verification.json`에 있다. 바닥·벽·기울어진 벽, 표면 이동과 회전, 다시 부착하기, 작은 크기, 실제 비행과 비행 도중 해제·장비 전환을 포함한 60개 검사를 통과했다. 각 면에서 2.65초씩 160프레임을 샘플링해 루프 경계를 지나도 7개 뼈의 자세가 유지됨을 확인했다.

## 에임을 바라보는 시선

동행 중인 버니아는 무기의 `Visual Aim Point`를 바라본다. 부착한 동안에는 얼굴을 표면 바깥쪽 노말에 맞추는 동작이 우선한다.

주인공의 `AimGaze` 컴포넌트가 같은 지점을 읽는다. 기본 설정은 좌우 최대 50도·상하 최대 25도의 70%만 반영하고 속도 10으로 보간한다. `Enable Head Gaze`, `Maximum Yaw`, `Maximum Pitch`, `Aim Weight`, `Interp Speed`를 캐릭터 Details에서 조정할 수 있다.

`ABP_PlayerAimPostProcess`는 기존 `ABP_PlayerEditable`과 `CR_PlayerFootIK`가 완성한 포즈를 입력으로 받아 목 2개와 머리만 보정한다. 원본 이동 그래프와 지형 Foot IK는 유지된다. 이 자산은 전용 네이티브 부모의 자세 보정을 연결하는 Input Pose → Output Pose 구조다.

시선 검증 `work/bunniar-attachment/gaze-verification.json`에서 19개 검사를 통과했다. 실제 머리 뼈의 좌우·상하 반응과 제한, 몸 방향 유지, 버니아의 같은 에임 추적, 기능 끄기와 복귀, 시선 레이어를 통과하는 이동 발걸음을 확인했다. 기존 이동 애니메이션과 Foot IK 자산의 디스크 해시도 유지됐다.

## 공통 상호작용과 접힌 귀의 현행 시제품

2026-10-22의 새 설계 적용은 `CommonInteraction.md`에 정리했다. NPC와 작은 물건은 실제 비행 도착을 별도로 통지받는다. NPC의 회수는 대화 종료와 분리되어 있고, 운반 물건은 좌클릭 단독 자유 시점 / 우클릭 추가 시점 고정·카메라 기준 이동 / 우클릭 Flick의 실제 이동 속도 보존을 사용한다. 기존 드래그 퍼즐의 좌클릭 시점 고정 규칙도 유지된다.

`LS_Bunniar_Attached`의 귀는 안쪽 L/R +10/-10도, 바깥쪽 -10/+10도, 안쪽 크기 .7 및 바깥쪽 크기 .55로 짧은 M 실루엣을 사용한다. 재생 구간 0~60과 FK 섹션 0~61, 끝 자세 고정은 동일하다. Root와 HeadLine은 이전 포즈를 보존하며 연결된 `A_Bunniar_Attached`를 갱신했다. 귀 모양과 길이는 원본 메시 대신 이 네 컨트롤에서 편집한다.

짧은 M 포즈의 실제 뒤쪽 깊이를 렌더로 측정한 뒤 현행 DA의 Back Contact Point를 (0,-25,70)으로 보정했다. 이전 포즈의 -54 값은 역사 기록이며 현행 접힘에는 사용하지 않는다. 마네킹 얼굴은 머리 뼈에서 앞쪽 13.5cm 지점으로 맞췄고 NPC의 접촉 높이와 앞쪽 거리는 Details에서 조절할 수 있다.

