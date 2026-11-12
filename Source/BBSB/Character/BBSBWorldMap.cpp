#include "Character/BBSBWorldMap.h"
#include "HAL/PlatformTime.h"
#include "UI/BBSBPaperTheme.h"
#include "Character/BBSBTerrainMapData.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
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
{ return Map && Point ? bFullMap ? MapAt+(Map->WorldToMap(Point->GetActorLocation())-ViewMinimum())*Zoom*MapSize:WorldToMinimap(Point->GetActorLocation()):FVector2D::ZeroVector; }
void UBBSBWorldMapWidget::ClampCenter()
{ ViewCenterUV.X=FMath::Clamp(ViewCenterUV.X,0.,1.);ViewCenterUV.Y=FMath::Clamp(ViewCenterUV.Y,0.,1.); }
void UBBSBWorldMapWidget::ZoomAtViewport(float NewZoom,FVector2D Anchor)
{
 if(!bFullMap)return;
 Anchor.X=FMath::Clamp(Anchor.X,0.,1.);Anchor.Y=FMath::Clamp(Anchor.Y,0.,1.);
 const FVector2D WorldAnchor=ViewMinimum()+Anchor/Zoom;
 const float Next=FMath::Clamp(NewZoom,1.f,32.f);if(FMath::IsNearlyEqual(Zoom,Next))return;
 Zoom=Next;ViewCenterUV=WorldAnchor+(FVector2D(.5)-Anchor)/Zoom;ClampCenter();InvalidateLayoutAndVolatility();
}
void UBBSBWorldMapWidget::ZoomBy(float Factor)
{ if(FMath::IsFinite(Factor) && Factor>0)ZoomAtViewport(Zoom*Factor,FVector2D(.5)); }
void UBBSBWorldMapWidget::ResetView()
{ Zoom=1;ViewCenterUV=FVector2D(.5);bFollowPlayer=false;InvalidateLayoutAndVolatility(); }
void UBBSBWorldMapWidget::CenterOnPlayer()
{ if(Map)Zoom=Map->DefaultViewZoom();bFollowPlayer=true;UpdateFollowingPlayer(); }
void UBBSBWorldMapWidget::UpdateFollowingPlayer()
{ if(bFollowPlayer && Map && Map->MapPlayer()) { ViewCenterUV=Map->WorldToMap(Map->MapPlayer()->GetActorLocation());ClampCenter();InvalidateLayoutAndVolatility(); } }
FVector2D UBBSBWorldMapWidget::NavigationButtonCenter(FName Key) const
{
 const float S=PaintScale;
 if(Key==TEXT("ZoomOut"))return ToolbarAt+FVector2D(16,14)*S;
 if(Key==TEXT("ZoomIn"))return ToolbarAt+FVector2D(54,14)*S;
 if(Key==TEXT("Reset"))return ToolbarAt+FVector2D(119,14)*S;
 if(Key==TEXT("Player"))return ToolbarAt+FVector2D(214,14)*S;
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
 const bool MiniShown=Map && Map->MapPlayer() && Map->ShouldDrawMinimap();
 const float Reveal=MinimapReveal.Sample(!bFullMap && MiniShown,FPlatformTime::Seconds(),.06f);
 if(!Map || !Map->MapPlayer() || (!bFullMap && !MiniShown))return Base;
 if(!bFullMap)
 {
  const float S=FMath::Clamp(float(FMath::Min(G.GetLocalSize().X/1280,G.GetLocalSize().Y/720)),.55f,1.8f);
  const float K=.86f+.14f*Reveal;
  const FGeometry Mini=G.MakeChild(G.GetLocalSize(),FSlateLayoutTransform(),FSlateRenderTransform(FScale2D(K),FVector2D(84,84)*S*(1-Reveal)),FVector2D(1));
  const int32 Top=PaintCircularMinimap(Mini,E,Base);PaintWorldPing(G,E,Top+1);return Top+10;
 }
 const auto* Player=Map->MapPlayer();const FVector2D V=G.GetLocalSize();const float S=FMath::Clamp(float(V.Y/720.f),.55f,1.8f);PaintScale=S;
 const FLinearColor Paper=BBSBPaper::Ink(),Mint(.16f,.39f,.31f),Grey=BBSBPaper::Muted(),Gold(.65f,.37f,.09f),Dark=BBSBPaper::Paper();
 const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 auto Box=[&](FVector2D P,FVector2D Size,FLinearColor C,int32 Z=0){FSlateDrawElement::MakeBox(E,Base+Z,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),White,ESlateDrawEffect::None,C);};
 auto Text=[&](const FString& T,FVector2D P,float Size,FLinearColor C,int32 Z=5){FSlateDrawElement::MakeText(E,Base+Z,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(P)),T,FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(8,FMath::RoundToInt(Size*S))),ESlateDrawEffect::None,C);};
 auto Lines=[&](const TArray<FVector2D>& P,FLinearColor C,float Width=1,int32 Z=4){FSlateDrawElement::MakeLines(E,Base+Z,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,Width*S);};
 FVector2D UVMin=FVector2D::ZeroVector,UVMax=FVector2D(1);
 if(bFullMap)
 {
  Box(FVector2D::ZeroVector,V,FLinearColor(.015f,.02f,.03f,.90f));
  const float Side=FMath::Min(float(V.Y-202*S),float(V.X-330*S));MapSize=FVector2D(FMath::Max(150.f*S,Side));
  MapAt=FVector2D((V.X-MapSize.X-260*S)*.5f,96*S);ListAt=MapAt+FVector2D(MapSize.X+24*S,0);ToolbarAt=MapAt+FVector2D(0,MapSize.Y+34*S);
  CloseAt=FVector2D(ListAt.X+175*S,MapAt.Y-52*S);CloseSize=FVector2D(36*S);
  BBSBPaper::SlatePanel(G,E,Base,MapAt-FVector2D(20,80)*S,FVector2D(MapSize.X+284*S,MapSize.Y+152*S),S);
  Box(MapAt,MapSize,BBSBPaper::Cell(),2);
  Text(TEXT("지도"),MapAt-FVector2D(0,65*S),27,Paper);
  Text(TEXT("휠 확대·축소 · 드래그 이동 · M / Esc 닫기"),MapAt+FVector2D(76,-48)*S,11,Grey);
  Box(CloseAt,CloseSize,BBSBPaper::Hover(),3);Text(TEXT("×"),CloseAt+FVector2D(10,4)*S,22,Paper);
  Text(TEXT("우클릭: 표식 등록/삭제 · 휠클릭: 핑"),MapAt-FVector2D(0,21*S),10,Grey);
  Text(TEXT("리스폰 표식"),ListAt,19,Paper);
  int32 Row=0;
  for(ABBSBRespawnPoint* Point:Map->Points)if(IsValid(Point))
  {
   const FVector2D At=ListAt+FVector2D(0,(36+Row++*48)*S);const bool Available=Map->IsPointAvailable(Point);
   const bool Hover=InMapRect(Pointer,At,FVector2D(220,42)*S);
   Box(At,FVector2D(220,42)*S,Hover && Available ? BBSBPaper::Hover():BBSBPaper::Cell(),3);
   Text(Available ? Point->PointName.ToString():TEXT("미등록 표식"),At+FVector2D(12,5)*S,14,Available ? Mint:Grey);
   Text(Available ? TEXT("클릭하여 버니아로 이동"):TEXT("먼저 표식에 상호작용하세요"),At+FVector2D(12,24)*S,9,Grey);
  }
  Text(TEXT("▲ 현재 위치    ◆ 등록한 표식    ◇ 미등록"),MapAt+FVector2D(0,MapSize.Y+12*S),10,Paper);
  auto Button=[&](FVector2D At,FVector2D Size,const FString& Label,bool Enabled)
  { const bool Hover=InMapRect(Pointer,At,Size);Box(At,Size,Enabled && Hover ? Mint:Grey,3);Box(At+FVector2D(S),Size-FVector2D(2*S),Dark,4);Text(Label,At+FVector2D(8,5)*S,11,Enabled ? Paper:Grey,5); };
  Button(ToolbarAt,FVector2D(32,28)*S,TEXT("−"),Zoom>1.001f);Button(ToolbarAt+FVector2D(38,0)*S,FVector2D(32,28)*S,TEXT("+"),Zoom<31.999f);
  Button(ToolbarAt+FVector2D(76,0)*S,FVector2D(86,28)*S,TEXT("전체 보기"),true);Button(ToolbarAt+FVector2D(169,0)*S,FVector2D(90,28)*S,TEXT("내 위치"),true);
  Text(FString::Printf(TEXT("가로 %.0f m"),Map->MapHalfExtent*2/(Zoom*100)),ToolbarAt+FVector2D(274,5)*S,12,Mint);
  UVMin=ViewMinimum();UVMax=UVMin+FVector2D(1.f/Zoom);
 }
 // Border strips leave transparent terrain genuinely transparent over the map backdrop.
 Box(MapAt-FVector2D(3*S),FVector2D(MapSize.X+6*S,3*S),Paper,1);
 Box(MapAt+FVector2D(-3*S,MapSize.Y),FVector2D(MapSize.X+6*S,3*S),Paper,1);
 Box(MapAt-FVector2D(3*S,0),FVector2D(3*S,MapSize.Y),Paper,1);
 Box(MapAt+FVector2D(MapSize.X,0),FVector2D(3*S,MapSize.Y),Paper,1);
 E.PushClip(FSlateClippingZone(G.ToPaintGeometry(MapSize,FSlateLayoutTransform(MapAt))));
 const FVector2D VisibleMin(FMath::Max(0.,UVMin.X),FMath::Max(0.,UVMin.Y)),VisibleMax(FMath::Min(1.,UVMax.X),FMath::Min(1.,UVMax.Y));
 if(Map->DisplayTexture() && VisibleMax.X>VisibleMin.X && VisibleMax.Y>VisibleMin.Y)
 {
  const FVector2D ImageAt=MapAt+(VisibleMin-UVMin)/(UVMax-UVMin)*MapSize,ImageSize=(VisibleMax-VisibleMin)/(UVMax-UVMin)*MapSize;
  FSlateBrush Brush;Brush.SetResourceObject(Map->DisplayTexture());Brush.ImageSize=ImageSize;Brush.SetUVRegion(FBox2f(FVector2f(VisibleMin),FVector2f(VisibleMax)));
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
 PaintAnnotations(G,E,Base+6);
 Text(TEXT("N"),MapAt+FVector2D(MapSize.X*.5f-5*S,5*S),12,Paper,8);E.PopClip();
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
 if(!bFullMap || !Map)return FReply::Unhandled();
 Pointer=G.AbsoluteToLocal(Event.GetScreenSpacePosition());
 const auto Button=Event.GetEffectingButton();
 if(Button==EKeys::RightMouseButton || Button==EKeys::MiddleMouseButton)
 {
  if(InMapRect(Pointer,MapAt,MapSize))
  {
   const FVector2D UV=ViewMinimum()+(Pointer-MapAt)/MapSize/Zoom;
   bool Removed=false;
   if(Button==EKeys::RightMouseButton)for(const auto& M:Map->UserMarkers)if(M.Level==Map->AnnotationLevel() && FVector2D::Distance(Pointer,MapAt+(Map->WorldToMap(M.Position)-ViewMinimum())*Zoom*MapSize)<14*PaintScale)
   { const FGuid Id=M.Id;Removed=Map->RemoveMapMarker(Id);break; }
   if(!Removed) { if(Button==EKeys::RightMouseButton)Map->AddMapMarker(UV);else Map->PingMapLocation(UV); }
  }
  return FReply::Handled();
 }
 if(Button!=EKeys::LeftMouseButton)return FReply::Unhandled();
 if(InMapRect(Pointer,CloseAt,CloseSize)) { Map->CloseMap();return FReply::Handled(); }
 const float S=PaintScale;
 if(InMapRect(Pointer,ToolbarAt,FVector2D(32,28)*S)) { ZoomBy(.8f);return FReply::Handled(); }
 if(InMapRect(Pointer,ToolbarAt+FVector2D(38,0)*S,FVector2D(32,28)*S)) { ZoomBy(1.25f);return FReply::Handled(); }
 if(InMapRect(Pointer,ToolbarAt+FVector2D(76,0)*S,FVector2D(86,28)*S)) { ResetView();return FReply::Handled(); }
 if(InMapRect(Pointer,ToolbarAt+FVector2D(169,0)*S,FVector2D(90,28)*S)) { CenterOnPlayer();return FReply::Handled(); }
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
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
 TerrainMaps.Add(TEXT("CombatFieldTest"),TSoftObjectPtr<UBBSBTerrainMapData>(FSoftObjectPath(TEXT("/Game/BBSB/Maps/DA_TerrainMap_CombatFieldTest.DA_TerrainMap_CombatFieldTest"))));
 TerrainMaps.Add(TEXT("Stage01_BunniarHalfStepCaravan"),TSoftObjectPtr<UBBSBTerrainMapData>(FSoftObjectPath(TEXT("/Game/BBSB/Maps/DA_TerrainMap_Stage01_BunniarHalfStepCaravan.DA_TerrainMap_Stage01_BunniarHalfStepCaravan"))));
}
void UBBSBWorldMapComponent::BeginPlay() { Super::BeginPlay();Player=Cast<ABBSBCharacter>(GetOwner()); }
FVector2D UBBSBWorldMapComponent::WorldToMap(FVector P) const
{ const float Width=FMath::Max(1.f,MapHalfExtent)*2;return FVector2D(.5+(P.Y-MapCenter.Y)/Width,.5-(P.X-MapCenter.X)/Width); }
FVector UBBSBWorldMapComponent::MapToWorld(FVector2D UV) const
{ return FVector(MapCenter.X+(.5-UV.Y)*MapHalfExtent*2,MapCenter.Y+(UV.X-.5)*MapHalfExtent*2,0); }
bool UBBSBWorldMapComponent::IsPointAvailable(ABBSBRespawnPoint* Point) const
{ return Player && Player->Respawn && IsValid(Point) && Point->bTravelAvailable && Player->Respawn->IsPointDiscovered(Point); }
FVector2D UBBSBWorldMapComponent::MapMarkerPosition(ABBSBRespawnPoint* Point) const
{ return FullWidget ? FullWidget->MarkerPosition(Point):FVector2D::ZeroVector; }
void UBBSBWorldMapComponent::RefreshPoints()
{
 Points.Reset();for(TActorIterator<ABBSBRespawnPoint> It(GetWorld());It;++It)Points.Add(*It);
 Points.Sort([](const ABBSBRespawnPoint& A,const ABBSBRespawnPoint& B){return A.TravelId()<B.TravelId();});
}
UTexture* UBBSBWorldMapComponent::DisplayTexture() const
{ return TerrainTexture ? static_cast<UTexture*>(TerrainTexture.Get()):static_cast<UTexture*>(MapTexture.Get()); }
void UBBSBWorldMapComponent::SetTerrainMapEnabled(bool Enabled)
{
 if(bUseTerrainMap==Enabled)return;bUseTerrainMap=Enabled;
 if(Capture)Capture->DestroyComponent();Capture=nullptr;MapTexture=nullptr;TerrainTexture=nullptr;AppliedTerrainMap=nullptr;bMapConfigured=false;
 if(bOriginalBoundsSaved) { MapCenter=OriginalMapCenter;MapHalfExtent=OriginalMapHalfExtent;MinimapHalfExtent=OriginalMinimapHalfExtent; }
}
void UBBSBWorldMapComponent::EnsurePresentation()
{
 if(!Player)return;
 if(!bOriginalBoundsSaved) { bOriginalBoundsSaved=true;OriginalMapCenter=MapCenter;OriginalMapHalfExtent=MapHalfExtent;OriginalMinimapHalfExtent=MinimapHalfExtent; }
 if(!bMapConfigured)
 {
  bMapConfigured=true;
  const FString LevelName=UGameplayStatics::GetCurrentLevelName(this,true);
  // A conventionally named bake makes new stages usable without editing the shared player Blueprint.
  const FString AssetName=TEXT("DA_TerrainMap_")+LevelName;
  const TSoftObjectPtr<UBBSBTerrainMapData> StageProfile(FSoftObjectPath(TEXT("/Game/BBSB/Maps/")+AssetName+TEXT(".")+AssetName));
  if(bUseTerrainMap)
   if(auto* Data=TerrainMaps.Contains(FName(*LevelName)) ? TerrainMaps.FindChecked(FName(*LevelName)).LoadSynchronous():StageProfile.LoadSynchronous();Data && Data->bEnabled && Data->Image && Data->WorldHalfExtent>=500 &&
    Data->SourceLevel.ToSoftObjectPath().GetLongPackageName()==UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()))
   { AppliedTerrainMap=Data;TerrainTexture=Data->Image;MapCenter=Data->WorldCenter;MapHalfExtent=Data->WorldHalfExtent;MinimapHalfExtent=Data->MinimapHalfExtent; }
 }
 if(!TerrainTexture && !MapTexture)
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
{ return Player && !bMapOpen && !Player->Hunting->bInventoryOpen && !Player->Interaction->bDialogueActive && !Player->Respawn->IsSequenceActive() && !Player->EquipmentGrowth->bConcertoActive && !Player->Combat->bDead; }
void UBBSBWorldMapComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DT,Type,Function);if(!Player || !Player->IsLocallyControlled())return;EnsurePresentation();InitializeMapMarkers();
 if(bMarkersDirty && Player->Hunting && Player->Hunting->bPersistProgress && GetWorld()->GetRealTimeSeconds()>=MarkerSaveAt)SaveMapMarkers();
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
 FullWidget->SetVisibility(ESlateVisibility::Visible);if(!FullWidget->IsInViewport())FullWidget->AddToViewport(1200);FullWidget->OpenPaper(EBBSBPaperEntrance::Center);bMapOpen=true;PC->bShowMouseCursor=true;
 PC->SetInputMode(FInputModeUIOnly().SetWidgetToFocus(FullWidget->TakeWidget()).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));FullWidget->CenterOnPlayer();FullWidget->SetKeyboardFocus();return true;
}
void UBBSBWorldMapComponent::CloseMap()
{
 if(!bMapOpen && !bOwnInput)return;bMapOpen=false;if(FullWidget) { FullWidget->CancelPointerDrag();FullWidget->ClosePaper(); }
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
{ SaveMapMarkers();CloseMap();if(FullWidget)FullWidget->RemoveFromParent();if(MiniWidget)MiniWidget->RemoveFromParent();if(Capture)Capture->DestroyComponent();Super::EndPlay(Reason); }
