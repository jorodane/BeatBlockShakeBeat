#include "Field/BBSBFieldActions.h"
#include "Engine/World.h"

FVector2D UBBSBFieldActionComponent::GestureReleaseDirection(int32 Channel) const
{
 if (Channel<0 || Channel>1) return FVector2D::ZeroVector;
 const FVector2D Motion=Channels[Channel].ReleaseMotion;
 return FVector2D(Motion.X,-Motion.Y).GetSafeNormal();
}
float UBBSBFieldActionComponent::GestureHeldSeconds(int32 Channel) const
{
 if (!GetWorld() || Channel<0 || Channel>1) return 0;
 return FMath::Max(0.f,float(GetWorld()->GetTimeSeconds()-Channels[Channel].PressedAt));
}
