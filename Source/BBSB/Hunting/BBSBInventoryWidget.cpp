#include "Hunting/BBSBInventoryWidget.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBItem.h"
#include "Hunting/BBSBItemIcon.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/InputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Input/Reply.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Field/BBSBFieldActions.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

namespace
{
bool InRect(FVector2D P,FVector2D Origin,FVector2D Size) { return P.X>=Origin.X && P.Y>=Origin.Y && P.X<=Origin.X+Size.X && P.Y<=Origin.Y+Size.Y; }
}

UBBSBInventoryWidget::UBBSBInventoryWidget(const FObjectInitializer& I):Super(I)
{ SetIsFocusable(false);SetVisibility(ESlateVisibility::SelfHitTestInvisible);ForceVolatile(true); }
TSharedRef<SWidget> UBBSBInventoryWidget::RebuildWidget()
{
 if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this,TEXT("InventoryTree"));
 if(!WidgetTree->RootWidget)WidgetTree->RootWidget=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("InventoryRoot"));
 if(auto* Root=Cast<UCanvasPanel>(WidgetTree->RootWidget);Root && !PanelHit)
 { Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);PanelHit=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("InventoryHitPanel"));PanelHit->SetVisibility(ESlateVisibility::Visible);PanelHit->SetColorAndOpacity(FLinearColor::Transparent);Root->AddChildToCanvas(PanelHit); }
 return Super::RebuildWidget();
}
UBBSBInventoryWidget::FLayout UBBSBInventoryWidget::Layout(const FGeometry& G) const
{
 const FVector2D V=G.GetLocalSize();FLayout L;L.Scale=FMath::Clamp(float(V.Y/720.f),.55f,2.5f);
 const int32 Rows=Hunting ? Hunting->Rows:8;L.Cell=FMath::Clamp(float((V.Y-242*L.Scale)/(Rows+4.5f)),24.f*L.Scale,54.f*L.Scale);
 L.Size=FVector2D(5*L.Cell+40*L.Scale,V.Y-32*L.Scale);L.Panel=FVector2D(V.X-L.Size.X-16*L.Scale,16*L.Scale);
 L.Grid=L.Panel+FVector2D(20,60)*L.Scale;L.Outer=L.Cell*1.9f;L.Inner=L.Cell*.8f;
 L.Close=L.Panel+FVector2D(L.Size.X-39*L.Scale,13*L.Scale);L.CloseSize=FVector2D(26*L.Scale);
 L.Sort=L.Panel+FVector2D(L.Size.X-107*L.Scale,13*L.Scale);L.SortSize=FVector2D(58,26)*L.Scale;
 L.SortPanel=L.Panel+FVector2D(20,48)*L.Scale;L.SortPanelSize=FVector2D(L.Size.X-40*L.Scale,148*L.Scale);
 L.SortApply=L.SortPanel+FVector2D(12,112)*L.Scale;L.SortApplySize=FVector2D(L.SortPanelSize.X-24*L.Scale,26*L.Scale);
 L.Wheel=FVector2D(L.Panel.X+L.Size.X*.5f,L.Grid.Y+Rows*L.Cell+28*L.Scale+L.Outer);
 L.Passive=FVector2D(L.Grid.X,L.Wheel.Y+L.Outer+25*L.Scale);return L;
}
FVector2D UBBSBInventoryWidget::CellTopLeft(const FLayout& L,int32 I) const
{
 const int32 Count=Hunting ? Hunting->Columns*Hunting->Rows:40;
 if(I<Count)return L.Grid+FVector2D(I%5,I/5)*L.Cell;
 I-=Count;if(I<10) { const float A=I*2*PI/10;return L.Wheel+FVector2D(FMath::Sin(A),-FMath::Cos(A))*((L.Outer+L.Inner)*.5f)-FVector2D(L.Cell*.5f); }
 return L.Passive+FVector2D((I-10)*2,0)*L.Cell;
}
FVector2D UBBSBInventoryWidget::SlotCenter(int32 I) const { return CellTopLeft(PaintedLayout,I)+FVector2D(PaintedLayout.Cell*.5f); }
FVector2D UBBSBInventoryWidget::CloseButtonCenter() const { return PaintedLayout.Close+PaintedLayout.CloseSize*.5; }
FVector2D UBBSBInventoryWidget::SortButtonCenter() const { return PaintedLayout.Sort+PaintedLayout.SortSize*.5; }
FVector2D UBBSBInventoryWidget::SortApplyCenter() const { return PaintedLayout.SortApply+PaintedLayout.SortApplySize*.5; }
bool UBBSBInventoryWidget::IsClosePoint(const FLayout& L,FVector2D P) const { return P.X>=L.Close.X && P.Y>=L.Close.Y && P.X<=L.Close.X+L.CloseSize.X && P.Y<=L.Close.Y+L.CloseSize.Y; }
int32 UBBSBInventoryWidget::PointerSlot(const FLayout& L,FVector2D P) const
{
 const int32 Count=Hunting ? Hunting->Columns*Hunting->Rows:40;
 const FVector2D D=P-L.Wheel;const float R=D.Size();
 if(R>=L.Inner && R<=L.Outer) { const float A=FMath::Fmod(FMath::Atan2(D.X,-D.Y)+2*PI,2*PI);return Count+FMath::RoundToInt(A/(2*PI/10))%10; }
 for(int32 I=0;I<Count+13;++I) { if(I>=Count && I<Count+10)continue;const auto A=CellTopLeft(L,I);if(P.X>=A.X && P.Y>=A.Y && P.X<A.X+L.Cell && P.Y<A.Y+L.Cell)return I; }
 return INDEX_NONE;
}
const FBBSBInventoryEntry* UBBSBInventoryWidget::EntryForSlot(int32 I) const
{
 if(!Hunting || I<0)return nullptr;const int32 Count=Hunting->Columns*Hunting->Rows;
 if(I<Count) { const int32 E=Hunting->EntryAt(I%5,I/5);return Hunting->Items.IsValidIndex(E) ? &Hunting->Items[E]:nullptr; }
 const int32 EquipmentSlot=I-Count;return Hunting->EquipmentEntryAt(EquipmentSlot==Hunting->CompanionWheelSlot ? 10:EquipmentSlot);
}
void UBBSBInventoryWidget::ResetInteraction()
{
 HeldId.Invalidate();bHoldingCompanionPosition=bClosePressed=bPointerInside=bSortOpen=bSortPressed=false;HoverIndex=INDEX_NONE;FocusIndex=0;bKeyboardMode=false;UIButtons.Reset();InvalidateLayoutAndVolatility();
 UpdatePointerSurface();
}
void UBBSBInventoryWidget::NativeDestruct() { UnregisterMenuKeys();ResetInteraction();Super::NativeDestruct(); }
void UBBSBInventoryWidget::RegisterMenuKeys()
{
 auto* PC=GetOwningPlayer();if(!PC || MenuInputOwner.Get()==PC)return;
 if(!MenuKeys)
 {
  MenuKeys=NewObject<UInputComponent>(this,TEXT("InventoryKeys"));MenuKeys->Priority=1000;MenuKeys->bBlockInput=false;MenuKeys->RegisterComponentWithWorld(GetWorld());
  const TArray<FKey> Keys={EKeys::Tab,EKeys::Escape,EKeys::Enter,EKeys::Left,EKeys::Right,EKeys::Up,EKeys::Down,EKeys::Gamepad_DPad_Left,EKeys::Gamepad_DPad_Right,EKeys::Gamepad_DPad_Up,EKeys::Gamepad_DPad_Down,EKeys::Gamepad_FaceButton_Bottom,EKeys::Gamepad_FaceButton_Right};
  for(const auto& Key:Keys) { FInputKeyBinding Binding(FInputChord(Key),IE_Pressed);Binding.bConsumeInput=true;Binding.KeyDelegate.GetDelegateForManualSet().BindUObject(this,&UBBSBInventoryWidget::HandleMenuKey,Key);MenuKeys->KeyBindings.Add(MoveTemp(Binding)); }
 }
 PC->PushInputComponent(MenuKeys);MenuInputOwner=PC;
}
void UBBSBInventoryWidget::UnregisterMenuKeys() { if(auto* PC=MenuInputOwner.Get())PC->PopInputComponent(MenuKeys);MenuInputOwner.Reset(); }
void UBBSBInventoryWidget::HandleMenuKey(FKey Key) { if(Hunting && Hunting->bInventoryOpen)ProcessMenuKey(Key); }
int32 UBBSBInventoryWidget::EquippedWheelSlot() const
{
 const auto* A=GetOwningPlayerPawn() ? GetOwningPlayerPawn()->FindComponentByClass<UBBSBFieldActionComponent>():nullptr;
 if(!Hunting || !A)return INDEX_NONE;const int32 Id=A->EquippedEquipmentId();
 for(int32 I=0;I<10;++I) { const auto* E=Hunting->EquipmentEntryAt(I);if(I==Hunting->CompanionWheelSlot ? Id==0:E && E->Item && E->Item->WeaponId==Id)return I; }
 return INDEX_NONE;
}
FText UBBSBInventoryWidget::SlotTooltip(int32 I) const
{
 if(!Hunting || I<0)return FText::GetEmpty();const int32 Count=Hunting->Columns*Hunting->Rows;
 if(I==Count+Hunting->CompanionWheelSlot)return FText::FromString(TEXT("버니아\n기본 동행 · 항상 사용 가능\n휠 안에서 선택 위치를 옮길 수 있어요."));
 if(const auto* E=EntryForSlot(I);E && E->Item)
  return FText::FromString(E->Item->DisplayName.ToString()+TEXT("\n")+E->Item->RarityLabel().ToString()+(E->Item->Description.IsEmpty() ? FString():TEXT("\n")+E->Item->Description.ToString()));
 if(I==Count+10)return FText::FromString(TEXT("버니아 추가 장비\n빈 상태가 기본 버니아예요.\n추가 장비를 이곳에 장착할 수 있어요."));
 if(I==Count+11)return FText::FromString(TEXT("방어구\n장착할 방어구를 이곳에 놓으세요."));
 if(I==Count+12)return FText::FromString(TEXT("장신구\n장착할 장신구를 이곳에 놓으세요."));
 return FText::GetEmpty();
}
void UBBSBInventoryWidget::ActivateSlot(int32 I)
{
 if(!Hunting || I<0)return;const int32 Count=Hunting->Columns*Hunting->Rows;
 if(bHoldingCompanionPosition) { if(I>=Count && I<Count+10 && Hunting->MoveCompanionWheelSlot(I-Count))bHoldingCompanionPosition=false; }
 else if(HeldId.IsValid())
 {
  const auto* Before=Hunting->FindOwnedEntry(HeldId);const int32 Quantity=Before ? Before->Quantity:0;
  if(Hunting->PlaceOwnedItem(HeldId,I)) { const auto* After=Hunting->FindOwnedEntry(HeldId);if(!After || After->Quantity==Quantity)HeldId.Invalidate(); }
 }
 else if(I==Count+Hunting->CompanionWheelSlot)bHoldingCompanionPosition=true;
 else if(const auto* E=EntryForSlot(I))HeldId=E->InstanceId;
 UpdatePointerSurface();InvalidateLayoutAndVolatility();
}
void UBBSBInventoryWidget::UpdatePointerSurface() { SetVisibility(HasCarriedItem() || bSortOpen || !UIButtons.IsEmpty() ? ESlateVisibility::Visible:ESlateVisibility::SelfHitTestInvisible); }
void UBBSBInventoryWidget::Navigate(int32 DX,int32 DY)
{
 bSortOpen=false;UpdatePointerSurface();bKeyboardMode=true;const int32 Count=Hunting ? Hunting->Columns*Hunting->Rows:40;FocusIndex=FMath::Clamp(FocusIndex,0,Count+12);
 const FVector2D From=SlotCenter(FocusIndex),Direction(DX,DY);double Best=MAX_dbl;int32 Next=FocusIndex;
 for(int32 I=0;I<Count+13;++I)
 {
  const FVector2D Delta=SlotCenter(I)-From;const double Along=FVector2D::DotProduct(Delta,Direction);if(Along<1)continue;
  const double Side=FMath::Abs(Delta.X*Direction.Y-Delta.Y*Direction.X),Score=Delta.Size()+Side*2;
  if(Score<Best) { Best=Score;Next=I; }
 }
 FocusIndex=Next;
 InvalidateLayoutAndVolatility();
}
int32 UBBSBInventoryWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
 Layer=Super::NativePaint(Args,G,Clip,Elements,Layer,Style,Enabled);if(!Hunting)return Layer;
 const auto L=Layout(G);PaintedLayout=L;const float S=L.Scale;const int32 Count=Hunting->Columns*Hunting->Rows;
 if(PanelHit)if(auto* PanelSlot=Cast<UCanvasPanelSlot>(PanelHit->Slot)) { if(!PanelSlot->GetPosition().Equals(L.Panel,.01))PanelSlot->SetPosition(L.Panel);if(!PanelSlot->GetSize().Equals(L.Size,.01))PanelSlot->SetSize(L.Size); }
 const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 const FLinearColor Paper(.95f,.92f,.86f,.97f),Ink(.15f,.105f,.17f),Faint(.38f,.31f,.37f),Rose(.64f,.29f,.42f),Mint(.24f,.58f,.44f);
 auto Box=[&](FVector2D P,FVector2D Size,FLinearColor C,int32 Z=0) { FSlateDrawElement::MakeBox(Elements,Layer+Z,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),White,ESlateDrawEffect::None,C); };
 auto Text=[&](const FString& V,FVector2D P,float Size,FLinearColor C,int32 Z=4) { FSlateDrawElement::MakeText(Elements,Layer+Z,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(P)),V,FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),FMath::Max(7,FMath::RoundToInt(Size*S))),ESlateDrawEffect::None,C); };
 auto Lines=[&](const TArray<FVector2D>& P,FLinearColor C,float W=1.6f,int32 Z=5) { FSlateDrawElement::MakeLines(Elements,Layer+Z,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,W*S); };
 auto Glyph=[&](UBBSBItemDefinition* Item,FName Kind,FVector2D C,float R,float Alpha,int32 Z)
 { BBSBItemIcon::DrawSlate(Elements,G,Layer+Z,Item,Kind,C,R,Alpha,S,Ink); };
 // Paint only the right panel; the left-hand character keeps the live world view.
 Box(L.Panel-FVector2D(2*S),L.Size+FVector2D(4*S),Ink,1);Box(L.Panel,L.Size,Paper,2);
 Text(TEXT("소지품"),L.Panel+FVector2D(19,14)*S,23,Ink);
 const bool SortHovered=bPointerInside && !bKeyboardMode && InRect(Pointer,L.Sort,L.SortSize);
 Box(L.Sort,L.SortSize,SortHovered || bSortOpen ? Mint:FLinearColor(.78f,.72f,.75f),3);Box(L.Sort+FVector2D(S),L.SortSize-FVector2D(2*S),SortHovered || bSortOpen ? FLinearColor(.83f,.91f,.84f):Paper,4);
 Text(TEXT("정렬"),L.Sort+FVector2D(16,6)*S,11,Ink,6);
 const bool CloseHovered=bPointerInside && !bKeyboardMode && IsClosePoint(L,Pointer);Box(L.Close,L.CloseSize,CloseHovered ? Mint:FLinearColor(.78f,.72f,.75f),3);Box(L.Close+FVector2D(S),L.CloseSize-FVector2D(2*S),CloseHovered ? FLinearColor(.83f,.91f,.84f):Paper,4);
 Lines({L.Close+FVector2D(8,8)*S,L.Close+FVector2D(18,18)*S},Ink,1.6f,6);Lines({L.Close+FVector2D(18,8)*S,L.Close+FVector2D(8,18)*S},Ink,1.6f,6);
 Text(TEXT("장착 휠"),FVector2D(L.Grid.X,L.Wheel.Y-L.Outer-22*S),12,Ink);
 if(Hunting->bEquipmentLayoutPending)Text(TEXT("행동 후 반영"),FVector2D(L.Grid.X+2.5f*L.Cell,L.Wheel.Y-L.Outer-19*S),9,Rose);
 Text(TEXT("추가 장비"),L.Passive-FVector2D(0,20*S),11,Faint);
 const int32 Active=FocusedSlot(),Equipped=EquippedWheelSlot();
 auto RingPoint=[&](float A,float R){return L.Wheel+FVector2D(FMath::Sin(A),-FMath::Cos(A))*R;};
 for(int32 I=0;I<Count+13;++I)
 {
  const auto P=CellTopLeft(L,I);const auto* E=EntryForSlot(I);const bool Buddy=I==Count+Hunting->CompanionWheelSlot;
  const bool Source=(E && E->InstanceId==HeldId && !Buddy) || (Buddy && bHoldingCompanionPosition);
  bool Valid=true;if(HasCarriedItem())Valid=bHoldingCompanionPosition ? I>=Count && I<Count+10:Hunting->CanPlaceOwnedItem(HeldId,I);
  const FLinearColor Border=I==Active ? (Valid ? Mint:FLinearColor(.8f,.23f,.32f)):FLinearColor(.66f,.59f,.62f);
  const bool InWheel=I>=Count && I<Count+10,InUse=InWheel && I-Count==Equipped;
  if(InWheel)
  {
   const float A=(I-Count)*2*PI/10,Half=PI/10-.045f;TArray<FVector2D> Arc,Outline;
   for(int32 J=0;J<=12;++J) { const float T=A-Half+J*2*Half/12;Arc.Add(RingPoint(T,(L.Outer+L.Inner)*.5f));Outline.Add(RingPoint(T,L.Outer)); }
   for(int32 J=12;J>=0;--J)Outline.Add(RingPoint(A-Half+J*2*Half/12,L.Inner));const FVector2D FirstPoint=Outline[0];Outline.Add(FirstPoint);
   Lines(Arc,InUse ? FLinearColor(.62f,.81f,.69f):I==Active ? FLinearColor(.79f,.89f,.80f):FLinearColor(.88f,.84f,.83f),(L.Outer-L.Inner-3*S)/S,3);
   Lines(Outline,I==Active ? Border:InUse ? Mint:FLinearColor(.66f,.59f,.62f),I==Active || InUse ? 2.2f:1.f,5);
   if(InUse) { const auto Mark=RingPoint(A,L.Outer+4*S);Box(Mark-FVector2D(2*S),FVector2D(4*S),Mint,8); }
  }
  else
  {
   Box(P,FVector2D(L.Cell-2*S),Border,3);const float Inset=I==Active ? 3*S:1*S;
   Box(P+FVector2D(Inset),FVector2D(L.Cell-2*S-2*Inset),I==Active ? FLinearColor(.83f,.91f,.84f):FLinearColor(.91f,.88f,.85f),4);
  }
  const FVector2D Center=P+FVector2D(L.Cell*.5f);const float Radius=L.Cell*(InWheel ? .21f:.27f);
  if(Buddy)Glyph(Hunting->EquippedItemAt(10),TEXT("Bunniar"),Center,Radius,Source ? .28f:1.f,6);
  else if(E && E->Item)Glyph(E->Item,E->Item->IconGlyph,Center,Radius,Source ? .28f:1.f,6);
  else if(I>=Count+10)Glyph(nullptr,I==Count+10 ? FName(TEXT("Bunniar")):I==Count+11 ? FName(TEXT("Armor")):FName(TEXT("Ring")),P+FVector2D(L.Cell*.5f),L.Cell*.24f,.25f,6);
  if(E && E->Quantity>1 && !InWheel) { const FString Amount=FString::FromInt(E->Quantity);Text(Amount,P+FVector2D(L.Cell-5*S-Amount.Len()*6*S,L.Cell-15*S),10,Ink,7); }
  if(InWheel)Text(FString::FromInt(I-Count+1),RingPoint((I-Count)*2*PI/10,L.Outer-7*S)-FVector2D(3*S,4*S),7,Faint,7);
  if(I>=Count+10)Text(I==Count+10 ? TEXT("버니아"):I==Count+11 ? TEXT("방어구"):TEXT("장신구"),P+FVector2D(0,L.Cell+2*S),10,Faint);
 }
 Text(TEXT("장착 중"),L.Wheel+FVector2D(-18,-18)*S,9,Mint);
 const auto* Current=Equipped>=0 ? EntryForSlot(Count+Equipped):nullptr;
 const FString Name=Equipped==Hunting->CompanionWheelSlot ? TEXT("버니아"):Current && Current->Item ? Current->Item->DisplayName.ToString():TEXT("없음");
 Text(Name,L.Wheel+FVector2D(-Name.Len()*5*S,0),10,Ink);
 const FVector2D Footer=L.Panel+FVector2D(20*S,L.Size.Y-48*S);
 Text(bKeyboardMode ? TEXT("방향 이동 · Enter/A 선택 · Esc/B 취소"):TEXT("클릭 집기/놓기 · 우클릭 취소"),Footer,9,Faint);
 Text(TEXT("Tab 닫기 · E 획득 / 길게 흡입"),Footer+FVector2D(0,17*S),9,Faint);
 const auto* Held=Hunting->FindOwnedEntry(HeldId);
 if(Held || bHoldingCompanionPosition)
 {
  auto* Item=Held ? Held->Item.Get():Hunting->EquippedItemAt(10);FVector2D At=(bKeyboardMode ? SlotCenter(FocusIndex):Pointer)+FVector2D(27,29)*S;
  At.X=FMath::Min(At.X,G.GetLocalSize().X-40*S);At.Y=FMath::Min(At.Y,G.GetLocalSize().Y-40*S);
  Glyph(Item,bHoldingCompanionPosition ? FName(TEXT("Bunniar")):Item ? Item->IconGlyph:NAME_None,At,18*S,.96f,18);
  const FString Amount=FString::FromInt(Held ? Held->Quantity:1);Text(Amount,At+FVector2D(11,11)*S+FVector2D(S),11,FLinearColor(.98f,.96f,.9f),19);Text(Amount,At+FVector2D(11,11)*S,11,Ink,20);
 }
 else
 {
  const FString Tip=bSortOpen ? FString():CloseHovered ? TEXT("닫기"):SortHovered ? TEXT("카테고리 · 종류 · 개수 순으로 정렬"):SlotTooltip(Active).ToString();
  if(!Tip.IsEmpty())
  {
   const float Width=FMath::Min(250*S,float(G.GetLocalSize().X-24*S));TArray<FString> Paragraphs,Rows;Tip.ParseIntoArray(Paragraphs,TEXT("\n"),false);
   const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();const auto Font=FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),FMath::Max(7,FMath::RoundToInt(11*S)));
   for(const auto& Paragraph:Paragraphs)
   {
    FString Row;
    for(TCHAR Ch:Paragraph) { if(!Row.IsEmpty() && Measure->Measure(Row+Ch,Font).X>Width-24*S) { Rows.Add(Row);Row.Reset(); }Row.AppendChar(Ch); }
    Rows.Add(Row);
   }
   const float Height=(Rows.Num()*17+22)*S;FVector2D At=(bKeyboardMode ? SlotCenter(FocusIndex):Pointer)+FVector2D(18,22)*S;
   if(At.X+Width>G.GetLocalSize().X-12*S)At.X=(bKeyboardMode ? SlotCenter(FocusIndex).X:Pointer.X)-Width-18*S;
   At.X=FMath::Max(12.f*S,float(At.X));At.Y=FMath::Clamp(float(At.Y),12.f*S,FMath::Max(12.f*S,float(G.GetLocalSize().Y-Height-12*S)));
   Box(At-FVector2D(S),FVector2D(Width+2*S,Height+2*S),Ink,30);Box(At,FVector2D(Width,Height),Paper,31);
   for(int32 J=0;J<Rows.Num();++J)Text(Rows[J],At+FVector2D(12,10+J*17)*S,J==0 ? 12:11,J==0 ? Ink:Faint,32);
  }
 }
 if(bSortOpen)
 {
  Box(L.SortPanel-FVector2D(S),L.SortPanelSize+FVector2D(2*S),Ink,50);Box(L.SortPanel,L.SortPanelSize,Paper,51);
  Text(TEXT("정렬 우선순위"),L.SortPanel+FVector2D(12,12)*S,11,Ink,52);
  const TCHAR* Rules[]={TEXT("1   카테고리"),TEXT("2   아이템 종류"),TEXT("3   개수 많은 순")};
  for(int32 I=0;I<3;++I)Text(Rules[I],L.SortPanel+FVector2D(12,38+I*21)*S,11,Faint,52);
  const bool ApplyHovered=bPointerInside && InRect(Pointer,L.SortApply,L.SortApplySize);
  Box(L.SortApply,L.SortApplySize,ApplyHovered ? Mint:FLinearColor(.72f,.83f,.73f),53);
  Text(TEXT("정렬하기"),L.SortApply+FVector2D((L.SortApplySize.X/S-44)*.5f,6)*S,11,Ink,54);
 }
 return Layer+60;
}
FReply UBBSBInventoryWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
 Pointer=G.AbsoluteToLocal(E.GetScreenSpacePosition());bPointerInside=true;bKeyboardMode=false;
 const auto L=Layout(G);HoverIndex=PointerSlot(L,Pointer);
 const bool SortButton=InRect(Pointer,L.Sort,L.SortSize),Popup=bSortOpen && InRect(Pointer,L.SortPanel,L.SortPanelSize);
 if(bSortOpen && !SortButton && !Popup) { bSortOpen=false;UpdatePointerSurface(); }
 if(!InRect(Pointer,L.Panel,L.Size) && !HasCarriedItem())return FReply::Unhandled();
 UIButtons.Add(E.GetEffectingButton());
 if(E.GetEffectingButton()==EKeys::RightMouseButton)
 { if(bSortOpen)bSortOpen=false;else { HeldId.Invalidate();bHoldingCompanionPosition=false; } }
 else if(E.GetEffectingButton()==EKeys::LeftMouseButton)
 {
  if(SortButton)bSortOpen=!bSortOpen;
  else if(Popup)bSortPressed=InRect(Pointer,L.SortApply,L.SortApplySize);
  else if(IsClosePoint(L,Pointer))bClosePressed=true;
  else { if(HoverIndex!=INDEX_NONE)FocusIndex=HoverIndex;ActivateSlot(HoverIndex); }
 }
 UpdatePointerSurface();InvalidateLayoutAndVolatility();FReply Reply=FReply::Handled();
 if(const auto* LP=GetOwningLocalPlayer();LP && LP->ViewportClient)if(auto Viewport=LP->ViewportClient->GetGameViewportWidget();Viewport.IsValid())Reply.SetUserFocus(Viewport.ToSharedRef(),EFocusCause::Mouse);
 return Reply;
}
FReply UBBSBInventoryWidget::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{
 const bool Owned=UIButtons.Remove(E.GetEffectingButton())>0,Left=E.GetEffectingButton()==EKeys::LeftMouseButton;
 const auto L=Layout(G);const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
 const bool Close=bClosePressed && Left && IsClosePoint(L,P),Sort=bSortPressed && Left && bSortOpen && InRect(P,L.SortApply,L.SortApplySize);
 bClosePressed=bSortPressed=false;if(Sort && Hunting) { Hunting->SortInventory();bSortOpen=false; }
 UpdatePointerSurface();InvalidateLayoutAndVolatility();if(Close && Hunting)Hunting->CloseInventory();return Owned ? FReply::Handled():FReply::Unhandled();
}
FReply UBBSBInventoryWidget::NativeOnMouseButtonDoubleClick(const FGeometry& G,const FPointerEvent& E) { return NativeOnMouseButtonDown(G,E); }
FReply UBBSBInventoryWidget::NativeOnMouseWheel(const FGeometry& G,const FPointerEvent& E)
{
 const auto L=Layout(G);const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition());return P.X>=L.Panel.X && P.X<=L.Panel.X+L.Size.X && P.Y>=L.Panel.Y && P.Y<=L.Panel.Y+L.Size.Y ? FReply::Handled():FReply::Unhandled();
}
FReply UBBSBInventoryWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
 const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition());bPointerInside=true;if(!P.Equals(Pointer,.1f))bKeyboardMode=false;Pointer=P;
 HoverIndex=PointerSlot(Layout(G),Pointer);if(!bKeyboardMode && HoverIndex!=INDEX_NONE)FocusIndex=HoverIndex;InvalidateLayoutAndVolatility();return FReply::Unhandled();
}
void UBBSBInventoryWidget::NativeOnMouseLeave(const FPointerEvent& E) { bPointerInside=false;HoverIndex=INDEX_NONE;InvalidateLayoutAndVolatility();Super::NativeOnMouseLeave(E); }
FReply UBBSBInventoryWidget::NativeOnKeyDown(const FGeometry&,const FKeyEvent& E) { return ProcessMenuKey(E.GetKey()) ? FReply::Handled():FReply::Unhandled(); }
bool UBBSBInventoryWidget::ProcessMenuKey(FKey K)
{
 if(K==EKeys::Tab) { ResetInteraction();if(Hunting)Hunting->CloseInventory(); }
 else if(K==EKeys::Escape || K==EKeys::Gamepad_FaceButton_Right) { if(bSortOpen) { bSortOpen=false;UpdatePointerSurface();InvalidateLayoutAndVolatility(); }else if(HasCarriedItem()) { HeldId.Invalidate();bHoldingCompanionPosition=false;UpdatePointerSurface();InvalidateLayoutAndVolatility(); }else if(Hunting)Hunting->CloseInventory(); }
 else if(K==EKeys::Left || K==EKeys::Gamepad_DPad_Left)Navigate(-1,0);
 else if(K==EKeys::Right || K==EKeys::Gamepad_DPad_Right)Navigate(1,0);
 else if(K==EKeys::Up || K==EKeys::Gamepad_DPad_Up)Navigate(0,-1);
 else if(K==EKeys::Down || K==EKeys::Gamepad_DPad_Down)Navigate(0,1);
 else if(K==EKeys::Enter || K==EKeys::Gamepad_FaceButton_Bottom) { bKeyboardMode=true;if(bSortOpen) { if(Hunting)Hunting->SortInventory();bSortOpen=false;UpdatePointerSurface();InvalidateLayoutAndVolatility(); }else ActivateSlot(FocusIndex); }
 else return false;
 return true;
}
