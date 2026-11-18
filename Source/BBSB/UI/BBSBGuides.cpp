#include "UI/BBSBGuides.h"
#include "UI/BBSBPaperTheme.h"
#include "Save/BBSBSession.h"
#include "BBSBCharacter.h"
#include "Field/BBSBFieldActions.h"
#include "Hunting/BBSBHunting.h"
#include "Character/BBSBWorldMap.h"
#include "Character/BBSBRespawn.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "Stage01/BBSBStage01.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Brushes/SlateColorBrush.h"
#include "InputCoreTypes.h"
#include "Caravan/BBSBCaravan.h"
#include "Components/PrimitiveComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "UI/BBSBOutlinedPaths.h"
#include "Engine/Engine.h"
#include "GameFramework/HUD.h"

UBBSBGuideSubsystem::UBBSBGuideSubsystem()
{
 for(const TCHAR* Name:{TEXT("BunniarAttach"),TEXT("BunniarMove")})
  GuideDefinitions.Add(TSoftObjectPtr<UBBSBGuideDefinition>(FSoftObjectPath(FString::Printf(TEXT("/Game/BBSB/Stage01/Guides/DA_Guide_%s.DA_Guide_%s"),Name,Name))));
}
UBBSBGuideSubsystem* UBBSBGuideSubsystem::Get(const UObject* Context)
{
 const auto* W=Context ? Context->GetWorld():nullptr;return W && W->GetGameInstance() ? W->GetGameInstance()->GetSubsystem<UBBSBGuideSubsystem>():nullptr;
}
UWorld* UBBSBGuideSubsystem::GetTickableGameObjectWorld() const { return GetGameInstance() ? GetGameInstance()->GetWorld():nullptr; }
void UBBSBGuideSubsystem::Deinitialize() { CloseGuide();if(Notebook)Notebook->RemoveFromParent();Notebook=nullptr;Super::Deinitialize(); }
UBBSBGuideDefinition* UBBSBGuideSubsystem::Definition(FName Id) const
{
 for(const auto& Soft:GuideDefinitions)if(auto* D=Soft.LoadSynchronous();D && D->GuideId==Id)return D;return nullptr;
}
TArray<FName> UBBSBGuideSubsystem::KnownGuides() const
{
 TArray<FName> Result;for(FName Id:SeenGuides)if(Definition(Id))Result.Add(Id);Result.Sort(FNameLexicalLess());return Result;
}
void UBBSBGuideSubsystem::Tick(float DT)
{
 UWorld* W=GetWorld();if(!W || !W->IsGameWorld() || W->bIsTearingDown)return;
 if(bGuideOpen && GuideWorld.Get()!=W) { CloseGuide();return; }
 const auto* S=UBBSBSessionSubsystem::Get(W);
 auto* P=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(W,0));
 bHintsVisible=S && S->bGameStarted && !S->bLoading && !S->bMenuOpen && !bOpeningActive && !bGuideOpen && !W->IsPaused() && P && !P->Combat->bDead && !P->Interaction->bDialogueActive && !P->Hunting->bInventoryOpen && !P->WorldMap->bMapOpen && !P->Respawn->IsSequenceActive();
 if(!bHintsVisible) { DoorAimSeconds=0;return; }
 TickHints(P,DT);
 const auto* B=Cast<ABBSBBunniarCompanion>(P->FieldActions->BunniarEquipment);
 if(B && B->bSurfaceAttached && B->SurfaceArrivalAlpha>=1 && B->SurfaceComponent)
 {
  const auto* Door=Cast<ABBSBStage01Device>(BBSBBunniarSurface::Resolve(B->SurfaceComponent->GetOwner()));
  if(Door && Door->Kind==EBBSBStage01Node::CabinDoor && !SeenGuides.Contains(TEXT("BunniarMove")))
  { DoorAimSeconds=0;UnlockAndShow(TEXT("BunniarMove"));return; }
 }
 if(P->FieldActions->IsGestureCaptured() || SeenGuides.Contains(TEXT("BunniarAttach"))) { DoorAimSeconds=0;return; }
 FHitResult Hit;const bool HitAim=P->FieldActions->TraceEquipmentAim(Hit);
 const auto* Door=HitAim ? Cast<ABBSBStage01Device>(BBSBBunniarSurface::Resolve(Hit.GetActor())):nullptr;
 if(!Door || Door->Kind!=EBBSBStage01Node::CabinDoor) { DoorAimSeconds=0;return; }
 DoorAimSeconds+=DT;if(DoorAimSeconds>=.25f) { DoorAimSeconds=0;UnlockAndShow(TEXT("BunniarAttach")); }
}
bool UBBSBGuideSubsystem::CanLearnControls() const
{
 const auto* S=UBBSBSessionSubsystem::Get(this);const UWorld* W=GetWorld();
 return W && W->IsGameWorld() && !W->IsPaused() && S && S->bGameStarted && !S->bMenuOpen && !S->bLoading && !bOpeningActive && !bGuideOpen;
}
void UBBSBGuideSubsystem::NotifyMoveInput()
{
 if(!bLearnedMove && CanLearnControls()) { bLearnedMove=true;if(auto* S=UBBSBSessionSubsystem::Get(this))S->RequestAutoSave(TEXT("이동 조작 익힘")); }
}
void UBBSBGuideSubsystem::NotifyLookInput()
{
 if(!bLearnedLook && CanLearnControls()) { bLearnedLook=true;if(auto* S=UBBSBSessionSubsystem::Get(this))S->RequestAutoSave(TEXT("시점 조작 익힘")); }
}
void UBBSBGuideSubsystem::NotifySurfaceMotion(FVector2D Delta)
{ if(Delta.SizeSquared()>=.5f && CanLearnControls())LastSurfaceMotionAt=GetWorld()->GetTimeSeconds(); }
void UBBSBGuideSubsystem::TickHints(ABBSBCharacter* P,float DT)
{
 GameplayHintSeconds+=DT;const float A=1.f-FMath::Exp(-12.f*DT);
 MoveHintAlpha=FMath::Lerp(MoveHintAlpha,!bLearnedMove && GameplayHintSeconds>.8f ? 1.f:0.f,A);
 LookHintAlpha=FMath::Lerp(LookHintAlpha,!bLearnedLook && GameplayHintSeconds>.8f ? 1.f:0.f,A);
 auto* B=Cast<ABBSBBunniarCompanion>(P->FieldActions->BunniarEquipment);
 auto* C=B && B->bSurfaceAttached && B->SurfaceArrivalAlpha>=1.f ? B->SurfaceComponent.Get():nullptr;
 if(HintSurface.Get()!=C) { HintSurface=C;DragHintAlpha=0;LastSurfaceMotionAt=-100; }
 bool Supported=false;bCarryHint=false;
 if(C)
 {
  HintPoint=B->SurfacePoint;
  if(const auto* D=Cast<ABBSBStage01Device>(BBSBBunniarSurface::Resolve(C->GetOwner())))
  {
   Supported=D->DragHint(B,DragHintAxis,bDragBothWays);
   const TCHAR* Caption=TEXT("[화살표 방향으로 드래그하여 이동]");
   switch(D->Kind)
   {
    case EBBSBStage01Node::CabinDoor:Caption=D->Progress>=.95f ? TEXT("[좌우로 드래그하여 닫기]"):TEXT("[좌우로 드래그하여 열기]");break;
    case EBBSBStage01Node::LowerLadder:Caption=TEXT("[아래로 드래그하여 사다리 내리기]");break;
    case EBBSBStage01Node::CargoLever:Caption=TEXT("[아래로 드래그하여 고정 풀기]");break;
    case EBBSBStage01Node::LightLever:Caption=TEXT("[아래로 드래그하여 조명 켜기]");break;
    case EBBSBStage01Node::Trapdoor:Caption=TEXT("[드래그하여 덮개 열기]");break;
    case EBBSBStage01Node::ReadyGate:Caption=TEXT("[화살표 방향으로 드래그하여 열기]");break;
    default:break;
   }
   DragHintText=FText::FromString(Caption);
  }
  else if(const auto* Object=Cast<ABBSBCarryObject>(C->GetOwner());Object && Object->bCarried)
  { Supported=true;bCarryHint=true;bDragBothWays=false;DragHintAxis=FVector2D(1,0);DragHintText=FText::FromString(TEXT("[앞으로 던지기]")); }
 }
 const bool Idle=GetWorld()->GetTimeSeconds()-LastSurfaceMotionAt>=HintIdleSeconds;
 DragHintAlpha=FMath::Lerp(DragHintAlpha,Supported && Idle ? 1.f:0.f,A);
}
bool UBBSBGuideSubsystem::Open(FName Id,bool Notes)
{
 auto* S=UBBSBSessionSubsystem::Get(this);auto* PC=UGameplayStatics::GetPlayerController(this,0);
 if(!S || !S->bGameStarted || S->bLoading || bOpeningActive || !PC || bGuideOpen)return false;
 if(!Notes && !Definition(Id))return false;
 if(const auto* P=Cast<ABBSBCharacter>(PC->GetPawn()))
 { bPausedPrimary=P->FieldActions->IsGestureChannelHeld(0);bPausedShortcut=P->FieldActions->bBunniarShortcutActive; }
 bReleasedPrimary=bReleasedShortcut=false;bHintsVisible=false;
 SelectedGuide=Id;Controller=PC;GuideWorld=GetWorld();bFromMenu=S->bMenuOpen;
 if(!bFromMenu)
 {
  bPreviousCursor=PC->bShowMouseCursor;PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bOwnMoveIgnore=bOwnLookIgnore=true;
  bOwnPause=!GetWorld()->IsPaused() && UGameplayStatics::SetGamePaused(this,true);
 }
 else if(S->Menu)S->Menu->SetVisibility(ESlateVisibility::Collapsed);
 if(!Notebook)Notebook=CreateWidget<UBBSBGuideNotebook>(PC,UBBSBGuideNotebook::StaticClass());Notebook->Guides=this;
 if(!Notebook->IsInViewport())Notebook->AddToViewport(2100);Notebook->SetVisibility(ESlateVisibility::Visible);Notebook->OpenPaper(EBBSBPaperEntrance::Center);bGuideOpen=true;
 PC->bShowMouseCursor=true;FInputModeUIOnly Input;Input.SetWidgetToFocus(Notebook->TakeWidget());Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Input);Notebook->Refresh();return true;
}
bool UBBSBGuideSubsystem::UnlockAndShow(FName Id)
{
 if(SeenGuides.Contains(Id) || !Definition(Id))return false;
 if(!Open(Id,false))return false;SeenGuides.Add(Id);Notebook->Refresh();
 if(auto* S=UBBSBSessionSubsystem::Get(this))S->RequestAutoSave(TEXT("가이드 기록"));return true;
}
void UBBSBGuideSubsystem::OpenNotes() { const auto Known=KnownGuides();Open(Known.IsEmpty() ? NAME_None:Known[0],true); }
void UBBSBGuideSubsystem::SelectGuide(FName Id) { if(bGuideOpen && SeenGuides.Contains(Id) && Definition(Id)) { SelectedGuide=Id;Notebook->Refresh(); } }
void UBBSBGuideSubsystem::CloseGuide()
{
 if(!bGuideOpen)return;if(Notebook)Notebook->ClosePaper();
 auto* PC=Controller.Get();auto* S=UBBSBSessionSubsystem::Get(this);
 if(PC)
 {
  if(bOwnMoveIgnore)PC->SetIgnoreMoveInput(false);if(bOwnLookIgnore)PC->SetIgnoreLookInput(false);
  if(bFromMenu && S && S->bMenuOpen && S->Menu)
  { S->Menu->SetVisibility(ESlateVisibility::Visible);FInputModeUIOnly Input;Input.SetWidgetToFocus(S->Menu->TakeWidget());PC->SetInputMode(Input);S->Menu->Refresh(); }
  else
  {
   PC->bShowMouseCursor=bPreviousCursor;FInputModeGameOnly Input;PC->SetInputMode(Input);
   // UI-only input consumes releases while paused. Reconcile only this guide's
   // held interaction, preserving a button that the player is still holding.
   if(auto* P=Cast<ABBSBCharacter>(PC->GetPawn()))
   {
    const bool MouseUp=FSlateApplication::IsInitialized() && !FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
    P->FieldActions->ReconcileGuideRelease(bPausedPrimary && (bReleasedPrimary || MouseUp),bPausedShortcut && bReleasedShortcut);
   }
  }
 }
 if(bOwnPause && GuideWorld.IsValid() && !GuideWorld->bIsTearingDown)UGameplayStatics::SetGamePaused(GuideWorld.Get(),false);
 bGuideOpen=bFromMenu=bOwnPause=bOwnMoveIgnore=bOwnLookIgnore=false;Controller.Reset();GuideWorld.Reset();
}
void UBBSBGuideSubsystem::ResetForNewGame()
{
 CloseGuide();SeenGuides.Reset();SelectedGuide=NAME_None;DoorAimSeconds=GameplayHintSeconds=0;
 bLearnedMove=bLearnedLook=false;MoveHintAlpha=LookHintAlpha=DragHintAlpha=0;HintSurface.Reset();LastSurfaceMotionAt=-100;
}
void UBBSBGuideSubsystem::GuideInputReleased(FKey Key)
{ if(Key==EKeys::LeftMouseButton)bReleasedPrimary=true;if(Key==EKeys::F)bReleasedShortcut=true; }


namespace
{
 UTextBlock* Label(UWidgetTree* T,const FString& Value,int32 Size,FLinearColor Color)
 { auto* L=T->ConstructWidget<UTextBlock>();L->SetText(FText::FromString(Value));L->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),Size));L->SetColorAndOpacity(Color);L->SetAutoWrapText(true);return L; }
 UButton* Button(UWidgetTree* T,const FString& Text)
 {
  auto* B=T->ConstructWidget<UButton>();FButtonStyle S;S.SetNormal(FSlateColorBrush(BBSBPaper::Cell())).SetHovered(FSlateColorBrush(BBSBPaper::Hover())).SetPressed(FSlateColorBrush(FLinearColor(.73f,.58f,.60f)));
  B->SetStyle(S);auto* L=Label(T,Text,18,BBSBPaper::Ink());Cast<UButtonSlot>(B->AddChild(L))->SetPadding(FMargin(14,12));return B;
 }
}
TSharedRef<SWidget> UBBSBGuideNotebook::RebuildWidget()
{
 if(!WidgetTree->RootWidget)
 {
  SetIsFocusable(true);auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
  auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(FLinearColor(.035f,.026f,.035f,.72f));auto* Full=Root->AddChildToCanvas(Shade);Full->SetAnchors(FAnchors(0,0,1,1));Full->SetOffsets(FMargin(0));
  auto* Fit=WidgetTree->ConstructWidget<UScaleBox>();Fit->SetStretch(EStretch::ScaleToFit);auto* FitSlot=Root->AddChildToCanvas(Fit);FitSlot->SetAnchors(FAnchors(0,0,1,1));FitSlot->SetOffsets(FMargin(40,35,40,35));
  auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(1100);Size->SetHeightOverride(670);Fit->AddChild(Size);PaperAnimationTarget=Size;
  auto* Layers=WidgetTree->ConstructWidget<UOverlay>();Size->AddChild(Layers);
  auto* Shadow=WidgetTree->ConstructWidget<UBorder>();Shadow->SetBrushColor(BBSBPaper::Shadow());auto* Sh=Layers->AddChildToOverlay(Shadow);Sh->SetPadding(FMargin(10,12,-10,-12));Sh->SetHorizontalAlignment(HAlign_Fill);Sh->SetVerticalAlignment(VAlign_Fill);
  auto* Ink=WidgetTree->ConstructWidget<UBorder>();Ink->SetBrushColor(BBSBPaper::Ink());Ink->SetPadding(FMargin(2));auto* InkSlot=Layers->AddChildToOverlay(Ink);InkSlot->SetHorizontalAlignment(HAlign_Fill);InkSlot->SetVerticalAlignment(VAlign_Fill);
  auto* Paper=WidgetTree->ConstructWidget<UBorder>();Paper->SetBrushColor(BBSBPaper::Paper());Paper->SetPadding(FMargin(28,24));Ink->AddChild(Paper);
  auto* Content=WidgetTree->ConstructWidget<UVerticalBox>();Paper->AddChild(Content);
  auto* Top=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChildToVerticalBox(Top)->SetPadding(FMargin(0,0,0,18));
  Top->AddChildToHorizontalBox(Label(WidgetTree,TEXT("여행의 기록"),13,BBSBPaper::Rose()))->SetVerticalAlignment(VAlign_Center);
  auto* Heading=Label(WidgetTree,TEXT("  여행 가이드"),30,BBSBPaper::Ink());Top->AddChildToHorizontalBox(Heading)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  Count=Label(WidgetTree,TEXT(""),16,BBSBPaper::Muted());Count->SetAutoWrapText(false);Top->AddChildToHorizontalBox(Count)->SetVerticalAlignment(VAlign_Center);
  CloseButton=Button(WidgetTree,TEXT("닫기  ×"));Cast<UTextBlock>(CloseButton->GetChildAt(0))->SetAutoWrapText(false);Top->AddChildToHorizontalBox(CloseButton)->SetPadding(FMargin(22,0,0,0));CloseButton->OnClicked.AddDynamic(this,&UBBSBGuideNotebook::Dismiss);
  auto* Body=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  auto* Left=WidgetTree->ConstructWidget<USizeBox>();Left->SetWidthOverride(245);Body->AddChildToHorizontalBox(Left)->SetPadding(FMargin(0,0,24,0));
  auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Left->AddChild(Scroll);List=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(List);
  auto* Right=WidgetTree->ConstructWidget<UVerticalBox>();Body->AddChildToHorizontalBox(Right)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  Category=Label(WidgetTree,TEXT(""),14,BBSBPaper::Rose());Right->AddChildToVerticalBox(Category);
  Title=Label(WidgetTree,TEXT(""),26,BBSBPaper::Ink());Right->AddChildToVerticalBox(Title)->SetPadding(FMargin(0,2,0,8));
  auto* ArtSize=WidgetTree->ConstructWidget<USizeBox>();ArtSize->SetHeightOverride(180);Right->AddChildToVerticalBox(ArtSize);
  auto* Art=WidgetTree->ConstructWidget<UOverlay>();ArtSize->AddChild(Art);
  Diagram=WidgetTree->ConstructWidget<UBBSBGuideIllustration>();auto* DS=Art->AddChildToOverlay(Diagram);DS->SetHorizontalAlignment(HAlign_Fill);DS->SetVerticalAlignment(VAlign_Fill);
  Artwork=WidgetTree->ConstructWidget<UImage>();auto* AS=Art->AddChildToOverlay(Artwork);AS->SetHorizontalAlignment(HAlign_Fill);AS->SetVerticalAlignment(VAlign_Fill);
  Description=Label(WidgetTree,TEXT(""),17,BBSBPaper::Ink());Right->AddChildToVerticalBox(Description)->SetPadding(FMargin(0,10,0,10));
  auto* StepScroll=WidgetTree->ConstructWidget<UScrollBox>();Right->AddChildToVerticalBox(StepScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Steps=WidgetTree->ConstructWidget<UVerticalBox>();StepScroll->AddChild(Steps);
  Content->AddChildToVerticalBox(Label(WidgetTree,TEXT("[Enter] 확인     [Esc] 닫기     본 안내는 메뉴의 ‘여행 가이드’에서 다시 볼 수 있어요."),14,BBSBPaper::Muted()))->SetPadding(FMargin(0,15,0,0));
 }
 return Super::RebuildWidget();
}
void UBBSBGuideNotebook::Refresh()
{
 if(!List || !Guides)return;List->ClearChildren();Steps->ClearChildren();Clicks.Reset();const auto Known=Guides->KnownGuides();Count->SetText(FText::FromString(FString::Printf(TEXT("발견한 안내 %d"),Known.Num())));
 for(FName Id:Known)if(auto* D=Guides->Definition(Id))
 {
  auto* B=Button(WidgetTree,(Id==Guides->SelectedGuide ? TEXT("●  "):TEXT("    "))+D->Title.ToString());Cast<UTextBlock>(B->GetChildAt(0))->SetAutoWrapText(false);List->AddChildToVerticalBox(B)->SetPadding(FMargin(0,0,0,8));
  auto* C=NewObject<UBBSBGuideClick>(this);C->Owner=Guides;C->Id=Id;B->OnClicked.AddDynamic(C,&UBBSBGuideClick::Click);Clicks.Add(C);
 }
 auto* D=Guides->Definition(Guides->SelectedGuide);
 Title->SetText(D ? D->Title:FText::FromString(TEXT("아직 비어 있는 페이지")));Category->SetText(D ? D->Category:FText::FromString(TEXT("탐험 기록")));
 Description->SetText(D ? D->Description:FText::FromString(TEXT("여행 중 새로 본 사용법이 여기에 기록돼요.")));
 Artwork->SetVisibility(D && D->Illustration ? ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);if(D && D->Illustration)Artwork->SetBrushFromTexture(D->Illustration);
 Diagram->GuideId=Guides->SelectedGuide;Diagram->SetVisibility(D && !D->Illustration ? ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 if(D)for(int32 I=0;I<D->Steps.Num();++I)
 {
  auto* Line=WidgetTree->ConstructWidget<UHorizontalBox>();Steps->AddChildToVerticalBox(Line)->SetPadding(FMargin(0,0,0,9));
  auto* N=Label(WidgetTree,FString::Printf(TEXT("%02d  "),I+1),20,BBSBPaper::Rose());Line->AddChildToHorizontalBox(N);
  auto* Words=WidgetTree->ConstructWidget<UVerticalBox>();Line->AddChildToHorizontalBox(Words)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Words->AddChildToVerticalBox(Label(WidgetTree,D->Steps[I].Name.ToString(),18,BBSBPaper::Ink()));Words->AddChildToVerticalBox(Label(WidgetTree,D->Steps[I].Description.ToString(),16,BBSBPaper::Muted()));
 }
 CloseButton->SetKeyboardFocus();
}
void UBBSBGuideClick::Click() { if(Owner)Owner->SelectGuide(Id); }
void UBBSBGuideNotebook::Dismiss() { if(Guides)Guides->CloseGuide(); }
FReply UBBSBGuideNotebook::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
 if(E.GetKey()==EKeys::Escape || E.GetKey()==EKeys::Enter || E.GetKey()==EKeys::Gamepad_FaceButton_Right) { Dismiss();return FReply::Handled(); }
 return Super::NativeOnKeyDown(G,E);
}
FReply UBBSBGuideNotebook::NativeOnKeyUp(const FGeometry& G,const FKeyEvent& E)
{ if(Guides)Guides->GuideInputReleased(E.GetKey());return Super::NativeOnKeyUp(G,E); }
FReply UBBSBGuideNotebook::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{ if(Guides)Guides->GuideInputReleased(E.GetEffectingButton());return Super::NativeOnMouseButtonUp(G,E); }
int32 UBBSBGuideIllustration::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Culling,FSlateWindowElementList& E,int32 L,const FWidgetStyle& Style,bool Enabled) const
{
 L=Super::NativePaint(Args,G,Culling,E,L,Style,Enabled);const FVector2D S=G.GetLocalSize();const float X=S.X/720.f,Y=S.Y/240.f;
 auto P=[&](float A,float B){return FVector2D(A*X,B*Y);};
 auto Box=[&](FVector2D At,FVector2D Size,FLinearColor Color){BBSBPaper::SlatePolygon(G,E,L+1,{At,At+FVector2D(Size.X,0),At+Size,At+FVector2D(0,Size.Y)},Color);};
 auto Line=[&](TArray<FVector2D> Points,FLinearColor C,float Width=2.f){FSlateDrawElement::MakeLines(E,L+3,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,C,true,Width);};
 auto Circle=[&](FVector2D At,float R,FLinearColor C,bool Fill)
 {TArray<FVector2D> V;for(int I=0;I<=48;++I)V.Add(At+P(FMath::Cos(I*PI/24)*R,FMath::Sin(I*PI/24)*R));if(Fill)BBSBPaper::SlatePolygon(G,E,L+2,V,C);else Line(V,C);};
 auto Text=[&](const FString& T,float A,float B,int Size,FLinearColor C){FSlateDrawElement::MakeText(E,L+4,G.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(P(A,B))),T,FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),Size),ESlateDrawEffect::None,C);};
 Box(P(0,0),S,FLinearColor(.85f,.81f,.73f));
 // A book illustration: split sliding door, Bunniar's back on a leaf, and the actual drag direction.
 Box(P(292,20),P(310,176),BBSBPaper::Ink());Box(P(300,28),P(140,160),FLinearColor(.45f,.56f,.53f));Box(P(452,28),P(142,160),FLinearColor(.56f,.65f,.60f));
 Line({P(323,48),P(415,48),P(415,166),P(323,166),P(323,48)},BBSBPaper::Paper());Line({P(477,48),P(570,48),P(570,166),P(477,166),P(477,48)},BBSBPaper::Paper());
 Circle(P(382,102),28,BBSBPaper::Paper(),true);Circle(P(382,102),28,BBSBPaper::Ink(),false);
 Line({P(365,84),P(355,58),P(347,81),P(356,100)},BBSBPaper::Ink(),5);Line({P(396,82),P(407,57),P(414,80),P(406,102)},BBSBPaper::Ink(),5);
 Circle(P(373,100),3,BBSBPaper::Rose(),true);Circle(P(390,100),3,BBSBPaper::Rose(),true);Line({P(377,113),P(384,117),P(391,112)},BBSBPaper::Rose());
 Line({P(235,103),P(312,103)},BBSBPaper::Rose(),4);Line({P(295,90),P(312,103),P(295,116)},BBSBPaper::Rose(),4);
 if(GuideId==TEXT("BunniarMove"))
 {
 Line({P(350,158),P(244,158),P(260,145)},BBSBPaper::Rose(),4);Line({P(492,158),P(648,158),P(632,145)},BBSBPaper::Rose(),4);
 }
 Box(P(87,66),P(62,86),BBSBPaper::Ink());Box(P(91,70),P(54,78),BBSBPaper::Paper());Line({P(118,70),P(118,101)},BBSBPaper::Ink());Circle(P(118,84),5,BBSBPaper::Rose(),true);
 const bool Move=GuideId==TEXT("BunniarMove");
 Text(Move ? TEXT("위치 조정"):TEXT("버니아 부착"),30,20,13,BBSBPaper::Rose());Text(TEXT("좌클릭 유지"),46,170,18,BBSBPaper::Ink());
 Text(Move ? TEXT("←  마우스를 좌우로  →"):TEXT("대상을 조준하고 버니아 보내기"),293,208,18,BBSBPaper::Rose());
 return L+5;
}

void UBBSBGuideSubsystem::DrawHints(AHUD* HUD,UCanvas* C) const
{
 if(!bHintsVisible || !HUD || !C)return;
 const float S=FMath::Clamp(C->SizeX/1280.f,.65f,1.4f),W=C->SizeX,H=C->SizeY;
 auto Color=[](FLinearColor V,float A){V.A*=A;return V;};
 auto Text=[&](const FString& T,FVector2D P,float A,float Scale=1.f)
 {
  FCanvasTextItem Item(P,FText::FromString(T),GEngine->GetMediumFont(),Color(BBSBPaper::Paper(),A));
  Item.Scale=FVector2D(S*Scale);Item.bOutlined=true;Item.OutlineColor=Color(BBSBPaper::Ink(),A);C->DrawItem(Item);
 };
 TArray<BBSBOutlinedPaths::FPath> Paths;
 auto Path=[&](TArray<FVector2D> Points,float Opacity,float Width,bool Closed=false)
 { BBSBOutlinedPaths::FPath P;P.Points=MoveTemp(Points);P.Color=Color(BBSBPaper::Paper(),Opacity);P.Width=Width*S;P.Border=2.2f*S;P.bClosed=Closed;Paths.Add(MoveTemp(P)); };
 auto Line=[&](FVector2D A,FVector2D B,float Opacity,float Width=2.f) { Path({A,B},Opacity,Width); };
 auto Keycap=[&](FVector2D P,float A)
 {
  const FVector2D Size(23*S);Path({P,P+FVector2D(Size.X,0),P+Size,P+FVector2D(0,Size.Y)},A,1.8f,true);
 };
 auto Mouse=[&](FVector2D P,float A,bool Right)
 {
  TArray<FVector2D> Outer;
  // Two continuous semicircles joined by straight sides; no midpoint step.
  for(int32 I=0;I<=24;++I) { const float T=PI+I*PI/24.f;Outer.Add(P+FVector2D(11.5f+FMath::Cos(T)*11.5f,11.5f+FMath::Sin(T)*11.5f)*S); }
  for(int32 I=0;I<=24;++I) { const float T=I*PI/24.f;Outer.Add(P+FVector2D(11.5f+FMath::Cos(T)*11.5f,22.5f+FMath::Sin(T)*11.5f)*S); }
  Path(MoveTemp(Outer),A,2.6f,true);
  const FVector2D Pressed=P+FVector2D(Right ? 16:7,6)*S;
  Line(Pressed,Pressed+FVector2D(0,7)*S,A,3);
  Line(P+FVector2D(11.5f,0)*S,P+FVector2D(11.5f,13)*S,A,1);
 };
 const float Time=GetWorld()->GetTimeSeconds();
 if(MoveHintAlpha>.01f)
 {
  const FVector2D P(W*.5f-242*S,H-245*S);
  const FVector2D Base=P+FVector2D(20,34)*S;
  const FVector2D Keys[]={FVector2D(27,-25),FVector2D(0,0),FVector2D(27,0),FVector2D(54,0)};const TCHAR* Names[]={TEXT("W"),TEXT("A"),TEXT("S"),TEXT("D")};
  for(int I=0;I<4;++I){const FVector2D K=Base+Keys[I]*S;Keycap(K,MoveHintAlpha);Text(Names[I],K+FVector2D(5,1)*S,MoveHintAlpha,.75f);}
  Text(TEXT("이동하기"),P+FVector2D(116,26)*S,MoveHintAlpha,.85f);
 }
 if(LookHintAlpha>.01f)
 {
  const FVector2D P(W*.5f+10*S,H-245*S);
  Mouse(P+FVector2D(30+FMath::Sin(Time*3)*8,20)*S,LookHintAlpha,false);
  Line(P+FVector2D(17,39)*S,P+FVector2D(12,39)*S,LookHintAlpha);Line(P+FVector2D(70,39)*S,P+FVector2D(76,39)*S,LookHintAlpha);
  Text(TEXT("마우스로 둘러보기"),P+FVector2D(88,27)*S,LookHintAlpha,.77f);
 }
 if(DragHintAlpha>.01f && HintSurface.IsValid())
 {
  FVector2D At;const auto* PC=HUD->GetOwningPlayerController();
  if(!PC || !PC->ProjectWorldLocationToScreen(HintPoint,At,true))return;
  At.X=FMath::Clamp(At.X,156*S,W-156*S);At.Y=FMath::Clamp(At.Y+88*S,85*S,H-150*S);
  const FVector2D Centre=At-FVector2D(0,17)*S,Axis=DragHintAxis.GetSafeNormal(),Perp(-Axis.Y,Axis.X);
  auto Arrow=[&](FVector2D D)
  { const FVector2D Tip=Centre+D*66*S,Tail=Centre+D*26*S;Line(Tail,Tip,DragHintAlpha,3.5f);Path({Tip-D*12*S+Perp*8*S,Tip,Tip-D*12*S-Perp*8*S},DragHintAlpha,3.5f); };
  if(!bCarryHint) { Arrow(Axis);if(bDragBothWays)Arrow(-Axis); }
  Mouse(Centre-FVector2D(11.5f,17)*S+(bCarryHint ? FVector2D::ZeroVector:Axis*(FMath::Sin(Time*3.4f)*9*S)),DragHintAlpha,bCarryHint);
  const FString Caption=DragHintText.ToString();float TW,TH;HUD->GetTextSize(Caption,TW,TH,GEngine->GetMediumFont(),S*.9f);
  Text(Caption,FVector2D(At.X-TW*.5f,At.Y+10*S),DragHintAlpha,.9f);
 }
 BBSBOutlinedPaths::Draw(C,Paths,BBSBPaper::Ink());
}
