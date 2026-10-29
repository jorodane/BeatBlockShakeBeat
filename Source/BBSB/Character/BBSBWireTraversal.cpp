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
#include "NiagaraComponent.h"
#include "Framework/Application/SlateApplication.h"

bool UBBSBTraversalComponent::TryWireRecall(ABBSBBunniarCompanion* Buddy)
{
 if(!CanWireRecall(Buddy)) return false;
 return BeginWirePull(Buddy,false);
}

bool UBBSBTraversalComponent::BeginWirePull(ABBSBBunniarCompanion* Buddy,bool Sky)
{
 auto* Surface=Buddy->SurfaceComponent.Get();
 const FVector Anchor=Buddy->bAirHovering ? Buddy->AirHoverPoint:Buddy->SurfacePoint;
 bSkyWire=Sky;
 auto* Move=Character->GetCharacterMovement();
 WireArrival=Buddy->bAirHovering ? EBBSBWireArrival::Air:Buddy->SurfaceNormal.Z>=Move->GetWalkableFloorZ() ? EBBSBWireArrival::Floor:EBBSBWireArrival::Wall;
 WireAnchor=Anchor;WireNormal=Buddy->SurfaceNormal.GetSafeNormal();WireBuddy=Buddy;WireSurface=Surface;
 if(Surface)
 {
  WireLocalAnchor=Surface->GetComponentTransform().InverseTransformPosition(Anchor);
  WireLocalNormal=Surface->GetComponentTransform().InverseTransformVectorNoScale(WireNormal);
 }
 --WireCharges;bWireRechargeArmed=false;Character->FieldActions->CancelEquipmentSelection();
 // Keep the committed anchor, but free the camera even when the anchor came from a surface lock.
 Buddy->ReleaseSurfaceView();
 if(Character->Combat) for(int32 Direction=0;Direction<4;++Direction) Character->Combat->ParryUp(Direction);
 if(auto* InputPC=Cast<APlayerController>(Character->GetController())) InputPC->RotationInput=FRotator::ZeroRotator;
 RestoreWallJumpMovement();
 WireSavedAirControl=Move->AirControl;WireSavedFlyingBraking=Move->BrakingDecelerationFlying;
 WireSavedMaxFlySpeed=Move->MaxFlySpeed;bWireSavedOrient=Move->bOrientRotationToMovement;
 EndAirWeaponHang();Character->StopJumping();Character->FieldActions->ResetActionState();Character->ConsumeMovementInputVector();
 Move->ClearAccumulatedForces();Move->SetMovementMode(MOVE_Flying);Move->GravityScale=0;Move->AirControl=0;
 Move->BrakingDecelerationFlying=0;Move->MaxFlySpeed=WireSpeed;Move->bOrientRotationToMovement=false;
 WireCurrentSpeed=FMath::Clamp(Move->Velocity.Size(),300.f,700.f);Move->Velocity=FVector::ZeroVector;
 WireStartedAt=GetWorld()->GetTimeSeconds();WireMotionAge=0;WirePreviousStep=0;WirePreviousPosition=Character->GetActorLocation();
 bWirePulling=true;bWireArrival=bWireRecovering=false;WireRecoveryAge=-1;WireRecoveryYaw=WireCatchAlpha=0;Buddy->bWireAnchored=true;
 Character->FieldActions->SetEquipmentPointerCapture(false);
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
 const bool WasPulling=bWirePulling,WasRecovering=bWireRecovering;
 bWirePulling=bWireArrival=bWireRecovering=false;bSkyWire=false;WireRecoveryAge=-1;WireRecoveryYaw=WireCatchAlpha=0;
 if(WasRecovering) EndGlide();
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
  const FVector Carry=WireDirection*WireCurrentSpeed;
  RestoreWireMovement(false);WireSurface.Reset();Move->Velocity=Carry;
  // Borrow Bunniar without another lift. Catch, spin and descent share this lock.
  bAirJumpUsed=true;BeginGlide(false);DoubleJumpAge=-1;DoubleJumpYaw=0;
  bWireRecovering=true;WireRecoveryAge=0;WireRecoveryStartedAt=GetWorld()->GetTimeSeconds();
  WireRecoveryVelocity=Carry;WireRecoveryYaw=WireCatchAlpha=0;
  Move->SetMovementMode(MOVE_Flying);Move->GravityScale=0;Move->AirControl=0;Move->BrakingDecelerationFlying=0;Move->MaxFlySpeed=WireSpeed;
  if(Presentation) StartMotionEffect(JumpVortexComponent,Presentation->DoubleJumpVortex);
 }
 else
 {
  bWireArrival=true;WireArrivalAt=GetWorld()->GetTimeSeconds();Move->Velocity=FVector::ZeroVector;Move->GravityScale=0;
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
 if(bWireRecovering)
 {
  WireRecoveryAge=WireMotionAge=GetWorld()->GetTimeSeconds()-WireRecoveryStartedAt;
  const float Duration=FMath::Max(.35f,WireRecoverySeconds),Spin=FMath::Min(WireSpinSeconds,Duration-.1f);
  const float Phase=FMath::SmoothStep(0.f,Duration,WireRecoveryAge);
  WireRecoveryYaw=-360.f*FMath::SmoothStep(0.f,Spin,WireRecoveryAge);
  WireCatchAlpha=FMath::SmoothStep(Spin*.75f,Duration-.03f,WireRecoveryAge);
  const float Retain=FMath::Lerp(1.f,WireMomentumRetention,Phase);
  Move->Velocity=FVector(WireRecoveryVelocity.X*Retain,WireRecoveryVelocity.Y*Retain,
   FMath::Lerp(float(WireRecoveryVelocity.Z),-WireCatchFallSpeed,FMath::SmoothStep(0.f,Spin,WireRecoveryAge)));
  if(WireRecoveryAge>=Duration) FinishWireRecovery();
  return;
 }
 if(bWireArrival)
 {
  if(!WireSurface.IsValid()) { CancelWireRecall();return; }
  WireMotionAge=GetWorld()->GetTimeSeconds()-WireArrivalAt;
  const bool Wall=WireArrival==EBBSBWireArrival::Wall;
  Move->Velocity=Wall && WireMotionAge>=WireWallSlideStartsAt ? -FVector::UpVector*WireWallSlideSpeed:FVector::ZeroVector;
  if(WireMotionAge>=(Wall ? WireWallContactSeconds:WireArrivalSeconds))
  {
   const bool Floor=WireArrival==EBBSBWireArrival::Floor;bWireArrival=false;
   if(!Floor)
   {
    if(TryWireLedge()) return;
    auto* Surface=WireSurface.Get();RestoreWireMovement(false);
    if(!BeginWallSlide(WireAnchor,WireNormal,Surface)) Move->Velocity=WireNormal*70.f;
   }
   else { RestoreWireMovement(true);Move->Velocity=FVector::ZeroVector;bAirJumpUsed=false;LastFallSpeed=0;bWasFalling=false; }
   WireSurface.Reset();
  }
  return;
 }
 if(!IsValid(WireBuddy) || !WireBuddy->bWireAnchored || (!bSkyWire && Character->FieldActions->FieldEquipment!=WireBuddy) ||
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

void UBBSBTraversalComponent::EnsureWireSegments()
{
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
}

void UBBSBTraversalComponent::UpdateWirePresentation()
{
 if(!Character) return;
 auto* Buddy=Cast<ABBSBBunniarCompanion>(Character->FieldActions ? Character->FieldActions->BunniarEquipment.Get():nullptr);
 bWirePreviewVisible=!IsWireTraversalActive() && CanWireRecall(Buddy);
 const bool Pull=bWirePulling && IsValid(WireBuddy),Visible=Pull || bWirePreviewVisible;
 const FVector Start=Character->GetMesh()->GetSocketLocation(TEXT("hand_l"));
 const FVector End=Pull ? WireBuddy->GetActorLocation()+FVector(0,0,10):bWirePreviewVisible ? Buddy->GetActorLocation()+FVector(0,0,10):Start;
 WirePreviewAnchor=bWirePreviewVisible ? (Buddy->bAirHovering ? Buddy->AirHoverPoint:Buddy->SurfacePoint):FVector::ZeroVector;
 if(Visible) EnsureWireSegments();
 const FVector Line=End-Start;
 const float Sag=Pull ? FMath::Min(18.f,Line.Size()*.035f)*(1.f-FMath::Clamp(WireCurrentSpeed/FMath::Max(1.f,WireSpeed),0.f,.9f)):FMath::Min(32.f,Line.Size()*.055f);
 auto Point=[&](float T){ return Start+Line*T-FVector::UpVector*(FMath::Sin(PI*T)*Sag); };
 auto Tangent=[&](float T){ return (Line-FVector::UpVector*(PI*FMath::Cos(PI*T)*Sag))/4.f; };
 for(int32 I=0;I<WireSegments.Num();++I) if(auto* Part=WireSegments[I].Get())
 {
  Part->SetVisibility(Visible);if(!Visible) continue;
  Part->SetStartScale(FVector2D(Pull ? .009f:.0075f));Part->SetEndScale(FVector2D(Pull ? .009f:.0075f));
  Part->SetWorldTransform(FTransform::Identity);
  const float A=float(I)/4.f,B=float(I+1)/4.f;Part->SetStartAndEnd(Point(A),Tangent(A),Point(B),Tangent(B),true);
 }
}

void UBBSBTraversalComponent::FinishWireRecovery()
{
 const FVector Velocity=Character->GetCharacterMovement()->Velocity;
 bWireRecovering=false;bSkyWire=false;WireRecoveryAge=-1;WireRecoveryYaw=0;WireCatchAlpha=1;WirePoseAlpha=0;WireSurface.Reset();
 RestoreWireMovement(false);auto* Move=Character->GetCharacterMovement();Move->Velocity=Velocity;
 if(bGliding) { GlidePoseAlpha=1;Move->AirControl=GlideAirControl;Move->GravityScale=Velocity.Z<=-GlideFallSpeed ? 0.f:GlideGravityScale; }
 if(JumpVortexComponent) JumpVortexComponent->Deactivate();
}

void UBBSBTraversalComponent::TickWireRecharge(float DT)
{
 const bool WasOnRechargeSurface=bWireOnRechargeSurface;
 bWireOnRechargeSurface=false;if(!Character || (Character->Combat && Character->Combat->bDead)) return;
 const auto* Move=Character->GetCharacterMovement();
 bWireOnRechargeSurface=Move->IsMovingOnGround() || ParkourMode==EBBSBParkourMode::LedgeHang || ParkourMode==EBBSBParkourMode::WallSlide || bWireArrival;
 if(!bWireOnRechargeSurface && (Move->IsFalling() || bWireRecovering))
 {
  auto* Capsule=Character->GetCapsuleComponent();const FVector Position=Character->GetActorLocation();TArray<FHitResult> Hits;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(BunniarWireRechargeContact),false,Character);Q.AddIgnoredActor(Character->FieldActions ? Character->FieldActions->BunniarEquipment.Get():nullptr);Q.bFindInitialOverlaps=true;
  const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius()+2.f,Capsule->GetScaledCapsuleHalfHeight()+2.f);
  GetWorld()->SweepMultiByChannel(Hits,Position,Position+FVector(0,0,-.1f),FQuat::Identity,ECC_Visibility,Shape,Q);
  for(const auto& Hit:Hits) if(Hit.bBlockingHit && Hit.GetComponent()) { bWireOnRechargeSurface=true;break; }
 }
 // Contact is a trigger, not a condition that must remain true throughout charging.
 // Using a wire while still supported must not re-arm recharge on the next frame.
 if(bWireOnRechargeSurface && !WasOnRechargeSurface) bWireRechargeArmed=true;
 WireCharges=FMath::Clamp(WireCharges,0,2);
 if(WireCharges>=2) { WireRechargeProgress=0;return; }
 if(!bWireRechargeArmed) return;
 WireRechargeProgress+=FMath::Max(0.f,DT)/FMath::Max(.5f,WireRechargeSeconds);
 while(WireRechargeProgress>=1.f && WireCharges<2) { ++WireCharges;WireRechargeProgress-=1.f; }
 if(WireCharges>=2) WireRechargeProgress=0;
}
