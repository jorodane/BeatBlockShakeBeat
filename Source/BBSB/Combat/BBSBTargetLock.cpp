#include "Combat/BBSBCombat.h"
#include "BBSBCharacter.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Caravan/BBSBCaravan.h"
#include "Field/BBSBFieldActions.h"
#include "Interaction/BBSBInteraction.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "GameFramework/PlayerInput.h"
#include "Framework/Application/SlateApplication.h"
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
 float LockSwitchAlpha(double Now,double StartedAt,float Seconds)
 {
  if(StartedAt<0 || Seconds<=0.f)return 1.f;
  const float T=FMath::Clamp(float((Now-StartedAt)/Seconds),0.f,1.f);
  return T*T*(3.f-2.f*T);
 }
 FVector BlendLockAim(const FVector& From,const FVector& To,float Alpha)
 {
  if(Alpha>=1.f || From.IsNearlyZero() || To.IsNearlyZero())return To;
  // Rotate the focus about the pawn with the same progress as the boom. A
  // straight chord can cross the pawn when targets lie on opposite sides.
  const FQuat Bearing=FQuat::Slerp(From.Rotation().Quaternion(),To.Rotation().Quaternion(),Alpha).GetNormalized();
  return Bearing.GetForwardVector()*FMath::Lerp(From.Size(),To.Size(),Alpha);
 }
 bool TargetVisible(const ACharacter* Player,const ABBSBInkEnemy* Enemy,const FVector& Eye)
 {
  if(!IsValid(Player) || !IsValid(Enemy)) return false;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBTargetLockSight),false,Player);Q.AddIgnoredActor(Enemy);
  if(Enemy->AimOccluder)Q.AddIgnoredActor(Enemy->AimOccluder);
  for(TActorIterator<ABBSBInkProjectile> It(Player->GetWorld());It;++It) Q.AddIgnoredActor(*It);
  FHitResult Hit;
  return !Player->GetWorld()->LineTraceSingleByObjectType(Hit,Eye,Enemy->GetAimPoint(),BBSBCombatVisuals::GeometryTypes(),Q);
 }

 struct FLockCandidate
 {
  ABBSBInkEnemy* Enemy=nullptr;
  FVector2D Screen=FVector2D::ZeroVector;
  double DistanceSquared=0;
 };
 TArray<FLockCandidate> LockCandidates(ACharacter* Player,APlayerController* PC,float Range,const ABBSBInkEnemy* Exclude=nullptr)
 {
  TArray<FLockCandidate> Result;if(!Player || !PC)return Result;
  int32 W=0,H=0;PC->GetViewportSize(W,H);if(W<=0 || H<=0)return Result;
  FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
  for(TActorIterator<ABBSBInkEnemy> It(Player->GetWorld());It;++It)
  {
   if(!IsValid(*It) || It->bDefeated || *It==Exclude)continue;
   const double Distance=FVector::DistSquared(Player->GetActorLocation(),It->GetActorLocation());
   if(Distance>FMath::Square(FMath::Max(100.f,Range)))continue;
   const FVector Direction=(It->GetAimPoint()-Eye).GetSafeNormal();
   if(FVector::DotProduct(Direction,View.Vector())<FMath::Cos(FMath::DegreesToRadians(65.f)))continue;
   FVector2D Screen;
   if(!PC->ProjectWorldLocationToScreen(It->GetAimPoint(),Screen,true) || Screen.X<0 || Screen.Y<0 || Screen.X>W || Screen.Y>H || !TargetVisible(Player,*It,Eye))continue;
   Result.Add({*It,Screen,Distance});
  }
  return Result;
 }
 FVector2D LockCursor(APlayerController* PC)
 {
  int32 W=0,H=0;PC->GetViewportSize(W,H);float X=W*.5f,Y=H*.5f;
  if(PC->bShowMouseCursor && !PC->GetMousePosition(X,Y)) { X=W*.5f;Y=H*.5f; }
  return FVector2D(X,Y);
 }

}

bool UBBSBCombatComponent::HasTargetLock() const
{
 return !bDead && IsValid(FieldLockedTarget) && !FieldLockedTarget->bDefeated;
}

bool UBBSBCombatComponent::IsLockedTargetInAttackRange() const
{
 return HasTargetLock() && IsTargetInAttackRange(FieldLockedTarget);
}

bool UBBSBCombatComponent::IsTargetInAttackRange(const ABBSBInkEnemy* Target) const
{
 if(!Character || !IsValid(Target) || Target->bDefeated) return false;
 const auto* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();if(!Actions) return false;
 const FVector Origin=Character->GetActorLocation()+FVector(0,0,35);
 FVector Closest;const auto* Capsule=Target->GetCapsuleComponent();
 const float SurfaceDistance=Capsule ? Capsule->GetClosestPointOnCollision(Origin,Closest):-1.f;
 const float Distance=SurfaceDistance>=0.f ? SurfaceDistance:FVector::Dist(Origin,Target->GetAimPoint());
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
 LockMouseMotion=FVector2D::ZeroVector;bLockPointerTracked=false;
 FieldLockedTarget=nullptr;TargetOccludedAt=-1;TargetSwitchStartedAt=-100;TargetViewLocalAim=FVector::ZeroVector;
 if(bTargetBoomSaved && Boom)
 {
  Boom->bEnableCameraLag=bTargetOriginalLag;Boom->bEnableCameraRotationLag=bTargetOriginalRotationLag;
 }
 bTargetBoomSaved=false;
}

bool UBBSBCombatComponent::CanSelectTargetLock() const
{
 if(!Character || bDead || IsStageLocked())return false;
 const auto* PC=Cast<APlayerController>(Character->GetController());
 const auto* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();
 const auto* Dialogue=Character->FindComponentByClass<UBBSBInteractionComponent>();
 return PC && PC->IsLocalController() && PC->GetViewTarget()==Character
  && !(Actions && (Actions->bEquipmentWheelOpen || Actions->bEquipmentInputBlocked)) && !(Dialogue && Dialogue->bDialogueActive);
}

ABBSBInkEnemy* UBBSBCombatComponent::NearestTargetToCursor(const ABBSBInkEnemy* Exclude,bool bAwareOnly) const
{
 auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;if(!PC)return nullptr;
 const FVector2D Cursor=LockCursor(PC);ABBSBInkEnemy* Best=nullptr;double BestPixels=MAX_dbl,BestWorld=MAX_dbl;bool BestInRange=false;
 for(const auto& Candidate:LockCandidates(Character,PC,TargetLockRange,Exclude))
 {
  if(bAwareOnly && !Candidate.Enemy->IsAwareOfPlayer(Character))continue;
  const double Pixels=FVector2D::DistSquared(Candidate.Screen,Cursor);
  const bool InRange=FieldLockedTarget && IsTargetInAttackRange(Candidate.Enemy);
  if(!Best || (InRange && !BestInRange) || (InRange==BestInRange && (Pixels<BestPixels || (Pixels==BestPixels && Candidate.DistanceSquared<BestWorld))))
  { Best=Candidate.Enemy;BestPixels=Pixels;BestWorld=Candidate.DistanceSquared;BestInRange=InRange; }
 }
 return Best;
}

void UBBSBCombatComponent::ActivateTargetLock(ABBSBInkEnemy* Target)
{
 if(!IsValid(Target) || Target->bDefeated || !Character)return;
 auto* PC=Cast<APlayerController>(Character->GetController());if(!PC)return;
 const FVector Bearing=Target->GetAimPoint()-Character->GetActorLocation();
 const bool Switching=FieldLockedTarget && FieldLockedTarget!=Target;
 if(Switching)
 {
  // Keep the acquisition's pawn-to-target composition. Rebasing its offset
  // against every new target leaves the eye behind while only the view turns.
  TargetSwitchFromOrbit=TargetOrbitRotation;
  TargetSwitchFromLocalAim=TargetViewLocalAim.IsNearlyZero() ? Bearing:TargetViewLocalAim;
  TargetSwitchStartedAt=GetWorld()->GetTimeSeconds();
 }
 else
 {
  TargetSwitchStartedAt=-100;
  TargetOrbitRotation=PC->GetControlRotation();
  TargetOrbitOffset=(TargetOrbitRotation-Bearing.Rotation()).GetNormalized();TargetOrbitOffset.Roll=0;
  TargetViewLocalAim=Bearing;
 }
 LockMouseMotion=FVector2D::ZeroVector;LockMouseNextAt=GetWorld()->GetRealTimeSeconds()+FMath::Max(.15f,TargetSwitchMouseCooldown);bLockPointerTracked=false;
 auto* Previous=FieldLockedTarget.Get();FieldLockedTarget=Target;
 if(IsValid(Previous) && Previous!=Target && Previous!=SelectedEnemy)Previous->SetSelected(false);
 Target->SetSelected(true);TargetOccludedAt=-1;PracticeFocusUntil=-100;
 bTargetOrbitHasBearing=Bearing.SizeSquared2D()>FMath::Square(50.f);bTargetReleaseBlending=false;
 if(Boom)
 {
  // A direct handover must not overwrite the original lag settings with the
  // temporary locked settings, or restoring free look would stay disabled.
  if(!bTargetBoomSaved)
  { bTargetOriginalLag=Boom->bEnableCameraLag;bTargetOriginalRotationLag=Boom->bEnableCameraRotationLag;bTargetBoomSaved=true; }
  Boom->bEnableCameraLag=false;Boom->bEnableCameraRotationLag=false;
 }
 SetFeedback(FString::Printf(TEXT("LOCK ON  |  %s"),*Target->EnemyName.ToString()));
}

void UBBSBCombatComponent::ToggleTargetLock()
{
 if(FieldLockedTarget) { ReleaseTargetLock();SetFeedback(TEXT("LOCK OFF"));return; }
 if(!CanSelectTargetLock())return;
 if(auto* Best=NearestTargetToCursor())ActivateTargetLock(Best);
 else SetFeedback(TEXT("No visible target in range"));
}

bool UBBSBCombatComponent::CycleTargetLock(int32 Step)
{
 if(Step==0 || !HasTargetLock() || !CanSelectTargetLock())return false;
 auto* PC=Cast<APlayerController>(Character->GetController());
 auto Candidates=LockCandidates(Character,PC,TargetLockRange);
 // Prefer another enemy that this equipment can actually reach. If none is
 // reachable, keep the normal outside-range cycle instead of trapping the lock.
 if(Candidates.ContainsByPredicate([&](const FLockCandidate& C){ return C.Enemy!=FieldLockedTarget && IsTargetInAttackRange(C.Enemy); }))
  Candidates.RemoveAll([&](const FLockCandidate& C){ return C.Enemy!=FieldLockedTarget && !IsTargetInAttackRange(C.Enemy); });
 Candidates.Sort([](const FLockCandidate& A,const FLockCandidate& B)
 {
  if(A.Screen.X!=B.Screen.X)return A.Screen.X<B.Screen.X;
  if(A.Screen.Y!=B.Screen.Y)return A.Screen.Y<B.Screen.Y;
  return A.Enemy->GetUniqueID()<B.Enemy->GetUniqueID();
 });
 const int32 Current=Candidates.IndexOfByPredicate([&](const FLockCandidate& Candidate){ return Candidate.Enemy==FieldLockedTarget; });
 ABBSBInkEnemy* Next=nullptr;
 if(Current!=INDEX_NONE && Candidates.Num()>1)Next=Candidates[(Current+(Step>0 ? 1:-1)+Candidates.Num())%Candidates.Num()].Enemy;
 else if(Current==INDEX_NONE)Next=NearestTargetToCursor(FieldLockedTarget);
 if(!Next || Next==FieldLockedTarget)return false;
 ActivateTargetLock(Next);return true;
}

bool UBBSBCombatComponent::HandleLockMouseInput(FVector2D ScreenDelta)
{
 const auto* Player=Cast<ABBSBCharacter>(Character);
 const auto* Actions=Player ? Player->FieldActions.Get():nullptr;
 if(!HasTargetLock() || !CanSelectTargetLock() || !Player || Player->Hunting->bInventoryOpen ||
    Player->EquipmentGrowth->bConcertoActive || (Actions && (Actions->IsGestureCaptured() || Actions->bEquipmentPointerCaptured)))
 { LockMouseMotion=FVector2D::ZeroVector;return false; }
 const double Now=GetWorld()->GetRealTimeSeconds();
 if(Now<LockMouseNextAt || ScreenDelta.IsNearlyZero())return false;
 if(Now-LockMouseWindowAt>FMath::Max(.1f,TargetSwitchMouseWindow)) { LockMouseWindowAt=Now;LockMouseMotion=FVector2D::ZeroVector; }
 LockMouseMotion+=ScreenDelta;
 if(LockMouseMotion.Size()<FMath::Max(40.f,TargetSwitchMousePixels))return false;
 const FVector2D Direction=LockMouseMotion.GetSafeNormal();LockMouseMotion=FVector2D::ZeroVector;
 LockMouseNextAt=Now+FMath::Max(.15f,TargetSwitchMouseCooldown);
 auto* PC=Cast<APlayerController>(Character->GetController());FVector2D Origin;
 if(!PC || !PC->ProjectWorldLocationToScreen(FieldLockedTarget->GetAimPoint(),Origin,true))return false;
 int32 W=0,H=0;PC->GetViewportSize(W,H);const double Diagonal=FMath::Max(1.,FVector2D(W,H).Size());
 ABBSBInkEnemy* Best=nullptr;double BestScore=MAX_dbl;
 for(const auto& Candidate:LockCandidates(Character,PC,TargetLockRange,FieldLockedTarget))
 {
  const FVector2D Offset=Candidate.Screen-Origin;const double Distance=Offset.Size();if(Distance<8.)continue;
  const double Along=FVector2D::DotProduct(Direction,Offset/Distance);if(Along<.5)continue;
  const double Score=(1.-Along)*2.+Distance/Diagonal;
  if(Score<BestScore) { Best=Candidate.Enemy;BestScore=Score; }
 }
 if(!Best)return false;ActivateTargetLock(Best);return true;
}

void UBBSBCombatComponent::TickTargetLockMouse()
{
 const auto* Player=Cast<ABBSBCharacter>(Character);auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr;
 if(!PC || !HasTargetLock() || !CanSelectTargetLock() || Player->Hunting->bInventoryOpen ||
    Player->EquipmentGrowth->bConcertoActive || Player->FieldActions->IsGestureCaptured() ||
    (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive()))
 { LockMouseMotion=FVector2D::ZeroVector;bLockPointerTracked=false;return; }
 FVector2D Motion=FVector2D::ZeroVector;
 if(PC->bShowMouseCursor)
 {
  float X,Y;if(PC->GetMousePosition(X,Y))
  { const FVector2D At(X,Y);if(bLockPointerTracked)Motion=At-LockLastPointer;LockLastPointer=At;bLockPointerTracked=true; }
 }
 else
 { bLockPointerTracked=false;if(PC->PlayerInput)Motion=FVector2D(PC->PlayerInput->GetRawKeyValue(EKeys::MouseX),-PC->PlayerInput->GetRawKeyValue(EKeys::MouseY)); }
 HandleLockMouseInput(Motion);
}

void UBBSBCombatComponent::NotifyLockedTargetDefeated(ABBSBInkEnemy* Defeated)
{
 if(!FieldLockedTarget || FieldLockedTarget!=Defeated)return;
 if(bAutoRetargetOnDefeat && CanSelectTargetLock())
  if(auto* Next=NearestTargetToCursor(Defeated,true)) { ActivateTargetLock(Next);return; }
 ReleaseTargetLock();
}

void UBBSBCombatComponent::TickTargetLock()
{
 TickTargetLockMouse();
 // Handles removal/death from another source as well as the immediate damage event.
 if(FieldLockedTarget && (!IsValid(FieldLockedTarget) || FieldLockedTarget->bDefeated))NotifyLockedTargetDefeated(FieldLockedTarget);
 TargetLockCameraLift=FMath::FInterpTo(TargetLockCameraLift,HasTargetLock() ? FMath::Max(0.f,TargetLockCameraHeight):0.f,GetWorld()->GetDeltaSeconds(),12.f);
 if(!FieldLockedTarget) return;
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 const auto* Dialogue=Character ? Character->FindComponentByClass<UBBSBInteractionComponent>():nullptr;
 if(!HasTargetLock() || !PC || PC->GetViewTarget()!=Character || IsStageLocked() || (Dialogue && Dialogue->bDialogueActive) ||
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
 if(TargetSwitchStartedAt>=0)
 {
  const float Alpha=LockSwitchAlpha(GetWorld()->GetTimeSeconds(),TargetSwitchStartedAt,TargetLockSwitchBlendSeconds);
  TargetOrbitRotation=FQuat::Slerp(TargetSwitchFromOrbit.Quaternion(),Desired.Quaternion(),Alpha).GetNormalized().Rotator();
  if(Alpha>=1.f)TargetSwitchStartedAt=-100;
 }
 else TargetOrbitRotation=FMath::RInterpConstantTo(TargetOrbitRotation,FMath::RInterpTo(TargetOrbitRotation,Desired,DT,12.f),DT,180.f);
 auto* Controller=Cast<APlayerController>(Character->GetController());
 Controller->RotationInput=FRotator::ZeroRotator;Controller->SetControlRotation(TargetOrbitRotation);
}

void UBBSBCombatComponent::ApplyTargetLockCamera(FMinimalViewInfo& View)
{
 if(!Character) return;
 auto* PC=Cast<APlayerController>(Character->GetController());if(!PC || PC->GetViewTarget()!=Character) { bTargetReleaseBlending=false;return; }
 if(HasTargetLock())
 {
  const FVector Bearing=FieldLockedTarget->GetAimPoint()-Character->GetActorLocation();
  const float Alpha=LockSwitchAlpha(GetWorld()->GetTimeSeconds(),TargetSwitchStartedAt,TargetLockSwitchBlendSeconds);
  TargetViewLocalAim=BlendLockAim(TargetSwitchFromLocalAim,Bearing,Alpha);
  const FVector Aim=Character->GetActorLocation()+TargetViewLocalAim-View.Location;
  // Compose below the target to keep the player's feet and nearby floor visible.
  // Rendered pitch must not steer the next frame's orbit.
  if(!Aim.IsNearlyZero())
  {
   View.Rotation=Aim.Rotation();
   View.Rotation.Pitch=FMath::ClampAngle(View.Rotation.Pitch-FMath::Clamp(TargetLockLookDownDegrees,0.f,30.f),-85.f,85.f);
  }
  PC->RotationInput=FRotator::ZeroRotator;return;
 }
 if(!bTargetReleaseBlending) return;
 const auto* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();
 const auto* Buddy=Actions ? Cast<ABBSBBunniarCompanion>(Actions->BunniarEquipment):nullptr;
 const auto* Dialogue=Character->FindComponentByClass<UBBSBInteractionComponent>();
 if(bDead || IsStageLocked() || (Buddy && Buddy->bSurfaceViewLocked) || (Actions && Actions->bWeaponAimLocked) || (Dialogue && Dialogue->bDialogueActive)) { bTargetReleaseBlending=false;return; }
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
