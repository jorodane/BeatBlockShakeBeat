#pragma once
#include "CoreMinimal.h"

namespace BBSBMinimap
{
 // Camera yaw zero looks north (+world X); east (+world Y) is screen right.
 inline FVector2D Project(FVector2D Delta,float Yaw,float Scale)
 {
  const float A=FMath::DegreesToRadians(Yaw),C=FMath::Cos(A),S=FMath::Sin(A);
  return FVector2D(Delta.Y*C-Delta.X*S,-Delta.X*C-Delta.Y*S)*Scale;
 }
 inline FVector2D Unproject(FVector2D Screen,float Yaw,float Scale)
 {
  const float A=FMath::DegreesToRadians(Yaw),C=FMath::Cos(A),S=FMath::Sin(A);
  return FVector2D(-Screen.X*S-Screen.Y*C,Screen.X*C-Screen.Y*S)/FMath::Max(.000001f,Scale);
 }
}
