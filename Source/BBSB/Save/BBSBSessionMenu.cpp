#include "Save/BBSBSession.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

namespace
{
 UTextBlock* Text(UWidgetTree* Tree,const FString& Label,int32 Size,FLinearColor Color)
 {
  auto* T=Tree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Label));T->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),Size));T->SetColorAndOpacity(Color);T->SetAutoWrapText(true);return T;
 }
}
TSharedRef<SWidget> UBBSBSessionMenu::RebuildWidget()
{
 if(!WidgetTree->RootWidget)
 {
  SetIsFocusable(true);auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
  auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(FLinearColor(.035f,.028f,.038f,.62f));
  auto* Background=Root->AddChildToCanvas(Shade);Background->SetAnchors(FAnchors(0,0,1,1));Background->SetOffsets(FMargin(0));
  auto* Box=WidgetTree->ConstructWidget<USizeBox>();Box->SetWidthOverride(590);
  auto* Panel=Root->AddChildToCanvas(Box);Panel->SetAnchors(FAnchors(.5f,.5f));Panel->SetAlignment(FVector2D(.5f,.5f));Panel->SetAutoSize(true);
  auto* Border=WidgetTree->ConstructWidget<UBorder>();Border->SetBrushColor(FLinearColor(.16f,.13f,.155f,.98f));Border->SetPadding(FMargin(28,22));Box->AddChild(Border);
  auto* Content=WidgetTree->ConstructWidget<UVerticalBox>();Border->AddChild(Content);
  Content->AddChildToVerticalBox(Text(WidgetTree,TEXT("Beat! Block, Shake~ Beat!"),29,FLinearColor(.96f,.88f,.91f)))->SetPadding(FMargin(0,0,0,16));
  auto* ScrollSize=WidgetTree->ConstructWidget<USizeBox>();ScrollSize->SetMaxDesiredHeight(445);Content->AddChildToVerticalBox(ScrollSize);
  auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();ScrollSize->AddChild(Scroll);Rows=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Rows);
  Message=Text(WidgetTree,TEXT(""),16,FLinearColor(.78f,.83f,.86f));Content->AddChildToVerticalBox(Message)->SetPadding(FMargin(0,14,0,0));
 }
 return Super::RebuildWidget();
}
UButton* UBBSBSessionMenu::AddButton(const FString& Label,bool Enabled)
{
 auto* Button=WidgetTree->ConstructWidget<UButton>();Button->SetIsEnabled(Enabled);Button->SetBackgroundColor(FLinearColor(.28f,.235f,.27f));
 auto* Caption=Text(WidgetTree,Label,bLoadList ? 17:22,FLinearColor(.98f,.95f,.97f));Caption->SetJustification(ETextJustify::Left);
 Cast<UButtonSlot>(Button->AddChild(Caption))->SetPadding(FMargin(17,bLoadList ? 10:14));Rows->AddChildToVerticalBox(Button)->SetPadding(FMargin(0,4));return Button;
}
void UBBSBSessionMenu::Refresh()
{
 if(!Rows || !Session)return;Rows->ClearChildren();Clicks.Reset();UButton* Focus=nullptr;
 if(auto* Size=Cast<USizeBox>(Rows->GetParent() ? Rows->GetParent()->GetParent():nullptr))Size->SetHeightOverride(bLoadList ? 445.f:Session->bGameStarted ? 340.f:260.f);
 if(bLoadList)
 {
  int32 Order=0;for(const auto& Entry:Session->Slots)
  {
   const FString Label=FString::Printf(TEXT("%02d  %s%s%s"),++Order,*Entry.Title,Entry.Details.IsEmpty() ? TEXT(""):TEXT("\n"),*Entry.Details);
   auto* B=AddButton(Label,Entry.bValid);auto* Click=NewObject<UBBSBSessionSlotClick>(this);Click->Session=Session;Click->Index=Entry.Index;B->OnClicked.AddDynamic(Click,&UBBSBSessionSlotClick::Click);Clicks.Add(Click);if(!Focus && Entry.bValid)Focus=B;
  }
  auto* B=AddButton(TEXT("뒤로"));B->OnClicked.AddDynamic(this,&UBBSBSessionMenu::Back);if(!Focus)Focus=B;
 }
 else
 {
  auto* B=AddButton(TEXT("이어서"),Session->CanContinue());B->OnClicked.AddDynamic(this,&UBBSBSessionMenu::Continue);if(Session->CanContinue())Focus=B;
  B=AddButton(TEXT("처음부터"));B->OnClicked.AddDynamic(this,&UBBSBSessionMenu::NewGame);if(!Focus)Focus=B;
  B=AddButton(TEXT("불러오기"));B->OnClicked.AddDynamic(this,&UBBSBSessionMenu::LoadList);
  if(Session->bGameStarted) { B=AddButton(TEXT("지금 저장"));B->OnClicked.AddDynamic(this,&UBBSBSessionMenu::Save); }
 }
 const FString Hint=bLoadList ? TEXT("최근 10개의 기록 · 새 기록이 쌓이면 가장 오래된 기록부터 바뀌어요."):TEXT("");
 Message->SetText(FText::FromString(Session->StatusText.IsEmpty() ? Hint:Session->StatusText));if(Focus)Focus->SetKeyboardFocus();
}
void UBBSBSessionMenu::Continue() { if(Session)Session->ContinueGame(); }
void UBBSBSessionMenu::NewGame() { if(Session)Session->StartNewGame(); }
void UBBSBSessionMenu::LoadList() { bLoadList=true;if(Session)Session->RefreshSlots();Refresh(); }
void UBBSBSessionMenu::Back() { bLoadList=false;Refresh(); }
void UBBSBSessionMenu::Save() { if(Session) { Session->SaveNow(TEXT("직접 저장"),true);Refresh(); } }
void UBBSBSessionSlotClick::Click() { if(Session)Session->LoadSlot(Index); }
FReply UBBSBSessionMenu::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
 if(E.GetKey()==EKeys::Escape || E.GetKey()==EKeys::Gamepad_FaceButton_Right)
 { if(bLoadList)Back();else if(Session && Session->bGameStarted)Session->CloseMenu();return FReply::Handled(); }
 return Super::NativeOnKeyDown(G,E);
}
