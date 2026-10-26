#include "Character/BBSBTraversal.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "Combat/BBSBCombat.h"
#include "Field/BBSBFieldActions.h"
#include "Interaction/BBSBInteraction.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Framework/Application/SlateApplication.h"

bool UBBSBTraversalComponent::TryWireRecall(ABBSBBunniarCompanion* Buddy)
{
 if(!Character || !IsValid(Buddy) || IsWireTraversalActive() || IsBunniarTraversalActive() || IsParkourActive() || !Character->FieldActions || Character->FieldActions->FieldEquipment!=Buddy) return false;
 const auto* PC=Cast<APlayerController>(Character->GetController());
 if(!PC || !PC->IsLocalController() || PC->IsMoveInputIgnored() || (Character->Combat && (!Character->Combat->CanAttack() || Character->Combat->bDead))) return false;
 auto* Surface=Buddy->SurfaceComponent.Get();
 if(!Buddy->bAirHovering && (!Buddy->bSurfaceAttached || !IsValid(Surface) || Surface->GetOwner()->Implements<UBBSBBunniarSurface>())) return false;
 const FVector Anchor=Buddy->bAirHovering ? Buddy->AirHoverPoint:Buddy->SurfacePoint;
 if(FVector::DistSquared(Character->GetActorLocation(),Anchor)>FMath::Square(Character->FieldActions->WeaponReach+100.f)) return false;
 // Reject a blocked line rather than allowing the pull to tunnel through another wall.
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BunniarWireStart),false,Character);Query.AddIgnoredActor(Buddy);FHitResult Hit;
 if(GetWorld()->LineTraceSingleByChannel(Hit,Character->GetActorLocation(),Anchor,ECC_Visibility,Query) && Hit.GetComponent()!=Surface) return false;
 auto* Move=Character->GetCharacterMovement();
 WireArrival=Buddy->bAirHovering ? EBBSBWireArrival::Air:Buddy->SurfaceNormal.Z>=Move->GetWalkableFloorZ() ? EBBSBWireArrival::Floor:EBBSBWireArrival::Wall;
 WireAnchor=Anchor;WireNormal=Buddy->SurfaceNormal.GetSafeNormal();WireBuddy=Buddy;WireSurface=Surface;
 if(Surface)
 {
  WireLocalAnchor=Surface->GetComponentTransform().InverseTransformPosition(Anchor);
  WireLocalNormal=Surface->GetComponentTransform().InverseTransformVectorNoScale(WireNormal);
 }
 WireSavedAirControl=Move->AirControl;WireSavedFlyingBraking=Move->BrakingDecelerationFlying;
 WireSavedMaxFlySpeed=Move->MaxFlySpeed;bWireSavedOrient=Move->bOrientRotationToMovement;
 EndAirWeaponHang();Character->StopJumping();Character->FieldActions->ResetActionState();Character->ConsumeMovementInputVector();
 Move->ClearAccumulatedForces();Move->SetMovementMode(MOVE_Flying);Move->GravityScale=0;Move->AirControl=0;
 Move->BrakingDecelerationFlying=0;Move->MaxFlySpeed=WireSpeed;Move->bOrientRotationToMovement=false;
 WireCurrentSpeed=FMath::Clamp(Move->Velocity.Size(),300.f,700.f);Move->Velocity=FVector::ZeroVector;
 WireStartedAt=GetWorld()->GetTimeSeconds();WireMotionAge=0;WirePreviousStep=0;WirePreviousPosition=Character->GetActorLocation();
 bWirePulling=true;bWireArrival=false;Buddy->bWireAnchored=true;
 Character->FieldActions->SetEquipmentPointerCapture(false);
 if(WireSegments.IsEmpty())
 {
  UStaticMesh* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
  for(int32 I=0;I<4;++I)
  {
   auto* Part=NewObject<USplineMeshComponent>(Character,MakeUniqueObjectName(Character,USplineMeshComponent::StaticClass(),TEXT("BunniarWire")));
   Part->SetupAttachment(Character->GetRootComponent());Part->SetAbsolute(true,true,true);
   Part->SetMobility(EComponentMobility::Movable);Part->SetStaticMesh(Cylinder);Part->SetForwardAxis(ESplineMeshAxis::Z,false);
   Part->SetStartScale(FVector2D(.009f));Part->SetEndScale(FVector2D(.009f));Part->SetSplineUpDir(FVector::ForwardVector,false);
   Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCastShadow(false);Part->bReceivesDecals=false;
   if(Presentation && Presentation->WireMaterial) Part->SetMaterial(0,Presentation->WireMaterial);
   Character->AddInstanceComponent(Part);Part->RegisterComponent();WireSegments.Add(Part);
  }
 }
 UpdateWirePresentation();return true;
}

void UBBSBTraversalComponent::RestoreWireMovement(bool bGrounded)
{
 if(!Character) return;
 auto* Move=Character->GetCharacterMovement();Move->ClearAccumulatedForces();Move->AirControl=WireSavedAirControl;
 Move->BrakingDecelerationFlying=WireSavedFlyingBraking;Move->MaxFlySpeed=WireSavedMaxFlySpeed;Move->bOrientRotationToMovement=bWireSavedOrient;
 Move->GravityScale=BaseGravityScale;Move->SetMovementMode(bGrounded ? MOVE_Walking:MOVE_Falling);
}

void UBBSBTraversalComponent::CancelWireRecall()
{
 if(!IsWireTraversalActive()) return;
 const bool WasPulling=bWirePulling;
 bWirePulling=bWireArrival=false;
 // Fade the flight pose on an aborted pull; contact poses belong to real arrivals.
 if(WasPulling) WireArrival=EBBSBWireArrival::Air;
 if(IsValid(WireBuddy)) WireBuddy->ReleaseSurface();
 RestoreWireMovement(false);
 if(Character)
 {
  Character->GetCharacterMovement()->Velocity=WasPulling ? WireDirection*FMath::Min(WireAirExitSpeed,WireCurrentSpeed):FVector::ZeroVector;
  Character->ConsumeMovementInputVector();
 }
 WireBuddy=nullptr;WireSurface.Reset();WirePreviousStep=0;UpdateWirePresentation();
}

void UBBSBTraversalComponent::ArriveFromWire()
{
 bWirePulling=false;WirePreviousStep=0;WireMotionAge=0;
 if(IsValid(WireBuddy)) WireBuddy->ReleaseSurface();
 WireBuddy=nullptr;UpdateWirePresentation();
 auto* Move=Character->GetCharacterMovement();Move->ClearAccumulatedForces();
 if(WireArrival==EBBSBWireArrival::Air)
 {
  RestoreWireMovement(false);Move->Velocity=WireDirection*FMath::Min(WireAirExitSpeed,WireCurrentSpeed);
  WireSurface.Reset();BeginGlide(false);
 }
 else
 {
  if(WireArrival==EBBSBWireArrival::Wall)
  {
   if(TryWireLedge()) return;
   auto* Surface=WireSurface.Get();RestoreWireMovement(false);
   const bool Holding=BeginWallSlide(WireAnchor,WireNormal,Surface);WireSurface.Reset();
   if(!Holding) Move->Velocity=WireNormal*70.f;
   return;
  }
  WireSurface.Reset();bWireArrival=true;WireArrivalAt=GetWorld()->GetTimeSeconds();Move->Velocity=FVector::ZeroVector;Move->GravityScale=0;
  if(WireArrival==EBBSBWireArrival::Wall && !WireNormal.GetSafeNormal2D().IsNearlyZero()) Character->SetActorRotation((-WireNormal.GetSafeNormal2D()).Rotation());
  if(Presentation)
  {
   UNiagaraSystem* FX=WireArrival==EBBSBWireArrival::Floor ? Presentation->WireLandingSmoke.Get():Presentation->WireWallSmoke.Get();
   SpawnSmoke(FX ? FX:Presentation->LandingSmoke.Get(),WireAnchor+WireNormal*4.f,WireNormal,WireArrival==EBBSBWireArrival::Floor ? 1.f:.65f);
  }
 }
}

void UBBSBTraversalComponent::TickWireMovement(float DT)
{
 if(!IsWireTraversalActive()) return;
 auto* PC=Cast<APlayerController>(Character->GetController());auto* Move=Character->GetCharacterMovement();
 const auto State=Character->FieldActions ? Character->FieldActions->ActiveState:EBBSBActionState::Idle;
 if(!PC || !PC->IsLocalController() || PC->IsMoveInputIgnored() || State==EBBSBActionState::Hit || State==EBBSBActionState::Dead || State==EBBSBActionState::Transition || (Character->Combat && (Character->Combat->bDead || Character->Combat->BlocksMovement())) ||
  (Character->Interaction && Character->Interaction->bDialogueActive) || (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive()))
 { CancelWireRecall();return; }
 Move->GravityScale=0;Character->ConsumeMovementInputVector();
 if(bWireArrival)
 {
  WireMotionAge=GetWorld()->GetTimeSeconds()-WireArrivalAt;
  const bool Wall=WireArrival==EBBSBWireArrival::Wall;
  Move->Velocity=Wall && WireMotionAge>=WireWallSlideStartsAt ? -FVector::UpVector*WireWallSlideSpeed:FVector::ZeroVector;
  if(WireMotionAge>=(Wall ? WireWallContactSeconds:WireArrivalSeconds))
  {
   const bool Floor=WireArrival==EBBSBWireArrival::Floor;bWireArrival=false;RestoreWireMovement(Floor);
   Move->Velocity=Floor ? FVector::ZeroVector:WireNormal*70.f;
   if(Floor) { bAirJumpUsed=false;LastFallSpeed=0;bWasFalling=false; }
  }
  return;
 }
 if(!IsValid(WireBuddy) || !WireBuddy->bWireAnchored || Character->FieldActions->FieldEquipment!=WireBuddy ||
  (WireArrival!=EBBSBWireArrival::Air && !WireSurface.IsValid())) { CancelWireRecall();return; }
 WireMotionAge=GetWorld()->GetTimeSeconds()-WireStartedAt;
 if(WireMotionAge>4.f) { CancelWireRecall();return; }
 if(WireSurface.IsValid())
 {
  const FTransform Frame=WireSurface->GetComponentTransform();WireAnchor=Frame.TransformPosition(WireLocalAnchor);WireNormal=Frame.TransformVectorNoScale(WireLocalNormal).GetSafeNormal();
 }
 const FVector Position=Character->GetActorLocation();
 const float Radius=Character->GetCapsuleComponent()->GetScaledCapsuleRadius(),HalfHeight=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 WireDestination=WireArrival==EBBSBWireArrival::Floor ? WireAnchor+FVector(0,0,HalfHeight+2.f):
  WireArrival==EBBSBWireArrival::Wall ? WireAnchor+WireNormal*(Radius+6.f):WireAnchor-(WireAnchor-Position).GetSafeNormal()*52.f;
 const FVector Delta=WireDestination-Position;const float Remaining=Delta.Size();
 if(Remaining<=8.f) { ArriveFromWire();return; }
 // CharacterMovement performs swept capsule movement. A blocked pull always aborts safely.
 if(WirePreviousStep>3.f && FVector::Distance(Position,WirePreviousPosition)<WirePreviousStep*.15f) { CancelWireRecall();return; }
 WireDirection=Delta/Remaining;WirePreviousPosition=Position;
 const float Exit=WireArrival==EBBSBWireArrival::Air ? WireAirExitSpeed:80.f;
 const float Desired=FMath::Min(WireSpeed,FMath::Sqrt(Exit*Exit+2.f*WireAcceleration*Remaining));
 WireCurrentSpeed=FMath::FInterpConstantTo(WireCurrentSpeed,Desired,DT,WireAcceleration);
 WirePreviousStep=FMath::Min(Remaining,WireCurrentSpeed*FMath::Max(DT,.0001f));Move->Velocity=WireDirection*(WirePreviousStep/FMath::Max(DT,.0001f));
 if(!WireDirection.GetSafeNormal2D().IsNearlyZero()) Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(),WireDirection.GetSafeNormal2D().Rotation(),DT,14.f));
 UpdateWirePresentation();
}

void UBBSBTraversalComponent::UpdateWirePresentation()
{
 if(!Character) return;
 const bool Visible=bWirePulling && IsValid(WireBuddy);
 const FVector Start=Character->GetMesh()->GetSocketLocation(TEXT("hand_l"));
 const FVector End=Visible ? WireBuddy->GetActorLocation()+FVector(0,0,10):Start;
 const FVector Line=End-Start;
 const float Sag=FMath::Min(18.f,Line.Size()*.035f)*(1.f-FMath::Clamp(WireCurrentSpeed/FMath::Max(1.f,WireSpeed),0.f,.9f));
 auto Point=[&](float T){ return Start+Line*T-FVector::UpVector*(FMath::Sin(PI*T)*Sag); };
 auto Tangent=[&](float T){ return (Line-FVector::UpVector*(PI*FMath::Cos(PI*T)*Sag))/4.f; };
 for(int32 I=0;I<WireSegments.Num();++I) if(auto* Part=WireSegments[I].Get())
 {
  Part->SetVisibility(Visible);if(!Visible) continue;
  Part->SetWorldTransform(FTransform::Identity);
  const float A=float(I)/4.f,B=float(I+1)/4.f;Part->SetStartAndEnd(Point(A),Tangent(A),Point(B),Tangent(B),true);
 }
}
