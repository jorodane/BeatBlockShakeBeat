#include "Character/BBSBTraversal.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "Combat/BBSBCombat.h"
#include "Field/BBSBFieldActions.h"
#include "Interaction/BBSBInteraction.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
 FCollisionQueryParams ParkourQuery(const ABBSBCharacter* Player)
 {
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBParkour),false,Player);
  if(Player && Player->FieldActions) Query.AddIgnoredActor(Player->FieldActions->BunniarEquipment);
  return Query;
 }
 float Ease(float T) { T=FMath::Clamp(T,0.f,1.f);return T*T*(3.f-2.f*T); }
}

bool UBBSBTraversalComponent::ValidParkourSurface(UPrimitiveComponent* Surface) const
{
 return IsValid(Surface) && Surface->IsRegistered() && Surface->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block && !Surface->IsSimulatingPhysics() &&
  IsValid(Surface->GetOwner()) && !Surface->GetOwner()->Implements<UBBSBBunniarSurface>();
}
bool UBBSBTraversalComponent::CapsuleClear(const FVector& Position) const
{
 const auto* Capsule=Character->GetCapsuleComponent();
 return !GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Pawn,
  FCollisionShape::MakeCapsule(FMath::Max(1.f,Capsule->GetScaledCapsuleRadius()-1.f),FMath::Max(2.f,Capsule->GetScaledCapsuleHalfHeight()-1.f)),ParkourQuery(Character));
}
bool UBBSBTraversalComponent::CapsulePathClear(const FVector& From,const FVector& To) const
{
 const auto* Capsule=Character->GetCapsuleComponent();FHitResult Hit;
 return !GetWorld()->SweepSingleByChannel(Hit,From,To,FQuat::Identity,ECC_Pawn,
  FCollisionShape::MakeCapsule(FMath::Max(1.f,Capsule->GetScaledCapsuleRadius()-1.f),FMath::Max(2.f,Capsule->GetScaledCapsuleHalfHeight()-1.f)),ParkourQuery(Character));
}
bool UBBSBTraversalComponent::PathClear(const FVector& From,const FVector& To,float Apex) const
{
 const FVector A(From.X,From.Y,Apex),B(To.X,To.Y,Apex);
 return CapsuleClear(To) && CapsulePathClear(From,A) && CapsulePathClear(A,B) && CapsulePathClear(B,To);
}
bool UBBSBTraversalComponent::FindLedge(const FVector& WallPoint,const FVector& Normal,UPrimitiveComponent* Surface,FLedgeCandidate& Out) const
{
 if(!Character || !ValidParkourSurface(Surface) || FMath::Abs(Normal.Z)>.35f) return false;
 const FVector N=Normal.GetSafeNormal2D();if(N.IsNearlyZero()) return false;
 const FVector Position=Character->GetActorLocation();const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
 const FVector Inside=WallPoint-N*10.f;
 const auto Query=ParkourQuery(Character);FHitResult Top;
 if(!GetWorld()->LineTraceSingleByChannel(Top,FVector(Inside.X,Inside.Y,Position.Z+LedgeReachAbove+12.f),FVector(Inside.X,Inside.Y,Position.Z-12.f),ECC_Visibility,Query) ||
  Top.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ() || !ValidParkourSurface(Top.GetComponent())) return false;
 const float Above=Top.ImpactPoint.Z-Position.Z;
 if(Above < -8.f || Above>LedgeReachAbove) return false;
 const FVector Edge(WallPoint.X,WallPoint.Y,Top.ImpactPoint.Z);
 const FVector Side=FVector::CrossProduct(FVector::UpVector,-N).GetSafeNormal();
 // Both hands need a real, sufficiently wide edge, not the tip of a pole.
 for(float Sign:{-1.f,1.f})
 {
  FHitResult Support;const FVector P=Edge-N*10.f+Side*(Sign*18.f);
  if(!GetWorld()->LineTraceSingleByChannel(Support,P+FVector(0,0,10),P-FVector(0,0,14),ECC_Visibility,Query) ||
   Support.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ() || FMath::Abs(Support.ImpactPoint.Z-Edge.Z)>8.f) return false;
 }
 const FVector StandXY=Edge-N*(Radius+22.f);FHitResult StandHit;
 if(!GetWorld()->LineTraceSingleByChannel(StandHit,StandXY+FVector(0,0,12),StandXY-FVector(0,0,18),ECC_Visibility,Query) ||
  StandHit.ImpactNormal.Z<Character->GetCharacterMovement()->GetWalkableFloorZ() || FMath::Abs(StandHit.ImpactPoint.Z-Edge.Z)>10.f) return false;
 const FVector Hang=Edge+N*(Radius+9.f)-FVector(0,0,LedgeHangBelow);
 const FVector Stand=StandHit.ImpactPoint+FVector(0,0,Half+3.f);
 if(!CapsuleClear(Hang) || !CapsulePathClear(Position,Hang) || !PathClear(Hang,Stand,Edge.Z+Half+10.f)) return false;
 Out.Surface=Surface;Out.Point=Edge;Out.Normal=N;Out.Hang=Hang;Out.Stand=Stand;return true;
}
bool UBBSBTraversalComponent::FindLowLedge(const FVector& WallPoint,const FVector& Normal,UPrimitiveComponent* Surface,FLedgeCandidate& Out) const
{
 if(!Character || !ValidParkourSurface(Surface) || FMath::Abs(Normal.Z)>.35f) return false;
 const FVector N=Normal.GetSafeNormal2D();if(N.IsNearlyZero()) return false;
 const FVector Position=Character->GetActorLocation();const auto* Move=Character->GetCharacterMovement();
 const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Character->GetCapsuleComponent()->GetScaledCapsuleRadius(),FeetZ=Position.Z-Half;
 const FVector Inside=WallPoint-N*10.f;const auto Query=ParkourQuery(Character);FHitResult Top;
 if(!GetWorld()->LineTraceSingleByChannel(Top,FVector(Inside.X,Inside.Y,FeetZ+LowVaultMaximumHeight+12.f),FVector(Inside.X,Inside.Y,FeetZ+6.f),ECC_Visibility,Query) ||
  Top.ImpactNormal.Z<Move->GetWalkableFloorZ() || !ValidParkourSurface(Top.GetComponent())) return false;
 const float Rise=Top.ImpactPoint.Z-FeetZ;if(Rise<6.f || Rise>LowVaultMaximumHeight) return false;
 const FVector Edge(WallPoint.X,WallPoint.Y,Top.ImpactPoint.Z),Side=FVector::CrossProduct(FVector::UpVector,-N).GetSafeNormal();
 for(float Sign:{-1.f,1.f})
 {
  FHitResult Support;const FVector P=Edge-N*10.f+Side*(Sign*18.f);
  if(!GetWorld()->LineTraceSingleByChannel(Support,P+FVector(0,0,10),P-FVector(0,0,14),ECC_Visibility,Query) ||
   !ValidParkourSurface(Support.GetComponent()) || Support.ImpactNormal.Z<Move->GetWalkableFloorZ() || FMath::Abs(Support.ImpactPoint.Z-Edge.Z)>8.f) return false;
 }
 const FVector StandXY=Edge-N*(Radius+22.f);FHitResult StandHit;
 if(!GetWorld()->LineTraceSingleByChannel(StandHit,StandXY+FVector(0,0,12),StandXY-FVector(0,0,18),ECC_Visibility,Query) ||
  !ValidParkourSurface(StandHit.GetComponent()) || StandHit.ImpactNormal.Z<Move->GetWalkableFloorZ() || FMath::Abs(StandHit.ImpactPoint.Z-Edge.Z)>10.f) return false;
 const FVector Stand=StandHit.ImpactPoint+FVector(0,0,Half+3.f);
 // A low edge needs a clear upward path, not room for a hanging body below it.
 float Apex,Seconds;if(!PlanParkourJump(Position,Stand,N,Stand.Z+ParkourJumpClearance,Apex,Seconds)) return false;
 Out.Surface=Surface;Out.Point=Edge;Out.Normal=N;Out.Hang=Position;Out.Stand=Stand;return true;
}
void UBBSBTraversalComponent::BeginLowMantle(const FLedgeCandidate& Ledge)
{
 const FVector From=Character->GetActorLocation();float Apex,Seconds;
 if(!PlanParkourJump(From,Ledge.Stand,Ledge.Normal,Ledge.Stand.Z+ParkourJumpClearance,Apex,Seconds)) return;
 BeginParkour(EBBSBParkourMode::Vault);ParkourFrom=ParkourJumpStart=From;
 ParkourSurface=Ledge.Surface;LedgePoint=Ledge.Point;LedgeNormal=Ledge.Normal;ParkourTo=Ledge.Stand;
 ParkourApex=Apex;ParkourJumpSeconds=ParkourDuration=Seconds;ParkourJumpPreparation=0;bParkourJumpPlanned=true;ParkourHandAlpha=ClimbFootAlpha=0;
 Character->SetActorRotation((-Ledge.Normal).Rotation());StartParkourJump();
 if(Presentation) SpawnSmoke(Presentation->JumpSmoke,From-FVector(0,0,Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),FVector::UpVector,.6f);
}
void UBBSBTraversalComponent::BeginParkour(EBBSBParkourMode Mode)
{
 RestoreWallJumpMovement();
 RestoreParkourRunExit();ParkourEntryVelocity=Character->GetVelocity();EndAirWeaponHang();EndGlide();
 // End a contacting dash before saving the movement settings that parkour restores.
 const int32 Pending=Character->FieldActions->PendingEquipmentId;Character->FieldActions->ResetActionState();Character->FieldActions->PendingEquipmentId=Pending;
 auto* Move=Character->GetCharacterMovement();
 ParkourSavedAirControl=Move->AirControl;ParkourSavedFlyingBraking=Move->BrakingDecelerationFlying;ParkourSavedMaxFlySpeed=Move->MaxFlySpeed;bParkourSavedOrient=Move->bOrientRotationToMovement;
 ParkourSavedFallingBraking=Move->BrakingDecelerationFalling;ParkourSavedFallingFriction=Move->FallingLateralFriction;bParkourJumpPlanned=bParkourJumping=false;
 if(auto* Buddy=Cast<ABBSBBunniarCompanion>(Character->FieldActions->BunniarEquipment)) Buddy->ReleaseSurface();
 Character->StopJumping();Character->ConsumeMovementInputVector();Move->ClearAccumulatedForces();Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Flying);
 Move->GravityScale=0;Move->AirControl=0;Move->BrakingDecelerationFlying=0;Move->MaxFlySpeed=1800.f;Move->bOrientRotationToMovement=false;
 ParkourMode=ParkourVisualMode=Mode;ParkourStartedAt=GetWorld()->GetTimeSeconds();ParkourAge=0;WallJumpAge=-1;bClimbQueued=false;
}
void UBBSBTraversalComponent::BeginLedgeHang(const FLedgeCandidate& Ledge)
{
 ParkourFrom=Character->GetActorLocation();BeginParkour(EBBSBParkourMode::LedgeHang);
 ParkourSurface=Ledge.Surface;LedgePoint=Ledge.Point;LedgeNormal=Ledge.Normal;ParkourTo=Ledge.Stand;ParkourHandAlpha=1.f;
 const FTransform Frame=ParkourSurface->GetComponentTransform();
 LedgeLocalPoint=Frame.InverseTransformPosition(LedgePoint);LedgeLocalNormal=Frame.InverseTransformVectorNoScale(LedgeNormal);LedgeLocalStand=Frame.InverseTransformPosition(ParkourTo);
}
bool UBBSBTraversalComponent::TryWireLedge()
{
 FLedgeCandidate Ledge;
 if(FindLowLedge(WireAnchor,WireNormal,WireSurface.Get(),Ledge))
 { RestoreWireMovement(false);WireSurface.Reset();BeginLowMantle(Ledge);return true; }
 if(!FindLedge(WireAnchor,WireNormal,WireSurface.Get(),Ledge)) return false;
 RestoreWireMovement(false);WireSurface.Reset();BeginLedgeHang(Ledge);return true;
}
bool UBBSBTraversalComponent::TryAutoLedge()
{
 if((!bAutoCatchLedges && !bAutoWallSlide) || !Character || IsParkourActive() || IsWireTraversalActive() || bGlideDashing || bAirLiftPending || bAirWeaponHanging || GetWorld()->GetTimeSeconds()<ParkourIgnoreUntil) return false;
 auto* Move=Character->GetCharacterMovement();const auto* PC=Cast<APlayerController>(Character->GetController());
 const bool AirDash=Move->IsFalling() && Character->FieldActions && Character->FieldActions->IsDodging();
 if(!PC || !PC->IsLocalController() || PC->IsMoveInputIgnored() || !Move->IsFalling() || !Character->FieldActions ||
  Character->FieldActions->IsGestureCaptured() || (!AirDash && !Character->FieldActions->CanStartJump())) return false;
 if(AirDash && ((FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive()) ||
    Character->FieldActions->bEquipmentWheelOpen || (Character->Interaction && Character->Interaction->bDialogueActive) ||
    (Character->Combat && (Character->Combat->bDead || Character->Combat->BlocksMovement())))) return false;
 const FVector Intent=Character->FieldActions->MovementIntent();
 // A dash keeps its committed direction even if the movement key has been released.
 const FVector Direction=(AirDash ? Move->Velocity:(!Intent.IsNearlyZero() ? Intent:Move->Velocity)).GetSafeNormal2D();
 if(Direction.IsNearlyZero()) return false;
 FHitResult Wall;const FVector Start=Character->GetActorLocation()+FVector(0,0,10);
 const float Radius=Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
 const auto Query=ParkourQuery(Character);
 auto ForwardWall=[&](const FVector& Probe,FHitResult& Hit)
 {
  return GetWorld()->SweepSingleByChannel(Hit,Probe,Probe+Direction*(Radius+72.f),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(8.f),Query) &&
   ValidParkourSurface(Hit.GetComponent()) && FMath::Abs(Hit.ImpactNormal.Z)<=.35f && FVector::DotProduct(Direction,-Hit.ImpactNormal.GetSafeNormal2D())>=.65f;
 };
 const bool HasWall=ForwardWall(Start,Wall);
 if(!AirDash && bAutoCatchLedges && !Intent.IsNearlyZero())
 {
  FLedgeCandidate Low;bool Found=HasWall && FindLowLedge(Wall.ImpactPoint,Wall.ImpactNormal,Wall.GetComponent(),Low);
  if(!Found)
  {
   FHitResult Lower;const FVector Probe=Character->GetActorLocation()-FVector(0,0,Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-12.f);
   Found=ForwardWall(Probe,Lower) && FindLowLedge(Lower.ImpactPoint,Lower.ImpactNormal,Lower.GetComponent(),Low);
  }
  if(Found)
  {
   if(Move->Velocity.Z>100.f)
   {
    if(!bPredictRisingMantle || IsBunniarTraversalActive()) return false;
    const float Gravity=FMath::Abs(Move->GetGravityZ());if(Gravity<1.f) return false;
    const float FeetApex=Character->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+FMath::Square(Move->Velocity.Z)/(2.f*Gravity);
    if(FeetApex>=Low.Point.Z+6.f) return false;
   }
   BeginLowMantle(Low);return true;
  }
 }
 if(!HasWall) return false;
 FLedgeCandidate Ledge;
 if(!AirDash && bAutoCatchLedges && FindLedge(Wall.ImpactPoint,Wall.ImpactNormal,Wall.GetComponent(),Ledge))
 {
  bool Predictive=false;
  if(Move->Velocity.Z>100.f)
  {
   if(!bPredictRisingMantle || IsBunniarTraversalActive()) return false;
   const float Gravity=FMath::Abs(Move->GetGravityZ());if(Gravity<1.f) return false;
   const float FeetApex=Character->GetActorLocation().Z-Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+FMath::Square(Move->Velocity.Z)/(2.f*Gravity);
   // Let an ordinary jump clear reachable tops; catch an unreachable ledge during ascent.
   if(FeetApex>=Ledge.Point.Z+6.f) return false;
   Predictive=true;
  }
  BeginLedgeHang(Ledge);bClimbQueued=Predictive;return true;
 }
 const float Gap=FVector::DotProduct(Character->GetActorLocation()-Wall.ImpactPoint,Wall.ImpactNormal.GetSafeNormal2D());
 if(bAutoWallSlide && (AirDash || (Move->Velocity.Z<=0.f && !Intent.IsNearlyZero())) && Gap<=Radius+18.f)
  return BeginWallSlide(Wall.ImpactPoint,Wall.ImpactNormal,Wall.GetComponent());
 return false;
}
bool UBBSBTraversalComponent::BeginWallSlide(const FVector& Point,const FVector& Normal,UPrimitiveComponent* Surface)
{
 if(!Character || !ValidParkourSurface(Surface) || FMath::Abs(Normal.Z)>.35f) return false;
 const FVector N=Normal.GetSafeNormal2D();if(N.IsNearlyZero()) return false;
 BeginParkour(EBBSBParkourMode::WallSlide);ParkourSurface=Surface;LedgePoint=Point;LedgeNormal=N;ParkourHandAlpha=ClimbFootAlpha=0;
 const FTransform Frame=Surface->GetComponentTransform();LedgeLocalPoint=Frame.InverseTransformPosition(Point);LedgeLocalNormal=Frame.InverseTransformVectorNoScale(N);
 auto* Move=Character->GetCharacterMovement();Move->SetMovementMode(MOVE_Falling);Move->GravityScale=0;Move->Velocity=FVector::ZeroVector;LastFallSpeed=0;bWasFalling=true;
 Character->SetActorRotation((-N).Rotation());return true;
}
void UBBSBTraversalComponent::TickWallSlide(float DT)
{
 auto* Move=Character->GetCharacterMovement();ParkourHandAlpha=ClimbFootAlpha=0;
 if(Move->IsMovingOnGround()) { CancelParkour();Move->SetMovementMode(MOVE_Walking);ParkourIgnoreUntil=GetWorld()->GetTimeSeconds()+.1f;return; }
 if(!Move->IsFalling()) { CancelParkour();return; }
 const FVector Position=Character->GetActorLocation();const float Radius=Character->GetCapsuleComponent()->GetScaledCapsuleRadius();FHitResult Wall;
 if(!GetWorld()->SweepSingleByChannel(Wall,Position+FVector(0,0,10),Position+FVector(0,0,10)-LedgeNormal*(Radius+24.f),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(8.f),ParkourQuery(Character)) ||
  !ValidParkourSurface(Wall.GetComponent()) || FMath::Abs(Wall.ImpactNormal.Z)>.35f || FVector::DotProduct(Wall.ImpactNormal.GetSafeNormal2D(),LedgeNormal)<.5f) { CancelParkour();return; }
 ParkourSurface=Wall.GetComponent();LedgePoint=Wall.ImpactPoint;LedgeNormal=Wall.ImpactNormal.GetSafeNormal2D();
 const FVector Intent=Character->FieldActions->MovementIntent();
 if(bAutoCatchLedges && !Intent.IsNearlyZero() && FVector::DotProduct(Intent,-LedgeNormal)>.5f)
 {
  FLedgeCandidate Ledge;
  if(FindLowLedge(Wall.ImpactPoint,Wall.ImpactNormal,Wall.GetComponent(),Ledge))
  { RestoreParkourMovement(false);ParkourMode=EBBSBParkourMode::None;BeginLowMantle(Ledge);return; }
  if(FindLedge(Wall.ImpactPoint,Wall.ImpactNormal,Wall.GetComponent(),Ledge))
  { RestoreParkourMovement(false);ParkourMode=EBBSBParkourMode::None;BeginLedgeHang(Ledge);bClimbQueued=true;return; }
 }
 Move->GravityScale=0;Move->Velocity=-LedgeNormal*35.f;
 if(ParkourAge>=WireWallSlideStartsAt) Move->Velocity.Z=-FMath::Max(1.f,WireWallSlideSpeed);
 LastFallSpeed=FMath::Max(0.f,-Move->Velocity.Z);
 Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(),(-LedgeNormal).Rotation(),DT,18.f));
}
bool UBBSBTraversalComponent::TryLowVault()
{
 if(!Character || IsParkourActive() || IsWireTraversalActive() || IsBunniarTraversalActive() || GetWorld()->GetTimeSeconds()<ParkourIgnoreUntil || !Character->FieldActions ||
  Character->FieldActions->IsGestureCaptured() || !Character->FieldActions->CanStartJump()) return false;
 auto* Move=Character->GetCharacterMovement();if(!Move->IsMovingOnGround()) return false;
 const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
 const FVector From=Character->GetActorLocation(),Feet=From-FVector(0,0,Half);
 const FVector Direction=Character->FieldActions->MovementDashDirection().GetSafeNormal2D();const auto Query=ParkourQuery(Character);FHitResult Wall;
 const FVector Probe=Feet+FVector(0,0,FMath::Max(38.f,Move->MaxStepHeight+8.f));
 if(!GetWorld()->SweepSingleByChannel(Wall,Probe,Probe+Direction*(Radius+100.f),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(8),Query) || !ValidParkourSurface(Wall.GetComponent()) ||
  FMath::Abs(Wall.ImpactNormal.Z)>.35f || FVector::DotProduct(Direction,-Wall.ImpactNormal.GetSafeNormal2D())<.65f) return false;
 const FVector N=Wall.ImpactNormal.GetSafeNormal2D(),Inside=Wall.ImpactPoint-N*10.f;FHitResult Top;
 if(!GetWorld()->LineTraceSingleByChannel(Top,FVector(Inside.X,Inside.Y,Feet.Z+LowVaultMaximumHeight+15.f),FVector(Inside.X,Inside.Y,Feet.Z+Move->MaxStepHeight),ECC_Visibility,Query) || Top.ImpactNormal.Z<Move->GetWalkableFloorZ()) return false;
 const float Height=Top.ImpactPoint.Z-Feet.Z;
 if(Height<Move->MaxStepHeight+3.f || Height>LowVaultMaximumHeight) return false;
 for(float Width=20.f;Width<=LowVaultMaximumDepth+.1f;Width+=20.f)
 {
  const FVector XY=Wall.ImpactPoint-N*(Width+Radius+12.f);FHitResult Ground;
  if(!GetWorld()->LineTraceSingleByChannel(Ground,FVector(XY.X,XY.Y,Top.ImpactPoint.Z+12.f),FVector(XY.X,XY.Y,Feet.Z-55.f),ECC_Visibility,Query) ||
   Ground.ImpactNormal.Z<Move->GetWalkableFloorZ() || Ground.ImpactPoint.Z>Feet.Z+15.f || Ground.ImpactPoint.Z<Feet.Z-50.f) continue;
  FVector To=Ground.ImpactPoint+FVector(0,0,Half+3.f);float Apex,Seconds;
  // Leave some running room beyond a thin obstacle instead of braking on its far edge.
  FHitResult Runout;const FVector Far=Ground.ImpactPoint-N*80.f;
  if(GetWorld()->LineTraceSingleByChannel(Runout,Far+FVector(0,0,18),Far-FVector(0,0,24),ECC_Visibility,Query) &&
   Runout.ImpactNormal.Z>=Move->GetWalkableFloorZ() && FMath::Abs(Runout.ImpactPoint.Z-Ground.ImpactPoint.Z)<12.f)
   To=Runout.ImpactPoint+FVector(0,0,Half+3.f);
  if(!PlanParkourJump(From,To,N,Top.ImpactPoint.Z+Half+ParkourJumpClearance,Apex,Seconds)) continue;
  BeginParkour(EBBSBParkourMode::Vault);ParkourSurface=Wall.GetComponent();ParkourFrom=ParkourJumpStart=From;ParkourTo=To;ParkourApex=Apex;ParkourJumpSeconds=ParkourDuration=Seconds;
  LedgePoint=FVector(Wall.ImpactPoint.X,Wall.ImpactPoint.Y,Top.ImpactPoint.Z);LedgeNormal=N;ParkourHandAlpha=0;ParkourJumpPreparation=0;bParkourJumpPlanned=true;StartParkourJump();
  Character->SetActorRotation((-N).Rotation());if(Presentation) SpawnSmoke(Presentation->JumpSmoke,Feet,FVector::UpVector,.6f);return true;
 }
 return false;
}
bool UBBSBTraversalComponent::TryLedgeClimb()
{
 if(ParkourMode!=EBBSBParkourMode::LedgeHang || !ParkourSurface.IsValid()) return false;
 if(GetWorld()->GetTimeSeconds()-ParkourStartedAt<.12f) { bClimbQueued=true;return true; }
 const FVector From=Character->GetActorLocation(),Dip=From-FVector(0,0,2.f);float Apex,Seconds;
 if(!CapsulePathClear(From,Dip) || !PlanParkourJump(Dip,ParkourTo,LedgeNormal,ParkourTo.Z+ParkourJumpClearance,Apex,Seconds)) return false;
 ClimbStartPelvisZ=Character->GetMesh()->GetSocketLocation(TEXT("pelvis")).Z-From.Z;
 ParkourEntryVelocity=Character->GetVelocity();ParkourFrom=From;ParkourJumpStart=Dip;ParkourApex=Apex;ParkourJumpSeconds=Seconds;ParkourJumpPreparation=FMath::Max(0.f,LedgePopPreparationSeconds);ParkourDuration=ParkourJumpPreparation+Seconds;
 ParkourMode=ParkourVisualMode=EBBSBParkourMode::Mantle;ParkourAge=0;ParkourStartedAt=GetWorld()->GetTimeSeconds();bParkourJumpPlanned=true;bParkourJumping=false;return true;
}
FVector UBBSBTraversalComponent::ParkourJumpPoint(const FVector& From,const FVector& To,const FVector& Normal,float Apex,float Gravity,float Seconds,float Age) const
{
 const float T=FMath::Clamp(Age,0.f,Seconds),U=FMath::Clamp(T/FMath::Max(.01f,Seconds*.96f),0.f,1.f);
 // Clear the lip continuously, without first recoiling away from the player's input.
 const float Forward=FMath::Square(Ease(U));FVector Point=FMath::Lerp(From,To,Forward);
 Point.Z=From.Z+FMath::Sqrt(2.f*Gravity*FMath::Max(0.f,Apex-From.Z))*T-.5f*Gravity*T*T;return Point;
}
bool UBBSBTraversalComponent::PlanParkourJump(const FVector& From,const FVector& To,const FVector& Normal,float MinimumApex,float& Apex,float& Seconds) const
{
 if(FMath::Abs(GetWorld()->GetGravityZ())<1.f || !CapsuleClear(To)) return false;
 const float Speed=FMath::Max(300.f,Character->GetCharacterMovement()->MaxWalkSpeed)*FMath::Clamp(ParkourPaceScale,.5f,2.f);
 const float Duration=FMath::Clamp((FVector::Dist2D(From,To)+FMath::Max(0.f,To.Z-From.Z)*.65f)/Speed+.04f,.26f,.52f);
 const float Minimum=FMath::Max(MinimumApex,FMath::Max(From.Z,To.Z)+FMath::Max(6.f,ParkourJumpClearance));
 for(float Extra:{0.f,8.f,16.f,28.f,44.f})
 {
  const float Height=Minimum+Extra;
  const float Gravity=2.f*FMath::Square(FMath::Sqrt(Height-From.Z)+FMath::Sqrt(Height-To.Z))/FMath::Square(Duration);
  FVector Previous=From;bool Clear=true;const auto* Capsule=Character->GetCapsuleComponent();
  const auto Query=ParkourQuery(Character);
  for(int32 I=1;I<=48;++I)
  {
   const FVector Next=ParkourJumpPoint(From,To,Normal,Height,Gravity,Duration,Duration*I/48.f);FHitResult Hit;
   const float FloorMargin=2.f*FMath::Clamp(I/8.f,0.f,1.f);
   if(GetWorld()->SweepSingleByChannel(Hit,Previous,Next,FQuat::Identity,ECC_Pawn,
    FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()+FloorMargin),Query)) { Clear=false;break; }
   Previous=Next;
  }
  if(Clear) { Apex=Height;Seconds=Duration;return true; }
 }
 return false;
}
void UBBSBTraversalComponent::StartParkourJump()
{
 auto* Move=Character->GetCharacterMovement();bParkourJumping=true;ParkourJumpStartedAt=GetWorld()->GetTimeSeconds();
 const float Rise=FMath::Max(0.f,ParkourApex-Character->GetActorLocation().Z),Drop=FMath::Max(0.f,ParkourApex-ParkourTo.Z);
 const float Gravity=2.f*FMath::Square(FMath::Sqrt(Rise)+FMath::Sqrt(Drop))/FMath::Square(FMath::Max(.1f,ParkourJumpSeconds));
 ParkourJumpGravityScale=Gravity/FMath::Max(1.f,FMath::Abs(GetWorld()->GetGravityZ()));
 Move->ClearAccumulatedForces();Move->SetMovementMode(MOVE_Falling);Move->GravityScale=ParkourJumpGravityScale;Move->AirControl=0;Move->BrakingDecelerationFalling=Move->FallingLateralFriction=0;
 Move->Velocity=FVector(ParkourEntryVelocity.X,ParkourEntryVelocity.Y,FMath::Sqrt(2.f*Gravity*Rise));
 LastFallSpeed=0;bWasFalling=true;ParkourHandAlpha=ClimbFootAlpha=0;
}
void UBBSBTraversalComponent::TickParkourJump(float DT)
{
 auto* Move=Character->GetCharacterMovement();Character->ConsumeMovementInputVector();ClimbFootAlpha=0;
 if(!bParkourJumping)
 {
  const float T=FMath::Clamp(ParkourAge/FMath::Max(.001f,ParkourJumpPreparation),0.f,1.f);
  const FVector Goal=FMath::Lerp(ParkourFrom,ParkourJumpStart,Ease(T));FHitResult Hit;
  Move->GravityScale=0;Move->Velocity=FVector::ZeroVector;Character->SetActorLocation(Goal,true,&Hit,ETeleportType::None);ParkourHandAlpha=1.f;
  if(Hit.bBlockingHit) { CancelParkour();return; }
  if(ParkourAge>=ParkourJumpPreparation) StartParkourJump();
  return;
 }
 if(Move->IsMovingOnGround()) { FinishParkourJump();return; }
 if(!Move->IsFalling()) { CancelParkour();return; }
 const float Age=GetWorld()->GetTimeSeconds()-ParkourJumpStartedAt;
 if(Age>ParkourJumpSeconds+.7f || Character->GetActorLocation().Z<ParkourJumpStart.Z-80.f)
 { const FVector Velocity=Move->Velocity;CancelParkour();Move->Velocity=Velocity;return; }
 Move->GravityScale=ParkourJumpGravityScale;Move->AirControl=0;Move->BrakingDecelerationFalling=Move->FallingLateralFriction=0;
 const FVector Next=ParkourJumpPoint(ParkourJumpStart,ParkourTo,LedgeNormal,ParkourApex,FMath::Abs(Move->GetGravityZ()),ParkourJumpSeconds,Age+DT);
 FVector Guide=(Next-Character->GetActorLocation())/FMath::Max(DT,.0001f);Guide.Z=0;
 const FVector Forward=-LedgeNormal;const float Along=FVector::DotProduct(Guide,Forward);
 if(Along<0.f) Guide-=Forward*Along;
 const float ArcSpeed=FVector::Dist2D(ParkourJumpStart,ParkourTo)*2.5f/FMath::Max(.1f,ParkourJumpSeconds);
 Guide=Guide.GetClampedToMaxSize(FMath::Max(Move->MaxWalkSpeed*1.5f,ArcSpeed));
 // Keep vertical gravity physical and avoid correcting incoming momentum backwards.
 Move->Velocity.X=Guide.X;Move->Velocity.Y=Guide.Y;
 ParkourHandAlpha=0;LastFallSpeed=FMath::Max(0.f,-Move->Velocity.Z);
 Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(),(-LedgeNormal).Rotation(),DT,18.f));
}
void UBBSBTraversalComponent::FinishParkourJump()
{
 bParkourJumpPlanned=bParkourJumping=false;ParkourMode=EBBSBParkourMode::None;ParkourHandAlpha=ClimbFootAlpha=0;bClimbQueued=false;ParkourSurface.Reset();
 auto* Move=Character->GetCharacterMovement();const FVector LandingVelocity=Move->Velocity;
 const FVector Intent=Character->FieldActions ? Character->FieldActions->MovementIntent():FVector::ZeroVector;
 RestoreParkourMovement(true);
 const FVector Exit=Intent.IsNearlyZero() ? FVector(LandingVelocity.X,LandingVelocity.Y,0):Intent.GetSafeNormal2D()*Move->MaxWalkSpeed;
 Move->Velocity=Exit;
 if(!Intent.IsNearlyZero())
 {
  ParkourExitFriction=Move->GroundFriction;ParkourExitBraking=Move->BrakingDecelerationWalking;ParkourExitBrakingFactor=Move->BrakingFrictionFactor;bParkourRunExitPending=true;
  Move->GroundFriction=Move->BrakingDecelerationWalking=Move->BrakingFrictionFactor=0;
  Character->AddMovementInput(Intent.GetSafeNormal2D(),Intent.Size());
 }
 bAirJumpUsed=false;LastFallSpeed=0;bWasFalling=false;ParkourIgnoreUntil=GetWorld()->GetTimeSeconds()+.2f;
}
void UBBSBTraversalComponent::RestoreParkourRunExit()
{
 if(!bParkourRunExitPending || !Character) return;
 auto* Move=Character->GetCharacterMovement();Move->GroundFriction=ParkourExitFriction;Move->BrakingDecelerationWalking=ParkourExitBraking;Move->BrakingFrictionFactor=ParkourExitBrakingFactor;bParkourRunExitPending=false;
}
void UBBSBTraversalComponent::RestoreParkourMovement(bool Grounded)
{
 if(!Character) return;auto* Move=Character->GetCharacterMovement();Move->ClearAccumulatedForces();Move->StopMovementImmediately();
 Move->AirControl=ParkourSavedAirControl;Move->BrakingDecelerationFlying=ParkourSavedFlyingBraking;Move->MaxFlySpeed=ParkourSavedMaxFlySpeed;Move->bOrientRotationToMovement=bParkourSavedOrient;
 Move->BrakingDecelerationFalling=ParkourSavedFallingBraking;Move->FallingLateralFriction=ParkourSavedFallingFriction;
 Move->GravityScale=BaseGravityScale;Move->SetMovementMode(Grounded ? MOVE_Walking:MOVE_Falling);
}
void UBBSBTraversalComponent::CancelParkour()
{
 if(!IsParkourActive()) return;bParkourJumpPlanned=bParkourJumping=false;ParkourMode=EBBSBParkourMode::None;ParkourHandAlpha=ClimbFootAlpha=0;bClimbQueued=false;RestoreParkourMovement(false);ParkourSurface.Reset();ParkourIgnoreUntil=GetWorld()->GetTimeSeconds()+ParkourRegrabDelay;
}
bool UBBSBTraversalComponent::DropParkour()
{
 if(!IsParkourActive()) return false;
 const FVector N=LedgeNormal;CancelParkour();Character->GetCharacterMovement()->Velocity=N*90.f-FVector(0,0,60);return true;
}
bool UBBSBTraversalComponent::TryWallJump()
{
 const bool Ledge=ParkourMode==EBBSBParkourMode::LedgeHang || ParkourMode==EBBSBParkourMode::WallSlide;
 if(!Character || (!Ledge && !(bWireArrival && WireArrival==EBBSBWireArrival::Wall))) return false;
 const FVector N=Ledge ? LedgeNormal:WireNormal.GetSafeNormal2D();const FVector Point=Ledge ? LedgePoint:WireAnchor;
 if(Ledge) CancelParkour();else CancelWireRecall();
 EndAirWeaponHang();Character->ConsumeMovementInputVector();Character->StopJumping();
 const FVector Intent=Character->FieldActions ? Character->FieldActions->MovementIntent():FVector::ZeroVector;
 const FVector Side=FVector::VectorPlaneProject(Intent,N).GetClampedToMaxSize(1.f);
 const FVector Velocity=N*WallJumpOutSpeed+Side*120.f+FVector::UpVector*WallJumpUpSpeed;
 RestoreWallJumpMovement();
 auto* Move=Character->GetCharacterMovement();
 WallJumpSavedAirControl=Move->AirControl;WallJumpSavedFallingBraking=Move->BrakingDecelerationFalling;WallJumpSavedFallingFriction=Move->FallingLateralFriction;
 bWallJumpPhysicsSaved=true;
 // Falling braking otherwise stops an unsteered kick while held input bypasses it.
 Move->AirControl=0;Move->BrakingDecelerationFalling=0;Move->FallingLateralFriction=0;
 Character->LaunchCharacter(Velocity,true,true);Character->SetActorRotation(Velocity.GetSafeNormal2D().Rotation());
 bAirJumpUsed=false;ParkourIgnoreUntil=GetWorld()->GetTimeSeconds()+ParkourRegrabDelay;WallJumpStartedAt=GetWorld()->GetTimeSeconds();WallJumpAge=0;
 if(Presentation) SpawnSmoke(Presentation->WireWallSmoke,Point,N,.6f);return true;
}
bool UBBSBTraversalComponent::IsWallJumpSteeringLocked() const
{
 return bWallJumpPhysicsSaved && GetWorld() && GetWorld()->GetTimeSeconds()-WallJumpStartedAt<FMath::Max(0.f,WallJumpSteeringLockSeconds);
}
void UBBSBTraversalComponent::RestoreWallJumpMovement()
{
 if(!bWallJumpPhysicsSaved) return;
 if(IsValid(Character))
 {
  auto* Move=Character->GetCharacterMovement();
  Move->AirControl=WallJumpSavedAirControl;Move->BrakingDecelerationFalling=WallJumpSavedFallingBraking;Move->FallingLateralFriction=WallJumpSavedFallingFriction;
 }
 bWallJumpPhysicsSaved=false;
}
void UBBSBTraversalComponent::TickWallJumpMovement()
{
 if(!bWallJumpPhysicsSaved || !Character) return;
 auto* Move=Character->GetCharacterMovement();
 const float Age=GetWorld()->GetTimeSeconds()-WallJumpStartedAt;
 if(!Move->IsFalling() || Age>=FMath::Max(WallJumpMomentumSeconds,WallJumpSteeringLockSeconds) ||
    (Character->FieldActions && !Character->FieldActions->CanMove()))
 {
  RestoreWallJumpMovement();return;
 }
 const bool Locked=IsWallJumpSteeringLocked();
 Move->AirControl=Locked ? 0.f:WallJumpSavedAirControl;
 if(Locked) Character->ConsumeMovementInputVector();
}
FVector UBBSBTraversalComponent::LedgeHand(bool Left) const
{
 const FVector Right=FVector::CrossProduct(FVector::UpVector,-LedgeNormal).GetSafeNormal();
 return LedgePoint+LedgeNormal*3.f+Right*(Left ? -15.f:15.f)+FVector::UpVector*1.f;
}
void UBBSBTraversalComponent::TickParkour(float DT)
{
 if(!IsParkourActive()) return;
 const auto* PC=Cast<APlayerController>(Character->GetController());const auto State=Character->FieldActions ? Character->FieldActions->ActiveState:EBBSBActionState::Idle;
 if(!PC || !PC->IsLocalController() || PC->IsMoveInputIgnored() || !ParkourSurface.IsValid() || !ValidParkourSurface(ParkourSurface.Get()) ||
  State==EBBSBActionState::Hit || State==EBBSBActionState::Dead || State==EBBSBActionState::Transition || (Character->Combat && Character->Combat->BlocksMovement()) ||
  (Character->Interaction && Character->Interaction->bDialogueActive) || (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive())) { CancelParkour();return; }
 const FVector Intent=Character->FieldActions ? Character->FieldActions->MovementIntent():FVector::ZeroVector;
 if(!Intent.IsNearlyZero() && FVector::DotProduct(Intent,LedgeNormal)>.35f) { DropParkour();return; }
 ParkourAge=GetWorld()->GetTimeSeconds()-ParkourStartedAt;
 if(ParkourMode==EBBSBParkourMode::WallSlide) { Character->ConsumeMovementInputVector();TickWallSlide(DT);return; }
 if(bParkourJumpPlanned) { TickParkourJump(DT);return; }
 if(ParkourMode!=EBBSBParkourMode::LedgeHang) { CancelParkour();return; }
 auto* Move=Character->GetCharacterMovement();Move->GravityScale=0;Move->Velocity=FVector::ZeroVector;Character->ConsumeMovementInputVector();
 const FTransform Frame=ParkourSurface->GetComponentTransform();LedgePoint=Frame.TransformPosition(LedgeLocalPoint);LedgeNormal=Frame.TransformVectorNoScale(LedgeLocalNormal).GetSafeNormal2D();ParkourTo=Frame.TransformPosition(LedgeLocalStand);
 const FVector Goal=FMath::Lerp(ParkourFrom,LedgePoint+LedgeNormal*(Character->GetCapsuleComponent()->GetScaledCapsuleRadius()+9.f)-FVector(0,0,LedgeHangBelow),Ease(ParkourAge/.12f));
 ParkourHandAlpha=1.f;ClimbFootAlpha=0;FHitResult Hit;Character->SetActorLocation(Goal,true,&Hit,ETeleportType::None);
 Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(),(-LedgeNormal).Rotation(),DT,18.f));
 if(Hit.bBlockingHit || FVector::DistSquared(Character->GetActorLocation(),Goal)>FMath::Square(4.f)) { CancelParkour();return; }
 if(ParkourAge>=.12f && (bClimbQueued || FVector::DotProduct(Intent,-LedgeNormal)>.5f))
 { bClimbQueued=false;TryLedgeClimb(); }
}

void UBBSBTraversalComponent::ApplyClimbCamera(float DT,FMinimalViewInfo& View)
{
 if(!Character || (!IsParkourActive() && FMath::Abs(ClimbCameraZ)<.01f)) { ClimbCameraZ=0;return; }
 float Target=0;
 if(ParkourMode==EBBSBParkourMode::Mantle)
 {
  const auto* Mesh=Character->GetMesh();float NeutralPelvis=4.f;
  if(const auto* Asset=Mesh->GetSkeletalMeshAsset())
  {
   const auto& Ref=Asset->GetRefSkeleton();FTransform Pose=FTransform::Identity;
   for(int32 Bone=Ref.FindBoneIndex(TEXT("pelvis"));Bone!=INDEX_NONE;Bone=Ref.GetParentIndex(Bone)) Pose=Pose*Ref.GetRefBonePose()[Bone];
   NeutralPelvis=Mesh->GetRelativeTransform().TransformPosition(Pose.GetLocation()).Z;
  }
  const float Phase=FMath::Clamp(ParkourAge/FMath::Max(.1f,ParkourDuration),0.f,1.f);
  const float RelativePelvis=Mesh->GetSocketLocation(TEXT("pelvis")).Z-Character->GetActorLocation().Z;
  Target=FMath::Clamp((RelativePelvis-FMath::Lerp(ClimbStartPelvisZ,NeutralPelvis,Ease(Phase)))*ClimbCameraFollow,-120.f,0.f);
 }
 if(ClimbCameraFrame!=GFrameCounter)
 {
  ClimbCameraFrame=GFrameCounter;ClimbCameraZ=FMath::FInterpTo(ClimbCameraZ,Target,DT,ClimbCameraBlendSpeed);
 }
 if(FMath::Abs(ClimbCameraZ)<.01f) return;
 const auto* Boom=Character->GetCameraBoom();const FVector Start=View.Location,End=Start+FVector::UpVector*ClimbCameraZ;FHitResult Hit;
 View.Location=GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,Boom ? Boom->ProbeChannel.GetValue():ECC_Camera,
  FCollisionShape::MakeSphere(Boom ? Boom->ProbeSize:12.f),ParkourQuery(Character)) ? (Hit.bStartPenetrating ? Start:Hit.Location):End;
}
