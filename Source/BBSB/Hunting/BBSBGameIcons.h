#pragma once
#include "CoreMinimal.h"
#include "Rendering/DrawElements.h"

enum class EBBSBGameIcon : uint8 { Attack,Cooldown,Beat,Hold,Flick,Dive,Shake,Level,Glide,Wire };

/** Shared outline symbols for attributes and gesture instructions. */
namespace BBSBGameIcons
{
inline void Draw(FSlateWindowElementList& Elements,const FGeometry& G,int32 Layer,EBBSBGameIcon Kind,FVector2D Center,float Radius,float Scale,FLinearColor Color)
{
 auto Stroke=[&](const TArray<FVector2D>& Shape,float Width=1.35f)
 {
  TArray<FVector2D> Points;Points.Reserve(Shape.Num());for(const auto& P:Shape)Points.Add(Center+P*Radius);
  FSlateDrawElement::MakeLines(Elements,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Width*Scale);
 };
 auto Circle=[&]() { TArray<FVector2D> Ring;for(int32 I=0;I<=32;++I)Ring.Add(FVector2D(FMath::Cos(I*PI/16),FMath::Sin(I*PI/16)));Stroke(Ring,1.1f); };
 if(Kind==EBBSBGameIcon::Attack)
 {
  Stroke({FVector2D(-.78,.86),FVector2D(.53,-.63),FVector2D(.87,-.94),FVector2D(.72,-.46),FVector2D(-.59,.99)});
  Stroke({FVector2D(-.79,.18),FVector2D(-.10,.81)},1.6f);return;
 }
 if(Kind==EBBSBGameIcon::Cooldown)
 {
  Stroke({FVector2D(-.68,-.88),FVector2D(.68,-.88)});Stroke({FVector2D(-.68,.88),FVector2D(.68,.88)});
  Stroke({FVector2D(-.5,-.84),FVector2D(-.43,-.46),FVector2D(.43,.46),FVector2D(.5,.84)});
  Stroke({FVector2D(.5,-.84),FVector2D(.43,-.46),FVector2D(-.43,.46),FVector2D(-.5,.84)});
  Stroke({FVector2D(-.31,-.58),FVector2D(.31,-.58)});Stroke({FVector2D(-.25,.64),FVector2D(.25,.64)});return;
 }
 if(Kind==EBBSBGameIcon::Level)
 { Stroke({FVector2D(-.7,.3),FVector2D(0,-.5),FVector2D(.7,.3)});Stroke({FVector2D(-.7,.85),FVector2D(0,.05),FVector2D(.7,.85)});return; }
 if(Kind==EBBSBGameIcon::Glide)
 { Stroke({FVector2D(-1,.3),FVector2D(-.65,-.6),FVector2D(0,.05),FVector2D(.65,-.6),FVector2D(1,.3)});Stroke({FVector2D(-.5,.2),FVector2D(0,.75),FVector2D(.5,.2)});return; }
 if(Kind==EBBSBGameIcon::Wire)
 { Stroke({FVector2D(-.8,.9),FVector2D(.65,-.55),FVector2D(.65,-.95),FVector2D(.13,-.95),FVector2D(.13,-.6)});return; }
 Circle();
 if(Kind==EBBSBGameIcon::Beat || Kind==EBBSBGameIcon::Hold)
 {
  Stroke({FVector2D(-.45,.26),FVector2D(-.45,.54),FVector2D(.45,.54),FVector2D(.45,.26)});
  if(Kind==EBBSBGameIcon::Beat)
  { Stroke({FVector2D(0,-.57),FVector2D(0,.08)});Stroke({FVector2D(-.23,-.14),FVector2D(0,.09),FVector2D(.23,-.14)}); }
  else { Stroke({FVector2D(-.18,-.5),FVector2D(-.18,-.02)});Stroke({FVector2D(.18,-.5),FVector2D(.18,-.02)}); }
 }
 else if(Kind==EBBSBGameIcon::Flick)
 { Stroke({FVector2D(-.5,.5),FVector2D(.5,-.5),FVector2D(.1,-.5)});Stroke({FVector2D(.5,-.5),FVector2D(.5,-.1)}); }
 else if(Kind==EBBSBGameIcon::Dive)
 { Stroke({FVector2D(0,-.6),FVector2D(0,.5)});Stroke({FVector2D(-.35,.15),FVector2D(0,.5),FVector2D(.35,.15)});Stroke({FVector2D(-.35,-.25),FVector2D(0,.1),FVector2D(.35,-.25)}); }
 else if(Kind==EBBSBGameIcon::Shake)
 { Stroke({FVector2D(-.6,.32),FVector2D(-.23,-.32),FVector2D(.23,.32),FVector2D(.6,-.32)}); }
}
}
