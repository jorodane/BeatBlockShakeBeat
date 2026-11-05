#include "Hunting/BBSBInventoryWidget.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBItem.h"
#include "Hunting/BBSBItemIcon.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "BBSBCharacter.h"
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
 if(auto* Root=Cast<UCanvasPanel>(WidgetTree->RootWidget);Root && !EquipmentHit) { EquipmentHit=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("InventoryEquipmentHit"));EquipmentHit->SetVisibility(ESlateVisibility::Visible);EquipmentHit->SetColorAndOpacity(FLinearColor::Transparent);Root->AddChildToCanvas(EquipmentHit); }
 return Super::RebuildWidget();
}
UBBSBInventoryWidget::FLayout UBBSBInventoryWidget::Layout(const FGeometry& G) const
{
 const FVector2D V=G.GetLocalSize();FLayout L;L.Scale=FMath::Clamp(float(FMath::Min(V.Y/720.,V.X/1180.)),.5f,2.5f);
 L.Cell=64*L.Scale;L.Size=FVector2D(360*L.Scale,V.Y-32*L.Scale);L.Panel=FVector2D(V.X-L.Size.X-16*L.Scale,16*L.Scale);
 L.Grid=L.Panel+FVector2D(20,90)*L.Scale;L.VisibleRows=FMath::Max(2,FMath::FloorToInt((L.Size.Y-148*L.Scale)/L.Cell));
 L.EquipmentSize=FVector2D(240,344)*L.Scale;L.Equipment=L.Panel+FVector2D(-252*L.Scale,L.Size.Y-L.EquipmentSize.Y);
 L.Outer=84*L.Scale;L.Inner=32*L.Scale;L.Wheel=L.Equipment+FVector2D(120,120)*L.Scale;L.Passive=L.Equipment+FVector2D(42,246)*L.Scale;
 L.Close=L.Panel+FVector2D(L.Size.X-39*L.Scale,13*L.Scale);L.CloseSize=FVector2D(26*L.Scale);
 L.Sort=L.Panel+FVector2D(L.Size.X-107*L.Scale,13*L.Scale);L.SortSize=FVector2D(58,26)*L.Scale;return L;
}
FVector2D UBBSBInventoryWidget::CellTopLeft(const FLayout& L,int32 I) const
{
 const int32 Count=Hunting ? Hunting->Columns*Hunting->Rows:40;
 if(I==Count+6)return L.Equipment+FVector2D(88,53)*L.Scale;
 if(I<Count)return L.Grid+FVector2D(I%5,I/5-GridScrollRow)*L.Cell;
 I-=Count;if(I<6) { const float A=I*2*PI/6;return L.Wheel+FVector2D(FMath::Sin(A),-FMath::Cos(A))*((L.Outer+L.Inner)*.5f)-FVector2D(L.Cell*.5f); }
 return L.Passive+FVector2D((I-7)*1.45f,0)*L.Cell;
}
int32 UBBSBInventoryWidget::InventoryDestination(int32 I) const
{ const int32 Count=Hunting ? Hunting->Columns*Hunting->Rows:40;return I<Count+6 ? I:I+4; }
FVector2D UBBSBInventoryWidget::SlotCenter(int32 I) const { return CellTopLeft(PaintedLayout,I)+FVector2D(PaintedLayout.Cell*.5f); }
FVector2D UBBSBInventoryWidget::CloseButtonCenter() const { return PaintedLayout.Close+PaintedLayout.CloseSize*.5; }
FVector2D UBBSBInventoryWidget::SortButtonCenter() const { return PaintedLayout.Sort+PaintedLayout.SortSize*.5; }
FVector2D UBBSBInventoryWidget::SortApplyCenter() const { return PaintedLayout.SortApply+PaintedLayout.SortApplySize*.5; }
bool UBBSBInventoryWidget::IsClosePoint(const FLayout& L,FVector2D P) const { return P.X>=L.Close.X && P.Y>=L.Close.Y && P.X<=L.Close.X+L.CloseSize.X && P.Y<=L.Close.Y+L.CloseSize.Y; }
int32 UBBSBInventoryWidget::PointerSlot(const FLayout& L,FVector2D P) const
{
 const int32 Count=Hunting ? Hunting->Columns*Hunting->Rows:40;
 const FVector2D D=P-L.Wheel;const float R=D.Size();
 if(R>=L.Inner && R<=L.Outer) { const float A=FMath::Fmod(FMath::Atan2(D.X,-D.Y)+2*PI,2*PI);return Count+FMath::RoundToInt(A/(2*PI/6))%6; }
 for(int32 I=0;I<Count+9;++I) { if(I==Count+6 || (I>=Count && I<Count+6) || (I<Count && (I/5<GridScrollRow || I/5>=GridScrollRow+L.VisibleRows)))continue;const auto A=CellTopLeft(L,I);if(P.X>=A.X && P.Y>=A.Y && P.X<A.X+L.Cell && P.Y<A.Y+L.Cell)return I; }
 return INDEX_NONE;
}
const FBBSBInventoryEntry* UBBSBInventoryWidget::EntryForSlot(int32 I) const
{
 if(!Hunting || I<0)return nullptr;const int32 Count=Hunting->Columns*Hunting->Rows;
 if(I<Count) { const int32 E=Hunting->EntryAt(I%5,I/5);return Hunting->Items.IsValidIndex(E) ? &Hunting->Items[E]:nullptr; }
 const int32 EquipmentSlot=I-Count<6 ? I-Count:I-Count+4;return Hunting->EquipmentEntryAt(EquipmentSlot==Hunting->CompanionWheelSlot ? 10:EquipmentSlot);
}
void UBBSBInventoryWidget::ResetInteraction()
{
 HeldId.Invalidate();HeldModule=NAME_None;SelectedModule=NAME_None;GridScrollRow=0;bHoldingCompanionPosition=bClosePressed=bPointerInside=bSortOpen=bSortPressed=false;HoverIndex=INDEX_NONE;FocusIndex=0;GrowthPage=0;bKeyboardMode=false;GrowthControls.Reset();UIButtons.Reset();InvalidateLayoutAndVolatility();
 UpdatePointerSurface();
}
void UBBSBInventoryWidget::NativeDestruct() { UnregisterMenuKeys();ResetInteraction();Super::NativeDestruct(); }
void UBBSBInventoryWidget::RegisterMenuKeys()
{
 auto* PC=GetOwningPlayer();if(!PC || MenuInputOwner.Get()==PC)return;
 if(!MenuKeys)
 {
  MenuKeys=NewObject<UInputComponent>(this,TEXT("InventoryKeys"));MenuKeys->Priority=1000;MenuKeys->bBlockInput=false;MenuKeys->RegisterComponentWithWorld(GetWorld());
  const TArray<FKey> Keys={EKeys::Tab,EKeys::Escape,EKeys::Enter,EKeys::Left,EKeys::Right,EKeys::Up,EKeys::Down,EKeys::Gamepad_DPad_Left,EKeys::Gamepad_DPad_Right,EKeys::Gamepad_DPad_Up,EKeys::Gamepad_DPad_Down,EKeys::Gamepad_FaceButton_Bottom,EKeys::Gamepad_FaceButton_Right,EKeys::PageUp,EKeys::PageDown,EKeys::Gamepad_LeftShoulder,EKeys::Gamepad_RightShoulder};
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
 for(int32 I=0;I<6;++I) { const auto* E=Hunting->EquipmentEntryAt(I);if(A->IsWheelSlotEquipped(I))return I; }
 return INDEX_NONE;
}
FText UBBSBInventoryWidget::SlotTooltip(int32 I) const
{
 if(!Hunting || I<0)return FText::GetEmpty();const int32 Count=Hunting->Columns*Hunting->Rows;
 if(I==Count+Hunting->CompanionWheelSlot)return FText::FromString(TEXT("버니아\n기본 동행 · 항상 사용 가능\n휠 안에서 선택 위치를 옮길 수 있어요."));
 if(const auto* E=EntryForSlot(I);E && E->Item)
  return FText::FromString(E->Item->DisplayName.ToString()+TEXT("\n")+E->Item->RarityLabel().ToString()+(E->Item->Description.IsEmpty() ? FString():TEXT("\n")+E->Item->Description.ToString()));
 if(I==Count+6)return FText::FromString(TEXT("버니아 추가 장비\n빈 상태가 기본 버니아예요.\n추가 장비를 이곳에 장착할 수 있어요."));
 if(I==Count+7)return FText::FromString(TEXT("방어구\n장착할 방어구를 이곳에 놓으세요."));
 if(I==Count+8)return FText::FromString(TEXT("장신구\n장착할 장신구를 이곳에 놓으세요."));
 return FText::GetEmpty();
}
void UBBSBInventoryWidget::ActivateSlot(int32 I)
{
 if(!Hunting || I<0)return;const int32 Count=Hunting->Columns*Hunting->Rows;
 if(bHoldingCompanionPosition) { if(I>=Count && I<Count+6 && Hunting->MoveCompanionWheelSlot(I-Count))bHoldingCompanionPosition=false; }
 else if(!HeldModule.IsNone())return;
 else if(HeldId.IsValid())
 {
  const auto* Before=Hunting->FindOwnedEntry(HeldId);const int32 Quantity=Before ? Before->Quantity:0;
  if(Hunting->PlaceOwnedItem(HeldId,InventoryDestination(I))) { const auto* After=Hunting->FindOwnedEntry(HeldId);if(!After || After->Quantity==Quantity)HeldId.Invalidate(); }
 }
 else if(I==Count+Hunting->CompanionWheelSlot)bHoldingCompanionPosition=true;
 else if(const auto* E=EntryForSlot(I))HeldId=E->InstanceId;
 UpdatePointerSurface();InvalidateLayoutAndVolatility();
}
void UBBSBInventoryWidget::UpdatePointerSurface() { SetVisibility(HasCarriedItem() || bSortOpen || !UIButtons.IsEmpty() ? ESlateVisibility::Visible:ESlateVisibility::SelfHitTestInvisible); }
void UBBSBInventoryWidget::Navigate(int32 DX,int32 DY)
{
 if(GrowthPage>0) { NavigateGrowth(DX,DY);return; }
 bSortOpen=false;UpdatePointerSurface();bKeyboardMode=true;const int32 Count=Hunting ? Hunting->Columns*Hunting->Rows:40;FocusIndex=FMath::Clamp(FocusIndex,0,Count+8);
 const FVector2D From=SlotCenter(FocusIndex),Direction(DX,DY);double Best=MAX_dbl;int32 Next=FocusIndex;
 for(int32 I=0;I<Count+9;++I)
 {
  if(I==Count+6)continue;
  const FVector2D Delta=SlotCenter(I)-From;const double Along=FVector2D::DotProduct(Delta,Direction);if(Along<1)continue;
  const double Side=FMath::Abs(Delta.X*Direction.Y-Delta.Y*Direction.X),Score=Delta.Size()+Side*2;
  if(Score<Best) { Best=Score;Next=I; }
 }
 FocusIndex=Next;
 if(FocusIndex<Count) { if(FocusIndex/5<GridScrollRow)GridScrollRow=FocusIndex/5;if(FocusIndex/5>=GridScrollRow+PaintedLayout.VisibleRows)GridScrollRow=FocusIndex/5-PaintedLayout.VisibleRows+1; }
 InvalidateLayoutAndVolatility();
}
FReply UBBSBInventoryWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
 Pointer=G.AbsoluteToLocal(E.GetScreenSpacePosition());bPointerInside=true;bKeyboardMode=false;
 const auto L=Layout(G);HoverIndex=PointerSlot(L,Pointer);
 const bool SortButton=GrowthPage!=1 && InRect(Pointer,L.Sort,L.SortSize),Popup=bSortOpen && InRect(Pointer,L.SortPanel,L.SortPanelSize);
 if(bSortOpen && !SortButton && !Popup) { bSortOpen=false;UpdatePointerSurface(); }
 if(!InRect(Pointer,L.Panel,L.Size) && !InRect(Pointer,L.Equipment,L.EquipmentSize) && !HasCarriedItem())return FReply::Unhandled();
 UIButtons.Add(E.GetEffectingButton());
 if(E.GetEffectingButton()==EKeys::RightMouseButton)
 { if(bSortOpen)bSortOpen=false;else { HeldId.Invalidate();HeldModule=NAME_None;bHoldingCompanionPosition=false; } }
 else if(E.GetEffectingButton()==EKeys::LeftMouseButton)
 {
  const int32 Tab=GrowthTabAt(L,Pointer);
  if(Tab!=INDEX_NONE)SetGrowthPage(Tab);
  else if(SortButton) { if(auto* P=Cast<ABBSBCharacter>(GetOwningPlayerPawn());GrowthPage==2 && P && P->EquipmentGrowth)P->EquipmentGrowth->SortBunniarInventory();else if(Hunting)Hunting->SortInventory();bSortOpen=false; }
  else if(Popup)bSortPressed=InRect(Pointer,L.SortApply,L.SortApplySize);
  else if(IsClosePoint(L,Pointer))bClosePressed=true;
  else if(GrowthPage>0)ActivateGrowthControl(GrowthControlAt(Pointer));
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
 const auto L=Layout(G);const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition());if(!InRect(P,L.Panel,L.Size) && !InRect(P,L.Equipment,L.EquipmentSize))return FReply::Unhandled();
 if(GrowthPage!=1) { int32 Rows=Hunting ? Hunting->Rows:8;if(auto* Player=Cast<ABBSBCharacter>(GetOwningPlayerPawn());GrowthPage==2 && Player && Player->EquipmentGrowth)Rows=FMath::DivideAndRoundUp(Player->EquipmentGrowth->BunniarBagCapacity(),5);GridScrollRow=FMath::Clamp(GridScrollRow-(E.GetWheelDelta()>0 ? 1:-1),0,FMath::Max(0,Rows-L.VisibleRows));InvalidateLayoutAndVolatility(); }return FReply::Handled();
}
FReply UBBSBInventoryWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
 const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition());bPointerInside=true;if(!P.Equals(Pointer,.1f))bKeyboardMode=false;Pointer=P;
 HoverIndex=GrowthPage>0 ? GrowthControlAt(Pointer):PointerSlot(Layout(G),Pointer);if(!bKeyboardMode && HoverIndex!=INDEX_NONE)FocusIndex=HoverIndex;InvalidateLayoutAndVolatility();return FReply::Unhandled();
}
void UBBSBInventoryWidget::NativeOnMouseLeave(const FPointerEvent& E) { bPointerInside=false;HoverIndex=INDEX_NONE;InvalidateLayoutAndVolatility();Super::NativeOnMouseLeave(E); }
FReply UBBSBInventoryWidget::NativeOnKeyDown(const FGeometry&,const FKeyEvent& E) { return ProcessMenuKey(E.GetKey()) ? FReply::Handled():FReply::Unhandled(); }
bool UBBSBInventoryWidget::ProcessMenuKey(FKey K)
{
 if(K==EKeys::Tab) { ResetInteraction();if(Hunting)Hunting->CloseInventory(); }
 else if(K==EKeys::Escape || K==EKeys::Gamepad_FaceButton_Right) { if(bSortOpen) { bSortOpen=false;UpdatePointerSurface();InvalidateLayoutAndVolatility(); }else if(HasCarriedItem()) { HeldId.Invalidate();HeldModule=NAME_None;bHoldingCompanionPosition=false;UpdatePointerSurface();InvalidateLayoutAndVolatility(); }else if(Hunting)Hunting->CloseInventory(); }
 else if(K==EKeys::PageUp || K==EKeys::Gamepad_LeftShoulder)SetGrowthPage((GrowthPage+2)%3);
 else if(K==EKeys::PageDown || K==EKeys::Gamepad_RightShoulder)SetGrowthPage((GrowthPage+1)%3);
 else if(K==EKeys::Left || K==EKeys::Gamepad_DPad_Left)Navigate(-1,0);
 else if(K==EKeys::Right || K==EKeys::Gamepad_DPad_Right)Navigate(1,0);
 else if(K==EKeys::Up || K==EKeys::Gamepad_DPad_Up)Navigate(0,-1);
 else if(K==EKeys::Down || K==EKeys::Gamepad_DPad_Down)Navigate(0,1);
 else if(K==EKeys::Enter || K==EKeys::Gamepad_FaceButton_Bottom) { bKeyboardMode=true;if(bSortOpen) { if(Hunting)Hunting->SortInventory();bSortOpen=false;UpdatePointerSurface();InvalidateLayoutAndVolatility(); }else if(GrowthPage>0)ActivateGrowthControl(FocusIndex);else ActivateSlot(FocusIndex); }
 else return false;
 return true;
}
