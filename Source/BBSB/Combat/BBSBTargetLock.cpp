#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Caravan/BBSBCaravan.h"
#include "Field/BBSBFieldActions.h"
#include "Interaction/BBSBInteraction.h"
#include "Camera/CameraTypes.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
 bool TargetVisible(const ACharacter* Player,const ABBSBInkEnemy* Enemy,const FVector& Eye)
 {
  if(!IsValid(Player) || !IsValid(Enemy)) return false;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBTargetLockSight),false,Player);Q.AddIgnoredActor(Enemy);
  for(TActorIterator<ABBSBInkProjectile> It(Player->GetWorld());It;++It) Q.AddIgnoredActor(*It);
  FHitResult Hit;
  return !Player->GetWorld()->LineTraceSingleByObjectType(Hit,Eye,Enemy->GetAimPoint(),BBSBCombatVisuals::GeometryTypes(),Q);
 }
}

bool UBBSBCombatComponent::HasTargetLock() const
{
 return !bDead && IsValid(FieldLockedTarget) && !FieldLockedTarget->bDefeated;
}

bool UBBSBCombatComponent::IsLockedTargetInAttackRange() const
{
 if(!HasTargetLock() || !Character) return false;
 const auto* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();if(!Actions) return false;
 const FVector Origin=Character->GetActorLocation()+FVector(0,0,35);
 FVector Closest;const auto* Capsule=FieldLockedTarget->GetCapsuleComponent();
 const float SurfaceDistance=Capsule ? Capsule->GetClosestPointOnCollision(Origin,Closest):-1.f;
 const float Distance=SurfaceDistance>=0.f ? SurfaceDistance:FVector::Dist(Origin,FieldLockedTarget->GetAimPoint());
 return Distance<=FMath::Max(100.f,Actions->WeaponReach);
}

void UBBSBCombatComponent::ReleaseTargetLock()
{
 // Continue free look from the displayed direction and keep the old eye while
 // the boom changes basis. Otherwise separating aim from orbit makes unlock snap.
 if(FieldLockedTarget && Character)
  if(auto* PC=Cast<APlayerController>(Character->GetController());PC && PC->PlayerCameraManager && PC->GetViewTarget()==Character)
  {
   const FQuat Control=PC->PlayerCameraManager->GetCameraRotation().Quaternion()*CameraRelativeRotation.Quaternion().Inverse();
   TargetReleaseLocalEye=Control.UnrotateVector(PC->PlayerCameraManager->GetCameraLocation()-Character->GetActorLocation());
   TargetReleaseElapsed=0;bTargetReleaseBlending=true;PC->SetControlRotation(Control.Rotator());
  }
 if(IsValid(FieldLockedTarget) && FieldLockedTarget!=SelectedEnemy) FieldLockedTarget->SetSelected(false);
 FieldLockedTarget=nullptr;TargetOccludedAt=-1;
 if(bTargetBoomSaved && Boom)
 {
  Boom->bEnableCameraLag=bTargetOriginalLag;Boom->bEnableCameraRotationLag=bTargetOriginalRotationLag;
 }
 bTargetBoomSaved=false;
}

void UBBSBCombatComponent::ToggleTargetLock()
{
 if(FieldLockedTarget) { ReleaseTargetLock();SetFeedback(TEXT("LOCK OFF"));return; }
 if(!Character || bDead || IsStageLocked()) return;
 auto* PC=Cast<APlayerController>(Character->GetController());
 const auto* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();
 const auto* Dialogue=Character->FindComponentByClass<UBBSBInteractionComponent>();
 const auto* Buddy=Actions ? Cast<ABBSBBunniarCompanion>(Actions->BunniarEquipment):nullptr;
 if(!PC || !PC->IsLocalController() || (Actions && Actions->bEquipmentWheelOpen) || (Dialogue && Dialogue->bDialogueActive) || (Buddy && Buddy->bSurfaceViewLocked)) return;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 int32 W=0,H=0;PC->GetViewportSize(W,H);if(W<=0 || H<=0) return;
 float MouseX=W*.5f,MouseY=H*.5f;if(PC->bShowMouseCursor) PC->GetMousePosition(MouseX,MouseY);const FVector2D Reticle(MouseX,MouseY);
 ABBSBInkEnemy* Best=nullptr;double Score=MAX_dbl;
 for(TActorIterator<ABBSBInkEnemy> It(GetWorld());It;++It)
 {
  if(It->bDefeated) continue;
  const float Distance=FVector::Dist(Character->GetActorLocation(),It->GetActorLocation());
  if(Distance>FMath::Max(100.f,TargetLockRange)) continue;
  const FVector Direction=(It->GetAimPoint()-Eye).GetSafeNormal();
  if(FVector::DotProduct(Direction,View.Vector())<FMath::Cos(FMath::DegreesToRadians(65.f))) continue;
  FVector2D Screen;
  if(!PC->ProjectWorldLocationToScreen(It->GetAimPoint(),Screen,true) || Screen.X<0 || Screen.Y<0 || Screen.X>W || Screen.Y>H || !TargetVisible(Character,*It,Eye)) continue;
  const double Candidate=FVector2D::DistSquared(Screen,Reticle)/(double(W)*W+double(H)*H)+.015*Distance/FMath::Max(100.f,TargetLockRange);
  if(Candidate<Score) { Score=Candidate;Best=*It; }
 }
 if(!Best) { SetFeedback(TEXT("No visible target in range"));return; }
 FieldLockedTarget=Best;Best->SetSelected(true);TargetOccludedAt=-1;PracticeFocusUntil=-100;
 TargetOrbitRotation=PC->GetControlRotation();
 const FVector Bearing=Best->GetAimPoint()-Character->GetActorLocation();
 TargetOrbitOffset=(TargetOrbitRotation-Bearing.Rotation()).GetNormalized();TargetOrbitOffset.Roll=0;
 bTargetOrbitHasBearing=Bearing.SizeSquared2D()>FMath::Square(50.f);bTargetReleaseBlending=false;
 if(Boom)
 {
  bTargetOriginalLag=Boom->bEnableCameraLag;bTargetOriginalRotationLag=Boom->bEnableCameraRotationLag;bTargetBoomSaved=true;
  Boom->bEnableCameraLag=false;Boom->bEnableCameraRotationLag=false;
 }
 SetFeedback(FString::Printf(TEXT("LOCK ON  |  %s"),*Best->EnemyName.ToString()));
}

void UBBSBCombatComponent::TickTargetLock()
{
 TargetLockCameraLift=FMath::FInterpTo(TargetLockCameraLift,HasTargetLock() ? FMath::Max(0.f,TargetLockCameraHeight):0.f,GetWorld()->GetDeltaSeconds(),12.f);
 if(!FieldLockedTarget) return;
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 const auto* Dialogue=Character ? Character->FindComponentByClass<UBBSBInteractionComponent>():nullptr;
 const auto* Actions=Character ? Character->FindComponentByClass<UBBSBFieldActionComponent>():nullptr;
 const auto* Buddy=Actions ? Cast<ABBSBBunniarCompanion>(Actions->BunniarEquipment):nullptr;
 if(!HasTargetLock() || !PC || PC->GetViewTarget()!=Character || IsStageLocked() || (Dialogue && Dialogue->bDialogueActive) || (Buddy && Buddy->bSurfaceViewLocked) ||
  FVector::DistSquared(Character->GetActorLocation(),FieldLockedTarget->GetActorLocation())>FMath::Square(FMath::Max(TargetLockRange,TargetLockBreakRange)))
 { ReleaseTargetLock();return; }
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 if(TargetVisible(Character,FieldLockedTarget,Eye)) TargetOccludedAt=-1;
 else
 {
  if(TargetOccludedAt<0) TargetOccludedAt=GetWorld()->GetTimeSeconds();
  if(GetWorld()->GetTimeSeconds()-TargetOccludedAt>=TargetLockOcclusionSeconds) { ReleaseTargetLock();return; }
 }
 FieldLockedTarget->SetSelected(true);
 // Orbit from the pawn-to-target bearing, never from the offset camera's look-at.
 // At close range the shoulder offset can exceed the target distance, so the
 // old eye -> control -> eye feedback has no fixed point and circles forever.
 const FVector Bearing=FieldLockedTarget->GetAimPoint()-Character->GetActorLocation();
 const float PlanarDistance=Bearing.Size2D();
 if(PlanarDistance<50.f) bTargetOrbitHasBearing=false;
 else if(PlanarDistance>75.f) bTargetOrbitHasBearing=true;
 FRotator Desired=(Bearing.Rotation()+TargetOrbitOffset).GetNormalized();
 // Above/below the player the bearing is undefined. Retain the orbit side until
 // there is enough separation, then take the shortest bounded turn.
 if(!bTargetOrbitHasBearing) Desired.Yaw=TargetOrbitRotation.Yaw;
 Desired.Pitch=FMath::ClampAngle(Desired.Pitch,-65.f,65.f);Desired.Roll=0;
 const float DT=GetWorld()->GetDeltaSeconds();
 TargetOrbitRotation=FMath::RInterpConstantTo(TargetOrbitRotation,FMath::RInterpTo(TargetOrbitRotation,Desired,DT,12.f),DT,180.f);
 auto* Controller=Cast<APlayerController>(Character->GetController());
 Controller->RotationInput=FRotator::ZeroRotator;Controller->SetControlRotation(TargetOrbitRotation);
}

void UBBSBCombatComponent::ApplyTargetLockCamera(FMinimalViewInfo& View)
{
 if(!Character) return;
 auto* PC=Cast<APlayerController>(Character->GetController());if(!PC || PC->GetViewTarget()!=Character) { bTargetReleaseBlending=false;return; }
 if(HasTargetLock())
 {
  const FVector Aim=FieldLockedTarget->GetAimPoint()-View.Location;
  // Only the rendered view points at the exact target. It must not steer the
  // next frame's boom; movement uses this displayed view separately.
  if(!Aim.IsNearlyZero()) View.Rotation=Aim.Rotation();
  PC->RotationInput=FRotator::ZeroRotator;return;
 }
 if(!bTargetReleaseBlending) return;
 const auto* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();
 const auto* Buddy=Actions ? Cast<ABBSBBunniarCompanion>(Actions->BunniarEquipment):nullptr;
 const auto* Dialogue=Character->FindComponentByClass<UBBSBInteractionComponent>();
 if(bDead || IsStageLocked() || (Buddy && Buddy->bSurfaceViewLocked) || (Dialogue && Dialogue->bDialogueActive)) { bTargetReleaseBlending=false;return; }
 const FQuat Control=PC->GetControlRotation().Quaternion();
 const float T=FMath::Clamp(TargetReleaseElapsed/.2f,0.f,1.f);
 const float Alpha=T*T*T*(T*(T*6.f-15.f)+10.f);
 const FVector Anchor=Character->GetActorLocation()+Control.RotateVector(TargetReleaseLocalEye);
 const FVector Desired=FMath::Lerp(Anchor,View.Location,Alpha);
 if(Boom && Boom->bDoCollisionTest)
 {
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBLockCameraRelease),false,Character);FHitResult Hit;
  const FVector Pivot=Boom->GetComponentLocation()+Boom->TargetOffset;
  View.Location=GetWorld()->SweepSingleByChannel(Hit,Pivot,Desired,FQuat::Identity,Boom->ProbeChannel,
   FCollisionShape::MakeSphere(Boom->ProbeSize),Query) ? Hit.Location:Desired;
 }
 else View.Location=Desired;
 View.Rotation=(Control*CameraRelativeRotation.Quaternion()).Rotator();
 TargetReleaseElapsed+=FMath::Max(0.f,GetWorld()->GetDeltaSeconds());
 if(T>=1.f) bTargetReleaseBlending=false;
}
