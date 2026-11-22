#include "Interaction/BBSBClimbable.h"

#include "BBSBCharacter.h"
#include "Character/BBSBTraversal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

UBBSBClimbable::UBBSBClimbable()
{
 InteractType = TEXT("Climb");
}

FBBSBClimbFrame UBBSBClimbable::WorldFrame() const
{
 FBBSBClimbFrame Frame = LocalFrame;
 if (const auto* Owner = GetOwner())
 {
  const auto Transform = Owner->GetActorTransform();
  Frame.Bottom = Transform.TransformPosition(LocalFrame.Bottom);
  Frame.Top = Transform.TransformPosition(LocalFrame.Top);
  Frame.Exit = Transform.TransformPosition(LocalFrame.Exit);
  Frame.Outward = Transform.TransformVectorNoScale(LocalFrame.Outward).GetSafeNormal2D();
 }
 if (Frame.Outward.IsNearlyZero()) Frame.Outward = (Frame.Top - Frame.Exit).GetSafeNormal2D();
 return Frame;
}

bool UBBSBClimbable::CanInteract_Implementation(ACharacter* Character) const
{
 return bClimbEnabled && bRouteReady;
}

bool UBBSBClimbable::TryMount(ABBSBCharacter* Character)
{
 if (!IsValid(Character) || !Character->Traversal || !CanStartInteraction(Character)) return false;
 const auto Frame = WorldFrame();
 auto* Traversal = Character->Traversal.Get();
 if (!Traversal->BeginLadder(GetOwner(), Frame.Bottom, Frame.Top, Frame.Exit, Frame.Outward)) return false;
 Traversal->TryLedgeClimb();
 return true;
}

UAnimSequence* UBBSBClimbable::AnimationFor(EBBSBClimbMotion Motion) const
{
 switch (Motion)
 {
 case EBBSBClimbMotion::Hold: return Animations.Hold;
 case EBBSBClimbMotion::Step: return Animations.Step;
 case EBBSBClimbMotion::Hop: return Animations.Hop;
 case EBBSBClimbMotion::Slide: return Animations.Slide;
 case EBBSBClimbMotion::Mantle: return Animations.Mantle;
 }
 return nullptr;
}

void UBBSBClimbable::NotifyClimbEnter(ABBSBCharacter* Character)
{
 if (Climber.IsValid()) return;
 Climber = Character;
 if (bPrimaryInteraction)
 {
  FBBSBInteractContext Context;
  Context.Character = Character;
  Context.Point = Character->GetActorLocation();
  Context.Normal = WorldFrame().Outward;
  Context.bHeld = true;
  BeginInteraction(Context);
 }
 OnClimbEnter.Broadcast(Character);
}

void UBBSBClimbable::NotifyClimbStay(ABBSBCharacter* Character, float Progress, EBBSBClimbMotion Motion)
{
 if (Climber.Get() != Character) return;
 if (IsInteracting()) ActiveContext.Point = Character->GetActorLocation();
 OnClimbStay.Broadcast(Character, Progress, Motion);
}

void UBBSBClimbable::NotifyClimbEnd(ABBSBCharacter* Character, bool bReachedTop)
{
 if (Climber.Get() != Character) return;
 Climber.Reset();
 OnClimbEnd.Broadcast(Character, bReachedTop);
 if (bPrimaryInteraction) EndInteraction();
}

void UBBSBClimbable::AddRungs(UInstancedStaticMeshComponent* Mesh, const FBBSBClimbFrame& Frame, float Spacing, float From, float To, FVector Layer)
{
 const FVector Axis = (Frame.Top - Frame.Bottom).GetSafeNormal();
 const FVector Side = FVector::CrossProduct(FVector::UpVector, -Frame.Outward).GetSafeNormal();
 if (!IsValid(Mesh) || Axis.IsNearlyZero() || Side.IsNearlyZero() || To <= From) return;
 Spacing = FMath::Clamp(Spacing, 20.f, 50.f);
 const FQuat Rotation = FRotationMatrix::MakeFromYZ(Side, Axis).ToQuat();
 const FVector GripOrigin = Frame.Bottom - Frame.Outward * 32.f + FVector::UpVector * 20.f + Layer;
 for (float Sign : {-1.f, 1.f})
 {
  const FVector Rail = GripOrigin + Axis * ((From + To) * .5f) + Side * (Sign * 26.f);
  Mesh->AddInstance(FTransform(Rotation, Rail, FVector(.08f, .08f, (To - From) / 100.f)), true);
 }
 for (float D = FMath::CeilToFloat(From / Spacing) * Spacing; D <= To; D += Spacing)
 {
  Mesh->AddInstance(FTransform(Rotation, GripOrigin + Axis * D, FVector(.10f, .60f, .08f)), true);
 }
}
