#include "Hunting/BBSBInventoryWidget.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Hunting/BBSBInventoryTheme.h"
#include "BBSBCharacter.h"
#include "Engine/World.h"
#include "Field/BBSBFieldActions.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"

namespace
{
 FString Category(const UBBSBItemDefinition* I)
 {
  if(!I)return FString();
  switch(I->EquipmentKind)
  {
   case EBBSBItemEquipmentKind::Weapon:return TEXT("무기");case EBBSBItemEquipmentKind::Companion:return TEXT("버니아 추가 장비");
   case EBBSBItemEquipmentKind::Armor:return TEXT("방어구");case EBBSBItemEquipmentKind::Accessory:return TEXT("장신구");case EBBSBItemEquipmentKind::Orb:return TEXT("속성 구슬");default:break;
  }
  static const TCHAR* Names[]={TEXT("장비"),TEXT("소모품"),TEXT("재료"),TEXT("퀘스트"),TEXT("기타"),TEXT("속성 구슬")};return Names[FMath::Clamp(int32(I->ItemCategory),0,5)];
 }
 struct FActionHint { EBBSBGameIcon Icon;FString Action,Flow; };
 struct FStatHint { EBBSBGameIcon Icon;FString Label; };
 TArray<FActionHint> Patterns(int32 Id)
 {
  switch(Id)
  {
   case 1:return {{EBBSBGameIcon::Hold,TEXT("Hold"),TEXT("차지")},{EBBSBGameIcon::Flick,TEXT("Flick / Dive"),TEXT("차지 방출 → 정박자 강화")}};
   case 2:return {{EBBSBGameIcon::Beat,TEXT("Beat"),TEXT("빠른 찌르기")},{EBBSBGameIcon::Flick,TEXT("Flick"),TEXT("입력 방향으로 밀어내기")}};
   case 3:return {{EBBSBGameIcon::Hold,TEXT("Hold"),TEXT("차지")},{EBBSBGameIcon::Shake,TEXT("Shake"),TEXT("차지 소모 → 회전 공격")},{EBBSBGameIcon::Flick,TEXT("Flick"),TEXT("차지 일괄 방출")}};
   case 4:return {{EBBSBGameIcon::Beat,TEXT("Beat"),TEXT("회전 투척 → 후속 베기")}};
   case 5:return {{EBBSBGameIcon::Beat,TEXT("Beat"),TEXT("커서 방향 투척")}};
   case 6:return {{EBBSBGameIcon::Beat,TEXT("좌클릭 Beat"),TEXT("왼손 독립 공격")},{EBBSBGameIcon::Beat,TEXT("우클릭 Beat"),TEXT("오른손 독립 공격")}};
   default:return {{EBBSBGameIcon::Beat,TEXT("Beat"),TEXT("공격")}};
  }
 }
 bool Inside(FVector2D P,FVector2D A,FVector2D Size) { return P.X>=A.X && P.Y>=A.Y && P.X<A.X+Size.X && P.Y<A.Y+Size.Y; }
}
void UBBSBInventoryWidget::PaintEquipment(const FGeometry& G,FSlateWindowElementList& E,int32 Layer) const
{
 const auto L=Layout(G);const float S=L.Scale;FBBSBInventoryDraw D{G,E,Layer,S};D.Panel(L.Equipment,L.EquipmentSize);
 D.Text(GrowthPage==2 ? TEXT("버니아 장착"):TEXT("장착 장비"),L.Equipment+FVector2D(14,13)*S,15,D.Paper());
 if(GrowthPage==2)return;
 const int32 Count=Hunting->Columns*Hunting->Rows,Equipped=EquippedWheelSlot();
 auto Ring=[&](float A,float R){return L.Wheel+FVector2D(FMath::Sin(A),-FMath::Cos(A))*R;};
 for(int32 I=0;I<6;++I)
 {
  const float A=I*2*PI/6,Half=PI/6-.045f;TArray<FVector2D> Arc,Outline;
  for(int32 J=0;J<=16;++J) { const float T=A-Half+J*2*Half/16;Arc.Add(Ring(T,(L.Outer+L.Inner)*.5));Outline.Add(Ring(T,L.Outer)); }
  for(int32 J=16;J>=0;--J)Outline.Add(Ring(A-Half+J*2*Half/16,L.Inner));const FVector2D FirstPoint=Outline[0];Outline.Add(FirstPoint);
  const bool Hover=GrowthPage==0 && FocusedSlot()==Count+I;const bool Selected=I==Equipped;
  D.Lines(Arc,Selected ? FLinearColor(.27f,.18f,.23f):D.CellGround(),(L.Outer-L.Inner-2*S)/S,3);D.Lines(Outline,Hover || Selected ? D.Paper():D.Faint(),Hover || Selected ? 2.f:1.f,5);
  const auto* Item=EntryForSlot(Count+I);const bool Buddy=I==Hunting->CompanionWheelSlot;const FVector2D Centre=Ring(A,(L.Inner+L.Outer)*.5);
  D.Icon(Buddy ? Hunting->EquippedItemAt(10):Item ? Item->Item.Get():nullptr,Buddy ? FName(TEXT("Bunniar")):Item && Item->Item ? Item->Item->IconGlyph:NAME_None,Centre,18*S,(Buddy && bHoldingCompanionPosition) || (Item && Item->InstanceId==HeldId) ? .3f:1.f);
  D.Text(FString::FromInt(I+1),Ring(A,L.Outer-7*S)-FVector2D(3,4)*S,7,D.Faint(),8);
  if(Selected) { const auto Mark=Ring(A,L.Outer+4*S);D.Box(Mark-FVector2D(2*S),FVector2D(4*S),D.Accent(),8); }
  if(GrowthPage==1) { FGrowthControl C;C.Key=*FString::Printf(TEXT("Wheel_%d"),I);C.Position=Centre-FVector2D(24*S);C.Size=FVector2D(48*S);C.Action=30;C.Index=Count+I;GrowthControls.Add(C); }
 }
 D.Text(TEXT("장착"),L.Wheel-FVector2D(12,7)*S,9,D.Paper());
 for(int32 I=7;I<9;++I)
 {
  const auto P=CellTopLeft(L,Count+I);const auto* Item=EntryForSlot(Count+I);D.Cell(P,FVector2D(L.Cell-2*S),Item && Item->Item ? Item->Item->RarityColor():D.Faint(),Item!=nullptr,GrowthPage==0 && FocusedSlot()==Count+I);
  D.Icon(Item ? Item->Item.Get():nullptr,I==7 ? FName(TEXT("Armor")):FName(TEXT("Ring")),P+FVector2D(L.Cell*.5),21*S,Item && Item->InstanceId==HeldId ? .3f:Item ? 1.f:.3f);
  D.Text(I==7 ? TEXT("방어구"):TEXT("장신구"),P+FVector2D(0,L.Cell+3*S),10,D.Faint());
  if(GrowthPage==1) { FGrowthControl C;C.Key=*FString::Printf(TEXT("Passive_%d"),I);C.Position=P;C.Size=FVector2D(L.Cell);C.Action=30;C.Index=Count+I;GrowthControls.Add(C); }
 }
 if(Hunting->bEquipmentLayoutPending)D.Text(TEXT("행동이 끝나면 반영돼요"),L.Equipment+FVector2D(15,218)*S,9,D.Accent());
}
int32 UBBSBInventoryWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& E,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
 Layer=Super::NativePaint(Args,G,Clip,E,Layer,Style,Enabled);if(!Hunting)return Layer;const auto L=Layout(G);PaintedLayout=L;const float S=L.Scale;FBBSBInventoryDraw D{G,E,Layer,S};GrowthControls.Reset();
 auto Hit=[&](UImage* Image,FVector2D P,FVector2D Size){if(Image)if(auto* Slot=Cast<UCanvasPanelSlot>(Image->Slot)) { if(!Slot->GetPosition().Equals(P,.01))Slot->SetPosition(P);if(!Slot->GetSize().Equals(Size,.01))Slot->SetSize(Size); }};
 Hit(PanelHit,L.Panel,L.Size);Hit(EquipmentHit,L.Equipment,L.EquipmentSize);D.Panel(L.Panel,L.Size);
 const TCHAR* Pages[]={TEXT("소지품"),TEXT("무기"),TEXT("버니아")};D.Text(Pages[GrowthPage],L.Panel+FVector2D(18,12)*S,21,D.Paper());
 const FString Notes=FString::Printf(TEXT("노트 %s"),*FText::AsNumber(Hunting->Notes).ToString());
 D.Text(Notes,L.Panel+FVector2D(110,23)*S,9,D.Accent());
 const float TW=(L.Size.X-40*S)/3;
 for(int32 I=0;I<3;++I) { const auto P=L.Panel+FVector2D(20*S+I*TW,51*S);D.Box(P,FVector2D(TW-3*S,25*S),I==GrowthPage ? FLinearColor(.26f,.20f,.23f):D.CellGround(),5);D.Lines({P+FVector2D(0,25*S),P+FVector2D(TW-3*S,25*S)},I==GrowthPage ? D.Paper():D.Faint(),I==GrowthPage ? 2.f:1.f,6);D.Text(Pages[I],P+FVector2D(8,5)*S,11,I==GrowthPage ? D.Paper():D.Faint(),7); }
 const bool SortHover=bPointerInside && Inside(Pointer,L.Sort,L.SortSize),CloseHover=bPointerInside && IsClosePoint(L,Pointer);
 if(GrowthPage!=1) { D.Box(L.Sort,L.SortSize,SortHover ? D.Accent():D.Faint(),5);D.Box(L.Sort+FVector2D(S),L.SortSize-FVector2D(2*S),D.Ground(),6);D.Text(TEXT("정렬"),L.Sort+FVector2D(15,5)*S,11,D.Paper(),7); }
 D.Box(L.Close,L.CloseSize,CloseHover ? D.Accent():D.Faint(),5);D.Box(L.Close+FVector2D(S),L.CloseSize-FVector2D(2*S),D.Ground(),6);D.Lines({L.Close+FVector2D(8,8)*S,L.Close+FVector2D(18,18)*S},D.Paper(),1.4f,7);D.Lines({L.Close+FVector2D(18,8)*S,L.Close+FVector2D(8,18)*S},D.Paper(),1.4f,7);
 PaintEquipment(G,E,Layer+10);
 const int32 Count=Hunting->Columns*Hunting->Rows;
 if(GrowthPage==0)
 {
  for(int32 I=GridScrollRow*5;I<FMath::Min(Count,(GridScrollRow+L.VisibleRows)*5);++I)
  {
   const auto P=CellTopLeft(L,I);const auto* Item=EntryForSlot(I);const bool Valid=!HeldId.IsValid() || Hunting->CanPlaceOwnedItem(HeldId,I);
   D.Cell(P,FVector2D(L.Cell-2*S),Item && Item->Item ? Item->Item->RarityColor():D.Faint(),Item!=nullptr,FocusedSlot()==I,Valid);
   if(Item && Item->Item) { D.Icon(Item->Item,Item->Item->IconGlyph,P+FVector2D(L.Cell*.5),L.Cell*.34f,Item->InstanceId==HeldId ? .3f:1.f);D.WeaponSocketDots(*Item,P,FVector2D(L.Cell-2*S),Item->InstanceId==HeldId ? .3f:1.f);if(Item->Quantity>1) { const auto Q=FString::FromInt(Item->Quantity);D.Text(Q,P+FVector2D(L.Cell-D.Measure(Q,11).X-6*S,L.Cell-18*S),11,D.Paper(),9); } }
  }
 }
 else PaintGrowthPanel(G,E,Layer+12);
 const bool GrowthMessage=!GrowthNotice.IsEmpty() && GetWorld()->GetRealTimeSeconds()<GrowthNoticeUntil;
 D.Text(GrowthMessage ? GrowthNotice:TEXT("클릭 집기/놓기 · 우클릭 취소"),L.Panel+FVector2D(20*S,L.Size.Y-43*S),GrowthMessage ? 9:10,GrowthMessage ? D.Accent():D.Faint(),15);D.Text(TEXT("Tab 닫기 · E 전환 · F 상호작용"),L.Panel+FVector2D(20*S,L.Size.Y-24*S),9,D.Faint(),15);
 auto* Player=Cast<ABBSBCharacter>(GetOwningPlayerPawn());auto* Growth=Player ? Player->EquipmentGrowth.Get():nullptr;
 const auto* Held=Hunting->FindOwnedEntry(HeldId);const auto* Module=Growth ? Growth->ModuleDefinition(HeldModule):nullptr;
 if(HasCarriedItem())
 {
  const auto Origin=bKeyboardMode && GrowthPage>0 && GrowthControls.IsValidIndex(FocusIndex) ? GrowthControls[FocusIndex].Position:bKeyboardMode ? SlotCenter(FocusIndex):Pointer;
  FVector2D P=Origin+FVector2D(42,45)*S;P.X=FMath::Min(P.X,G.GetLocalSize().X-39*S);P.Y=FMath::Min(P.Y,G.GetLocalSize().Y-39*S);
  if(Module)D.ModuleIcon(Module,P,32*S,.96f,70);else D.Icon(Held ? Held->Item.Get():Hunting->EquippedItemAt(10),bHoldingCompanionPosition ? FName(TEXT("Bunniar")):Held && Held->Item ? Held->Item->IconGlyph:NAME_None,P,32*S,.96f,70);
  if(Held && Held->Quantity>1)D.Text(FString::FromInt(Held->Quantity),P+FVector2D(22,24)*S,12,D.Paper(),72);
 }
 else if(GrowthPage==0)PaintTooltip(G,E,Layer+90,EntryForSlot(FocusedSlot()),nullptr,SlotTooltip(FocusedSlot()).ToString());
 else
 {
  const int32 Active=bKeyboardMode ? FocusIndex:GrowthControlAt(Pointer);
  if(GrowthControls.IsValidIndex(Active))
  {
   const auto& C=GrowthControls[Active];const FBBSBInventoryEntry* Item=Hunting->FindOwnedEntry(C.Item);const UBBSBModuleDefinition* M=Growth ? Growth->ModuleDefinition(C.Module):nullptr;FBBSBInventoryEntry Socket;
   if(C.Action==30 || C.Action==22)Item=EntryForSlot(C.Action==22 ? Count+6:C.Index);
   if(C.Action==2)if(const auto* W=Hunting->FindOwnedEntry(InspectedWeapon);W && W->Orbs.IsValidIndex(C.Index)) { Socket.Item=W->Orbs[C.Index].Item;Socket.Quantity=1;Item=Socket.Item ? &Socket:nullptr; }
   PaintTooltip(G,E,Layer+90,Item,M,C.Tooltip);
  }
 }
 return Layer+150;
}
int32 UBBSBInventoryWidget::PaintTooltip(const FGeometry& G,FSlateWindowElementList& E,int32 Layer,const FBBSBInventoryEntry* Entry,const UBBSBModuleDefinition* Module,const FString& Hint) const
{
 if(!bPointerInside && !bKeyboardMode)return Layer;
 const auto* Item=Entry ? Entry->Item.Get():nullptr;if(!Item && !Module && Hint.IsEmpty())return Layer;
 const float S=PaintedLayout.Scale,W=350*S;FBBSBInventoryDraw D{G,E,Layer,S};const float RarityRow=24*S,Header=Item || Module ? 112*S:0;
 const FString Name=Item ? Item->DisplayName.ToString():Module ? Module->DisplayName.ToString():FString();
 const FString Description=Item ? Item->Description.ToString():Module ? Module->Description.ToString():Hint;
 auto Wrap=[&](const FString& Text,float Width,float FontSize)
 {
  TArray<FString> Out,Paragraphs;Text.ParseIntoArrayLines(Paragraphs,false);
  for(const auto& Paragraph:Paragraphs) { FString Row;for(TCHAR C:Paragraph) { if(!Row.IsEmpty() && D.Measure(Row+C,FontSize).X>Width) { Out.Add(Row);Row.Reset(); }Row.AppendChar(C); }Out.Add(Row); }return Out;
 };
 const auto Rows=Wrap(Description,W-28*S,11);const bool Weapon=Item && Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon;
 const int32 SocketCount=Weapon ? FMath::Min(32,Entry->SocketCount):0;
 const float SocketHeight=SocketCount>0 ? (32+FMath::DivideAndRoundUp(SocketCount,8)*32)*S:0;
 auto* Player=Cast<ABBSBCharacter>(GetOwningPlayerPawn());auto* Growth=Player ? Player->EquipmentGrowth.Get():nullptr;TArray<FStatHint> Stats;TArray<FActionHint> Actions;
 if(Weapon)
 {
  const bool Ecstasy=Hunting->IsEcstasyWeapon(Entry->InstanceId);
  Stats.Add({EBBSBGameIcon::Level,Ecstasy ? FString::Printf(TEXT("Lv.%d · 엑스타시 · 공격력 +%.0f%%"),Entry->WeaponLevel,Item->EcstasyAttackBonus*100):FString::Printf(TEXT("Lv.%d · 엑스태틱 %lld / %lld"),Entry->WeaponLevel,Hunting->WeaponExperienceInLevel(Entry->InstanceId),Hunting->WeaponExperienceToNextLevel(Entry->InstanceId))});
  const auto Snapshot=Growth ? Growth->WeaponSnapshot(Entry->InstanceId):FBBSBWeaponSnapshot();const float Multiplier=Snapshot.InstanceId.IsValid() ? Snapshot.DamageMultiplier:Item->WeaponDamageMultiplier*(1+(FMath::Max(1,Entry->WeaponLevel)-1)*Item->DamagePerLevel);
  Stats.Add({EBBSBGameIcon::Attack,FString::Printf(TEXT("공격력  %.1f"),(Player ? Player->FieldActions->BaseDamage:18.f)*Multiplier+Snapshot.FlatDamage)});Stats.Add({EBBSBGameIcon::Cooldown,FString::Printf(TEXT("쿨타임  %.2f초"),Item->NormalBeatCooldown*Snapshot.CooldownMultiplier)});
  Actions=Patterns(Item->WeaponId);
 }
 if(Module && Growth) { Stats.Add({EBBSBGameIcon::Level,FString::Printf(TEXT("강화 단계  Lv.%d / %d"),Growth->ModuleLevel(Module->ModuleId),Module->MaximumLevel)});Stats.Add({Module->Slot==EBBSBModuleSlot::Aerial ? EBBSBGameIcon::Glide:EBBSBGameIcon::Wire,Module->Slot==EBBSBModuleSlot::Aerial ? TEXT("공중 모듈 · 활강"):TEXT("방향 모듈 · 와이어")}); }
 float ActionHeight=Actions.IsEmpty() ? 0:12*S;
 for(const auto& A:Actions)ActionHeight+=(29+Wrap(A.Flow,W-52*S,11).Num()*18)*S;
 const float H=Header+(Rows.Num()*18+24)*S+SocketHeight+(Stats.Num()>0 ? (Stats.Num()*20+18)*S:0)+ActionHeight;
 FVector2D P=(bKeyboardMode ? GrowthPage>0 && GrowthControls.IsValidIndex(FocusIndex) ? GrowthControls[FocusIndex].Position:SlotCenter(FocusIndex):Pointer)+FVector2D(18,22)*S;
 if(P.X+W>G.GetLocalSize().X-10*S)P.X=(bKeyboardMode ? PaintedLayout.Panel.X:Pointer.X)-W-18*S;
 P.X=FMath::Max(10.f*S,float(P.X));P.Y=FMath::Clamp(float(P.Y),10.f*S,FMath::Max(10.f*S,float(G.GetLocalSize().Y-H-10*S)));
 D.Panel(P,FVector2D(W,H));const auto Rare=Item ? Item->RarityColor():D.Paper();float Y=P.Y+12*S;
 if(Header>0)
 {
  auto Band=FMath::Lerp(D.Ground(),Rare,.22f);Band.A=1;
  D.Box(P,FVector2D(W,RarityRow),Band,3);D.Gradient(P+FVector2D(0,RarityRow),FVector2D(W,Header-RarityRow),Band,D.Ground(),false,3);
  const FString Rarity=Item ? Item->RarityLabel().ToString():TEXT("일반");D.Text(Rarity,P+FVector2D((W-D.Measure(Rarity,11).X)*.5f,5*S),11,D.Paper(),8);
  if(Item)D.Icon(const_cast<UBBSBItemDefinition*>(Item),Item->IconGlyph,P+FVector2D(43,68)*S,30*S,1,8);else D.ModuleIcon(Module,P+FVector2D(43,68)*S,30*S,1,8);
  auto Titles=Wrap(Name,W-96*S,16);for(int32 I=0;I<FMath::Min(2,Titles.Num());++I)D.Text(Titles[I],P+FVector2D(82,35+I*20)*S,16,D.Paper(),8);
  D.Text(Item ? Category(Item):TEXT("버니아 모듈"),P+FVector2D(82,87)*S,10,D.Paper(),8);
  D.Lines({P+FVector2D(12*S,Header),P+FVector2D(W-12*S,Header)},D.Faint(),1,8);Y=P.Y+Header+12*S;
 }
 for(const auto& Row:Rows) { D.Text(Row,FVector2D(P.X+14*S,Y),11,D.Paper(),8);Y+=18*S; }Y+=12*S;
 if(SocketCount>0)
 {
  D.Text(FString::Printf(TEXT("구슬 슬롯  %d"),Entry->SocketCount),FVector2D(P.X+14*S,Y),10,D.Faint(),8);Y+=24*S;
  for(int32 I=0;I<SocketCount;++I) { const FVector2D Center(P.X+(27.5f+(I%8)*39)*S,Y+(13.5f+(I/8)*32)*S);const auto* Orb=Entry->Orbs.IsValidIndex(I) ? Entry->Orbs[I].Item.Get():nullptr;
   D.OrbSlot(Center,13.5f*S,Orb ? D.OrbColor(Orb):D.Faint(),Orb!=nullptr,false);if(Orb)D.Icon(const_cast<UBBSBItemDefinition*>(Orb),Orb->IconGlyph,Center,9*S,1,8); }
  Y+=(FMath::DivideAndRoundUp(SocketCount,8)*32+8)*S;
 }
 if(!Stats.IsEmpty()) { D.Lines({FVector2D(P.X+14*S,Y),FVector2D(P.X+W-14*S,Y)},D.Faint(),1,8);Y+=10*S;for(const auto& Row:Stats) { D.GameIcon(Row.Icon,FVector2D(P.X+22*S,Y+8*S),7.5f*S,D.Paper());D.Text(Row.Label,FVector2D(P.X+38*S,Y),11,D.Paper(),8);Y+=20*S; }Y+=8*S; }
 if(!Actions.IsEmpty())
 {
  D.Lines({FVector2D(P.X+14*S,Y),FVector2D(P.X+W-14*S,Y)},D.Faint(),1,8);Y+=12*S;
  for(const auto& A:Actions)
  {
   D.GameIcon(A.Icon,FVector2D(P.X+22*S,Y+8*S),8*S,D.Accent());D.Text(A.Action,FVector2D(P.X+38*S,Y),12,D.Accent(),8);Y+=19*S;
   for(const auto& Row:Wrap(A.Flow,W-52*S,11)) { D.Text(Row,FVector2D(P.X+38*S,Y),11,D.Paper(),8);Y+=18*S; }Y+=10*S;
  }
 }
 return Layer+15;
}
