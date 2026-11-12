#pragma once
#include "CoreMinimal.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

/** Shared with dialogue: warm paper, angular ink border and a hard offset shadow. */
namespace BBSBPaper
{
 inline FLinearColor Paper(){return FLinearColor(.96f,.92f,.83f,1);}
 inline FLinearColor Ink(){return FLinearColor(.075f,.048f,.065f,1);}
 inline FLinearColor Shadow(){return FLinearColor(.028f,.019f,.027f,.92f);}
 inline FLinearColor Cell(){return FLinearColor(.87f,.82f,.73f,1);}
 inline FLinearColor Muted(){return FLinearColor(.36f,.29f,.31f,1);}
 inline FLinearColor Rose(){return FLinearColor(.48f,.22f,.31f,1);}
 inline FLinearColor Hover(){return FLinearColor(.86f,.73f,.70f,1);}
 inline TArray<FVector2D> Shape(FVector2D P,FVector2D Size,float S)
 {
  const float Cut=FMath::Min(8.f*S,float(FMath::Min(Size.X,Size.Y))*.18f);
  return {P,P+FVector2D(Size.X-Cut,0),P+FVector2D(Size.X,Cut),P+Size,P+FVector2D(Cut,Size.Y),P+FVector2D(0,Size.Y-Cut)};
 }
 inline void SlatePolygon(const FGeometry& G,FSlateWindowElementList& E,int32 Layer,const TArray<FVector2D>& P,FLinearColor Color)
 {
  const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));TArray<FSlateVertex> V;TArray<SlateIndex> I;
  for(const auto& Point:P)V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(Point),FVector2f(.5,.5),Color.ToFColorSRGB()));
  for(int32 N=1;N+1<P.Num();++N)I.Append({0,SlateIndex(N),SlateIndex(N+1)});
  FSlateDrawElement::MakeCustomVerts(E,Layer,FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White),V,I,nullptr,0,0);
 }
 inline void SlatePanel(const FGeometry& G,FSlateWindowElementList& E,int32 Layer,FVector2D P,FVector2D Size,float S)
 {
  SlatePolygon(G,E,Layer,Shape(P+FVector2D(7,8)*S,Size,S),Shadow());
  SlatePolygon(G,E,Layer+1,Shape(P-FVector2D(2*S),Size+FVector2D(4*S),S),Ink());
  SlatePolygon(G,E,Layer+2,Shape(P,Size,S),Paper());
 }
 inline void CanvasPolygon(UCanvas* Canvas,const TArray<FVector2D>& P,FLinearColor Color)
 {
  TArray<FCanvasUVTri> Tris;
  for(int32 N=1;N+1<P.Num();++N) { FCanvasUVTri T;T.V0_Pos=P[0];T.V1_Pos=P[N];T.V2_Pos=P[N+1];T.V0_UV=T.V1_UV=T.V2_UV=FVector2D::ZeroVector;T.V0_Color=T.V1_Color=T.V2_Color=Color;Tris.Add(T); }
  if(!Tris.IsEmpty()){FCanvasTriangleItem Item(Tris,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);}
 }
 inline void CanvasPanel(UCanvas* Canvas,FVector2D P,FVector2D Size,float S)
 {
  CanvasPolygon(Canvas,Shape(P+FVector2D(7,8)*S,Size,S),Shadow());
  CanvasPolygon(Canvas,Shape(P-FVector2D(2*S),Size+FVector2D(4*S),S),Ink());
  CanvasPolygon(Canvas,Shape(P,Size,S),Paper());
 }
}
