#include "Interaction/BBSBInteraction.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "Field/BBSBFieldActions.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Texture2D.h"

void UBBSBInteractionComponent::UpdateBunniarPortraitAttachment(float DT,bool bImmediate)
{
 const auto* Buddy=Player && Player->FieldActions ? Cast<ABBSBBunniarCompanion>(Player->FieldActions->BunniarEquipment):nullptr;
 UPrimitiveComponent* Surface=Buddy ? Buddy->SurfaceComponent.Get():nullptr;
 const bool Attached=Buddy && Buddy->bSurfaceAttached && IsValid(Surface) && IsValid(ActiveNPC) && Surface->GetOwner()==ActiveNPC;
 if(Attached && (bImmediate || !bBunniarPortraitAttached || BunniarPortraitSurface.Get()!=Surface))
 {
  // Store the real contact in the head's frame once. Head reactions, idle
  // animation and a different conversation camera must not slide the UI pin.
  const FTransform Head=ActiveNPC->HeadTarget->GetComponentTransform();
  BunniarPortraitLocalPoint=Head.InverseTransformPosition(Buddy->SurfacePoint);
  BunniarPortraitLocalNormal=Head.InverseTransformVectorNoScale(Buddy->SurfaceNormal).GetSafeNormal();
 }
 if(bImmediate && !Attached) { BunniarPortraitLocalPoint=FVector::ZeroVector;BunniarPortraitLocalNormal=FVector::UpVector; }
 const float Facing=Attached && Surface==ActiveNPC->FaceTarget ? -1.f:1.f;
 if(bImmediate)
 {
  BunniarPortraitFacing=BunniarFlipFrom=BunniarFlipTo=Facing;BunniarFlipElapsed=FMath::Max(.03f,BunniarPortraitFlipSeconds);
 }
 else
 {
  if(Facing!=BunniarFlipTo) { BunniarFlipFrom=BunniarPortraitFacing;BunniarFlipTo=Facing;BunniarFlipElapsed=0; }
  else BunniarFlipElapsed+=FMath::Max(0.f,DT);
  const float Turn=FMath::Clamp(BunniarFlipElapsed/FMath::Max(.03f,BunniarPortraitFlipSeconds),0.f,1.f);
  const float Ease=.5f-.5f*FMath::Cos(Turn*PI);
  BunniarPortraitFacing=Turn>=1.f ? BunniarFlipTo:FMath::Lerp(BunniarFlipFrom,BunniarFlipTo,Ease);
 }
 bBunniarPortraitAttached=Attached;BunniarPortraitSurface=Attached ? Surface:nullptr;
 const float Travel=Attached ? (Buddy->AnimationSet ? Buddy->AnimationSet->AttachTravelSeconds:.24f):(Buddy ? Buddy->ReturnTravelSeconds:.4f);
 BunniarPortraitAttachmentAlpha=bImmediate ? (Attached ? 1.f:0.f):FMath::FInterpConstantTo(BunniarPortraitAttachmentAlpha,Attached ? 1.f:0.f,FMath::Max(0.f,DT),1.f/FMath::Max(.05f,Travel));
}

FVector2D UBBSBInteractionComponent::BunniarAttachmentPortraitUV() const
{
 if(!IsValid(ActiveNPC))return FVector2D(.5,.23);
 const UTexture2D* Portrait=PortraitForSpeaker(EBBSBDialogueSpeaker::Partner);
 const float Aspect=Portrait ? float(Portrait->GetSizeX())/FMath::Max(1,Portrait->GetSizeY()):.75f;
 const float Yaw=FMath::DegreesToRadians(ActiveNPC->PortraitViewYaw);
 const FVector ScreenRight(FMath::Sin(Yaw),-FMath::Cos(Yaw),0);
 const FVector Contact=BunniarPortraitLocalPoint/FMath::Max(1.f,ActiveNPC->HeadAttachmentHeight);
 // A small outward offset represents Bunniar's own round body resting on the
 // contact surface rather than burying its centre inside the illustrated head.
 const FVector Offset=Contact*ActiveNPC->PortraitHeadRadius+BunniarPortraitLocalNormal*.065f;
 FVector2D UV=ActiveNPC->PortraitHeadAnchor+FVector2D(FVector::DotProduct(Offset,ScreenRight)/FMath::Max(.1f,Aspect),-Offset.Z);
 UV.X=1.f-UV.X;return UV;
}
