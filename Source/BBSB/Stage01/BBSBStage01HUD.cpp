#include "Stage01/BBSBStage01.h"
#include "BBSBCharacter.h"
#include "Field/BBSBRepairTool.h"
#include "Hunting/BBSBHUDTypography.h"
#include "Hunting/BBSBHunting.h"
#include "Character/BBSBWorldMap.h"
#include "Character/BBSBRespawn.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "EngineUtils.h"
FString ABBSBStage01Director::Objective() const
{
 switch(Phase)
 {
 case EBBSBStage01Phase::Opening:return TEXT("4호차 · 문짝에 버니아를 붙여 양옆으로 밀고 밖으로 나가기");
 case EBBSBStage01Phase::Outside:return TEXT("외부 · 버니아를 집어 조용히 시켜보고, 6호차 가판대 상인에게 말 걸기");
 case EBBSBStage01Phase::FindTools:return TEXT("도구 회수 · 상자와 사다리를 이용해 절벽 위 압수 보관소 찾기");
 case EBBSBStage01Phase::ReturnToMerchant:return TEXT("나무 판과 고무 망치 회수 · 6호차 상인에게 돌아가기");
 case EBBSBStage01Phase::ShopBattle:return TEXT("6호차 · 함께 쏘는 조준탄을 피하며 버니아들을 진정시키기");
 case EBBSBStage01Phase::Cargo:return TEXT("5호차 · 전원 버튼 → 고정 레버 → 화물 발판 → 트랩도어");
 case EBBSBStage01Phase::Rooftops:return TEXT("지붕 우회 · 4호차 지붕을 건너 3호차 열린 천장으로 내려가기");
 case EBBSBStage01Phase::Trio:return TEXT("3호차 · 세 줄 탄막의 빈틈으로 이동하며 트리오 진정시키기");
 case EBBSBStage01Phase::Restock:return TEXT("2호차 · 예술단 상인에게 말 걸고 보상·보급 사용법 알아보기");
 case EBBSBStage01Phase::FindLight:return TEXT("1호차 · 상단장의 쪽지를 읽고 조명 레버 작동시키기");
 case EBBSBStage01Phase::ReturnForReady:return TEXT("2호차 · 보급 후 무대로 향하는 문에 버니아를 붙여 옆으로 열기");
 case EBBSBStage01Phase::ApproachCaptain:return TEXT("무대 · 연주 소리를 따라가 상단장 만나기");
 case EBBSBStage01Phase::Captain:return TEXT("상단장 · 삑사리를 피하고, 붙잡히면 방향 리듬 패링");
 case EBBSBStage01Phase::Complete:return TEXT("버니아 반 발자국 상단 · 소동을 해결했다");
 }
 return FString();
}
FVector ABBSBStage01Director::ObjectivePoint() const
{
 auto Node=[&](EBBSBStage01Node Kind) { auto* D=FindNode(Kind);return D ? D->GetActorLocation():GetActorLocation(); };
 switch(Phase)
 {
 case EBBSBStage01Phase::Opening:return Node(EBBSBStage01Node::CabinDoor);
 case EBBSBStage01Phase::FindTools:
  if(!Solved.Contains(TEXT("DetourLadder")))return Node(EBBSBStage01Node::LowerLadder);
  return Node(EBBSBStage01Node::ToolCache);
 case EBBSBStage01Phase::ShopBattle:for(ABBSBStage01Singer* S:Singers)if(S && S->Voice==EBBSBStage01Voice::Utility && !S->bDefeated)return S->GetActorLocation();break;
 case EBBSBStage01Phase::Cargo:
  if(!Solved.Contains(TEXT("CargoPower")))return Node(EBBSBStage01Node::CargoButton);
  if(!Solved.Contains(TEXT("CargoWinch")))return Node(EBBSBStage01Node::CargoLever);
  return Node(EBBSBStage01Node::Trapdoor);
 case EBBSBStage01Phase::Rooftops:return TrioEntry;
 case EBBSBStage01Phase::Trio:return TrioCenter;
 case EBBSBStage01Phase::FindLight:return Node(EBBSBStage01Node::LightLever);
 case EBBSBStage01Phase::ReturnForReady:return Node(EBBSBStage01Node::ReadyGate);
 case EBBSBStage01Phase::ApproachCaptain:case EBBSBStage01Phase::Captain:return CaptainCenter;
 default:break;
 }
 const bool Utility=Phase<=EBBSBStage01Phase::ShopBattle;
 for(TActorIterator<ABBSBStage01NPC> It(GetWorld());It;++It)if(It->Director==this && It->CarNumber==(Utility ? 6:2))return It->GetActorLocation();
 return GetActorLocation();
}
void ABBSBStage01HUD::DrawHUD()
{
 Super::DrawHUD();if(!Canvas)return;auto* Player=Cast<ABBSBCharacter>(GetOwningPawn());if(!Player)return;
 ABBSBStage01Director* D=nullptr;for(TActorIterator<ABBSBStage01Director> It(GetWorld());It;++It) { D=*It;break; }if(!D)return;
 if(Player->Hunting->bInventoryOpen || Player->WorldMap->bMapOpen || Player->Interaction->bDialogueActive || Player->Respawn->IsSequenceActive())return;
 const float S=FMath::Clamp(Canvas->ClipX/1440.f,.65f,1.3f),W=Canvas->ClipX,H=Canvas->ClipY;
 const FLinearColor Paper(.14f,.12f,.15f,.88f),Ink(.96f,.92f,.88f),Pink(.91f,.56f,.65f),Mint(.54f,.82f,.75f);
 auto Text=[&](const FString& Value,FLinearColor Color,float X,float Y,float Pixels)
 { FCanvasTextItem Item(FVector2D(X,Y),FText::FromString(Value),BBSBHUDTypography::NumberFont(Pixels),Color);Item.bOutlined=true;Item.OutlineColor=FLinearColor(.10f,.08f,.12f,.8f);Canvas->DrawItem(Item); };
 DrawRect(Paper,22*S,22*S,FMath::Min(930*S,W-44*S),73*S);
 Text(TEXT("버니아 반 발자국 상단"),Pink,35*S,30*S,16*S);
 Text(D->Objective(),Ink,35*S,61*S,14*S);
 if(!D->Caption.IsEmpty())
 {
  const float X=W*.5f-360*S,Y=H*.73f;
  TArray<FString> Paragraphs,Lines;D->Caption.ParseIntoArrayLines(Paragraphs);for(const FString& P:Paragraphs)for(int32 Start=0;Start<P.Len();Start+=38)Lines.Add(P.Mid(Start,38));
  DrawRect(Paper,X,Y,720*S,(24+24*Lines.Num())*S);for(int32 I=0;I<Lines.Num();++I)Text(Lines[I],Ink,X+18*S,Y+(10+24*I)*S,17*S);
 }
 if(D->Phase>=EBBSBStage01Phase::Restock)Text(FString::Printf(TEXT("[R] 회복 차  %d / 3"),D->Tea),Mint,24*S,H-90*S,16*S);
 if(Player->RepairTool && Player->RepairTool->IsRepairToolEquipped())
 {
  const float X=W*.5f,Y=H*.58f;
  if(Player->RepairTool->bBoardPlaced)
  {
   DrawRect(Paper,X-110*S,Y,220*S,25*S);DrawRect(Mint,X-105*S,Y+19*S,210*S*Player->RepairTool->NextBeatProgress(),3*S);
   Text(FString::Printf(TEXT("판 타격  %d / 3"),Player->RepairTool->BoardHits),Ink,X-54*S,Y+2*S,14*S);
  }
 }
 if(D->bGrabActive)
 {
  const float X=W*.5f,Y=H*.48f,Spacing=78*S;DrawRect(Paper,X-350*S,Y-75*S,700*S,150*S);
  Text(TEXT("방향 리듬 패링  ·  W ↑   A ←   S ↓   D →"),Ink,X-230*S,Y-58*S,17*S);
  for(int32 I=0;I<8;++I)
  {
   const uint8 Mask=D->GrabMask(I);if(!Mask)break;
   const float Seconds=D->GrabNoteSeconds(I),P=X+Seconds*Spacing; if(P<X-315*S || P>X+315*S)continue;
   FString Note;if(Mask&1)Note+=TEXT("↑");if(Mask&8)Note+=TEXT("←");if(Mask&4)Note+=TEXT("↓");if(Mask&2)Note+=TEXT("→");
   Text(Note,I<D->GrabNoteIndex ? Mint:Pink,P-20*S,Y,30*S);
  }
  DrawLine(X,Y-10*S,X,Y+45*S,Ink,3*S);Text(FString::Printf(TEXT("성공 %d"),D->GrabSuccesses),Mint,X-35*S,Y+50*S,15*S);
 }
 // The objective marker guides a route without hiding the crosshair or taking camera control.
 const FVector Point=D->ObjectivePoint();
 if(Project(Point).Z>0)
 {
  const FVector P=Project(Point);const float X=FMath::Clamp(float(P.X),25*S,W-25*S),Y=FMath::Clamp(float(P.Y)-55*S,110*S,H-120*S);
  DrawLine(X-9*S,Y,X,Y-9*S,Pink,2*S);DrawLine(X,Y-9*S,X+9*S,Y,Pink,2*S);DrawLine(X+9*S,Y,X,Y+9*S,Pink,2*S);DrawLine(X,Y+9*S,X-9*S,Y,Pink,2*S);
  Text(FString::Printf(TEXT("%.0f m"),FVector::Dist(Point,Player->GetActorLocation())/100.f),Ink,X+14*S,Y-7*S,13*S);
 }
}
