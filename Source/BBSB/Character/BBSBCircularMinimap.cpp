#include "Character/BBSBWorldMap.h"
#include "Character/BBSBMinimapMath.h"
#include "Character/BBSBRespawn.h"
#include "BBSBCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Texture.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"

float UBBSBWorldMapComponent::MinimapViewYaw() const
{
 if(Player)if(const auto* PC=Cast<APlayerController>(Player->GetController()))
  return PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Yaw:PC->GetControlRotation().Yaw;
 return Player ? Player->GetActorRotation().Yaw:0;
}
FVector2D UBBSBWorldMapWidget::WorldToMinimap(FVector Position) const
{
 if(!Map || !Map->MapPlayer())return FVector2D::ZeroVector;
 const FVector D=Position-Map->MapPlayer()->GetActorLocation();
 return MapAt+MapSize*.5+BBSBMinimap::Project(FVector2D(D.X,D.Y),PaintedMinimapYaw,float(MapSize.X*.5)/FMath::Max(1.f,Map->MinimapHalfExtent));
}
bool UBBSBWorldMapWidget::IsInsideMinimap(FVector Position,float EdgePadding) const
{ return FVector2D::DistSquared(WorldToMinimap(Position),MapAt+MapSize*.5)<=FMath::Square(FMath::Max(0.,MapSize.X*.5-EdgePadding*PaintScale)); }
FVector2D UBBSBWorldMapWidget::CompassPosition(FName Direction) const
{
 const FVector2D D=Direction==TEXT("N") ? FVector2D(1,0):Direction==TEXT("E") ? FVector2D(0,1):Direction==TEXT("S") ? FVector2D(-1,0):Direction==TEXT("W") ? FVector2D(0,-1):FVector2D::ZeroVector;
 return MapAt+MapSize*.5+BBSBMinimap::Project(D,PaintedMinimapYaw,float(MapSize.X*.5-20*PaintScale));
}

namespace
{
 struct FMiniVertex { FVector2D P,UV;float Alpha=1; };
 // UV clipping prevents clamped texture edges from smearing beyond the real world-map bounds.
 void ClipMiniPolygon(TArray<FMiniVertex>& P,int32 Axis,float Boundary,bool Greater)
 {
  TArray<FMiniVertex> Out;if(P.IsEmpty())return;FMiniVertex A=P.Last();
  auto Coord=[&](const FMiniVertex& V){return Axis==0 ? V.UV.X:V.UV.Y;};
  for(const FMiniVertex& B:P)
  {
   const double DA=Coord(A)-Boundary,DB=Coord(B)-Boundary;const bool IA=Greater ? DA>=0:DA<=0,IB=Greater ? DB>=0:DB<=0;
   if(IA!=IB) { const double T=DA/(DA-DB);Out.Add({FMath::Lerp(A.P,B.P,T),FMath::Lerp(A.UV,B.UV,T),FMath::Lerp(A.Alpha,B.Alpha,float(T))}); }
   if(IB)Out.Add(B);A=B;
  }
  P=MoveTemp(Out);
 }
}

int32 UBBSBWorldMapWidget::PaintCircularMinimap(const FGeometry& G,FSlateWindowElementList& E,int32 Layer) const
{
 const FVector2D Size=G.GetLocalSize();const float S=FMath::Clamp(float(FMath::Min(Size.X/1280,Size.Y/720)),.55f,1.8f),Radius=84*S;PaintScale=S;
 const float FrameReach=Radius+5.5f*S;
 const FVector2D Center=Size-FVector2D(FrameReach);MapAt=Center-FVector2D(Radius);MapSize=FVector2D(Radius*2);PaintedMinimapYaw=Map->MinimapViewYaw();
 const FLinearColor Paper(.97f,.93f,.86f),Ink(.025f,.022f,.035f,.9f),Mint(.48f,.91f,.76f);
 const auto Renderer=FSlateApplication::Get().GetRenderer();const auto Transform=G.GetAccumulatedRenderTransform();
 constexpr int32 Segments=128;
 auto Ring=[&](float R,FLinearColor Color,float Width,int32 Z)
 {
  TArray<FVector2D> P;P.Reserve(Segments+1);for(int32 I=0;I<=Segments;++I) { const double A=I*2*PI/Segments;P.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*R); }
  FSlateDrawElement::MakeLines(E,Layer+Z,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color,true,Width*S);
 };
 // A genuine disk, rather than a rectangular image covered by a round frame.
 {
  const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
  Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform,FVector2f(Center),FVector2f(.5,.5),FLinearColor(.025f,.022f,.035f,.62f).ToFColorSRGB()));
  for(int32 I=0;I<=Segments;++I) { const double A=I*2*PI/Segments;Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform,FVector2f(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*(Radius+3*S)),FVector2f(.5,.5),FLinearColor(.025f,.022f,.035f,.62f).ToFColorSRGB())); }
  for(int32 I=0;I<Segments;++I)Indices.Append({SlateIndex(0),SlateIndex(I+1),SlateIndex(I+2)});
  FSlateDrawElement::MakeCustomVerts(E,Layer+1,Renderer->GetResourceHandle(*White),Vertices,Indices,nullptr,0,0);
 }
 if(UTexture* Texture=Map->DisplayTexture())
 {
  FSlateBrush Brush;Brush.SetResourceObject(Texture);Brush.ImageSize=MapSize;
  TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;Vertices.Reserve(Segments*12);Indices.Reserve(Segments*18);
  const float Scale=Radius/FMath::Max(1.f,Map->MinimapHalfExtent);const FVector2D PlayerUV=Map->WorldToMap(Map->MapPlayer()->GetActorLocation());
  auto Vertex=[&](FVector2D Local,float Alpha)
  {
   const FVector2D D=BBSBMinimap::Unproject(Local,PaintedMinimapYaw,Scale);
   return FMiniVertex{Center+Local,PlayerUV+FVector2D(D.Y,-D.X)/(Map->MapHalfExtent*2),Alpha};
  };
  auto Triangle=[&](FMiniVertex A,FMiniVertex B,FMiniVertex C)
  {
   TArray<FMiniVertex> P={A,B,C};ClipMiniPolygon(P,0,0,true);ClipMiniPolygon(P,0,1,false);ClipMiniPolygon(P,1,0,true);ClipMiniPolygon(P,1,1,false);
   if(P.Num()<3)return;const int32 First=Vertices.Num();
   for(const auto& V:P)Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform,FVector2f(V.P),FVector2f(V.UV),FColor(255,255,255,uint8(FMath::Clamp(V.Alpha,0.f,1.f)*255))));
   for(int32 I=1;I+1<P.Num();++I)Indices.Append({SlateIndex(First),SlateIndex(First+I),SlateIndex(First+I+1)});
  };
  const float Inner=Radius-.85f*S;
  for(int32 I=0;I<Segments;++I)
  {
   const float A=I*2*PI/Segments,B=(I+1)*2*PI/Segments;const FVector2D U(FMath::Cos(A),FMath::Sin(A)),V(FMath::Cos(B),FMath::Sin(B));
   const auto IA=Vertex(U*Inner,1),IB=Vertex(V*Inner,1),OA=Vertex(U*Radius,0),OB=Vertex(V*Radius,0);
   Triangle(Vertex(FVector2D::ZeroVector,1),IA,IB);Triangle(IA,OA,OB);Triangle(IA,OB,IB);
  }
  if(!Indices.IsEmpty())FSlateDrawElement::MakeCustomVerts(E,Layer+2,Renderer->GetResourceHandle(Brush),Vertices,Indices,nullptr,0,0);
 }
 Ring(Radius+3*S,Ink,5,3);Ring(Radius+2*S,Paper,1.8f,4);
 auto Lines=[&](const TArray<FVector2D>& P,FLinearColor C,float Width,int32 Z){FSlateDrawElement::MakeLines(E,Layer+Z,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,Width*S);};
 for(ABBSBRespawnPoint* Point:Map->Points)if(IsValid(Point) && IsInsideMinimap(Point->GetActorLocation(),8))
 {
  const FVector2D P=WorldToMinimap(Point->GetActorLocation());const float R=5*S;
  const FLinearColor C=Map->MapPlayer()->Respawn->ActivePoint==Point ? FLinearColor(1,.78f,.37f):Map->IsPointAvailable(Point) ? Mint:FLinearColor(.55f,.56f,.61f);
  const TArray<FVector2D> D={P+FVector2D(0,-R),P+FVector2D(R,0),P+FVector2D(0,R),P+FVector2D(-R,0),P+FVector2D(0,-R)};Lines(D,Ink,4,5);Lines(D,C,2,6);
 }
 PaintAnnotations(G,E,Layer+7);
 const float R=7*S;const TArray<FVector2D> Arrow={Center+FVector2D(0,-R),Center+FVector2D(R*.65,R*.7),Center+FVector2D(0,R*.35),Center+FVector2D(-R*.65,R*.7),Center+FVector2D(0,-R)};
 Lines(Arrow,Ink,4,9);Lines(Arrow,Paper,2,10);
 PaintEquipmentRing(G,E,Layer);
 auto Label=[&](const FString& Text,FVector2D At,FLinearColor C,int32 FontSize)
 {
  auto Font=FCoreStyle::GetDefaultFontStyle("Bold",FMath::RoundToInt(FontSize*S));Font.OutlineSettings.OutlineSize=1;Font.OutlineSettings.OutlineColor=Ink;
  const FVector2D Extent=Renderer->GetFontMeasureService()->Measure(Text,Font);
  FSlateDrawElement::MakeText(E,Layer+11,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(At-Extent*.5)),Text,Font,ESlateDrawEffect::None,C);
 };
 for(const FName D:{FName(TEXT("N")),FName(TEXT("E")),FName(TEXT("S")),FName(TEXT("W"))})
 {
  const FVector2D Out=(CompassPosition(D)-Center).GetSafeNormal(),Side(-Out.Y,Out.X);const float Length=(D==TEXT("N") ? 10.f:5.f)*S;
  const FVector2D Tip=Center+Out*(Radius+2*S),Base=Tip-Out*Length;
  const FLinearColor Color=D==TEXT("N") ? Mint:Paper;
  TArray<FVector2D> CompassArrow={Base+Side*Length*.42f,Tip,Base-Side*Length*.42f,Base+Side*Length*.42f};
  Lines(CompassArrow,Ink,3.6f,22);Lines(CompassArrow,Color,1.8f,23);
  Label(D.ToString(),CompassPosition(D),Color,12);
 }
 return Layer+24;
}
