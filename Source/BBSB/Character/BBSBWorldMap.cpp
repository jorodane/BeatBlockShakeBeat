#include "Character/BBSBWorldMap.h"
#include "Character/BBSBRespawn.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Interaction/BBSBInteraction.h"
#include "Combat/BBSBCombat.h"
#include "Field/BBSBFieldActions.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
 bool InMapRect(FVector2D P,FVector2D At,FVector2D Size)
 { return P.X>=At.X && P.Y>=At.Y && P.X<=At.X+Size.X && P.Y<=At.Y+Size.Y; }
}

TSharedRef<SWidget> UBBSBWorldMapWidget::RebuildWidget()
{
 SetIsFocusable(bFullMap);ForceVolatile(true);
 if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this,TEXT("MapTree"));
 if(!WidgetTree->RootWidget)
 {
  auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
  auto* Surface=WidgetTree->ConstructWidget<UImage>();Surface->SetColorAndOpacity(FLinearColor::Transparent);
  auto* SurfaceSlot=Root->AddChildToCanvas(Surface);SurfaceSlot->SetAnchors(FAnchors(0,0,1,1));SurfaceSlot->SetOffsets(FMargin(0));
 }
 return Super::RebuildWidget();
}
FVector2D UBBSBWorldMapWidget::MarkerPosition(const ABBSBRespawnPoint* Point) const
{ return Map && Point ? MapAt+(Map->WorldToMap(Point->GetActorLocation())-ViewMinimum())*Zoom*MapSize:FVector2D::ZeroVector; }
void UBBSBWorldMapWidget::ClampCenter()
{ ViewCenterUV.X=FMath::Clamp(ViewCenterUV.X,0.,1.);ViewCenterUV.Y=FMath::Clamp(ViewCenterUV.Y,0.,1.); }
void UBBSBWorldMapWidget::ZoomAtViewport(float NewZoom,FVector2D Anchor)
{
 if(!bFullMap)return;
 Anchor.X=FMath::Clamp(Anchor.X,0.,1.);Anchor.Y=FMath::Clamp(Anchor.Y,0.,1.);
 const FVector2D WorldAnchor=ViewMinimum()+Anchor/Zoom;
 const float Next=FMath::Clamp(NewZoom,1.f,8.f);if(FMath::IsNearlyEqual(Zoom,Next))return;
 Zoom=Next;ViewCenterUV=WorldAnchor+(FVector2D(.5)-Anchor)/Zoom;ClampCenter();InvalidateLayoutAndVolatility();
}
void UBBSBWorldMapWidget::ZoomBy(float Factor)
{ if(FMath::IsFinite(Factor) && Factor>0)ZoomAtViewport(Zoom*Factor,FVector2D(.5)); }
void UBBSBWorldMapWidget::ResetView()
{ Zoom=1;ViewCenterUV=FVector2D(.5);bFollowPlayer=false;InvalidateLayoutAndVolatility(); }
void UBBSBWorldMapWidget::CenterOnPlayer()
{ bFollowPlayer=true;UpdateFollowingPlayer(); }
void UBBSBWorldMapWidget::UpdateFollowingPlayer()
{ if(bFollowPlayer && Map && Map->MapPlayer()) { ViewCenterUV=Map->WorldToMap(Map->MapPlayer()->GetActorLocation());ClampCenter();InvalidateLayoutAndVolatility(); } }
FVector2D UBBSBWorldMapWidget::NavigationButtonCenter(FName Key) const
{
 const float S=PaintScale;
 if(Key==TEXT("ZoomOut"))return ToolbarAt+FVector2D(16,14)*S;
 if(Key==TEXT("ZoomIn"))return ToolbarAt+FVector2D(54,14)*S;
 if(Key==TEXT("Reset"))return ToolbarAt+FVector2D(107,14)*S;
 if(Key==TEXT("Player"))return ToolbarAt+FVector2D(190,14)*S;
 return FVector2D::ZeroVector;
}
void UBBSBWorldMapWidget::CancelPointerDrag()
{
 bPanPending=bDragging=false;
 if(FSlateApplication::IsInitialized())if(const auto Widget=GetCachedWidget();Widget.IsValid() && Widget->HasMouseCapture())FSlateApplication::Get().ReleaseAllPointerCapture();
}
int32 UBBSBWorldMapWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Cull,FSlateWindowElementList& E,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled) const
{
 const int32 Base=Super::NativePaint(Args,G,Cull,E,Layer,Style,ParentEnabled);
 if(!Map || !Map->MapPlayer() || (!bFullMap && !Map->ShouldDrawMinimap()))return Base;
 const auto* Player=Map->MapPlayer();const FVector2D V=G.GetLocalSize();const float S=FMath::Clamp(float(V.Y/720.f),.55f,1.8f);PaintScale=S;
 const FLinearColor Paper(.97f,.93f,.86f),Mint(.48f,.91f,.76f),Grey(.55f,.56f,.61f),Gold(1.f,.78f,.37f),Dark(.065f,.055f,.085f,.96f);
 const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 auto Box=[&](FVector2D P,FVector2D Size,FLinearColor C,int32 Z=0){FSlateDrawElement::MakeBox(E,Base+Z,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),White,ESlateDrawEffect::None,C);};
 auto Text=[&](const FString& T,FVector2D P,float Size,FLinearColor C,int32 Z=5){FSlateDrawElement::MakeText(E,Base+Z,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(P)),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(8,FMath::RoundToInt(Size*S))),ESlateDrawEffect::None,C);};
 auto Lines=[&](const TArray<FVector2D>& P,FLinearColor C,float Width=1,int32 Z=4){FSlateDrawElement::MakeLines(E,Base+Z,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,Width*S);};
 FVector2D UVMin,UVMax;
 if(bFullMap)
 {
  Box(FVector2D::ZeroVector,V,FLinearColor(.015f,.02f,.03f,.90f));
  const float Side=FMath::Min(float(V.Y-202*S),float(V.X-330*S));MapSize=FVector2D(FMath::Max(150.f*S,Side));
  MapAt=FVector2D((V.X-MapSize.X-260*S)*.5f,96*S);ListAt=MapAt+FVector2D(MapSize.X+24*S,0);ToolbarAt=MapAt+FVector2D(0,MapSize.Y+34*S);
  CloseAt=FVector2D(ListAt.X+175*S,MapAt.Y-52*S);CloseSize=FVector2D(36*S);
  Text(TEXT("지도"),MapAt-FVector2D(0,65*S),27,Paper);
  Text(TEXT("휠 확대·축소 · 드래그 이동 · M / Esc 닫기"),MapAt+FVector2D(76,-48)*S,11,Grey);
  Box(CloseAt,CloseSize,FLinearColor(.25f,.20f,.29f));Text(TEXT("×"),CloseAt+FVector2D(10,4)*S,22,Paper);
  Text(TEXT("리스폰 표식"),ListAt,19,Paper);
  int32 Row=0;
  for(ABBSBRespawnPoint* Point:Map->Points)if(IsValid(Point))
  {
   const FVector2D At=ListAt+FVector2D(0,(36+Row++*48)*S);const bool Available=Map->IsPointAvailable(Point);
   const bool Hover=InMapRect(Pointer,At,FVector2D(220,42)*S);
   Box(At,FVector2D(220,42)*S,Hover && Available ? FLinearColor(.22f,.37f,.34f):FLinearColor(.12f,.11f,.15f));
   Text(Available ? Point->PointName.ToString():TEXT("미등록 표식"),At+FVector2D(12,5)*S,14,Available ? Mint:Grey);
   Text(Available ? TEXT("클릭하여 버니아로 이동"):TEXT("먼저 표식에 상호작용하세요"),At+FVector2D(12,24)*S,9,Grey);
  }
  Text(TEXT("▲ 현재 위치    ◆ 등록한 표식    ◇ 미등록"),MapAt+FVector2D(0,MapSize.Y+12*S),10,Paper);
  auto Button=[&](FVector2D At,FVector2D Size,const FString& Label,bool Enabled)
  { const bool Hover=InMapRect(Pointer,At,Size);Box(At,Size,Enabled && Hover ? Mint:Grey,1);Box(At+FVector2D(S),Size-FVector2D(2*S),Dark,2);Text(Label,At+FVector2D(8,5)*S,11,Enabled ? Paper:Grey,5); };
  Button(ToolbarAt,FVector2D(32,28)*S,TEXT("−"),Zoom>1.001f);Button(ToolbarAt+FVector2D(38,0)*S,FVector2D(32,28)*S,TEXT("+"),Zoom<7.999f);
  Button(ToolbarAt+FVector2D(76,0)*S,FVector2D(62,28)*S,TEXT("100%"),true);Button(ToolbarAt+FVector2D(145,0)*S,FVector2D(90,28)*S,TEXT("내 위치"),true);
  Text(FString::Printf(TEXT("%d%%"),FMath::RoundToInt(Zoom*100)),ToolbarAt+FVector2D(250,5)*S,12,Mint);
  UVMin=ViewMinimum();UVMax=UVMin+FVector2D(1.f/Zoom);
 }
 else
 {
  MapSize=FVector2D(164*S);MapAt=FVector2D(20,116)*S;
  const float Fraction=FMath::Clamp(Map->MinimapHalfExtent/FMath::Max(1.f,Map->MapHalfExtent),.025f,.5f);
  FVector2D Center=Map->WorldToMap(Player->GetActorLocation());Center.X=FMath::Clamp(Center.X,Fraction,1.-Fraction);Center.Y=FMath::Clamp(Center.Y,Fraction,1.-Fraction);
  UVMin=Center-FVector2D(Fraction);UVMax=Center+FVector2D(Fraction);
 }
 Box(MapAt-FVector2D(3*S),MapSize+FVector2D(6*S),Paper,1);Box(MapAt,MapSize,Dark,2);
 E.PushClip(FSlateClippingZone(G.ToPaintGeometry(MapSize,FSlateLayoutTransform(MapAt))));
 const FVector2D VisibleMin(FMath::Max(0.,UVMin.X),FMath::Max(0.,UVMin.Y)),VisibleMax(FMath::Min(1.,UVMax.X),FMath::Min(1.,UVMax.Y));
 if(Map->MapTexture && VisibleMax.X>VisibleMin.X && VisibleMax.Y>VisibleMin.Y)
 {
  const FVector2D ImageAt=MapAt+(VisibleMin-UVMin)/(UVMax-UVMin)*MapSize,ImageSize=(VisibleMax-VisibleMin)/(UVMax-UVMin)*MapSize;
  FSlateBrush Brush;Brush.SetResourceObject(Map->MapTexture);Brush.ImageSize=ImageSize;Brush.SetUVRegion(FBox2f(FVector2f(VisibleMin),FVector2f(VisibleMax)));
  FSlateDrawElement::MakeBox(E,Base+3,G.ToPaintGeometry(ImageSize,FSlateLayoutTransform(ImageAt)),&Brush,ESlateDrawEffect::None,FLinearColor::White);
 }
 auto Project=[&](FVector P){return MapAt+(Map->WorldToMap(P)-UVMin)/(UVMax-UVMin)*MapSize;};
 for(ABBSBRespawnPoint* Point:Map->Points)if(IsValid(Point))
 {
  const FVector2D P=Project(Point->GetActorLocation());if(!InMapRect(P,MapAt+FVector2D(8*S),MapSize-FVector2D(16*S)))continue;
  const bool Known=Map->IsPointAvailable(Point),Active=Player->Respawn->ActivePoint==Point;
  const float R=(bFullMap ? 9.f:5.f)*S;const FLinearColor C=Active ? Gold:Known ? Mint:Grey;
  const TArray<FVector2D> Diamond={P+FVector2D(0,-R),P+FVector2D(R,0),P+FVector2D(0,R),P+FVector2D(-R,0),P+FVector2D(0,-R)};
  Lines(Diamond,FLinearColor(.02f,.03f,.04f),4,4);Lines(Diamond,C,2,5);
  if(bFullMap && Known)Text(Point->PointName.ToString(),P+FVector2D(13,-8)*S,11,Paper,6);
 }
 const FVector2D At=Project(Player->GetActorLocation());const FVector F=Player->GetActorForwardVector();const FVector2D Forward(F.Y,-F.X),Right(-Forward.Y,Forward.X);const float R=(bFullMap ? 9:7)*S;
 if(InMapRect(At,MapAt,MapSize))
 { const TArray<FVector2D> Arrow={At+Forward*R,At-Forward*R*.7+Right*R*.65,At-Forward*R*.35,At-Forward*R*.7-Right*R*.65,At+Forward*R};Lines(Arrow,FLinearColor::Black,4,7);Lines(Arrow,Paper,2,8); }
 Text(TEXT("N"),MapAt+FVector2D(MapSize.X*.5f-5*S,5*S),12,Paper,8);E.PopClip();
 if(!bFullMap)Text(TEXT("M  지도"),MapAt+FVector2D(0,MapSize.Y+7*S),10,Paper);
 return Base+10;
}
FReply UBBSBWorldMapWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& Event)
{
 if(bFullMap)
 {
  const auto Key=Event.GetKey();
  if(Key==EKeys::M || Key==EKeys::Escape || Key==EKeys::Gamepad_FaceButton_Right) { if(Map)Map->CloseMap();return FReply::Handled(); }
  if(Key==EKeys::Add || Key==EKeys::Equals) { ZoomBy(1.25f);return FReply::Handled(); }
  if(Key==EKeys::Subtract || Key==EKeys::Hyphen) { ZoomBy(.8f);return FReply::Handled(); }
  if(Key==EKeys::Zero || Key==EKeys::NumPadZero) { ResetView();return FReply::Handled(); }
  if(Key==EKeys::Home) { CenterOnPlayer();return FReply::Handled(); }
 }
 return Super::NativeOnKeyDown(G,Event);
}
FReply UBBSBWorldMapWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& Event)
{
 if(!bFullMap || !Map || Event.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
 Pointer=G.AbsoluteToLocal(Event.GetScreenSpacePosition());
 if(InMapRect(Pointer,CloseAt,CloseSize)) { Map->CloseMap();return FReply::Handled(); }
 const float S=PaintScale;
 if(InMapRect(Pointer,ToolbarAt,FVector2D(32,28)*S)) { ZoomBy(.8f);return FReply::Handled(); }
 if(InMapRect(Pointer,ToolbarAt+FVector2D(38,0)*S,FVector2D(32,28)*S)) { ZoomBy(1.25f);return FReply::Handled(); }
 if(InMapRect(Pointer,ToolbarAt+FVector2D(76,0)*S,FVector2D(62,28)*S)) { ResetView();return FReply::Handled(); }
 if(InMapRect(Pointer,ToolbarAt+FVector2D(145,0)*S,FVector2D(90,28)*S)) { CenterOnPlayer();return FReply::Handled(); }
 int32 Row=0;for(ABBSBRespawnPoint* Point:Map->Points)if(IsValid(Point))
 { const FVector2D Entry=ListAt+FVector2D(0,(36+Row++*48)*S);if(InMapRect(Pointer,Entry,FVector2D(220,42)*S)) { if(Map->IsPointAvailable(Point))Map->TravelTo(Point);return FReply::Handled(); } }
 if(InMapRect(Pointer,MapAt,MapSize))
 { DragStart=PreviousDrag=Pointer;bPanPending=true;bDragging=false;return FReply::Handled().CaptureMouse(TakeWidget()); }
 return FReply::Handled();
}
FReply UBBSBWorldMapWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& Event)
{
 Pointer=G.AbsoluteToLocal(Event.GetScreenSpacePosition());
 if(bPanPending)
 {
  if((Pointer-DragStart).Size()>4*PaintScale)bDragging=true;
  if(bDragging) { bFollowPlayer=false;ViewCenterUV-=(Pointer-PreviousDrag)/MapSize/Zoom;ClampCenter();InvalidateLayoutAndVolatility(); }
  PreviousDrag=Pointer;
 }
 return bFullMap ? FReply::Handled():FReply::Unhandled();
}
FReply UBBSBWorldMapWidget::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& Event)
{
 if(!bPanPending || Event.GetEffectingButton()!=EKeys::LeftMouseButton)return bFullMap ? FReply::Handled():FReply::Unhandled();
 Pointer=G.AbsoluteToLocal(Event.GetScreenSpacePosition());const bool Click=!bDragging && (Pointer-DragStart).Size()<=4*PaintScale;bPanPending=bDragging=false;
 if(Click && Map && InMapRect(Pointer,MapAt,MapSize))for(ABBSBRespawnPoint* Point:Map->Points)if(IsValid(Point) && FVector2D::Distance(Pointer,MarkerPosition(Point))<16*PaintScale)
 { if(Map->IsPointAvailable(Point))Map->TravelTo(Point);break; }
 return FReply::Handled().ReleaseMouseCapture();
}
FReply UBBSBWorldMapWidget::NativeOnMouseWheel(const FGeometry& G,const FPointerEvent& Event)
{
 if(!bFullMap)return FReply::Unhandled();Pointer=G.AbsoluteToLocal(Event.GetScreenSpacePosition());
 if(InMapRect(Pointer,MapAt,MapSize)) { bFollowPlayer=false;ZoomAtViewport(Zoom*FMath::Pow(1.25f,FMath::Clamp(Event.GetWheelDelta(),-4.f,4.f)),(Pointer-MapAt)/MapSize); }
 return FReply::Handled();
}
FReply UBBSBWorldMapWidget::NativeOnMouseButtonDoubleClick(const FGeometry& G,const FPointerEvent& Event)
{ return NativeOnMouseButtonDown(G,Event); }
void UBBSBWorldMapWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{ bPanPending=bDragging=false;Super::NativeOnMouseCaptureLost(Event); }

UBBSBWorldMapComponent::UBBSBWorldMapComponent()
{ PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork; }
void UBBSBWorldMapComponent::BeginPlay() { Super::BeginPlay();Player=Cast<ABBSBCharacter>(GetOwner()); }
FVector2D UBBSBWorldMapComponent::WorldToMap(FVector P) const
{ const float Width=FMath::Max(1.f,MapHalfExtent)*2;return FVector2D(.5+(P.Y-MapCenter.Y)/Width,.5-(P.X-MapCenter.X)/Width); }
FVector UBBSBWorldMapComponent::MapToWorld(FVector2D UV) const
{ return FVector(MapCenter.X+(.5-UV.Y)*MapHalfExtent*2,MapCenter.Y+(UV.X-.5)*MapHalfExtent*2,0); }
bool UBBSBWorldMapComponent::IsPointAvailable(ABBSBRespawnPoint* Point) const
{ return Player && Player->Respawn && Player->Respawn->IsPointDiscovered(Point); }
FVector2D UBBSBWorldMapComponent::MapMarkerPosition(ABBSBRespawnPoint* Point) const
{ return FullWidget ? FullWidget->MarkerPosition(Point):FVector2D::ZeroVector; }
void UBBSBWorldMapComponent::RefreshPoints()
{
 Points.Reset();for(TActorIterator<ABBSBRespawnPoint> It(GetWorld());It;++It)Points.Add(*It);
 Points.Sort([](const ABBSBRespawnPoint& A,const ABBSBRespawnPoint& B){return A.TravelId()<B.TravelId();});
}
void UBBSBWorldMapComponent::EnsurePresentation()
{
 if(!Player)return;
 if(!MapTexture)
 {
  MapTexture=NewObject<UTextureRenderTarget2D>(this,TEXT("WorldMapTexture"));MapTexture->RenderTargetFormat=RTF_RGBA8;MapTexture->ClearColor=FLinearColor(.07f,.09f,.1f);MapTexture->InitAutoFormat(FMath::Clamp(TextureResolution,512,4096),FMath::Clamp(TextureResolution,512,4096));MapTexture->UpdateResourceImmediate(true);
  Capture=NewObject<USceneCaptureComponent2D>(Player,TEXT("WorldMapCapture"));Player->AddInstanceComponent(Capture);Capture->SetupAttachment(Player->GetRootComponent());Capture->SetAbsolute(true,true,true);
  Capture->ProjectionType=ECameraProjectionMode::Orthographic;Capture->OrthoWidth=MapHalfExtent*2;Capture->bAutoCalculateOrthoPlanes=true;Capture->CaptureSource=SCS_FinalColorLDR;Capture->TextureTarget=MapTexture;Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
  Capture->ShowFlags.SetAtmosphere(false);Capture->ShowFlags.SetFog(false);Capture->ShowFlags.SetVolumetricFog(false);Capture->ShowFlags.SetPostProcessing(false);Capture->ShowFlags.SetTemporalAA(false);Capture->ShowFlags.SetMotionBlur(false);
  Capture->RegisterComponent();Capture->SetWorldLocationAndRotation(FVector(MapCenter,12000),FRotator(-90,0,0));
 }
 if(!MiniWidget)
 { auto* PC=Cast<APlayerController>(Player->GetController());MiniWidget=CreateWidget<UBBSBWorldMapWidget>(PC,UBBSBWorldMapWidget::StaticClass());if(MiniWidget) { MiniWidget->Map=this;MiniWidget->SetVisibility(ESlateVisibility::HitTestInvisible);MiniWidget->AddToViewport(10); } }
}
bool UBBSBWorldMapComponent::ShouldDrawMinimap() const
{ return Player && !bMapOpen && !Player->Hunting->bInventoryOpen && !Player->Respawn->IsSequenceActive() && !Player->EquipmentGrowth->bConcertoActive && !Player->Combat->bDead; }
void UBBSBWorldMapComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DT,Type,Function);if(!Player || !Player->IsLocallyControlled())return;EnsurePresentation();
 if(bMapOpen && FullWidget)FullWidget->UpdateFollowingPlayer();
 const double Now=GetWorld()->GetTimeSeconds();if(Now>=NextPointScanAt) { NextPointScanAt=Now+1;RefreshPoints(); }
 if(Capture && Now>=NextCaptureAt)
 { NextCaptureAt=Now+8;Capture->HiddenActors.Reset();Capture->HiddenActors.Add(Player);if(Player->FieldActions->BunniarEquipment)Capture->HiddenActors.Add(Player->FieldActions->BunniarEquipment);Capture->CaptureScene(); }
}
void UBBSBWorldMapComponent::ToggleMap() { if(bMapOpen)CloseMap();else OpenMap(); }
bool UBBSBWorldMapComponent::OpenMap()
{
 if(!Player || bMapOpen || Player->Respawn->IsSequenceActive() || Player->Combat->bDead || Player->EquipmentGrowth->bConcertoActive || Player->Interaction->bDialogueActive || Player->Combat->IsStageLocked())return false;
 auto* PC=Cast<APlayerController>(Player->GetController());if(!PC || !PC->IsLocalController())return false;
 Player->Hunting->CloseInventory();Player->FieldActions->CancelEquipmentSelection();EnsurePresentation();RefreshPoints();
 bOriginalBlock=Player->FieldActions->bEquipmentInputBlocked;Player->FieldActions->bEquipmentInputBlocked=true;bOriginalCursor=PC->bShowMouseCursor;
 PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bOwnInput=true;if(Player->GetCharacterMovement()->IsMovingOnGround())Player->GetCharacterMovement()->StopMovementImmediately();Player->ConsumeMovementInputVector();PC->RotationInput=FRotator::ZeroRotator;
 if(!FullWidget) { FullWidget=CreateWidget<UBBSBWorldMapWidget>(PC,UBBSBWorldMapWidget::StaticClass());if(!FullWidget) { bMapOpen=true;CloseMap();return false; }FullWidget->Map=this;FullWidget->bFullMap=true; }
 FullWidget->SetVisibility(ESlateVisibility::Visible);FullWidget->AddToViewport(1200);bMapOpen=true;PC->bShowMouseCursor=true;
 PC->SetInputMode(FInputModeUIOnly().SetWidgetToFocus(FullWidget->TakeWidget()).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));FullWidget->SetKeyboardFocus();return true;
}
void UBBSBWorldMapComponent::CloseMap()
{
 if(!bMapOpen && !bOwnInput)return;bMapOpen=false;if(FullWidget) { FullWidget->CancelPointerDrag();FullWidget->RemoveFromParent(); }
 if(Player)
 {
  Player->FieldActions->bEquipmentInputBlocked=bOriginalBlock;
  if(auto* PC=Cast<APlayerController>(Player->GetController()))
  { if(bOwnInput) { PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false); }PC->RotationInput=FRotator::ZeroRotator;PC->bShowMouseCursor=bOriginalCursor;
    if(bOriginalCursor)PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));else PC->SetInputMode(FInputModeGameOnly().SetConsumeCaptureMouseDown(false)); }
 }
 bOwnInput=false;
}
bool UBBSBWorldMapComponent::TravelTo(ABBSBRespawnPoint* Point)
{ return Player && Player->Respawn && Player->Respawn->RequestFastTravel(Point); }
void UBBSBWorldMapComponent::EndPlay(const EEndPlayReason::Type Reason)
{ CloseMap();if(MiniWidget)MiniWidget->RemoveFromParent();if(Capture)Capture->DestroyComponent();Super::EndPlay(Reason); }
