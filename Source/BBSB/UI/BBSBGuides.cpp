#include "UI/BBSBGuides.h"
#include "Interaction/BBSBDraggable.h"
#include "Interaction/BBSBInteractable.h"
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
#include "Engine/Texture2D.h"
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
  else if (const auto* Drag = Cast<UBBSBDraggable>(UBBSBInteractable::Find(BBSBBunniarSurface::Resolve(C->GetOwner()))))
  {
   Supported = Drag->GetDragHint(DragHintAxis, DragHintText);
   bDragBothWays = true;
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
 if (!Notebook) Notebook = CreateWidget<UBBSBGuideNotebook>(PC, UBBSBGuideNotebook::StaticClass());
 if (!Notebook) return false;
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
 Notebook->Guides = this;
 Notebook->SetNotesMode(Notes);
 if (!Notebook->IsInViewport()) Notebook->AddToViewport(2100);
 Notebook->SetVisibility(ESlateVisibility::Visible);
 Notebook->OpenPaper(EBBSBPaperEntrance::Center);
 bGuideOpen = true;
 PC->bShowMouseCursor=true;FInputModeUIOnly Input;Input.SetWidgetToFocus(Notebook->TakeWidget());Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Input);Notebook->Refresh();return true;
}
bool UBBSBGuideSubsystem::UnlockAndShow(FName Id)
{
 if(SeenGuides.Contains(Id) || !Definition(Id))return false;
 if(!Open(Id,false))return false;SeenGuides.Add(Id);
 if(auto* S=UBBSBSessionSubsystem::Get(this))S->RequestAutoSave(TEXT("가이드 기록"));return true;
}
void UBBSBGuideSubsystem::OpenNotes() { const auto Known=KnownGuides();Open(Known.IsEmpty() ? NAME_None:Known[0],true); }
void UBBSBGuideSubsystem::SelectGuide(FName Id) { if(bGuideOpen && Notebook && Notebook->IsNotesMode() && SeenGuides.Contains(Id) && Definition(Id)) { SelectedGuide=Id;Notebook->Refresh(); } }
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
 UTextBlock* GuideLabel(UWidgetTree* Tree, const FText& Text, int32 Size, FLinearColor Color, FName Name = NAME_None)
 {
  auto* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
  Label->SetText(Text);
  Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size));
  Label->SetColorAndOpacity(Color);
  Label->SetAutoWrapText(true);
  return Label;
 }

 UButton* GuideButton(UWidgetTree* Tree, const FText& Text, bool bPrimary, FName Name = NAME_None)
 {
  auto* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
  FButtonStyle Style;
  Style.SetNormal(FSlateColorBrush(bPrimary ? BBSBPaper::Ink() : BBSBPaper::Cell()));
  Style.SetHovered(FSlateColorBrush(bPrimary ? BBSBPaper::Rose() : BBSBPaper::Hover()));
  Style.SetPressed(FSlateColorBrush(FLinearColor(.73f, .58f, .60f)));
  Button->SetStyle(Style);
  auto* Label = GuideLabel(Tree, Text, 18, bPrimary ? BBSBPaper::Paper() : BBSBPaper::Ink());
  if (bPrimary) Label->SetJustification(ETextJustify::Center);
  auto* Slot = CastChecked<UButtonSlot>(Button->AddChild(Label));
  Slot->SetPadding(FMargin(16, 11));
  Slot->SetHorizontalAlignment(HAlign_Fill);
  return Button;
 }
}

TSharedRef<SWidget> UBBSBGuideNotebook::RebuildWidget()
{
 if (!WidgetTree->RootWidget)
 {
  SetIsFocusable(true);
  auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
  WidgetTree->RootWidget = Root;

  auto* Shade = WidgetTree->ConstructWidget<UBorder>();
  Shade->SetBrushColor(FLinearColor(.035f, .026f, .035f, .72f));
  auto* Full = Root->AddChildToCanvas(Shade);
  Full->SetAnchors(FAnchors(0, 0, 1, 1));
  Full->SetOffsets(FMargin(0));

  auto* Fit = WidgetTree->ConstructWidget<UScaleBox>();
  Fit->SetStretch(EStretch::ScaleToFit);
  Fit->SetStretchDirection(EStretchDirection::DownOnly);
  auto* FitSlot = Root->AddChildToCanvas(Fit);
  FitSlot->SetAnchors(FAnchors(0, 0, 1, 1));
  FitSlot->SetOffsets(FMargin(32, 24, 32, 26));

  PanelSize = WidgetTree->ConstructWidget<USizeBox>();
  Fit->AddChild(PanelSize);
  PaperAnimationTarget = PanelSize;

  auto* Layers = WidgetTree->ConstructWidget<UOverlay>();
  PanelSize->AddChild(Layers);
  auto* Shadow = WidgetTree->ConstructWidget<UBorder>();
  Shadow->SetBrushColor(BBSBPaper::Shadow());
  auto* ShadowSlot = Layers->AddChildToOverlay(Shadow);
  ShadowSlot->SetPadding(FMargin(10, 12, -10, -12));
  ShadowSlot->SetHorizontalAlignment(HAlign_Fill);
  ShadowSlot->SetVerticalAlignment(VAlign_Fill);

  auto* Ink = WidgetTree->ConstructWidget<UBorder>();
  Ink->SetBrushColor(BBSBPaper::Ink());
  Ink->SetPadding(FMargin(2));
  auto* InkSlot = Layers->AddChildToOverlay(Ink);
  InkSlot->SetHorizontalAlignment(HAlign_Fill);
  InkSlot->SetVerticalAlignment(VAlign_Fill);
  auto* Paper = WidgetTree->ConstructWidget<UBorder>();
  Paper->SetBrushColor(BBSBPaper::Paper());
  Paper->SetPadding(FMargin(26, 22, 26, 20));
  Ink->AddChild(Paper);

  auto* Content = WidgetTree->ConstructWidget<UVerticalBox>();
  Paper->AddChild(Content);
  JournalHeader = GuideLabel(WidgetTree, NSLOCTEXT("BBSBGuide", "JournalTitle", "여행 가이드"), 29,
                             BBSBPaper::Ink(), TEXT("GuideJournalHeader"));
  Content->AddChildToVerticalBox(JournalHeader)->SetPadding(FMargin(0, 0, 0, 12));

  auto* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
  Content->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  JournalNavigation = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GuideJournalNavigation"));
  JournalNavigation->SetWidthOverride(224);
  Body->AddChildToHorizontalBox(JournalNavigation)->SetPadding(FMargin(0, 0, 22, 0));
  auto* ListScroll = WidgetTree->ConstructWidget<UScrollBox>();
  JournalNavigation->AddChild(ListScroll);
  List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GuideList"));
  ListScroll->AddChild(List);

  auto* Detail = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GuideDetail"));
  Body->AddChildToHorizontalBox(Detail)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  Title = GuideLabel(WidgetTree, FText(), 27, BBSBPaper::Ink(), TEXT("GuideTitle"));
  Detail->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 0, 0, 10));

  ArtFrame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GuideArtFrame"));
  Detail->AddChildToVerticalBox(ArtFrame)->SetPadding(FMargin(0, 0, 0, 12));
  auto* ArtFit = WidgetTree->ConstructWidget<UScaleBox>();
  ArtFit->SetStretch(EStretch::ScaleToFit);
  ArtFrame->AddChild(ArtFit);
  Artwork = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("GuideArtwork"));
  Artwork->SetVisibility(ESlateVisibility::HitTestInvisible);
  ArtFit->AddChild(Artwork);

  DetailScroll = WidgetTree->ConstructWidget<UScrollBox>();
  Detail->AddChildToVerticalBox(DetailScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  auto* Copy = WidgetTree->ConstructWidget<UVerticalBox>();
  DetailScroll->AddChild(Copy);
  Description = GuideLabel(WidgetTree, FText(), 17, BBSBPaper::Ink(), TEXT("GuideDescription"));
  Copy->AddChildToVerticalBox(Description)->SetPadding(FMargin(0, 0, 9, 10));
  Steps = WidgetTree->ConstructWidget<UVerticalBox>();
  Copy->AddChildToVerticalBox(Steps);

  auto* Close = GuideButton(WidgetTree, NSLOCTEXT("BBSBGuide", "CloseGuide", "닫기"), true, TEXT("GuideCloseButton"));
  Detail->AddChildToVerticalBox(Close)->SetPadding(FMargin(0, 13, 0, 0));
  Close->OnClicked.AddDynamic(this, &UBBSBGuideNotebook::Dismiss);
 }
 return Super::RebuildWidget();
}

void UBBSBGuideNotebook::Refresh()
{
 if (!List || !Guides) return;
 PanelSize->SetWidthOverride(bNotesMode ? 1050.f : 710.f);
 PanelSize->SetHeightOverride(bNotesMode ? 712.f : 694.f);
 JournalHeader->SetVisibility(bNotesMode ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
 JournalNavigation->SetVisibility(bNotesMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
 List->ClearChildren();
 Clicks.Reset();
 if (bNotesMode)
 {
  for (FName Id : Guides->KnownGuides())
  {
   const auto* Guide = Guides->Definition(Id);
   if (!Guide) continue;
   auto* Button = GuideButton(WidgetTree, Guide->Title, Id == Guides->SelectedGuide);
   List->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 0, 0, 8));
   auto* Click = NewObject<UBBSBGuideClick>(this);
   Click->Owner = Guides;
   Click->Id = Id;
   Button->OnClicked.AddDynamic(Click, &UBBSBGuideClick::Click);
   Clicks.Add(Click);
  }
 }

 const auto* Guide = Guides->Definition(Guides->SelectedGuide);
 Title->SetText(Guide ? Guide->Title : NSLOCTEXT("BBSBGuide", "EmptyGuide", "아직 본 가이드가 없어요"));
 Description->SetText(Guide ? Guide->Description : NSLOCTEXT("BBSBGuide", "EmptyGuideDescription", "여행 중에 본 사용법을 여기에 기록해요."));
 ArtFrame->SetHeightOverride(bNotesMode ? 270.f : 290.f);
 ArtFrame->SetVisibility(Guide && Guide->Illustration ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
 Artwork->SetBrushFromTexture(Guide ? Guide->Illustration.Get() : nullptr, true);
 Steps->ClearChildren();
 if (Guide)
 {
  for (const auto& Step : Guide->Steps)
  {
   auto* Words = WidgetTree->ConstructWidget<UVerticalBox>();
   Steps->AddChildToVerticalBox(Words)->SetPadding(FMargin(0, 0, 9, 7));
   Words->AddChildToVerticalBox(GuideLabel(WidgetTree, Step.Name, 17, BBSBPaper::Ink()));
   Words->AddChildToVerticalBox(GuideLabel(WidgetTree, Step.Description, 15, BBSBPaper::Muted()));
  }
 }
 DetailScroll->ScrollToStart();
}

void UBBSBGuideClick::Click()
{
 if (Owner) Owner->SelectGuide(Id);
}

void UBBSBGuideNotebook::Dismiss()
{
 if (Guides) Guides->CloseGuide();
}

FReply UBBSBGuideNotebook::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
 const FKey Key = Event.GetKey();
 if (Key == EKeys::Escape || Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Right)
 {
  if (!Event.IsRepeat()) Dismiss();
  return FReply::Handled();
 }
 return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UBBSBGuideNotebook::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
 if (Guides) Guides->GuideInputReleased(Event.GetKey());
 return Super::NativeOnKeyUp(Geometry, Event);
}

FReply UBBSBGuideNotebook::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
 if (Guides) Guides->GuideInputReleased(Event.GetEffectingButton());
 return Super::NativeOnMouseButtonUp(Geometry, Event);
}

void UBBSBGuideSubsystem::DrawHints(AHUD* HUD, UCanvas* Canvas) const
{
 if (!bHintsVisible || !HUD || !Canvas) return;
 const float Scale = FMath::Clamp(Canvas->SizeX / 1280.f, .65f, 1.4f);
 const float Width = Canvas->SizeX, Height = Canvas->SizeY;
 const auto* Catalog = UBBSBInteractTypes::DefaultCatalog();
 auto Icon = [&](FName Name, FVector2D Centre, float Size, float Alpha, float Angle = 0.f)
 {
  auto* Texture = Catalog ? Catalog->HintIcons.FindRef(Name).Get() : nullptr;
  if (!Texture) return;
  const float Aspect = float(Texture->GetSizeX()) / FMath::Max(1, Texture->GetSizeY());
  const FVector2D Pixels(FMath::Min(1.f, Aspect) * Size * Scale, FMath::Min(1.f, 1.f / Aspect) * Size * Scale);
  const FVector2D TopLeft = Centre - Pixels * .5f;
  HUD->DrawTexture(Texture, TopLeft.X, TopLeft.Y, Pixels.X, Pixels.Y, 0, 0, 1, 1,
                   FLinearColor::White.CopyWithNewOpacity(Alpha), BLEND_Translucent, 1.f, false, Angle, FVector2D(.5));
 };
 auto Text = [&](const FString& Value, FVector2D Position, float Alpha, float Size, bool bKey = false)
 {
  const FLinearColor Foreground = bKey ? BBSBPaper::Ink() : BBSBPaper::Paper();
  FCanvasTextItem Item(Position, FText::FromString(Value), GEngine->GetMediumFont(), Foreground.CopyWithNewOpacity(Alpha));
  Item.Scale = FVector2D(Scale * Size);
  Item.bOutlined = !bKey;
  Item.OutlineColor = BBSBPaper::Ink().CopyWithNewOpacity(Alpha);
  Canvas->DrawItem(Item);
 };

 const float Time = GetWorld()->GetTimeSeconds();
 if (MoveHintAlpha > .01f)
 {
  const FVector2D Position(Width * .5f - 242 * Scale, Height - 245 * Scale);
  const FVector2D Keys[] = {FVector2D(59, 17), FVector2D(31, 45), FVector2D(59, 45), FVector2D(87, 45)};
  const TCHAR* Names[] = {TEXT("W"), TEXT("A"), TEXT("S"), TEXT("D")};
  for (int32 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
  {
   const FVector2D Centre = Position + Keys[Index] * Scale;
   Icon(TEXT("Keycap"), Centre, 25, MoveHintAlpha);
   float TextWidth, TextHeight;
   HUD->GetTextSize(Names[Index], TextWidth, TextHeight, GEngine->GetMediumFont(), Scale * .7f);
   Text(Names[Index], Centre - FVector2D(TextWidth, TextHeight) * .5f, MoveHintAlpha, .7f);
  }
  Text(TEXT("이동하기"), Position + FVector2D(113, 26) * Scale, MoveHintAlpha, .85f);
 }
 if (LookHintAlpha > .01f)
 {
  const FVector2D Position(Width * .5f + 10 * Scale, Height - 245 * Scale);
  Icon(TEXT("MouseLook"), Position + FVector2D(43 + FMath::Sin(Time * 3) * 8, 36) * Scale, 42, LookHintAlpha);
  Text(TEXT("마우스로 둘러보기"), Position + FVector2D(88, 27) * Scale, LookHintAlpha, .77f);
 }
 if (DragHintAlpha > .01f && HintSurface.IsValid())
 {
  FVector2D Anchor;
  const auto* PC = HUD->GetOwningPlayerController();
  if (!PC || !PC->ProjectWorldLocationToScreen(HintPoint, Anchor, true)) return;
  Anchor.X = FMath::Clamp(Anchor.X, 156 * Scale, Width - 156 * Scale);
  Anchor.Y = FMath::Clamp(Anchor.Y + 88 * Scale, 85 * Scale, Height - 150 * Scale);
  const FVector2D Centre = Anchor - FVector2D(0, 17) * Scale;
  const FVector2D Axis = DragHintAxis.GetSafeNormal();
  if (!bCarryHint)
  {
   const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X));
   Icon(TEXT("Arrow"), Centre + Axis * (48 * Scale), 37, DragHintAlpha, Angle);
   if (bDragBothWays) Icon(TEXT("Arrow"), Centre - Axis * (48 * Scale), 37, DragHintAlpha, Angle + 180.f);
  }
  const FVector2D MouseNudge = bCarryHint ? FVector2D::ZeroVector : Axis * (FMath::Sin(Time * 3.4f) * 9 * Scale);
  Icon(bCarryHint ? TEXT("MouseRight") : TEXT("MouseLeft"), Centre + MouseNudge, 43, DragHintAlpha);
  const FString Caption = DragHintText.ToString();
  float TextWidth, TextHeight;
  HUD->GetTextSize(Caption, TextWidth, TextHeight, GEngine->GetMediumFont(), Scale * .9f);
  Text(Caption, Anchor + FVector2D(-TextWidth * .5f, 10 * Scale), DragHintAlpha, .9f);
 }
}

