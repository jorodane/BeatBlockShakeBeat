#include "Character/BBSBTraversal.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Field/BBSBFieldActions.h"
#include "Interaction/BBSBInteraction.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

bool UBBSBTraversalComponent::CanWireRecall(ABBSBBunniarCompanion* Buddy) const
{
 if(!Character || WireCharges<=0 || !IsValid(Buddy) || IsWireTraversalActive() || IsBunniarTraversalActive() || IsParkourActive() || !Character->FieldActions || Character->FieldActions->FieldEquipment!=Buddy || Character->FieldActions->IsDodging()) return false;
 const auto* PC=Cast<APlayerController>(Character->GetController());
 if(!PC || !PC->IsLocalController() || PC->IsMoveInputIgnored() || Character->FieldActions->PendingEquipmentId!=INDEX_NONE || (Character->Combat && (!Character->Combat->CanAttack() || Character->Combat->bDead)) || (Character->Interaction && Character->Interaction->bDialogueActive)) return false;
 auto* Surface=Buddy->SurfaceComponent.Get();
 if(!Buddy->bAirHovering && (!Buddy->bSurfaceAttached || !IsValid(Surface) || Surface->GetOwner()->Implements<UBBSBBunniarSurface>())) return false;
 const FVector Anchor=Buddy->bAirHovering ? Buddy->AirHoverPoint:Buddy->SurfacePoint;
 if(FVector::DistSquared(Character->GetActorLocation(),Anchor)>FMath::Square(Character->FieldActions->WeaponReach+100.f)) return false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BunniarWireAvailable),false,Character);Q.AddIgnoredActor(Buddy);
 for(TActorIterator<ABBSBInkProjectile> It(GetWorld());It;++It) Q.AddIgnoredActor(*It);
 FHitResult Hit;
 return !GetWorld()->LineTraceSingleByChannel(Hit,Character->GetActorLocation(),Anchor,ECC_Visibility,Q) || Hit.GetComponent()==Surface;
}

bool UBBSBTraversalComponent::TrySkyWire()
{
 auto* Actions=Character ? Character->FieldActions.Get():nullptr;
 auto* Buddy=Actions ? Cast<ABBSBBunniarCompanion>(Actions->BunniarEquipment):nullptr;
 auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 if(!Actions || WireCharges<=0 || !Buddy || IsWireTraversalActive() || IsParkourActive() || !PC || !PC->IsLocalController() || PC->IsMoveInputIgnored() || Actions->PendingEquipmentId!=INDEX_NONE || Actions->bEquipmentWheelOpen || Actions->bEquipmentInputBlocked || (!Actions->IsDodging() && !Actions->CanMove()) ||
  (Character->Combat && (Character->Combat->bDead || !Character->Combat->CanAttack() || Character->Combat->BlocksMovement())) || (Character->Interaction && Character->Interaction->bDialogueActive)) return false;
 const auto* Move=Character->GetCharacterMovement();
 if(!Move || (!Move->IsMovingOnGround() && !Move->IsFalling())) return false;
 // Prefer the second press's current movement direction; an active dash keeps
 // its heading, while an idle/falling double tap uses the displayed camera.
 FVector Horizontal=Actions->MovementIntent();
 if(Horizontal.IsNearlyZero() && Actions->IsDodging()) Horizontal=Actions->ActiveDashDirection();
 if(Horizontal.IsNearlyZero()) { FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);Horizontal=FRotator(0,View.Yaw,0).Vector(); }
 Horizontal=Horizontal.GetSafeNormal2D();
 if(Horizontal.IsNearlyZero()) return false;
 const float Elevation=FMath::DegreesToRadians(FMath::Clamp(SkyWireElevation,20.f,75.f));
 const FVector Start=Character->GetActorLocation();const float Distance=FMath::Clamp(SkyWireDistance,200.f,1000.f);
 if(const auto* Combat=Character->Combat.Get();Combat && Combat->HasTargetLock())
 {
  FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
  const FVector ViewForward=FRotator(0,View.Yaw,0).Vector();
  const FVector ViewRight=FVector::CrossProduct(FVector::UpVector,ViewForward);
  const float Side=FVector::DotProduct(Horizontal,ViewRight),Forward=FVector::DotProduct(Horizontal,ViewForward);
  const FVector ToTarget=(Combat->FieldLockedTarget->GetActorLocation()-Start).GetSafeNormal2D();
  const float Range=FVector::Dist2D(Start,Combat->FieldLockedTarget->GetActorLocation());
  if(FMath::Abs(Side)>.75f && FMath::Abs(Forward)<.25f && Range>50.f)
  {
   // Aim along a chord rather than the tangent. Keep the straight pull, elevation and recovery unchanged.
   // Include the existing 52 cm stand-off and recovery carry so the whole action stays near its starting range.
   const float RecoveryCarry=FMath::Min(WireAirExitSpeed,WireSpeed)*FMath::Max(.35f,WireRecoverySeconds)*(1.f+FMath::Clamp(WireMomentumRetention,0.f,1.f))*.5f;
   const float Travel=(FMath::Max(0.f,Distance-52.f)+RecoveryCarry)*FMath::Cos(Elevation);
   // At very close range a full-length chord cannot fit. Retain a clear lateral component instead of aiming through the enemy.
   const float Inward=FMath::Clamp(Travel/(2.f*Range),0.f,.85f);
   const FVector TargetRight=FVector::CrossProduct(FVector::UpVector,ToTarget);
   Horizontal=ToTarget*Inward+TargetRight*(FMath::Sign(Side)*FMath::Sqrt(1.f-Inward*Inward));
  }
 }
 const FVector Direction=Horizontal*FMath::Cos(Elevation)+FVector::UpVector*FMath::Sin(Elevation);
 FVector Anchor=Start+Direction*Distance;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BunniarSkyWirePath),false,Character);Q.AddIgnoredActor(Buddy);
 for(TActorIterator<ABBSBInkEnemy> It(GetWorld());It;++It) Q.AddIgnoredActor(*It);
 for(TActorIterator<ABBSBInkProjectile> It(GetWorld());It;++It) Q.AddIgnoredActor(*It);
 const auto* Capsule=Character->GetCapsuleComponent();FHitResult Wall;
 const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),FMath::Max(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-2.f));
 if(GetWorld()->SweepSingleByObjectType(Wall,Start,Anchor,FQuat::Identity,BBSBCombatVisuals::GeometryTypes(),Shape,Q))
 {
  const float FreeDistance=Wall.Distance-15.f;if(Wall.bStartPenetrating || FreeDistance<200.f) return false;
  Anchor=Start+Direction*FreeDistance;
 }
 // Only an accepted, explicitly requested sky wire replaces a free deployed anchor.
 // Empty stock or a blocked route returned above without disturbing placement.
 if(Buddy->bSurfaceAttached || Buddy->bAirHovering)
 {
  const auto* Surface=Buddy->SurfaceComponent.Get();
  if(Surface && Surface->GetOwner()->Implements<UBBSBBunniarSurface>()) return false;
  Buddy->ReleaseSurface();
 }
 EndGlide();
 if(!Buddy->BeginWireAirAnchor(Anchor)) return false;
 return BeginWirePull(Buddy,true);
}
