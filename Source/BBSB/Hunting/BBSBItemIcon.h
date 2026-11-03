#pragma once
#include "CoreMinimal.h"
class UBBSBItemDefinition;
class AHUD;
class UCanvas;
class FSlateWindowElementList;
struct FGeometry;
namespace BBSBItemIcon
{
 void DrawSlate(FSlateWindowElementList& Elements,const FGeometry& Geometry,int32 Layer,UBBSBItemDefinition* Item,FName Kind,FVector2D Center,float Radius,float Alpha,float Scale,FLinearColor Ink);
 void DrawCanvas(AHUD* HUD,UCanvas* Canvas,UBBSBItemDefinition* Item,FName Kind,FVector2D Center,float Radius,float Alpha,float Scale,FLinearColor Ink);
}
