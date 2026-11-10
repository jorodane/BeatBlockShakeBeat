#include "Stage01/BBSBStage01.h"
#include "Save/BBSBSession.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Character/BBSBRespawn.h"
#include "Character/BBSBTraversal.h"
#include "Field/BBSBRepairTool.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Hunting/BBSBItem.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "InputCoreTypes.h"
ABBSBStage01Director::ABBSBStage01Director()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StoryOrigin")));
 RepairTool=TSoftObjectPtr<UBBSBItemDefinition>(FSoftObjectPath(TEXT("/Game/BBSB/Stage01/Items/DA_RepairTool.DA_RepairTool")));
 TrioReward=TSoftObjectPtr<UBBSBItemDefinition>(FSoftObjectPath(TEXT("/Game/BBSB/Stage01/Items/DA_TrioResonance.DA_TrioResonance")));
 HarmonyOrb=TSoftObjectPtr<UBBSBItemDefinition>(FSoftObjectPath(TEXT("/Game/BBSB/Stage01/Items/DA_QuietHarmonyOrb.DA_QuietHarmonyOrb")));
}
void ABBSBStage01Director::BeginPlay()
{
 Super::BeginPlay();StartedAt=GetWorld()->GetTimeSeconds();NextGrabAt=StartedAt+8;
 for(TActorIterator<ABBSBStage01Device> It(GetWorld());It;++It)if(It->Director==this)Devices.Add(*It);
 for(TActorIterator<ABBSBStage01Singer> It(GetWorld());It;++It)if(It->Director==this)Singers.Add(*It);
 if(bPersistProgress && !UBBSBSessionSubsystem::Owns(GetWorld()))if(auto* Save=Cast<UBBSBStage01Save>(UGameplayStatics::LoadGameFromSlot(SaveSlotName,0));Save && Save->Version==1)
 { Phase=Save->Phase;Solved=Save->Solved;Quieted=Save->Quieted;bRewardGranted=Save->bRewardGranted;Tea=Save->Tea; }
 for(auto* P:{CabinCheckpoint.Get(),UtilityCheckpoint.Get(),ArtCheckpoint.Get()})if(P)P->OnActivated.AddDynamic(this,&ABBSBStage01Director::RefillAtCheckpoint);
 ApplyProgress();
}
void ABBSBStage01Director::BindPlayer()
{
 Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if(!Player || !Player->InputComponent || !Player->FieldActions->BunniarEquipment)return;
 auto* PC=Cast<APlayerController>(Player->GetController());if(!PC || !PC->IsLocalController())return;
 Controls=NewObject<UInputComponent>(this,TEXT("Stage01StoryKeys"));Controls->RegisterComponent();Controls->Priority=20;
 Controls->BindKey(EKeys::R,IE_Pressed,this,&ABBSBStage01Director::UseTea);PC->PushInputComponent(Controls);
 Player->FieldActions->bShowPracticeHUD=false;PC->ClientSetHUD(ABBSBStage01HUD::StaticClass());bBound=true;
 UpdateTutorialGate();
 if(!HasTool())Player->FieldActions->RequestEquipment(0);else if(Phase!=EBBSBStage01Phase::Complete)GiveRepairTool();
 if(Phase==EBBSBStage01Phase::Opening)if(auto* Buddy=Cast<ABBSBBunniarCompanion>(Player->FieldActions->BunniarEquipment))Buddy->BeginOpeningRest(BookstandPose);
 if(CabinCheckpoint && (!Player->Respawn->ActivePoint || !Player->Respawn->ActivePoint->bTravelAvailable))CabinCheckpoint->ActivateFor(Player);
 if(Phase==EBBSBStage01Phase::Opening)Say(TEXT("레파라도: ...또 삑사리야? 입에 반창고까지 붙였는데.\n버니아는 입으로 노래하는 게 아니었다."),6);
 else Say(Objective(),5);
}
void ABBSBStage01Director::EndPlay(const EEndPlayReason::Type Reason)
{
 EndGrab();FinishReveal();
 if(Player)
 {
  if(bMoveIgnored)if(auto* PC=Cast<APlayerController>(Player->GetController()))PC->SetIgnoreMoveInput(false);
  Player->FieldActions->bTutorialEquipmentGate=Player->FieldActions->bTutorialTraversalGate=false;Player->FieldActions->TutorialWeaponInstance.Invalidate();
  Player->FieldActions->bTutorialConcertoGate=false;
  if(auto* PC=Cast<APlayerController>(Player->GetController());PC && Controls)PC->PopInputComponent(Controls);
 }
 if(CaptainAudio)CaptainAudio->DestroyComponent();Super::EndPlay(Reason);
}
void ABBSBStage01Director::Say(const FString& Text,float Seconds) { Caption=Text;CaptionUntil=GetWorld()->GetTimeSeconds()+Seconds; }
void ABBSBStage01Director::Advance(EBBSBStage01Phase Next)
{
 if(Next==Phase)return;
 if((Next==EBBSBStage01Phase::Trio || Next==EBBSBStage01Phase::Captain) && UBBSBSessionSubsystem::Owns(GetWorld()))
  UBBSBSessionSubsystem::Get(GetWorld())->SaveNow(Next==EBBSBStage01Phase::Trio ? TEXT("중간보스 전"):TEXT("보스 전"),true);
 Phase=Next;NextGrabAt=GetWorld()->GetTimeSeconds()+8;ApplyProgress();UpdateTutorialGate();SaveProgress();Say(Objective());
}
void ABBSBStage01Director::SaveProgress()
{
 if(UBBSBSessionSubsystem::Owns(GetWorld())) { UBBSBSessionSubsystem::Get(GetWorld())->MarkChanged();return; }
 if(!bPersistProgress)return;auto* Save=Cast<UBBSBStage01Save>(UGameplayStatics::CreateSaveGameObject(UBBSBStage01Save::StaticClass()));
 Save->Phase=Phase;Save->Solved=Solved;Save->Quieted=Quieted;Save->Tea=Tea;Save->bRewardGranted=bRewardGranted;UGameplayStatics::SaveGameToSlot(Save,SaveSlotName,0);
}
void ABBSBStage01Director::UpdateTutorialGate()
{
 if(!Player)return;auto* A=Player->FieldActions.Get();A->bTutorialEquipmentGate=A->bTutorialTraversalGate=bEnforceTutorialLoadout && Phase!=EBBSBStage01Phase::Complete;A->TutorialWeaponInstance=HasTool() ? ToolInstance:FGuid();
 if(UtilityCheckpoint)UtilityCheckpoint->bTravelAvailable=Phase>=EBBSBStage01Phase::Cargo;
 A->bTutorialConcertoGate=bEnforceTutorialLoadout && Phase<EBBSBStage01Phase::Restock;
 if(ArtCheckpoint)ArtCheckpoint->bTravelAvailable=Phase>=EBBSBStage01Phase::Restock;
}
bool ABBSBStage01Director::GiveRepairTool()
{
 if(!Player)return false;auto* H=Player->Hunting.Get();auto* Def=RepairTool.LoadSynchronous();if(!Def)return false;
 for(const auto& E:H->Items)if(E.Item==Def)ToolInstance=E.InstanceId;
 for(const auto& E:H->EquippedItems)if(E.Item==Def)ToolInstance=E.InstanceId;
 if(!ToolInstance.IsValid())ToolInstance=H->AddWeaponInstance(Def,1,1);
 if(!ToolInstance.IsValid()) { Say(TEXT("도구를 챙길 공간이 필요해요. Tab으로 가방 한 칸을 비워주세요."));return false; }
 Player->FieldActions->TutorialWeaponInstance=ToolInstance;
 // Normal inventory transactions swap the displaced weapon back into the tool's previous bag cell.
 bool Equipped=false;for(const auto& E:H->EquippedItems)if(E.InstanceId==ToolInstance)Equipped=true;
 if(!Equipped && !H->PlaceOwnedItem(ToolInstance,H->Columns*H->Rows)) { Say(TEXT("도구는 가방에 보관했어요. Tab에서 장비 칸에 놓아주세요."));return false; }
 H->NotifyGrowthChanged();bToolEquipPending=true;return true;
}
ABBSBStage01Device* ABBSBStage01Director::FindNode(EBBSBStage01Node Kind) const { for(ABBSBStage01Device* D:Devices)if(D && D->Kind==Kind)return D;return nullptr; }
bool ABBSBStage01Director::AllQuiet(EBBSBStage01Voice Voice) const { bool Found=false;for(ABBSBStage01Singer* S:Singers)if(S && S->Voice==Voice) { Found=true;if(!S->bDefeated)return false; }return Found; }
void ABBSBStage01Director::ApplyProgress()
{
 for(ABBSBStage01Device* D:Devices)if(D)
 {
  bool Open=Solved.Contains(D->NodeId);
  if(D->Kind==EBBSBStage01Node::UtilityDoor)Open=Phase>=EBBSBStage01Phase::ShopBattle;
  if(D->Kind==EBBSBStage01Node::TrioExit)Open=Phase>=EBBSBStage01Phase::Restock;
  if(D->Kind==EBBSBStage01Node::ReadyGate)Open=Phase>=EBBSBStage01Phase::ApproachCaptain;
  if(D->Kind!=EBBSBStage01Node::CabinDoor || !D->HasActorBegunPlay())D->SetSolved(Open);
  else D->bSolved=Open;
 }
 if(CaptainLight)if(auto* L=CaptainLight->FindComponentByClass<ULightComponent>())L->SetVisibility(Phase>=EBBSBStage01Phase::ReturnForReady);
 for(ABBSBStage01Singer* S:Singers)if(S && Quieted.Contains(S->SingerId)) { S->bDefeated=true;S->Health=0;S->SetActorEnableCollision(false); }
}
void ABBSBStage01Director::Tick(float DT)
{
 Super::Tick(DT);if(!bBound)BindPlayer();if(!Player || !bBound)return;
 const double Now=GetWorld()->GetTimeSeconds();if(Now>CaptionUntil)Caption.Empty();
 UpdateTutorialGate();TickReveal();
 if(Phase>=EBBSBStage01Phase::Restock && !bRewardGranted)TryGrantReward();
 if(HasTool() && Phase!=EBBSBStage01Phase::Complete && !ToolInstance.IsValid())GiveRepairTool();
 if(bToolEquipPending && Player->FieldActions->RequestWeaponInstance(ToolInstance))bToolEquipPending=false;
 if(Player->Combat->bDead || Player->Respawn->IsSequenceActive())
 {
  EndGrab();FinishReveal();if(bLadderMoving) { bLadderMoving=false;if(bMoveIgnored)if(auto* PC=Cast<APlayerController>(Player->GetController()))PC->SetIgnoreMoveInput(false);bMoveIgnored=false; }
  bPlayerWasDead=true;return;
 }
 if(bRecoverFromCliffFall && Player->GetActorLocation().Z<RecoveryFloorZ) { Player->Respawn->BeginDeath();return; }
 if(bPlayerWasDead)
 {
  bPlayerWasDead=false;for(ABBSBStage01Singer* S:Singers)if(S && !Quieted.Contains(S->SingerId) && (S->Voice==EBBSBStage01Voice::Trio || S->Voice==EBBSBStage01Voice::Captain))S->Rearm();NextGrabAt=Now+8;
 }
 if(bLadderMoving)
 {
  const float T=FMath::Clamp(float((Now-LadderStarted)/LadderDuration),0.f,1.f);
  // Gain clearance beside the cliff before moving onto its top. Keep the sweep active.
  const float Rise=FMath::SmoothStep(0.f,.74f,T),Across=FMath::SmoothStep(.70f,1.f,T);
  FVector P=FMath::Lerp(LadderFrom,LadderTo,Across);P.Z=FMath::Lerp(LadderFrom.Z,LadderTo.Z+LadderClearance,Rise)-LadderClearance*FMath::SmoothStep(.94f,1.f,T);
  if(!LadderAccess.IsNearlyZero())
  {
   const FVector UnderHole(LadderAccess.X,LadderAccess.Y,LadderFrom.Z),OverHole(LadderAccess.X,LadderAccess.Y,LadderTo.Z+LadderClearance);
   if(T<.30f)P=FMath::Lerp(LadderFrom,UnderHole,FMath::SmoothStep(0.f,.30f,T));
   else if(T<.72f)P=FMath::Lerp(UnderHole,OverHole,FMath::SmoothStep(.30f,.72f,T));
   else P=FMath::Lerp(OverHole,LadderTo+FVector(0,0,LadderClearance),FMath::SmoothStep(.72f,.96f,T))-FVector(0,0,LadderClearance*FMath::SmoothStep(.96f,1.f,T));
  }
  FHitResult Hit;Player->SetActorLocation(P,true,&Hit,ETeleportType::None);
  if(T>=1 || (Hit.bBlockingHit && T>.2f)) { bLadderMoving=false;Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);if(bMoveIgnored)if(auto* PC=Cast<APlayerController>(Player->GetController()))PC->SetIgnoreMoveInput(false);bMoveIgnored=false;Player->FieldActions->ResetActionState(); }
 }
 if(Phase==EBBSBStage01Phase::Cargo && Solved.Contains(TEXT("CargoTrapdoor")) && Player->GetActorLocation().Z>RoofEntry.Z-130)Advance(EBBSBStage01Phase::Rooftops);
 if(Phase==EBBSBStage01Phase::Rooftops && FVector::Dist2D(Player->GetActorLocation(),TrioCenter)<1100 && Player->GetActorLocation().Z<TrioCenter.Z+500)
 { Advance(EBBSBStage01Phase::Trio);Say(TEXT("버니아 트리오: 빰!\n레파라도: ...귀찮게 됐네."),4); }
 if(Phase==EBBSBStage01Phase::ApproachCaptain && FVector::Dist2D(Player->GetActorLocation(),CaptainCenter)<1500 && FMath::Abs(Player->GetActorLocation().Z-CaptainCenter.Z)<400)
 { Advance(EBBSBStage01Phase::Captain);Say(TEXT("상단장: ...분명 어제까지는 잘 됐는데.\n레파라도: 일단, 그 삑사리부터 멈추죠.")); }
 TickCombat(DT);TickGrab(DT);TickMusic();
}
void ABBSBStage01Director::TalkTo(ABBSBStage01NPC* NPC)
{
 if(!Player || !NPC)return;
 if(NPC->CarNumber==6)
 {
  if(Phase<=EBBSBStage01Phase::Outside) { Advance(EBBSBStage01Phase::FindTools);NPC->Lines={FText::FromString(TEXT("안쪽에서 다 같이 삑사리를 내고 있어. 문을 잠가뒀지.")),FText::FromString(TEXT("네 나무 판과 고무 망치? 위험 물품이라며 저 위에 압수해뒀더라.")),FText::FromString(TEXT("밖의 상자와 사다리를 써봐. 도구를 찾으면 돌아와."))}; }
  else if(Phase==EBBSBStage01Phase::ReturnToMerchant) { Advance(EBBSBStage01Phase::ShopBattle);NPC->Lines={FText::FromString(TEXT("찾았구나! 뒤쪽 문을 열어줄게.")),FText::FromString(TEXT("모두 같은 박자에 쏴. 한곳에 서 있지 말고, 쏘고 난 틈을 노려."))}; }
  else NPC->Lines={FText::FromString(Phase<EBBSBStage01Phase::Cargo ? TEXT("도구를 되찾고 나면, 저 뒤 문으로 들어가줘."):TEXT("한결 조용하네. 화물차 안의 장치로 위쪽 길을 열 수 있을 거야."))};
 }
 else
 {
  if(Phase==EBBSBStage01Phase::Restock) { Advance(EBBSBStage01Phase::FindLight);NPC->Lines={FText::FromString(TEXT("트리오가 남긴 공명 조각은 옆 교환대에서 구슬로 바꿀 수 있어.")),FText::FromString(TEXT("표식에서는 회복 차를 채울 수 있어. 차는 R로 마셔.")),FText::FromString(TEXT("상단장님이 혼자 연주 중이야. 1호차에서 조명 레버를 켜줄래?"))}; }
  else if(Phase==EBBSBStage01Phase::ReturnForReady)NPC->Lines={FText::FromString(TEXT("혼자 계속 연주하고 계셨구나...")),FText::FromString(TEXT("보급을 마치면 버니아를 무대 쪽 문에 붙여 옆으로 열어줘."))};
  else NPC->Lines={FText::FromString(TEXT("상점과 표식은 언제든 써도 돼. 무대에 들어가기 전에 준비하고 가."))};
 }
 Player->Interaction->BeginDialogue(NPC);
}
bool ABBSBStage01Director::UseDevice(ABBSBStage01Device* D)
{
 if(!D || !Player)return false;
 switch(D->Kind)
 {
 case EBBSBStage01Node::CabinDoor: if(Phase==EBBSBStage01Phase::Opening)Advance(EBBSBStage01Phase::Outside);break;
 case EBBSBStage01Node::LowerLadder:case EBBSBStage01Node::ClimbLadder:
  if(D->bSolved || D->Kind==EBBSBStage01Node::ClimbLadder)return D->MountLadder();break;
 case EBBSBStage01Node::ToolCache:
  if(Phase>=EBBSBStage01Phase::ReturnToMerchant) { Say(TEXT("나무 판과 고무 망치는 이미 챙겼다."));break; }
  if(Phase<EBBSBStage01Phase::FindTools) { Say(TEXT("압수 물품... 6호차 상인에게 먼저 물어보자."));return false; }
  if(!GiveRepairTool())return false;Advance(EBBSBStage01Phase::ReturnToMerchant);Say(TEXT("나무 판과 고무 망치를 되찾았다!\n좌클릭: 빠른 한 방 · 우클릭 유지 + 좌클릭: 판을 대고 세 번"),7);break;
 case EBBSBStage01Node::UtilityDoor: if(Phase<EBBSBStage01Phase::ShopBattle) { Say(TEXT("6호차 상인이 문을 잠갔다. 먼저 말을 걸어보자."));return false; }break;
 case EBBSBStage01Node::CargoButton: if(Phase<EBBSBStage01Phase::Cargo)return false;break;
 case EBBSBStage01Node::CargoLever:
  if(!Solved.Contains(TEXT("CargoPower"))) { Say(TEXT("화물 전원을 먼저 켜주세요."));return false; }break;
 case EBBSBStage01Node::CargoLift:
  if(!Solved.Contains(TEXT("CargoWinch"))) { Say(TEXT("화물 고정 레버를 먼저 내려주세요."));return false; }
  if(D->bSolved) { Solved.Remove(D->NodeId);D->SetSolved(false);SaveProgress();Say(TEXT("승강 발판을 아래로 부른다."));return true; }
  if(D->Targets.IsEmpty() || !Player->GetMovementBase() || Player->GetMovementBase()->GetOwner()!=D->Targets[0]) { Say(TEXT("승강 발판 위에 올라선 뒤 작동시켜 주세요."));return false; }
  D->SetSolved(true);Solved.Add(D->NodeId);SaveProgress();Say(TEXT("승강 발판이 올라간다. 위쪽 덮개에 버니아를 붙여 옆으로 밀어 열자."));return true;
 case EBBSBStage01Node::Trapdoor:
  if(!Solved.Contains(TEXT("CargoWinch"))) { Say(TEXT("고정 레버가 잠겨 있다."));return false; }break;
 case EBBSBStage01Node::TrioExit: if(Phase<EBBSBStage01Phase::Restock) { Say(TEXT("삑사리가 문 장치까지 울리고 있다. 트리오를 진정시키자."));return false; }break;
 case EBBSBStage01Node::CaptainNote:Say(TEXT("상단장의 쪽지: 어제까진 괜찮았는데, 오늘은 한 음도 마음대로 나가지 않는다.\n한 번만 더. 이번에는 제대로 연주할 수 있겠지."),8);break;
 case EBBSBStage01Node::LightLever:
  if(Phase<EBBSBStage01Phase::FindLight) { Say(TEXT("아직 조명 요청을 받지 않았다. 2호차로 가보자."));return false; }
  if(Phase==EBBSBStage01Phase::FindLight) { Advance(EBBSBStage01Phase::ReturnForReady);BeginReveal(); }
  Say(TEXT("스포트라이트 아래, 상단장이 홀로 연주하고 있다.\n2호차로 돌아가 준비를 마치자."),7);break;
 case EBBSBStage01Node::BuyTea:
  if(Tea>=3) { Say(TEXT("회복 차가 가득 찼어요. 표식에서도 리필할 수 있어요."));return false; }
  if(!Player->Hunting->SpendNotes(10)) { Say(TEXT("노트가 10개 필요해요."));return false; }++Tea;SaveProgress();Say(TEXT("회복 차 +1 · R로 마시기"));return true;
 case EBBSBStage01Node::BuyParts:
 {
  auto* Item=LoadObject<UBBSBItemDefinition>(nullptr,TEXT("/Game/BBSB/Hunting/Items/DA_Item_InkDrop.DA_Item_InkDrop"));
  if(!Player->Hunting->SpendNotes(15)) { Say(TEXT("노트가 15개 필요해요."));return false; }
  if(Player->Hunting->AddItem(Item,1)!=1) { Player->Hunting->AddNotes(15);Say(TEXT("가방에 빈칸이 필요해요."));return false; }Say(TEXT("정비 부품을 샀어요. Tab에서 확인하세요."));return true;
 }
 case EBBSBStage01Node::TuneReward:
 {
  auto* Reward=TrioReward.LoadSynchronous();auto* Orb=HarmonyOrb.LoadSynchronous();
  if(!Reward || !Orb || Player->Hunting->ItemCount(Reward)<1) { Say(TEXT("트리오의 공명 조각을 가져와 주세요."));return false; }
  if(Player->Hunting->AddItem(Orb,1)!=1) { Say(TEXT("가방 한 칸을 비워주세요."));return false; }
  if(!Player->Hunting->ConsumeItem(Reward,1)) { Player->Hunting->ConsumeItem(Orb,1);return false; }
  Say(TEXT("잔잔한 공명 구슬을 얻었어요.\nTab에서 도구의 둥근 슬롯에 끼우면 공격력이 올라가요."),8);return true;
 }
 case EBBSBStage01Node::ReadyGate:
  if(Phase<EBBSBStage01Phase::ReturnForReady) { Say(TEXT("1호차의 조명을 켜고 돌아와 주세요."));return false; }
  if(Phase==EBBSBStage01Phase::ReturnForReady)Advance(EBBSBStage01Phase::ApproachCaptain);break;
 case EBBSBStage01Node::Departure:
  if(Phase!=EBBSBStage01Phase::Complete) { Say(TEXT("아직 상단장과의 연주가 끝나지 않았다."));return false; }Say(TEXT("상단의 소동을 해결했어요. 주변을 자유롭게 둘러볼 수 있어요."));return true;
 }
 Solved.Add(D->NodeId);D->SetSolved(true);SaveProgress();return true;
}
void ABBSBStage01Director::ReportQuiet(ABBSBStage01Singer* S)
{
 if(!S)return;Quieted.Add(S->SingerId);SaveProgress();
 if(S->Voice==EBBSBStage01Voice::Utility && Phase==EBBSBStage01Phase::ShopBattle && AllQuiet(EBBSBStage01Voice::Utility)) { ClearEncounterShots();Advance(EBBSBStage01Phase::Cargo);Say(TEXT("6호차가 조용해졌다. 5호차 장치를 이용해 지붕으로 우회하자."));if(UtilityCheckpoint)UtilityCheckpoint->ActivateFor(Player); }
 if(S->Voice==EBBSBStage01Voice::Trio && Phase==EBBSBStage01Phase::Trio && AllQuiet(EBBSBStage01Voice::Trio))
 {
  EndGrab();ClearEncounterShots();TryGrantReward();
  Advance(EBBSBStage01Phase::Restock);Say(TEXT("트리오가 조용해졌다. 공명 조각을 챙겨 2호차 상점으로 가보자."),7);
 }
 if(S->Voice==EBBSBStage01Voice::Captain && Phase==EBBSBStage01Phase::Captain)
 {
  EndGrab();ClearEncounterShots();Advance(EBBSBStage01Phase::Complete);
  auto* Glide=LoadObject<UBBSBModuleDefinition>(nullptr,TEXT("/Game/BBSB/Growth/Modules/DA_Module_Glide.DA_Module_Glide"));if(Glide)Player->EquipmentGrowth->GrantModule(Glide,true);
  Say(TEXT("상단장: ...내 탓만은 아니었던 거구나.\n레파라도: 그러니까, 이제 좀 쉬세요.\n활강 모듈 획득 · 반 발자국 더 나아갈 준비가 되었다"),12);
 }
}
void ABBSBStage01Director::RefillAtCheckpoint(ABBSBCharacter* Character) { if(Character!=Player || Phase<EBBSBStage01Phase::Restock)return;Tea=3;SaveProgress();Say(TEXT("회복 차 3 / 3 · R로 마시기")); }
void ABBSBStage01Director::UseTea()
{
 if(!Player || Tea<=0 || Player->Combat->bDead || Player->Respawn->IsSequenceActive() || bGrabActive)return;
 if(Player->Combat->Health>=Player->Combat->MaximumHealth) { Say(TEXT("생명력이 가득 찼어요."));return; }
 --Tea;Player->Combat->Health=FMath::Min(Player->Combat->MaximumHealth,Player->Combat->Health+35);SaveProgress();Say(TEXT("회복 차 · 생명력 +35"));
}
void ABBSBStage01Director::TryGrantReward()
{
 if(bRewardGranted || !Player || GetWorld()->GetTimeSeconds()-LastRewardTry<1)return;LastRewardTry=GetWorld()->GetTimeSeconds();auto* Item=TrioReward.LoadSynchronous();if(!Item)return;
 // A full bag keeps the reward pending; freeing a cell claims it without repeating the boss.
 if(Player->Hunting->ItemCount(Item)>0 || Player->Hunting->AddItem(Item,1)==1) { bRewardGranted=true;SaveProgress(); }
}
void ABBSBStage01Director::BeginReveal()
{
 if(!Player || RevealCamera)return;auto* PC=Cast<APlayerController>(Player->GetController());if(!PC)return;
 Player->Interaction->EndDialogue();FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
 RevealCamera=GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),FTransform::Identity,Params);if(!RevealCamera)return;
 // The global outline output consumes a pass supplied by the normal player camera.
 // Carry that input into the cinematic camera as well.
 RevealCamera->GetCameraComponent()->PostProcessSettings=Player->GetFollowCamera()->PostProcessSettings;
 RevealCamera->GetCameraComponent()->PostProcessBlendWeight=Player->GetFollowCamera()->PostProcessBlendWeight;
 RevealPrevious=PC->GetViewTarget();RevealUntil=GetWorld()->GetTimeSeconds()+RevealSeconds;
 PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);Player->GetCharacterMovement()->StopMovementImmediately();
 TickReveal();PC->SetViewTargetWithBlend(RevealCamera,.35f);
}
void ABBSBStage01Director::TickReveal()
{
 if(!RevealCamera)return;if(GetWorld()->GetTimeSeconds()>=RevealUntil) { FinishReveal();return; }
 const float T=1.f-FMath::Clamp(float((RevealUntil-GetWorld()->GetTimeSeconds())/FMath::Max(.5f,RevealSeconds)),0.f,1.f);
 const FVector P=CaptainCenter+FMath::Lerp(FVector(-650,-1100,380),FVector(-450,-900,270),T);
 RevealCamera->SetActorLocationAndRotation(P,(CaptainCenter+FVector(0,0,80)-P).Rotation());
}
void ABBSBStage01Director::FinishReveal()
{
 if(!RevealCamera)return;if(auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr)
 { PC->SetViewTargetWithBlend(RevealPrevious.IsValid() ? RevealPrevious.Get():Player.Get(),.35f);PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false); }
 RevealCamera->SetLifeSpan(.45f);RevealCamera=nullptr;RevealPrevious.Reset();
}
bool ABBSBStage01Director::MoveAlongLadder(FVector Destination,float Seconds,float Clearance,FVector Access)
{
 if(!Player || bLadderMoving || bGrabActive || Player->Respawn->IsSequenceActive())return false;
 auto* PC=Cast<APlayerController>(Player->GetController());if(!PC)return false;
 Player->Traversal->DropParkour();Player->Traversal->EndGlide();Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
 LadderFrom=Player->GetActorLocation();LadderTo=Destination;LadderAccess=Access;LadderStarted=GetWorld()->GetTimeSeconds();LadderDuration=Seconds;LadderClearance=Clearance;bLadderMoving=true;
 PC->SetIgnoreMoveInput(true);bMoveIgnored=true;Player->FieldActions->InterruptAction(EBBSBActionState::Transition,Seconds+.1f);Say(TEXT("사다리를 잡고 올라간다"),Seconds);return true;
}
void ABBSBStage01Director::ResetDraft()
{
 EndGrab();Phase=EBBSBStage01Phase::Opening;Solved.Reset();Quieted.Reset();bRewardGranted=false;Tea=0;for(ABBSBStage01Singer* S:Singers)if(S)S->Rearm();ApplyProgress();UpdateTutorialGate();if(Player)Player->FieldActions->RequestEquipment(0);SaveProgress();
}
void ABBSBStage01Director::TickMusic()
{
 if(!Player)return;const double Now=GetWorld()->GetTimeSeconds();
 if(!CaptainAudio)
 {
  CaptainAudio=NewObject<UAudioComponent>(this,TEXT("CaptainDistantSolo"));CaptainAudio->bAutoActivate=false;CaptainAudio->bIsUISound=true;CaptainAudio->RegisterComponent();
  CaptainMelody=NewObject<USoundWaveProcedural>(this);CaptainMelody->SetSampleRate(24000);CaptainMelody->NumChannels=1;CaptainMelody->Duration=INDEFINITELY_LOOPING_DURATION;CaptainAudio->SetSound(CaptainMelody);
 }
 const float Distance=FVector::Dist(Player->GetActorLocation(),CaptainCenter),Volume=Phase==EBBSBStage01Phase::Complete ? 0.f:FMath::Clamp(1.f-Distance/6500.f,0.f,1.f)*.22f;
 CaptainAudio->SetVolumeMultiplier(Volume);
 if(Volume>.001 && Now-LastMelodyAt>.64)
 {
  LastMelodyAt=Now;const float Notes[]={220.f,261.626f,329.628f,293.665f,246.942f,293.665f,349.228f,329.628f};const int32 Step=FMath::FloorToInt(Now/.68)%8;
  TArray<int16> Samples;Samples.SetNumUninitialized(15600);for(int32 I=0;I<Samples.Num();++I)
  { const float T=I/24000.f,Env=FMath::Min(1.f,T/.012f)*FMath::Exp(-T*5.f);const float F=Notes[Step]*(Step==6 ? 1.014f:1.f);Samples[I]=int16((FMath::Sin(2*PI*F*T)+.25f*FMath::Sin(4*PI*F*T))*Env*3500); }
  CaptainMelody->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Samples.Num()*sizeof(int16));if(!CaptainAudio->IsPlaying())CaptainAudio->Play();
 }
}
