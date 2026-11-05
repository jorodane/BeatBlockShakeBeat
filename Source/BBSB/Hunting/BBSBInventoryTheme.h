#pragma once
#include "Hunting/BBSBItem.h"
#include "Hunting/BBSBItemIcon.h"
#include "Hunting/BBSBGameIcons.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/Texture2D.h"
#include "Brushes/SlateRoundedBoxBrush.h"

struct FBBSBInventoryDraw
{
 const FGeometry& G;FSlateWindowElementList& E;int32 Layer;float S;
 static FLinearColor Paper() { return FLinearColor(.94f,.92f,.90f); }
 static FLinearColor Ground() { return FLinearColor(.095f,.077f,.085f,.98f); }
 static FLinearColor CellGround() { return FLinearColor(.052f,.043f,.05f,.96f); }
 static FLinearColor Faint() { return FLinearColor(.65f,.60f,.61f); }
 static FLinearColor Accent() { return FLinearColor(.97f,.76f,.80f); }
 void Box(FVector2D P,FVector2D Size,FLinearColor C,int32 Z=0) const
 { FSlateDrawElement::MakeBox(E,Layer+Z,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),ESlateDrawEffect::None,C); }
 void Text(const FString& T,FVector2D P,float Size,FLinearColor C,int32 Z=4) const
 { FSlateDrawElement::MakeText(E,Layer+Z,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(P)),T,FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),FMath::Max(7,FMath::RoundToInt(Size*S))),ESlateDrawEffect::None,C); }
 FVector2D Measure(const FString& T,float Size) const
 { return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T,FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),FMath::Max(7,FMath::RoundToInt(Size*S)))); }
 void Lines(const TArray<FVector2D>& P,FLinearColor C,float Width=1,int32 Z=5) const
 { FSlateDrawElement::MakeLines(E,Layer+Z,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,Width*S); }
 void Gradient(FVector2D P,FVector2D Size,FLinearColor A,FLinearColor B,bool Horizontal,int32 Z) const
 {
  TArray<FSlateGradientStop> Stops;Stops.Emplace(FVector2f::ZeroVector,A);Stops.Emplace(FVector2f(Horizontal ? Size.X:0,Horizontal ? 0:Size.Y),B);
  // Slate names the orientation of the stop lines, rather than the colour axis.
  FSlateDrawElement::MakeGradient(E,Layer+Z,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),Stops,Horizontal ? Orient_Vertical:Orient_Horizontal);
 }
 void Panel(FVector2D P,FVector2D Size) const { Box(P-FVector2D(S),Size+FVector2D(2*S),Paper(),1);Box(P,Size,Ground(),2); }
 void Cell(FVector2D P,FVector2D Size,FLinearColor Rarity,bool Filled,bool Active,bool Valid=true) const
 {
  Box(P,Size,Active ? (Valid ? Paper():FLinearColor(1,.35f,.43f)):FLinearColor(.56f,.51f,.53f),3);
  const float In=Active ? 2.5f*S:S;Box(P+FVector2D(In),Size-FVector2D(In*2),CellGround(),4);
  if(Filled)
  {
   const FVector2D At=P+FVector2D(In),Area=Size-FVector2D(2*In);const float W=FMath::Min(Area.X,Area.Y)*.10f;
   Rarity.A=.38f;FLinearColor Fade=Rarity;Fade.A=0;
   Gradient(At,FVector2D(W,Area.Y),Rarity,Fade,true,5);Gradient(At+FVector2D(Area.X-W,0),FVector2D(W,Area.Y),Fade,Rarity,true,5);
   Gradient(At,FVector2D(Area.X,W),Rarity,Fade,false,5);Gradient(At+FVector2D(0,Area.Y-W),FVector2D(Area.X,W),Fade,Rarity,false,5);
  }
 }
 void OrbSlot(FVector2D Center,float Radius,FLinearColor Rarity,bool Filled,bool Active,int32 Z=5) const
 {
  const FLinearColor Outline=Active ? Paper():Filled ? Rarity:Faint();
  const FSlateRoundedBoxBrush Brush(CellGround(),Outline,(Active ? 2.f:1.f)*S);
  FSlateDrawElement::MakeBox(E,Layer+Z,G.ToPaintGeometry(FVector2D(2*Radius),FSlateLayoutTransform(Center-FVector2D(Radius))),&Brush,ESlateDrawEffect::None,CellGround());
 }
 static FLinearColor OrbColor(const UBBSBItemDefinition* Orb)
 { auto C=Orb ? Orb->Color:Faint();C.A=1;return C; }
 void GameIcon(EBBSBGameIcon Kind,FVector2D Center,float Radius,FLinearColor Color,int32 Z=8) const
 { BBSBGameIcons::Draw(E,G,Layer+Z,Kind,Center,Radius,S,Color); }
 void WeaponSocketDots(const FBBSBInventoryEntry& Item,FVector2D P,FVector2D Size,float Alpha=1) const
 {
  if(!Item.Item || Item.Item->EquipmentKind!=EBBSBItemEquipmentKind::Weapon || Item.SocketCount<=0)return;
  const int32 Count=Item.SocketCount;int32 Visible=FMath::Min(Count,6);FString Overflow;float ExtraWidth=0,Width=0;
  do
  {
   Overflow=Count>Visible ? FString::Printf(TEXT("+%d"),Count-Visible):FString();ExtraWidth=Overflow.IsEmpty() ? 0:Measure(Overflow,7).X+3*S;
   Width=(Visible*7-2)*S+ExtraWidth;if(Width<=Size.X-10*S || Visible==1)break;--Visible;
  }while(true);
  const float Start=P.X+Size.X-5*S-Width,Y=P.Y+Size.Y-6*S;
  for(int32 I=0;I<Visible;++I)
  {
   const auto* Orb=Item.Orbs.IsValidIndex(I) ? Item.Orbs[I].Item.Get():nullptr;auto Fill=Orb ? OrbColor(Orb):CellGround();auto Edge=Orb ? Paper():Faint();Fill.A=Edge.A=Alpha;
   const FSlateRoundedBoxBrush Brush(Fill,Edge,.55f*S);const FVector2D Center(Start+(2.5f+I*7)*S,Y);
   FSlateDrawElement::MakeBox(E,Layer+12,G.ToPaintGeometry(FVector2D(5*S),FSlateLayoutTransform(Center-FVector2D(2.5f*S))),&Brush,ESlateDrawEffect::None,Fill);
  }
  if(!Overflow.IsEmpty()) { auto C=Paper();C.A=Alpha;Text(Overflow,FVector2D(Start+(Visible*7-2)*S+3*S,Y-4*S),7,C,13); }
 }
 void Icon(UBBSBItemDefinition* Item,FName Glyph,FVector2D P,float R,float Alpha=1,int32 Z=7) const
 { BBSBItemIcon::DrawSlate(E,G,Layer+Z,Item,Glyph,P,R,Alpha,S,Paper()); }
 void ModuleIcon(const UBBSBModuleDefinition* M,FVector2D C,float R,float Alpha=1,int32 Z=7) const
 {
  if(!M)return;
  if(M->Icon) { FSlateBrush Brush;Brush.SetResourceObject(M->Icon);Brush.ImageSize=FVector2D(R*2);FSlateDrawElement::MakeBox(E,Layer+Z,G.ToPaintGeometry(FVector2D(R*2),FSlateLayoutTransform(C-FVector2D(R))),&Brush,ESlateDrawEffect::None,FLinearColor(1,1,1,Alpha));return; }
  auto Color=Paper();Color.A=Alpha;
  if(M->Action==EBBSBModuleAction::Glide)
  { Lines({C+FVector2D(-R,R*.2f),C+FVector2D(-R*.65f,-R*.65f),C,C+FVector2D(R*.65f,-R*.65f),C+FVector2D(R,R*.2f)},Color,2,Z);Lines({C+FVector2D(-R*.55f,R*.05f),C+FVector2D(0,R*.65f),C+FVector2D(R*.55f,R*.05f)},Color,2,Z); }
  else
  { TArray<FVector2D> Loop;for(int32 I=0;I<=24;++I)Loop.Add(C+FVector2D(FMath::Cos(I*PI/12),FMath::Sin(I*PI/12))*R*.6f);Lines(Loop,Color,2,Z);Lines({C+FVector2D(-R,R),C+FVector2D(R,-R),C+FVector2D(R*.35f,-R),C+FVector2D(R,-R),C+FVector2D(R,-R*.35f)},Color,2,Z); }
 }
};
