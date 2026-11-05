#include "Hunting/BBSBItemIcon.h"
#include "Hunting/BBSBItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "GameFramework/HUD.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "Layout/Geometry.h"

namespace
{
template<typename FDrawLines>
void Glyph(UBBSBItemDefinition* Item,FName Kind,FVector2D C,float R,float Alpha,FLinearColor Ink,FDrawLines&& Lines)
{
 const int32 Z=0;
  FLinearColor Color=Item ? FMath::Lerp(Ink,Item->Color,.28f):Ink;Color.A=Alpha;TArray<FVector2D> P;
  auto Circle=[&](FVector2D At,float Radius) { TArray<FVector2D> Q;for(int32 I=0;I<=24;++I)Q.Add(At+FVector2D(FMath::Cos(I*PI/12),FMath::Sin(I*PI/12))*Radius);Lines(Q,Color,1.6f,Z); };
  if(Kind==TEXT("Bunniar")) { Circle(C+FVector2D(0,R*.2f),R*.65f);Lines({C+FVector2D(-R*.55f,-R*.1f),C+FVector2D(-R*.8f,-R),C+FVector2D(-R*.2f,-R*.6f)},Color,1.8f,Z);Lines({C+FVector2D(R*.55f,-R*.1f),C+FVector2D(R*.8f,-R),C+FVector2D(R*.2f,-R*.6f)},Color,1.8f,Z); }
  else if(Kind==TEXT("Sword")) { Lines({C+FVector2D(-R*.75f,R*.85f),C+FVector2D(R*.65f,-R*.8f),C+FVector2D(R*.85f,-R*.95f),C+FVector2D(R*.8f,-R*.6f),C+FVector2D(-R*.75f,R*.85f)},Color,1.8f,Z);Lines({C+FVector2D(-R*.8f,R*.15f),C+FVector2D(-R*.05f,R*.8f)},Color,2,Z); }
  else if(Kind==TEXT("Armor"))P={C+FVector2D(-R*.9f,-R*.55f),C+FVector2D(-R*.4f,-R),C+FVector2D(0,-R*.7f),C+FVector2D(R*.4f,-R),C+FVector2D(R*.9f,-R*.55f),C+FVector2D(R*.55f,-R*.15f),C+FVector2D(R*.45f,R*.85f),C+FVector2D(-R*.45f,R*.85f),C+FVector2D(-R*.55f,-R*.15f),C+FVector2D(-R*.9f,-R*.55f)};
  else if(Kind==TEXT("Ring")) { Circle(C,R*.75f);Circle(C+FVector2D(0,-R*.7f),R*.25f); }
  else if(Kind==TEXT("Core")) { for(int32 I=0;I<=24;++I)P.Add(C+FVector2D(FMath::Cos(I*PI/12),FMath::Sin(I*PI/12))*R*(I%2 ? .78f:1.f));Circle(C,R*.35f); }
  else if(Kind==TEXT("Shard"))P={C+FVector2D(0,-R),C+FVector2D(R*.6f,0),C+FVector2D(0,R),C-FVector2D(R*.6f,0),C+FVector2D(0,-R)};
  else if(Kind==TEXT("Note")) { Circle(C+FVector2D(-R*.25f,R*.57f),R*.3f);Lines({C+FVector2D(R*.05f,R*.58f),C+FVector2D(R*.05f,-R*.85f),C+FVector2D(R*.7f,-R*.55f),C+FVector2D(R*.55f,-R*.1f)},Color,1.8f,Z); }
  else if(Item)P={C+FVector2D(0,-R),C+FVector2D(R*.65f,R*.15f),C+FVector2D(R*.5f,R*.7f),C+FVector2D(0,R*.9f),C+FVector2D(-R*.5f,R*.7f),C+FVector2D(-R*.65f,R*.15f),C+FVector2D(0,-R)};
  else { Lines({C-FVector2D(R*.35f,0),C+FVector2D(R*.35f,0)},Color,1,Z);Lines({C-FVector2D(0,R*.35f),C+FVector2D(0,R*.35f)},Color,1,Z); }
  if(!P.IsEmpty())Lines(P,Color,1.8f,Z);
}
}
void BBSBItemIcon::DrawSlate(FSlateWindowElementList& Elements,const FGeometry& G,int32 Layer,UBBSBItemDefinition* Item,FName Kind,FVector2D C,float R,float Alpha,float Scale,FLinearColor Ink)
{
 if(Item && Item->Icon)
 {
  FSlateBrush Brush;Brush.SetResourceObject(Item->Icon);Brush.ImageSize=FVector2D(2*R);
  FSlateDrawElement::MakeBox(Elements,Layer,G.ToPaintGeometry(FVector2D(2*R),FSlateLayoutTransform(C-FVector2D(R))),&Brush,ESlateDrawEffect::None,FLinearColor(1,1,1,Alpha));return;
 }
 Glyph(Item,Kind,C,R,Alpha,Ink,[&](const TArray<FVector2D>& Points,FLinearColor Color,float Width,int32)
 { FSlateDrawElement::MakeLines(Elements,Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Width*Scale); });
}
void BBSBItemIcon::DrawCanvas(AHUD* HUD,UCanvas* Canvas,UBBSBItemDefinition* Item,FName Kind,FVector2D C,float R,float Alpha,float Scale,FLinearColor Ink)
{
 if(!HUD || !Canvas)return;
 if(Item && Item->Icon)
 { Canvas->K2_DrawTexture(Item->Icon,C-FVector2D(R),FVector2D(2*R),FVector2D::ZeroVector,FVector2D::UnitVector,FLinearColor(1,1,1,Alpha));return; }
 Glyph(Item,Kind,C,R,Alpha,Ink,[&](const TArray<FVector2D>& Points,FLinearColor Color,float Width,int32)
 { for(int32 I=1;I<Points.Num();++I)HUD->DrawLine(Points[I-1].X,Points[I-1].Y,Points[I].X,Points[I].Y,Color,Width*Scale); });
}
