#include "Hunting/BBSBInventoryWidget.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Hunting/BBSBItemIcon.h"
#include "Hunting/BBSBInventoryTheme.h"
#include "BBSBCharacter.h"
#include "Engine/World.h"
#include "Field/BBSBFieldActions.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

namespace
{
bool Contains(FVector2D P,FVector2D At,FVector2D Size) { return P.X>=At.X && P.Y>=At.Y && P.X<=At.X+Size.X && P.Y<=At.Y+Size.Y; }
TArray<const FBBSBInventoryEntry*> Weapons(const UBBSBHuntingComponent* H)
{
 TArray<const FBBSBInventoryEntry*> Entries;
 for(const auto& E:H->EquippedItems)if(E.Item && E.Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon)Entries.Add(&E);
 for(const auto& E:H->Items)if(E.Item && E.Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon)Entries.Add(&E);
 return Entries;
}
}
void UBBSBInventoryWidget::SetGrowthPage(int32 Page)
{ GrowthPage=FMath::Clamp(Page,0,2);DismantleConfirmation.Invalidate();GridScrollRow=0;if(auto* P=Cast<ABBSBCharacter>(GetOwningPlayerPawn());P && P->EquipmentGrowth)P->EquipmentGrowth->EnsureBunniarBag();bSortOpen=bSortPressed=false;bKeyboardMode=false;FocusIndex=0;HoverIndex=INDEX_NONE;UpdatePointerSurface();InvalidateLayoutAndVolatility(); }
FVector2D UBBSBInventoryWidget::GrowthTabCenter(int32 Page) const
{
 const auto& L=PaintedLayout;const float W=(L.Size.X-40*L.Scale)/3;return L.Panel+FVector2D(20*L.Scale+(FMath::Clamp(Page,0,2)+.5f)*W,63.5f*L.Scale);
}
FVector2D UBBSBInventoryWidget::GrowthControlCenter(FName Key) const
{ const auto* C=GrowthControls.FindByPredicate([&](const auto& Control){return Control.Key==Key;});return C ? C->Position+C->Size*.5:FVector2D::ZeroVector; }
int32 UBBSBInventoryWidget::GrowthTabAt(const FLayout& L,FVector2D P) const
{
 const float Width=(L.Size.X-40*L.Scale)/3;
 for(int32 I=0;I<3;++I)if(Contains(P,L.Panel+FVector2D(20*L.Scale+I*Width,51*L.Scale),FVector2D(Width-3*L.Scale,25*L.Scale)))return I;
 return INDEX_NONE;
}
int32 UBBSBInventoryWidget::GrowthControlAt(FVector2D P) const
{ return GrowthControls.IndexOfByPredicate([&](const auto& C){return Contains(P,C.Position,C.Size) && (C.Action!=2 || (P-C.Position-C.Size*.5).SizeSquared()<=FMath::Square(C.Size.X*.5));}); }
void UBBSBInventoryWidget::NavigateGrowth(int32 DX,int32 DY)
{
 if(GrowthControls.IsEmpty())return;bKeyboardMode=true;FocusIndex=FMath::Clamp(FocusIndex,0,GrowthControls.Num()-1);
 const auto From=GrowthControls[FocusIndex].Position+GrowthControls[FocusIndex].Size*.5;const FVector2D Direction(DX,DY);float Best=MAX_flt;int32 Next=FocusIndex;
 for(int32 I=0;I<GrowthControls.Num();++I)if(GrowthControls[I].Enabled)
 {
  const auto D=GrowthControls[I].Position+GrowthControls[I].Size*.5-From;const float Along=FVector2D::DotProduct(D,Direction);if(Along<1)continue;
  const float Score=D.Size()+2*FMath::Abs(D.X*Direction.Y-D.Y*Direction.X);if(Score<Best) { Best=Score;Next=I; }
 }
 if(Next==FocusIndex && GrowthPage==2 && DY!=0 && GrowthControls[FocusIndex].Action==20)
 {
  const int32 Cell=GrowthControls[FocusIndex].Index+DY*5;auto* P=Cast<ABBSBCharacter>(GetOwningPlayerPawn());auto* Growth=P ? P->EquipmentGrowth.Get():nullptr;
  if(Growth && Cell>=0 && Cell<Growth->BunniarBagCapacity())
  {
   GridScrollRow=FMath::Clamp(GridScrollRow+DY,0,FMath::Max(0,FMath::DivideAndRoundUp(Growth->BunniarBagCapacity(),5)-PaintedLayout.VisibleRows));
   FocusIndex=FMath::Clamp((Cell/5-GridScrollRow)*5+Cell%5,0,PaintedLayout.VisibleRows*5-1);InvalidateLayoutAndVolatility();return;
  }
 }
 FocusIndex=Next;InvalidateLayoutAndVolatility();
}
bool UBBSBInventoryWidget::ActivateGrowthControl(int32 Index)
{
 if(!GrowthControls.IsValidIndex(Index) || !GrowthControls[Index].Enabled || !Hunting)return false;
 const auto C=GrowthControls[Index];auto* Player=Cast<ABBSBCharacter>(GetOwningPlayerPawn());auto* Growth=Player ? Player->EquipmentGrowth.Get():nullptr;
 bool Applied=false;
 switch(C.Action)
 {
  case 1:
  {
   const auto W=Weapons(Hunting);const int32 Current=W.IndexOfByPredicate([&](const auto* E){return E->InstanceId==InspectedWeapon;});
   if(W.Num()>0) { InspectedWeapon=W[(FMath::Max(0,Current)+C.Index+W.Num())%W.Num()]->InstanceId;DismantleConfirmation.Invalidate();SocketPage=0;Applied=true; }break;
  }
  case 2:
  {
   const auto* Orb=Hunting->FindEntry(HeldId);
   Applied=Orb && Orb->Item && Orb->Item->EquipmentKind==EBBSBItemEquipmentKind::Orb ? Hunting->SocketOrb(InspectedWeapon,C.Index,HeldId):!HeldId.IsValid() && Hunting->RemoveOrb(InspectedWeapon,C.Index);
   if(!Hunting->FindOwnedEntry(HeldId))HeldId.Invalidate();
   if(Growth && !Applied)Growth->Feedback=TEXT("장착할 구슬과 가방의 빈칸을 확인해 주세요");break;
  }
  case 3:HeldModule=NAME_None;HeldId=C.Item;bHoldingCompanionPosition=false;Applied=true;break;
  case 4:SocketPage=FMath::Max(0,SocketPage+C.Index);Applied=true;break;
  case 5:Applied=Hunting->EquipOwnedWeapon(InspectedWeapon);if(Growth && !Applied)Growth->Feedback=TEXT("장비 휠에 빈칸이 필요해요");break;
  case 6:OrbPage=FMath::Max(0,OrbPage+C.Index);Applied=true;break;
  case 7:Applied=Hunting->UseEcstaticCrystals(InspectedWeapon);break;
  case 8:DismantleConfirmation=InspectedWeapon;DismantleConfirmationAt=GetWorld()->GetRealTimeSeconds();HeldId.Invalidate();HeldModule=NAME_None;Applied=true;break;
  case 9:
   if(DismantleConfirmation==InspectedWeapon && GetWorld()->GetRealTimeSeconds()-DismantleConfirmationAt>=.35) { Applied=Hunting->DismantleWeapon(InspectedWeapon);if(Applied)InspectedWeapon.Invalidate(); }
   DismantleConfirmation.Invalidate();break;
  case 14:DismantleConfirmation.Invalidate();Applied=true;break;
  case 10:if(Growth)if(const auto* D=Growth->ModuleDefinition(C.Module))Applied=Growth->EquipModule(D->Slot,C.Module);break;
  case 11:if(Growth)Applied=Growth->UpgradeModule(C.Module);break;
  case 12:if(Growth)Applied=Growth->EquipModule(EBBSBModuleSlot(C.Index),NAME_None);break;
  case 20:ActivateModuleCell(C.Index);Applied=true;break;
  case 21:ActivateModuleSlot(C.Index);Applied=true;break;
  case 22:ActivateSlot(Hunting->Columns*Hunting->Rows+6);if(Growth)Growth->EnsureBunniarBag();Applied=true;break;
  case 30:ActivateSlot(C.Index);Applied=true;break;
  case 13:ModulePage=FMath::Max(0,ModulePage+C.Index);Applied=true;break;
 }
 if(Growth && (C.Action==7 || C.Action==9)) { GrowthNotice=Growth->Feedback;GrowthNoticeUntil=GetWorld()->GetRealTimeSeconds()+3.2; }
 UpdatePointerSurface();InvalidateLayoutAndVolatility();return Applied;
}

int32 UBBSBInventoryWidget::PaintGrowthPanel(const FGeometry& G,FSlateWindowElementList& Elements,int32 Layer) const
{
 const auto L=Layout(G);const float S=L.Scale;const FVector2D At=L.Grid;const float W=L.Size.X-40*S;
 auto* Player=Cast<ABBSBCharacter>(GetOwningPlayerPawn());auto* Growth=Player ? Player->EquipmentGrowth.Get():nullptr;
 FBBSBInventoryDraw Draw{G,Elements,Layer,S};const auto Ink=Draw.Paper(),Faint=Draw.Faint(),Mint=Draw.Accent(),Rose=Draw.Accent();
 const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 auto Box=[&](FVector2D P,FVector2D Size,FLinearColor Color,int32 Z=0) { FSlateDrawElement::MakeBox(Elements,Layer+Z,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),White,ESlateDrawEffect::None,Color); };
 auto Text=[&](const FString& Label,FVector2D P,float Size,FLinearColor Color,int32 Z=8)
 { FSlateDrawElement::MakeText(Elements,Layer+Z,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(P)),Label,FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),FMath::Max(7,FMath::RoundToInt(Size*S))),ESlateDrawEffect::None,Color); };
 auto Button=[&](FName Key,FVector2D P,FVector2D Size,const FString& Label,int32 Action,FGuid Id,int32 Number,FName Module,bool Enabled,const FString& Tip)
 {
  FGrowthControl C;C.Key=Key;C.Position=P;C.Size=Size;C.Action=Action;C.Item=Id;C.Index=Number;C.Module=Module;C.Enabled=Enabled;C.Tooltip=Tip;
  const int32 Index=GrowthControls.Add(C);const bool Hover=bKeyboardMode ? Index==FocusIndex:bPointerInside && Contains(Pointer,P,Size);
  Box(P,Size,Hover && Enabled ? Draw.Paper():Draw.Faint(),3);Box(P+FVector2D(S),Size-FVector2D(2*S),Hover && Enabled ? FLinearColor(.25f,.20f,.23f):Draw.CellGround(),4);
  if(!Label.IsEmpty())Text(Label,P+FVector2D(6,6)*S,10,Enabled ? Ink:Faint,8);return Index;
 };
 auto Icon=[&](UBBSBItemDefinition* Item,FName Kind,FVector2D Center,float R) { BBSBItemIcon::DrawSlate(Elements,G,Layer+7,Item,Kind,Center,R,1.f,S,Ink); };
 if(GrowthPage==1)
 {
  const auto List=Weapons(Hunting);int32 Current=List.IndexOfByPredicate([&](const auto* E){return E->InstanceId==InspectedWeapon;});
  if(Current==INDEX_NONE && List.Num()>0) { Current=List.IndexOfByPredicate([&](const auto* E){return Player && E->InstanceId==Player->FieldActions->EquippedWeaponInstance;});Current=FMath::Max(0,Current);InspectedWeapon=List[Current]->InstanceId; }
  if(List.IsEmpty())Text(TEXT("획득한 무기가 없어요"),At,12,Faint);
  else
  {
   const auto& E=*List[Current];const auto Stats=Growth ? Growth->WeaponSnapshot(E.InstanceId):FBBSBWeaponSnapshot();
   Button(TEXT("WeaponPrevious"),At,FVector2D(26,28)*S,TEXT("<"),1,FGuid(),-1,NAME_None,List.Num()>1,TEXT("이전 무기"));
   Button(TEXT("WeaponNext"),At+FVector2D(W-26*S,0),FVector2D(26,28)*S,TEXT(">"),1,FGuid(),1,NAME_None,List.Num()>1,TEXT("다음 무기"));
   Text(E.Item->DisplayName.ToString(),At+FVector2D(34,2)*S,14,Ink);Text(FString::Printf(TEXT("%d / %d"),Current+1,List.Num()),At+FVector2D(35,23)*S,9,Faint);
   const bool Ecstasy=Hunting->IsEcstasyWeapon(E.InstanceId);
   const int64 Required=Hunting->WeaponExperienceToNextLevel(E.InstanceId),Progress=Hunting->WeaponExperienceInLevel(E.InstanceId);
   Text(Ecstasy ? FString::Printf(TEXT("Lv.%d · 엑스타시"),E.WeaponLevel):FString::Printf(TEXT("Lv.%d · 엑스태틱 %lld / %lld"),E.WeaponLevel,Progress,Required),At+FVector2D(0,46)*S,11,Rose);
   Box(At+FVector2D(0,62)*S,FVector2D(W,3*S),Draw.CellGround(),3);
   Box(At+FVector2D(0,62)*S,FVector2D(W*(Ecstasy ? 1.f:Required>0 ? float(Progress)/Required:0.f),3*S),Rose,4);
   Draw.GameIcon(EBBSBGameIcon::Attack,At+FVector2D(8,76)*S,7.5f*S,Ink);Text(FString::Printf(TEXT("공격력 %.1f"),Player ? Player->FieldActions->BaseDamage*Stats.DamageMultiplier+Stats.FlatDamage:0),At+FVector2D(23,68)*S,10,Ink);
   Draw.GameIcon(EBBSBGameIcon::Cooldown,At+FVector2D(168,76)*S,7.5f*S,Ink);Text(FString::Printf(TEXT("쿨타임 %.2f초"),E.Item->NormalBeatCooldown*Stats.CooldownMultiplier),At+FVector2D(183,68)*S,10,Ink);
   const bool Equipped=Player && Player->FieldActions->EquippedEquipmentId()!=0 && Player->FieldActions->EquippedWeaponInstance==E.InstanceId;
   Button(TEXT("WeaponEquip"),At+FVector2D(0,92)*S,FVector2D(W,28*S),Equipped ? TEXT("장착 중"):TEXT("이 무기 장착"),5,E.InstanceId,0,NAME_None,!Equipped,TEXT("가방의 무기는 빈 장비 휠 칸에 배치해요"));
   Text(FString::Printf(TEXT("속성 구슬 · %d칸"),E.SocketCount),At+FVector2D(0,135)*S,12,Ink);const int32 Pages=FMath::Max(1,FMath::DivideAndRoundUp(E.SocketCount,6)),Page=FMath::Clamp(SocketPage,0,Pages-1);
   Button(TEXT("SocketPrevious"),At+FVector2D(W-55*S,130*S),FVector2D(25,24)*S,TEXT("<"),4,FGuid(),-1,NAME_None,Page>0,TEXT("이전 소켓"));
   Button(TEXT("SocketNext"),At+FVector2D(W-25*S,130*S),FVector2D(25,24)*S,TEXT(">"),4,FGuid(),1,NAME_None,Page<Pages-1,TEXT("다음 소켓"));
   const float Cell=(W+6*S)/3;
   for(int32 I=Page*6;I<FMath::Min(E.SocketCount,Page*6+6);++I)
   {
    const FVector2D P=At+FVector2D(((I-Page*6)%3)*Cell,163*S+((I-Page*6)/3)*53*S);const auto* Orb=E.Orbs.IsValidIndex(I) ? E.Orbs[I].Item.Get():nullptr;
    const FVector2D Center=P+FVector2D((Cell-6*S)*.5f,22*S);FGrowthControl C;C.Key=*FString::Printf(TEXT("Socket_%d"),I);C.Position=Center-FVector2D(22*S);C.Size=FVector2D(44*S);C.Action=2;C.Index=I;C.Tooltip=Orb ? Orb->DisplayName.ToString()+TEXT(" · 클릭하여 회수"):TEXT("선택한 구슬을 장착해요");
    const int32 Control=GrowthControls.Add(C);const bool Hover=bKeyboardMode ? FocusIndex==Control:bPointerInside && (Pointer-Center).SizeSquared()<=FMath::Square(22*S);
    Draw.OrbSlot(Center,22*S,Orb ? Draw.OrbColor(Orb):Draw.Faint(),Orb!=nullptr,Hover);
    if(Orb)Icon(const_cast<UBBSBItemDefinition*>(Orb),Orb->IconGlyph,Center,15*S);
    Text(FString::FromInt(I+1),Center+FVector2D(-3,23)*S,8,Faint);
   }
   if(E.SocketCount==0)Text(TEXT("이 무기는 소켓이 없어요"),At+FVector2D(0,180)*S,11,Faint);
   Text(TEXT("보유 구슬 · 선택 후 소켓 클릭"),At+FVector2D(0,283)*S,10,Ink);
   TArray<const FBBSBInventoryEntry*> Orbs;for(const auto& Orb:Hunting->Items)if(Orb.Item && Orb.Item->EquipmentKind==EBBSBItemEquipmentKind::Orb)Orbs.Add(&Orb);
   const int32 OrbPages=FMath::Max(1,FMath::DivideAndRoundUp(Orbs.Num(),9)),OP=FMath::Clamp(OrbPage,0,OrbPages-1);
   for(int32 I=OP*9;I<FMath::Min(Orbs.Num(),OP*9+9);++I)
   {
    const auto& Orb=*Orbs[I];const FVector2D P=At+FVector2D(((I-OP*9)%3)*Cell,305*S+((I-OP*9)/3)*47*S);
    Button(*FString::Printf(TEXT("Orb_%s"),*Orb.InstanceId.ToString()),P,FVector2D(Cell-6*S,41*S),TEXT(""),3,Orb.InstanceId,0,NAME_None,true,Orb.Item->DisplayName.ToString()+TEXT("\n")+Orb.Item->Description.ToString());
    Icon(Orb.Item,Orb.Item->IconGlyph,P+FVector2D((Cell-6*S)*.5,17*S),17*S);if(Orb.Quantity>1)Text(FString::FromInt(Orb.Quantity),P+FVector2D(Cell/S-25,25)*S,9,Faint);
   }
   if(Orbs.IsEmpty())Text(TEXT("사냥으로 속성 구슬을 획득해 보세요"),At+FVector2D(0,315)*S,10,Faint);
   Button(TEXT("OrbPrevious"),At+FVector2D(0,450)*S,FVector2D(26,24)*S,TEXT("<"),6,FGuid(),-1,NAME_None,OP>0,TEXT("이전 구슬"));
   Button(TEXT("OrbNext"),At+FVector2D(W-26*S,450*S),FVector2D(26,24)*S,TEXT(">"),6,FGuid(),1,NAME_None,OP<OrbPages-1,TEXT("다음 구슬"));
   auto* Crystal=Hunting->EcstaticCrystalDefinition();const int32 Crystals=Crystal ? Hunting->ItemCount(Crystal):0;
   const FString CrystalLabel=Crystal ? FString::Printf(TEXT("결정 사용 · +%d 엑스태틱 · 보유 %d개"),Crystal->ResourceValue,Crystals):TEXT("엑스태틱 결정");
   Button(TEXT("UseCrystal"),At+FVector2D(0,485)*S,FVector2D(W,26*S),CrystalLabel,7,E.InstanceId,0,NAME_None,Crystal && Crystals>0 && !Ecstasy,TEXT("지금 살펴보는 무기에 결정 1개를 사용해요. 협주곡은 충전되지 않아요."));
   const auto Preview=Hunting->PreviewDismantleWeapon(E.InstanceId);
   if(DismantleConfirmation==E.InstanceId)
   {
    Button(TEXT("ConfirmDismantle"),At+FVector2D(0,516)*S,FVector2D(W*.67f,24*S),TEXT("정말 파기 · 되돌릴 수 없음"),9,E.InstanceId,0,NAME_None,Preview.bAllowed && GetWorld()->GetRealTimeSeconds()-DismantleConfirmationAt>=.35,TEXT("구슬은 전부 반환하고 투자한 엑스태틱 일부를 결정으로 돌려받아요."));
    Button(TEXT("CancelDismantle"),At+FVector2D(W*.69f,516*S),FVector2D(W*.31f,24*S),TEXT("취소"),14,FGuid(),0,NAME_None,true,TEXT("이 무기를 보관해요"));
   }
   else Button(TEXT("DismantleWeapon"),At+FVector2D(0,516)*S,FVector2D(W,24*S),FString::Printf(TEXT("무기 파기 · 결정 %d개 / 구슬 %d개 반환"),Preview.CrystalCount,Preview.ReturnedOrbs),8,E.InstanceId,0,NAME_None,Preview.bAllowed,Preview.bAllowed ? TEXT("가방에 있는 무기를 파기해요. 다음 화면에서 한 번 더 확인해요."):Preview.Reason.ToString());
  }
 }
 else if(Growth)
 {
  Growth->EnsureBunniarBag();const int32 Capacity=Growth->BunniarBagCapacity();
  for(int32 I=GridScrollRow*5;I<FMath::Min(Capacity,(GridScrollRow+L.VisibleRows)*5);++I)
  {
   const auto P=L.Grid+FVector2D(I%5,I/5-GridScrollRow)*L.Cell;const FName Id=Growth->ModuleAtBunniarCell(I);const auto* M=Growth->ModuleDefinition(Id);const FGuid ItemId=Growth->ItemAtBunniarCell(I);const auto* Entry=Hunting->FindEntry(ItemId);
   FGrowthControl C;C.Key=*FString::Printf(TEXT("ModuleCell_%d"),I);C.Position=P;C.Size=FVector2D(L.Cell-2*S);C.Action=20;C.Index=I;C.Module=Id;C.Item=ItemId;const int32 Index=GrowthControls.Add(C);
   const bool Hover=bKeyboardMode ? FocusIndex==Index:bPointerInside && Contains(Pointer,P,C.Size);
   const auto* HeldItem=Hunting->FindOwnedEntry(HeldId);const bool Valid=!HeldId.IsValid() || (HeldItem && HeldItem->Item && HeldItem->Item->EquipmentKind==EBBSBItemEquipmentKind::Companion);
   Draw.Cell(P,C.Size,Entry && Entry->Item ? Entry->Item->RarityColor():Draw.Paper(),M || Entry,Hover,Valid);
   const auto Center=P+FVector2D(L.Cell*.5f);
   if(M)Draw.ModuleIcon(M,Center,L.Cell*.34f,HeldModule==Id ? .3f:1.f);
   else if(Entry && Entry->Item)Draw.Icon(Entry->Item,Entry->Item->IconGlyph,Center,L.Cell*.34f,HeldId==ItemId ? .3f:1.f);
   if(M)Draw.Text(FString::Printf(TEXT("Lv.%d"),Growth->ModuleLevel(Id)),P+FVector2D(5*S,L.Cell-17*S),9,Draw.Paper(),9);
  }
  const int32 Count=Hunting->Columns*Hunting->Rows;const auto P=CellTopLeft(L,Count+6);const auto* Item=EntryForSlot(Count+6);
  const int32 UpgradeIndex=Button(TEXT("BunniarEquipment"),P,FVector2D(L.Cell),TEXT(""),22,FGuid(),0,NAME_None,true,TEXT("버니아 추가 장비 · 빈칸은 기본 버니아예요"));
  Draw.Cell(P,FVector2D(L.Cell-2*S),Item && Item->Item ? Item->Item->RarityColor():Draw.Paper(),Item!=nullptr,bKeyboardMode ? FocusIndex==UpgradeIndex:bPointerInside && Contains(Pointer,P,FVector2D(L.Cell)));
  Draw.Icon(Item ? Item->Item.Get():nullptr,TEXT("Bunniar"),P+FVector2D(L.Cell*.5f),L.Cell*.34f,Item && Item->InstanceId==HeldId ? .3f:Item ? 1.f:.35f);
  Draw.Text(TEXT("추가 장비"),P+FVector2D(5,L.Cell/S+4)*S,10,Draw.Faint());
  for(int32 I=0;I<2;++I)
  {
   const FName Id=Growth->RequestedModule(EBBSBModuleSlot(I));const auto* M=Growth->ModuleDefinition(Id);const FVector2D AtSlot=L.Equipment+FVector2D(I==0 ? 35:138,160)*S;
   const auto* Held=Growth->ModuleDefinition(HeldModule);const bool Valid=!Held || int32(Held->Slot)==I;
   const int32 Index=Button(I==0 ? TEXT("ModuleSlot_Aerial"):TEXT("ModuleSlot_Directional"),AtSlot,FVector2D(L.Cell),TEXT(""),21,FGuid(),I,Id,true,I==0 ? TEXT("공중 모듈"):TEXT("방향 모듈"));
   Draw.Cell(AtSlot,FVector2D(L.Cell-2*S),Draw.Paper(),M!=nullptr,bKeyboardMode ? FocusIndex==Index:bPointerInside && Contains(Pointer,AtSlot,FVector2D(L.Cell)),Valid);
   if(M)Draw.ModuleIcon(M,AtSlot+FVector2D(L.Cell*.5f),L.Cell*.34f,HeldModule==Id ? .3f:1.f);
   Draw.Text(I==0 ? TEXT("공중 모듈"):TEXT("방향 모듈"),AtSlot+FVector2D(0,L.Cell/S+4)*S,10,Draw.Faint());
   if(M)Draw.Text(FString::Printf(TEXT("Lv.%d"),Growth->ModuleLevel(Id)),AtSlot+FVector2D(5,46)*S,9,Draw.Paper(),9);
  }
  const auto* Selected=Growth->ModuleDefinition(SelectedModule);
  if(!Selected && !Growth->OwnedModules.IsEmpty())Selected=Growth->OwnedModules[0].Definition;
  if(Selected)
  {
   Draw.Text(Selected->DisplayName.ToString()+FString::Printf(TEXT(" Lv.%d"),Growth->ModuleLevel(Selected->ModuleId)),L.Equipment+FVector2D(16,257)*S,11,Draw.Paper());
   const int32 Level=Growth->ModuleLevel(Selected->ModuleId),Cost=Selected->UpgradeCost(Level);
   const FString Label=Level>=Selected->MaximumLevel ? TEXT("최대 단계"):FString::Printf(TEXT("강화  ·  %d / %d"),Selected->UpgradeMaterial ? Hunting->ItemCount(Selected->UpgradeMaterial):0,Cost);
   Button(FName(*(FString(TEXT("ModuleUpgrade_"))+Selected->ModuleId.ToString())),L.Equipment+FVector2D(16,282)*S,FVector2D(208,28)*S,Label,11,FGuid(),0,Selected->ModuleId,Growth->CanUpgradeModule(Selected->ModuleId),Selected->Description.ToString());
  }
  if(Growth->bModuleChangePending)Draw.Text(TEXT("행동이 끝나면 반영돼요"),L.Equipment+FVector2D(16,321)*S,9,Draw.Accent());
 }
 return Layer+40;
}
