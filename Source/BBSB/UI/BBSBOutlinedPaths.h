#pragma once
#include "CoreMinimal.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"

/** Joined screen-space ribbons: one complete outline pass, then one complete fill pass. */
namespace BBSBOutlinedPaths
{
 struct FPath
 {
  TArray<FVector2D> Points;
  FLinearColor Color;
  float Width=2,Border=2;
  bool bClosed=false;
 };
 inline void Draw(UCanvas* Canvas,const TArray<FPath>& Paths,FLinearColor Outline)
 {
  if(!Canvas)return;
  for(int32 Pass=0;Pass<2;++Pass)
  {
   TArray<FCanvasUVTri> Tris;
   for(const FPath& Path:Paths)
   {
    const int32 N=Path.Points.Num();if(N<2)continue;
    const float Half=(Path.Width+(Pass==0 ? 2*Path.Border:0))*.5f;
    FLinearColor Color=Pass==0 ? Outline:Path.Color;if(Pass==0)Color.A*=Path.Color.A;
    TArray<FVector2D> Left,Right;Left.Reserve(N);Right.Reserve(N);
    for(int32 I=0;I<N;++I)
    {
     const FVector2D Incoming=(Path.Points[I]-Path.Points[I==0 ? (Path.bClosed ? N-1:0):I-1]).GetSafeNormal();
     const FVector2D Outgoing=(Path.Points[I==N-1 ? (Path.bClosed ? 0:N-1):I+1]-Path.Points[I]).GetSafeNormal();
     const FVector2D A=Incoming.IsNearlyZero() ? Outgoing:Incoming,B=Outgoing.IsNearlyZero() ? Incoming:Outgoing;
     const FVector2D NA(-A.Y,A.X),NB(-B.Y,B.X),Join=(NA+NB).GetSafeNormal();
     const FVector2D Offset=Join*FMath::Min(Half*3.f,Half/FMath::Max(.25f,float(FVector2D::DotProduct(Join,NB))));
     Left.Add(Path.Points[I]+Offset);Right.Add(Path.Points[I]-Offset);
    }
    auto Triangle=[&](FVector2D A,FVector2D B,FVector2D C)
    {
     FCanvasUVTri T;T.V0_Pos=A;T.V1_Pos=B;T.V2_Pos=C;T.V0_UV=T.V1_UV=T.V2_UV=FVector2D::ZeroVector;
     T.V0_Color=T.V1_Color=T.V2_Color=Color;Tris.Add(T);
    };
    for(int32 I=0;I<(Path.bClosed ? N:N-1);++I)
    { const int32 J=(I+1)%N;Triangle(Left[I],Right[I],Right[J]);Triangle(Left[I],Right[J],Left[J]); }
   }
   if(!Tris.IsEmpty()) { FCanvasTriangleItem Item(Tris,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item); }
  }
 }
}
