#include "Caravan/BBSBBunniarSurface.h"
#include "UI/BBSBGuides.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBLootPickup.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "BBSBCharacter.h"
#include "Character/BBSBCameraRotation.h"
#include "Combat/BBSBCombat.h"
#include "Character/BBSBTraversal.h"
#include "Interaction/BBSBInteraction.h"
#include "Interaction/BBSBInteractable.h"
#include "Interaction/BBSBDragMotion.h"
#include "Camera/CameraTypes.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "SceneView.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"

void ABBSBBunniarDevice::BunniarAttached_Implementation(ABBSBBunniarCompanion* Bunniar,FVector Point,FVector Normal)
{
 SurfaceDragPixels=FVector2D::ZeroVector;
 if (Director) Director->InteractWith(this);
}
void ABBSBBunniarDevice::BunniarDragged_Implementation(ABBSBBunniarCompanion* Bunniar,FVector2D Delta,FVector2D Total) { SurfaceDragPixels=Total; }
void ABBSBBunniarDevice::BunniarReleased_Implementation(ABBSBBunniarCompanion* Bunniar)
{
 bPowered=false;
 if (Bunniar && Bunniar->AttachedDevice==this) Bunniar->AttachedDevice=nullptr;
}

void ABBSBBunniarCompanion::BeginPlay()
{
 Super::BeginPlay();
 if (!AnimationSet) AnimationSet=AnimationLibrary.LoadSynchronous();
 if (AnimationSet && AnimationSet->Mesh) Visual->SetSkeletalMeshAsset(AnimationSet->Mesh);
 Visual->SetAnimInstanceClass(UBBSBBunniarAnimInstance::StaticClass());
}
void ABBSBBunniarCompanion::BeginGlidePresentation()
{
 ReleaseSurface();bGliding=true;bSurfaceReturning=false;
 SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=GetWorld()->GetTimeSeconds();
}
void ABBSBBunniarCompanion::EndGlidePresentation()
{
 bGliding=false;bSurfaceReturning=true;
 SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=GetWorld()->GetTimeSeconds();ReturnTransitSeconds=ReturnTravelSeconds;
}
void ABBSBBunniarCompanion::EndPlay(const EEndPlayReason::Type Reason)
{
 ReleaseSurface(); Super::EndPlay(Reason);
}
void ABBSBBunniarCompanion::LockSurfaceView(APlayerController* Controller)
{
 if(!bEnableSurfaceCameraFocus || bSurfaceViewLocked || !Controller) return;
 if(bSurfaceCameraBlending) BeginSurfaceCameraBlend(Controller);
 SurfaceLookController=Controller;bSurfaceViewLocked=true;SurfaceRetreatGoal=SurfaceCameraRetreat;SurfaceFramingBlockedAt=-100;
 if(const APawn* Pawn=Controller->GetPawn())
  SurfaceOrbitOffset=(Controller->GetControlRotation()-(SurfacePoint-Pawn->GetActorLocation()).Rotation()).GetNormalized();
 // Own one counter entry; releasing this attachment must preserve any other system's look lock.
 Controller->SetIgnoreLookInput(true);Controller->RotationInput=FRotator::ZeroRotator;
}
void ABBSBBunniarCompanion::ReleaseSurfaceView()
{
 if(bSurfaceViewLocked && SurfaceLookController.IsValid())
 {
  // Continue free look from the image actually shown, rather than the boom's orbit basis.
  const auto* Player=Cast<ABBSBCharacter>(SurfaceLookController->GetPawn());
  if(Player && !Player->Combat->IsStageLocked() && !Player->Combat->HasTargetLock() && SurfaceLookController->PlayerCameraManager && SurfaceLookController->GetViewTarget()==Player)
  {
   const UCameraComponent* Camera=Player->GetFollowCamera();
   const FRotator Local=Camera && !Camera->bUsePawnControlRotation ? Camera->GetRelativeRotation():FRotator::ZeroRotator;
   SurfaceLookController->SetControlRotation(BBSBCameraRotation::ControlFromView(SurfaceLookController->PlayerCameraManager->GetCameraRotation(),Local));
   BeginSurfaceCameraBlend(SurfaceLookController.Get());
  }
  SurfaceLookController->SetIgnoreLookInput(false);SurfaceLookController->RotationInput=FRotator::ZeroRotator;
 }
 bSurfaceViewLocked=false;SurfaceLookController.Reset();SurfaceRetreatGoal=0;bSurfaceFocusSuppressed=false;SurfaceResumeStartedAt=-100;
}
void ABBSBBunniarCompanion::BeginSurfaceCameraBlend(APlayerController* Controller)
{
 const APawn* Pawn=Controller ? Controller->GetPawn():nullptr;
 if(!Pawn || !Controller->PlayerCameraManager) return;
 // Preserve the complete rendered eye, including the old boom basis and extra distance.
 // Store it around the pawn in the current control frame so walking and fresh mouse input
 // continue immediately instead of leaving a world-fixed camera behind during recovery.
 SurfaceBlendController=Controller;
 SurfaceBlendViewRotation=Controller->PlayerCameraManager->GetCameraRotation();
 SurfaceBlendLocalEye=Controller->GetControlRotation().Quaternion().UnrotateVector(Controller->PlayerCameraManager->GetCameraLocation()-Pawn->GetActorLocation());
 SurfaceBlendElapsed=0;SurfaceBlendRetreat=SurfaceCameraRetreat;bSurfaceCameraBlending=true;
}
void ABBSBBunniarCompanion::ApplySurfaceCameraBlend(float DeltaTime,FMinimalViewInfo& View)
{
 if(!bSurfaceCameraBlending) { if(!bSurfaceViewLocked) SurfaceCameraRetreat=SurfaceRetreatVelocity=0;return; }
 auto* PC=SurfaceBlendController.Get();
 const auto* Player=PC ? Cast<ABBSBCharacter>(PC->GetPawn()):nullptr;
 if(!Player || Player->Combat->IsStageLocked() || Player->Combat->HasTargetLock() || Player->Combat->bDead ||
  Player->Interaction->bDialogueActive || PC->GetViewTarget()!=Player)
 {
  bSurfaceCameraBlending=false;SurfaceBlendController.Reset();SurfaceCameraRetreat=SurfaceRetreatVelocity=0;return;
 }
 const float Duration=bSurfaceViewLocked && SurfaceResumeStartedAt>=0 ? .16f:FMath::Max(.1f,SurfaceCameraReturnSeconds);
 const float T=FMath::Clamp(SurfaceBlendElapsed/Duration,0.f,1.f);
 // Quintic easing starts and finishes with zero speed and acceleration; there is no long tail.
 const float Weight=1.f-T*T*T*(T*(T*6.f-15.f)+10.f);
 const FQuat Control=PC->GetControlRotation().Quaternion();
 if(!bSurfaceViewLocked)
 {
  // Release may happen after the boom tick. Do not show its stale orbit rotation for a frame.
  const UCameraComponent* Camera=Player->GetFollowCamera();
  const FRotator Destination=(Camera && !Camera->bUsePawnControlRotation ? Control*Camera->GetRelativeRotation().Quaternion():Control).Rotator();
  View.Rotation=BBSBCameraRotation::BlendLevel(SurfaceBlendViewRotation,Destination,1.f-Weight);
  SurfaceCameraRetreat=SurfaceBlendRetreat*Weight;SurfaceRetreatVelocity=0;
 }
 const FVector Start=View.Location;
 const FVector Anchor=Player->GetActorLocation()+Control.RotateVector(SurfaceBlendLocalEye);
 const FVector Desired=FMath::Lerp(Start,Anchor,Weight);
 const USpringArmComponent* Boom=Player->GetCameraBoom();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BunniarFocusReturn),false,Player);Query.AddIgnoredActor(this);
 FHitResult Hit;
 View.Location=GetWorld()->SweepSingleByChannel(Hit,Start,Desired,FQuat::Identity,Boom ? Boom->ProbeChannel.GetValue():ECC_Camera,
  FCollisionShape::MakeSphere(Boom ? Boom->ProbeSize:12.f),Query) ? (Hit.bStartPenetrating ? Start:Hit.Location):Desired;
 SurfaceBlendElapsed+=FMath::Max(0.f,DeltaTime);
 if(T>=1.f) { bSurfaceCameraBlending=false;SurfaceBlendController.Reset(); }
}
bool ABBSBBunniarCompanion::CanMaintainSurfaceAttachment() const
{
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 const auto* Enemy=IsValid(SurfaceComponent) ? Cast<ABBSBInkEnemy>(SurfaceComponent->GetOwner()):nullptr;
 if(!Player || !Player->FieldActions || !Player->Combat || (Enemy && Enemy->bDefeated)) return false;
 if (!BBSBBunniarSurface::CanUse(SurfaceRecipient.Get(), Player)) return false;
 if(const auto* Prop=IsValid(SurfaceComponent) ? Cast<ABBSBCarryObject>(SurfaceComponent->GetOwner()):nullptr;
  Prop && (Prop->bBroken || !SurfaceComponent->IsQueryCollisionEnabled()))return false;
 // Interactive props use their current physical contact, just like ordinary surfaces.
 // The handler actor may be far away from the door leaf or ladder being dragged.
 if(!bWireAnchored && IsValid(SurfaceComponent))
 {
  FVector Closest;
  const float Distance=Enemy ? Enemy->GetCapsuleComponent()->GetClosestPointOnCollision(Player->GetActorLocation()+FVector(0,0,35),Closest):
   FVector::Distance(Player->GetActorLocation(),(SurfaceLocalFrame*SurfaceComponent->GetComponentTransform()).GetLocation());
  if(!FMath::IsFinite(Distance) || Distance<0.f || Distance>Player->FieldActions->WeaponReach+.1f) return false;
 }
 return bSurfaceAttached && IsValid(SurfaceComponent) && Player && Player->FieldActions->InteractionEquipment()==this &&
  (Player->FieldActions->IsGestureChannelHeld(0) || bWireAnchored) && !Player->Combat->bDead &&
  (!bSurfaceViewLocked || !Player->Combat->IsStageLocked()) &&
  (!FSlateApplication::IsInitialized() || FSlateApplication::Get().IsActive());
}
bool ABBSBBunniarCompanion::ApplySurfaceRetreat(float DeltaTime,float Required,FMinimalViewInfo& View)
{
 const auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if(!Player) return false;
 // Reserve room ahead of movement, retain it during this lock, and return only on release.
 // A continuously damped distance replaces the old instantaneous per-frame maximum.
 if(bSurfaceViewLocked) SurfaceRetreatGoal=FMath::Max(SurfaceRetreatGoal,FMath::Min(Player->FieldActions->WeaponReach,Required+(Required>0 ? 25.f:0.f)));
 else SurfaceRetreatGoal=0;
 const float Omega=2.f/FMath::Max(.1f,SurfaceCameraSmoothSeconds);
 const int32 Steps=FMath::Max(1,FMath::CeilToInt(FMath::Max(0.f,DeltaTime)*120.f));
 const float H=FMath::Max(0.f,DeltaTime)/Steps;
 for(int32 I=0;I<Steps;++I)
 {
  const float Offset=SurfaceCameraRetreat-SurfaceRetreatGoal;
  const float V=(SurfaceRetreatVelocity-Omega*(SurfaceRetreatVelocity+Omega*Offset)*H)*FMath::Exp(-Omega*H);
  const float Acceleration=FMath::Max(10.f,SurfaceCameraMaxAcceleration),Speed=FMath::Max(10.f,SurfaceCameraMaxSpeed);
  const float NextV=FMath::Clamp(FMath::Clamp(V,SurfaceRetreatVelocity-Acceleration*H,SurfaceRetreatVelocity+Acceleration*H),-Speed,Speed);
  SurfaceCameraRetreat=FMath::Max(0.f,SurfaceCameraRetreat+(SurfaceRetreatVelocity+NextV)*.5f*H);SurfaceRetreatVelocity=NextV;
 }
 if(SurfaceCameraRetreat<.01f && FMath::Abs(SurfaceRetreatVelocity)<.1f && SurfaceRetreatGoal==0) { SurfaceCameraRetreat=SurfaceRetreatVelocity=0;return false; }
 const USpringArmComponent* Boom=Player->GetCameraBoom();
 const FVector Start=View.Location,Desired=Start-View.Rotation.Vector()*SurfaceCameraRetreat;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BunniarFocusFraming),false,Player);Query.AddIgnoredActor(this);
 FHitResult Hit;
 if(GetWorld()->SweepSingleByChannel(Hit,Start,Desired,FQuat::Identity,Boom ? Boom->ProbeChannel.GetValue():ECC_Camera,
  FCollisionShape::MakeSphere(Boom ? Boom->ProbeSize:12.f),Query))
 {
  View.Location=Hit.bStartPenetrating ? Start:Hit.Location;
  // A low camera can back away along a floor instead of trying to retreat through it.
  FVector Remaining=Desired-View.Location;bool bSlidAlongSurface=false;
  for(int32 Pass=0;Pass<3;++Pass)
  {
   const FVector Slide=FVector::VectorPlaneProject(Remaining,Hit.ImpactNormal);
   if(Slide.SizeSquared()<.01f || (Hit.bStartPenetrating && Hit.PenetrationDepth>1.f)) break;
   const FVector From=View.Location+Hit.ImpactNormal*.1f;FHitResult Next;
   if(!GetWorld()->SweepSingleByChannel(Next,From,From+Slide,FQuat::Identity,Boom ? Boom->ProbeChannel.GetValue():ECC_Camera,FCollisionShape::MakeSphere(Boom ? Boom->ProbeSize:12.f),Query))
   { View.Location=From+Slide;bSlidAlongSurface=true;break; }
   View.Location=Next.bStartPenetrating ? From:Next.Location;Remaining=From+Slide-View.Location;Hit=Next;
  }
  const float Actual=FVector::Distance(Start,View.Location);const bool Blocked=Actual+1.f<SurfaceCameraRetreat;
  // Keep the spring's scalar distance after a successful slide; feeding the
  // shorter projected distance back every frame makes a floor stop its progress.
  if(!bSlidAlongSurface) { SurfaceCameraRetreat=Actual;if(Blocked) SurfaceRetreatVelocity=0; }
  return Blocked;
 }
 View.Location=Desired;return false;
}
bool ABBSBBunniarCompanion::IsPlainSurfaceAttached() const
{
 AActor* Surface=bSurfaceAttached && IsValid(SurfaceComponent) ? SurfaceComponent->GetOwner():nullptr;
 return IsValid(Surface) && !Cast<ABBSBInkEnemy>(Surface) && !BBSBBunniarSurface::Resolve(Surface);
}

bool ABBSBBunniarCompanion::IsSurfaceFocusActive() const
{
 const auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 return bEnableSurfaceCameraFocus && bSurfaceViewLocked && bSurfaceAttached && IsValid(SurfaceComponent) && Player && Player->Combat &&
  !Player->Combat->HasTargetLock() && !Player->Combat->bDead && !Player->Combat->IsStageLocked();
}

void ABBSBBunniarCompanion::ApplySurfaceFocus(float DeltaTime,FMinimalViewInfo& View)
{
 if(!bEnableSurfaceCameraFocus)
 {
  // Disabling framing also relinquishes an already-held look lock, without
  // detaching the companion or changing the camera's free-look view.
  if(bSurfaceViewLocked)ReleaseSurfaceView();
  bSurfaceCameraBlending=false;SurfaceBlendController.Reset();
  SurfaceCameraRetreat=SurfaceRetreatVelocity=SurfaceRetreatGoal=0;
  if(IsPlainSurfaceAttached())if(auto* P=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))
   if(P->FieldActions->bEquipmentPointerCaptured)P->FieldActions->SetEquipmentPointerCapture(false);
  return;
 }
 if(!bSurfaceViewLocked) { ApplySurfaceCameraBlend(DeltaTime,View);return; }
 if(!CanMaintainSurfaceAttachment() || !SurfaceLookController.IsValid()) { ReleaseSurface();ApplySurfaceCameraBlend(DeltaTime,View);return; }
 auto* Player=Cast<ABBSBCharacter>(SurfaceLookController->GetPawn());
 if(!Player) { ReleaseSurface();return; }
 if(Player->Combat->HasTargetLock())
 {
  // Keep the attached point and this focus's input-lock counter, but do not
  // write the primary camera or show the equipment's white focus outline.
  bSurfaceFocusSuppressed=true;bSurfaceCameraBlending=false;SurfaceBlendController.Reset();
  SurfaceCameraRetreat=SurfaceRetreatVelocity=SurfaceRetreatGoal=0;SurfaceFramingBlockedAt=-100;return;
 }
 const bool ResumingFocus=bSurfaceFocusSuppressed;
 if(ResumingFocus)
 {
  bSurfaceFocusSuppressed=false;
  SurfacePoint=(SurfaceLocalFrame*SurfaceComponent->GetComponentTransform()).GetLocation();
  SurfaceOrbitOffset=(SurfaceLookController->GetControlRotation()-(SurfacePoint-Player->GetActorLocation()).Rotation()).GetNormalized();
  SurfaceResumeRotation=SurfaceLookController->PlayerCameraManager ? SurfaceLookController->PlayerCameraManager->GetCameraRotation().Quaternion():View.Rotation.Quaternion();
  SurfaceResumeStartedAt=GetWorld()->GetTimeSeconds();BeginSurfaceCameraBlend(SurfaceLookController.Get());
 }
 const FMinimalViewInfo UnlockedView=View;
 SurfacePoint=(SurfaceLocalFrame*SurfaceComponent->GetComponentTransform()).GetLocation();
 // Drive the boom from a player-relative bearing, not from its own offset eye.
 // Feeding the eye's look-at back into the boom causes an endless orbit near the contact.
 const FVector FromPawn=SurfacePoint-SurfaceLookController->GetPawn()->GetActorLocation();
 FRotator Orbit=(FromPawn.Rotation()+SurfaceOrbitOffset).GetNormalized();
 if(FromPawn.SizeSquared2D()<1.f) Orbit.Yaw=SurfaceLookController->GetControlRotation().Yaw;
 Orbit.Pitch=FMath::ClampAngle(Orbit.Pitch,-85.f,85.f);Orbit.Roll=0;
 SurfaceLookController->SetControlRotation(BBSBCameraRotation::ConstrainControl(FMath::RInterpTo(SurfaceLookController->GetControlRotation(),Orbit,DeltaTime,12.f),Player->GetFollowCamera()->GetRelativeRotation()));
 // CalcCamera supplies this frame's collision-adjusted eye. Final view focus is exact.
 const FVector ToPoint=SurfacePoint-View.Location;
 if(!ToPoint.IsNearlyZero()) View.Rotation=ToPoint.Rotation();
 // Use the engine's actual aspect/FOV policy, including narrow PIE viewports.
 int32 Width=0,Height=0;SurfaceLookController->GetViewportSize(Width,Height);
 const FIntRect Rect(0,0,FMath::Max(Width,1),FMath::Max(Height,1));
 FSceneViewProjectionData Projection;Projection.SetViewRectangle(Rect);
 FMinimalViewInfo ProjectionView=View;
 const ULocalPlayer* LocalPlayer=SurfaceLookController->GetLocalPlayer();
 FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(ProjectionView,LocalPlayer ? LocalPlayer->AspectRatioAxisConstraint:GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint,Rect,Projection);
 FBox Body=Player->GetCapsuleComponent()->Bounds.GetBox();
 if(const USkeletalMeshComponent* Mesh=Player->GetMesh();Mesh && Mesh->IsRegistered()) Body+=Mesh->Bounds.GetBox();
 auto RequiredRetreat=[&](const FMinimalViewInfo& POV,const FBox& Bounds,float Margin)
 {
  const double Safe=1.-2.*FMath::Clamp(Margin,.02f,.25f);
  const double ScaleX=Projection.ProjectionMatrix.M[0][0]/FMath::Max(.1,Safe-FMath::Abs(Projection.ProjectionMatrix.M[2][0]));
  const double ScaleY=Projection.ProjectionMatrix.M[1][1]/FMath::Max(.1,Safe-FMath::Abs(Projection.ProjectionMatrix.M[2][1]));
  double Required=0;const FQuat Rotation=POV.Rotation.Quaternion();
  for(int32 I=0;I<8;++I)
  {
   const FVector Corner((I&1) ? Bounds.Max.X:Bounds.Min.X,(I&2) ? Bounds.Max.Y:Bounds.Min.Y,(I&4) ? Bounds.Max.Z:Bounds.Min.Z);
   const FVector P=Rotation.UnrotateVector(Corner-POV.Location);
   Required=FMath::Max(Required,FMath::Max3(FMath::Abs(P.Y)*ScaleX-P.X,FMath::Abs(P.Z)*ScaleY-P.X,double(POV.GetFinalPerspectiveNearClipPlane()+5)-P.X));
  }
  return float(Required);
 };
 const float Margin=SurfaceFocusScreenMargin+.04f;
 const float Required=RequiredRetreat(View,Body,Margin);
 const FVector Lead=Player->GetVelocity()*FMath::Max(.25f,SurfaceCameraSmoothSeconds*2.f);
 FMinimalViewInfo Future=View;Future.Location+=Lead;Future.Rotation=(SurfacePoint-Future.Location).Rotation();
 const float Predicted=FMath::Min(Player->FieldActions->WeaponReach,RequiredRetreat(Future,Body.ShiftBy(Lead),Margin));
 if(ResumingFocus)
 {
  // The matched resume blend already eases from the primary eye. Reserve its
  // fully framed endpoint now rather than starting a second slow retreat there.
  const float Need=FMath::Max(Required,Predicted);
  SurfaceCameraRetreat=SurfaceRetreatGoal=Need>0.f ? FMath::Min(Player->FieldActions->WeaponReach,Need+25.f):0.f;
  SurfaceRetreatVelocity=0;
 }
 const float PreviousRetreat=SurfaceCameraRetreat;
 const bool CameraBlocked=ApplySurfaceRetreat(DeltaTime,FMath::Min(Player->FieldActions->WeaponReach,FMath::Max(Required,Predicted)),View);
 if(!(SurfacePoint-View.Location).IsNearlyZero()) View.Rotation=(SurfacePoint-View.Location).Rotation();
 const float StillRequired=RequiredRetreat(View,Body,SurfaceFocusScreenMargin);
 if(StillRequired>1.f && (CameraBlocked || Required>Player->FieldActions->WeaponReach))
 {
  const float CorrectedGoal=SurfaceCameraRetreat+StillRequired*1.15f+5.f;
  SurfaceRetreatGoal=FMath::Max(SurfaceRetreatGoal,FMath::Min(Player->FieldActions->WeaponReach,CorrectedGoal));
  const bool Advancing=SurfaceCameraRetreat>PreviousRetreat+.02f;
  if(Advancing) SurfaceFramingBlockedAt=-100;
  else if(SurfaceFramingBlockedAt<0) SurfaceFramingBlockedAt=GetWorld()->GetTimeSeconds();
  if(SurfaceFramingBlockedAt>=0 && GetWorld()->GetTimeSeconds()-SurfaceFramingBlockedAt>.7f)
  {
   // Framing may release the camera; a valid held attachment has its own lifetime.
   ReleaseSurfaceView();View=UnlockedView;ApplySurfaceCameraBlend(DeltaTime,View);return;
  }
 }
 else SurfaceFramingBlockedAt=-100;
 ApplySurfaceCameraBlend(DeltaTime,View);
 if(!bSurfaceViewLocked) return;
 if(!(SurfacePoint-View.Location).IsNearlyZero()) View.Rotation=(SurfacePoint-View.Location).Rotation();
 const float Resume=FMath::Clamp(float((GetWorld()->GetTimeSeconds()-SurfaceResumeStartedAt)/.16),0.f,1.f);
 if(Resume<1.f) View.Rotation=BBSBCameraRotation::BlendLevel(SurfaceResumeRotation.Rotator(),View.Rotation,Resume*Resume*(3.f-2.f*Resume));
 SurfaceLookController->RotationInput=FRotator::ZeroRotator;
}
bool ABBSBBunniarCompanion::TraceSurfaceTravel(FVector From,FVector To,float Radius,FHitResult& Hit,const UPrimitiveComponent* Escape) const
{
 const auto* Player=Cast<ABBSBCharacter>(GetOwner());
 if(!GetWorld() || !Player || !Player->FieldActions)return false;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BunniarSurfaceTravel),false,Player);Query.AddIgnoredActor(this);Query.bFindInitialOverlaps=true;
 // The hidden physics carrier is not the visible Bunniar's surface. Its round
 // selector moves with the mesh and remains a real obstacle for this sweep.
 for(TActorIterator<ABBSBCarryObject> It(GetWorld());It;++It)
  if(It->CarrySurfaceComponent()!=It->Body)Query.AddIgnoredComponent(It->Body.Get());
 if(Escape)Query.AddIgnoredComponent(Escape);
 // After contact, the carried body owns the swept movement. Its own moving
 // contact must never be mistaken for a new obstacle that releases the hold.
 if(bSurfaceAttached && bSurfaceArrived && IsValid(SurfaceComponent))
  if(const auto* Prop=Cast<ABBSBCarryObject>(SurfaceComponent->GetOwner());Prop && Prop->bCarried)
  { Query.AddIgnoredActor(Prop);if(Prop->GetOwner())Query.AddIgnoredActor(Prop->GetOwner()); }
 for(int32 Attempt=0;Attempt<32;++Attempt)
 {
  if(!GetWorld()->SweepSingleByChannel(Hit,From,To,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(Radius),Query))return false;
  const auto* Component=Hit.GetComponent();
  // NPC head/face selectors and pickup volumes are generous pointer targets,
  // not solid geometry. Their query-only shells must not stop visible arrival.
  if(Component && Component->GetCollisionEnabled()==ECollisionEnabled::QueryOnly &&
     Component->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Ignore && Component->GetCollisionResponseToChannel(ECC_Camera)==ECR_Ignore)
  {
   const auto* Carry=Cast<ABBSBCarryObject>(Component->GetOwner());
   if(!Carry || Carry->CarrySurfaceComponent()!=Component) { Query.AddIgnoredComponent(Component);continue; }
  }
  return true;
 }
 return true;
}
bool ABBSBBunniarCompanion::ClipSurfaceHit(FHitResult& Hit) const
{
 // Camera aim chooses the destination; the rendered companion chooses whether
 // that contact is reachable. Never launch from an obstructed ideal rest anchor.
 const FBox Bounds=RestClearanceBounds();const FVector Scale=GetActorScale3D();
 const float Radius=FMath::Max(2.f,float((Bounds.GetExtent()*Scale.GetAbs()).GetMin())*.5f);
 FHitResult Cover;
 if(!TraceSurfaceTravel(GetActorTransform().TransformPosition(Bounds.GetCenter()),Hit.ImpactPoint+Hit.ImpactNormal*(Radius+1.f),Radius,Cover))return true;
 if(!Cover.bStartPenetrating && Cover.GetComponent()==Hit.GetComponent() && FVector::DotProduct(Cover.ImpactNormal,Hit.ImpactNormal)>.5f &&
    FVector::DistSquared(Cover.ImpactPoint,Hit.ImpactPoint)<=FMath::Square(Radius*2.f+2.f))return true;
 Hit=Cover;return false;
}
bool ABBSBBunniarCompanion::SurfaceRestReference(FVector Goal,float Radius,FVector& Home) const
{
 const auto* Player=Cast<ABBSBCharacter>(GetOwner());
 if(!Player || !Player->FieldActions || FVector::DistSquared(Goal,Player->GetActorLocation())>FMath::Square(Player->FieldActions->WeaponReach+25.f))return false;
 // The requested equipment anchor is independent of this companion's last
 // collision correction. Do not advance the resting-pose interpolation cache.
 const FBox Bounds=RestClearanceBounds();
 Home=Player->FieldActions->FieldEquipmentRestTransform().GetLocation()+FVector::UpVector*(Bounds.GetCenter().Z*GetActorScale3D().Z);
 float Recovery=0;
 for(int32 Pass=0;Pass<4;++Pass)
 {
  FHitResult H;if(!TraceSurfaceTravel(Home,Home,Radius,H) || !H.bStartPenetrating)break;
  const float Distance=H.PenetrationDepth+.5f;if(Recovery+Distance>60.f)return false;
  Home+=H.Normal*Distance;Recovery+=Distance;
 }
 FHitResult Start,End,Path;
 return (!TraceSurfaceTravel(Home,Home,Radius,Start) || !Start.bStartPenetrating) &&
  (!TraceSurfaceTravel(Goal,Goal,Radius,End) || !End.bStartPenetrating) && !TraceSurfaceTravel(Home,Goal,Radius,Path);
}
bool ABBSBBunniarCompanion::CanApproachFromRest(const FHitResult& Hit) const
{
 const FBox Bounds=RestClearanceBounds();const FVector Scale=GetActorScale3D();
 const float Radius=FMath::Max(2.f,float((Bounds.GetExtent()*Scale.GetAbs()).GetMin())*.5f);
 FVector Home;return SurfaceRestReference(Hit.ImpactPoint+Hit.ImpactNormal*(Radius+2.f),Radius,Home);
}
float ABBSBBunniarCompanion::SurfaceTravelSpeed(FVector Goal) const
{
 const bool Launch=bSurfaceTargetPending || (bSurfaceAttached && !bAimFollowing && !bSurfaceArrived) ||
  (bAimFollowing && GetWorld()->GetTimeSeconds()-SurfaceTransitAt<ReturnTravelSeconds);
 // Use the same distance/time scale as recall for the outgoing leg. Keep the
 // existing slower limits for later aim-follow motion and obstacle sliding.
 return Launch ? FMath::Max(AimFollowMaximumSpeed,float(FVector::Distance(SurfaceTransitFrom.GetLocation(),Goal))/FMath::Max(.05f,ReturnTravelSeconds)):AimFollowMaximumSpeed;
}
bool ABBSBBunniarCompanion::MoveSurfaceTravel(FVector Goal,const FQuat& Rotation,const FVector* FollowTarget)
{
 const FBox Bounds=RestClearanceBounds();const FVector Scale=GetActorScale3D();
 const float Radius=FMath::Max(2.f,float((Bounds.GetExtent()*Scale.GetAbs()).GetMin())*.5f);
 const FVector Offset=Rotation.RotateVector(Bounds.GetCenter()*Scale),Desired=Goal+Offset,Final=(FollowTarget ? *FollowTarget:Goal)+Offset;
 FVector Center=GetActorTransform().TransformPosition(Bounds.GetCenter());
 const float DT=FMath::Max(0.f,GetWorld()->GetDeltaSeconds());
 float Budget=FMath::Max(float(FVector::Distance(Center,Desired)),DT*120.f);
 if(FollowTarget)Budget=FMath::Min(Budget,FMath::Max(100.f,SurfaceTravelSpeed(*FollowTarget))*DT);
 auto Pose=[&](){ SetActorLocation(Center-Offset);SetActorRotation(Rotation); };
 // Recover mild initial overlaps in small checked steps. Other geometry is
 // still queried, and every step must reduce penetration of the starting part.
 for(int32 Pass=0;Pass<3 && Budget>.001f;++Pass)
 {
  FHitResult H;if(!TraceSurfaceTravel(Center,Center,Radius,H) || !H.bStartPenetrating)break;
  const FVector Escape=Center+H.Normal*FMath::Min(float(H.PenetrationDepth+.5f),FMath::Min(Budget,20.f));FHitResult Other,End;
  if(TraceSurfaceTravel(Center,Escape,Radius,Other,H.GetComponent()))break;
  const bool EndBlocked=TraceSurfaceTravel(Escape,Escape,Radius,End);
  if(EndBlocked && End.bStartPenetrating && (End.GetComponent()!=H.GetComponent() || End.PenetrationDepth>=H.PenetrationDepth-.01f))break;
  Budget-=FVector::Distance(Center,Escape);Center=Escape;
 }
 FVector Home;const bool ReferenceClear=FollowTarget && SurfaceRestReference(Final,Radius,Home);
 FVector End=Desired,PreviousNormal=FVector::ZeroVector;bool Blocked=false;
 for(int32 Pass=0;Pass<4 && Budget>.001f;++Pass)
 {
  const FVector Delta=(End-Center).GetClampedToMaxSize(Budget);if(Delta.IsNearlyZero(.01f))break;
  FHitResult H;
  if(!TraceSurfaceTravel(Center,Center+Delta,Radius,H)) { Center+=Delta;break; }
  Blocked=true;if(H.bStartPenetrating)break;
  const FVector Contact=H.Location+H.Normal*.25f;
  Budget=FMath::Max(0.f,Budget-float(FVector::Distance(Center,Contact)));Center=Contact;
  // Guide along the current surface toward the clear rest-to-aim line. There
  // are no cardinal waypoint scans or long lateral detours to change sides.
  FVector Along=FVector::VectorPlaneProject((ReferenceClear ? FMath::ClosestPointOnSegment(Center,Home,Final):Desired)-Center,H.Normal);
  if(ReferenceClear && Along.SizeSquared()<1.f)Along=FVector::VectorPlaneProject(Home-Center,H.Normal);
  if(!PreviousNormal.IsNearlyZero() && FVector::DotProduct(Along,PreviousNormal)<-.01f)
  {
   const FVector Edge=FVector::CrossProduct(PreviousNormal,H.Normal).GetSafeNormal();Along=Edge*FVector::DotProduct(Along,Edge);
  }
  PreviousNormal=H.Normal;
  const float SlideBudget=FollowTarget ? FMath::Min(Budget,FMath::Max(100.f,AimSurfaceSlideSpeed)*DT):Budget;
  End=Center+Along.GetClampedToMaxSize(SlideBudget);
 }
 Pose();
 // A speed-limited frame still reaches its intermediate point normally; a
 // blocked frame cannot commit any interaction before genuine arrival.
 return !Blocked && Center.Equals(Desired,1.f);
}
bool ABBSBBunniarCompanion::HandleEquipmentGesture(EBBSBFieldGesture Gesture,int32 Channel,float Charge,bool Initial)
{
 if(bOpeningRest && Initial && Gesture==EBBSBFieldGesture::Beat && Channel==0)EndOpeningRest();
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if (!Player || Player->FieldActions->InteractionEquipment()!=this || Player->Combat->IsStageLocked() || Player->FieldActions->bEquipmentInputBlocked) return false;
 if(Player->Traversal && Player->Traversal->IsWireTraversalActive()) return true;
 if(bSurfaceTargetPending && Channel==1)return true;
 auto* Object=bSurfaceAttached && IsValid(SurfaceComponent) ? Cast<ABBSBCarryObject>(SurfaceComponent->GetOwner()):nullptr;
 if (Channel == 1 && Object && Object->bCarried) return true;
 if (Initial && Gesture==EBBSBFieldGesture::Beat)
 {
  if (Channel==0) { if(!AttachFromCursor()) return false; }
  else if(!Object)
  {
   AActor* SurfaceOwner=IsValid(SurfaceComponent) ? SurfaceComponent->GetOwner():nullptr;
   const bool FreeAnchor=bAirHovering || (bSurfaceAttached && SurfaceOwner && (!BBSBBunniarSurface::Resolve(SurfaceOwner) || SurfaceOwner->IsA<ABBSBInkEnemy>()));
   if(FreeAnchor && Player->Traversal && Player->Traversal->TryWireRecall(this)) return true;
   if(!FreeAnchor) ReleaseSurface();
  }
 }
 if (Channel==0 && !bAimFollowing && !bSurfaceTargetPending && bSurfaceAttached && IsValid(SurfaceComponent))
  if (AActor* Target=SurfaceRecipient.Get(); Target && Target->Implements<UBBSBBunniarSurface>())
   IBBSBBunniarSurface::Execute_BunniarGesture(Target,this,Gesture,Player->FieldActions->GestureReleaseDirection(Channel),Initial ? 0.f:Player->FieldActions->GestureHeldSeconds(Channel));
 return true;
}
bool ABBSBBunniarCompanion::AttachFromCursor()
{
 if (bGliding) return false;
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr;
 if (!PC || Player->FieldActions->InteractionEquipment()!=this || bSurfaceAttached || bAirHovering) return false;
 FHitResult Hit;
 if(!Player->FieldActions->TraceEquipmentAim(Hit))return Player->FieldActions->bBunniarShortcutActive ? false:BeginAimFollowFromCursor(PC);
 if(BBSBBunniarSurface::Resolve(Hit.GetActor()) || Cast<ABBSBInkEnemy>(Hit.GetActor()))return BeginInteractionFlight(Hit,PC);
 return Player->FieldActions->bBunniarShortcutActive ? false:BeginAimFollowFromCursor(PC);
}
void ABBSBBunniarCompanion::ClearInteractionFlight()
{
 bSurfaceTargetPending=false;PendingSurfaceComponent.Reset();PendingSurfaceHandler.Reset();PendingHoldDistance=-1.f;
}
bool ABBSBBunniarCompanion::BeginInteractionFlight(const FHitResult& Hit,APlayerController* PC)
{
 auto* Player=Cast<ABBSBCharacter>(PC ? PC->GetPawn():nullptr);auto* Component=Hit.GetComponent();
 AActor* Handler=BBSBBunniarSurface::Resolve(Hit.GetActor());if(!Handler)Handler=Cast<ABBSBInkEnemy>(Hit.GetActor());
 if(!Player || !IsValid(Component) || !IsValid(Handler))return false;
 if (!BBSBBunniarSurface::CanUse(Handler, Player)) return false;
 const auto* Prop=Cast<ABBSBCarryObject>(Hit.GetActor());if(Prop && !Prop->CanBeCarriedBy(Player))return false;
 ClearInteractionFlight();SurfaceRecipient.Reset();
 PendingSurfaceComponent=Component;PendingSurfaceHandler=Handler;
 PendingLocalPoint=Component->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
 PendingLocalNormal=Component->GetComponentQuat().UnrotateVector(Hit.ImpactNormal).GetSafeNormal();
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 if(PC->GetViewTarget()==Player && Player->GetFollowCamera()->IsActive())Eye=Player->GetFollowCamera()->GetComponentLocation();
 if(PC->GetViewTarget()==Player && Player->GetFollowCamera()->IsActive())View=Player->GetFollowCamera()->GetComponentRotation();
 FVector Ray=View.Vector(),MouseOrigin,MouseRay;
 if(PC->bShowMouseCursor && PC->DeprojectMousePositionToWorld(MouseOrigin,MouseRay))Ray=MouseRay.GetSafeNormal();
 PendingHoldDistance=Prop ? FVector::Distance(Eye,Hit.ImpactPoint):-1.f;
 PendingHoldOffset=Prop ? View.UnrotateVector(Hit.ImpactPoint-Eye-Ray*PendingHoldDistance):FVector::ZeroVector;
 PendingStartedAt=PendingProgressAt=GetWorld()->GetTimeSeconds();PendingProgressPosition=GetActorLocation();
 SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=PendingStartedAt;SurfaceArrivalAlpha=0;
 bSurfaceTargetPending=true;bAimFollowing=bSurfaceReturning=bSurfaceArrived=false;
 ReleaseSurfaceView();Player->FieldActions->SetEquipmentPointerCapture(false);
 UpdateInteractionFlight(true);return bSurfaceTargetPending || bSurfaceAttached;
}
void ABBSBBunniarCompanion::UpdateInteractionFlight(bool Initial)
{
 if(!bSurfaceTargetPending)return;
 auto* Player=Cast<ABBSBCharacter>(GetOwner());auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr;
 auto* Component=PendingSurfaceComponent.Get();auto* Handler=PendingSurfaceHandler.Get();
 if(!Player || !PC || !Player->FieldActions || !Player->Combat || Player->Combat->bDead ||
  Player->FieldActions->InteractionEquipment()!=this || (!Initial && !Player->FieldActions->IsGestureChannelHeld(0)) ||
  !IsValid(Component) || !Component->IsQueryCollisionEnabled() || !IsValid(Handler)) { ReleaseSurface();return; }
 AActor* Physical=Component->GetOwner();
 const auto* Prop=Cast<ABBSBCarryObject>(Physical);const auto* Enemy=Cast<ABBSBInkEnemy>(Physical);
 if((Prop && !Prop->CanBeCarriedBy(Player)) || (Enemy && Enemy->bDefeated) ||
  (Enemy ? Physical:BBSBBunniarSurface::Resolve(Physical))!=Handler || !BBSBBunniarSurface::CanUse(Handler, Player)) { ReleaseSurface();return; }
 const FVector Point=Component->GetComponentTransform().TransformPosition(PendingLocalPoint);
 if(FVector::DistSquared(Point,Player->GetActorLocation())>FMath::Square(Player->FieldActions->WeaponReach+.1f)) { ReleaseSurface();return; }
 const double Now=GetWorld()->GetTimeSeconds();
 if(FVector::DistSquared(GetActorLocation(),PendingProgressPosition)>FMath::Square(2.f)) { PendingProgressPosition=GetActorLocation();PendingProgressAt=Now; }
 if(Now-PendingStartedAt>FMath::Max(.5f,InteractionFlightSeconds) || Now-PendingProgressAt>.75) { ReleaseSurface();return; }
 const FVector Normal=Component->GetComponentQuat().RotateVector(PendingLocalNormal).GetSafeNormal();
 FHitResult Hit(Physical,Component,Point,Normal);Hit.bBlockingHit=true;
 const float ClickDistance=PendingHoldDistance;const FVector ClickOffset=PendingHoldOffset;
 if(AttachToHit(Hit,PC))
 {
  if(auto* Carry=Cast<ABBSBCarryObject>(Physical);Carry && ClickDistance>=0)Carry->PreserveCarryClick(ClickDistance,ClickOffset);
  ClearInteractionFlight();return;
 }
 // A blocked send retains the original component and local contact, even if
 // it leaves the cursor ray. A preview never activates the obstacle it touches.
 FHitResult Preview=Hit;ClipSurfaceHit(Preview);if(CanApproachFromRest(Hit))Preview=Hit;
 if(!IsValid(Preview.GetComponent())) { ReleaseSurface();return; }
 FVector Up=FVector::VectorPlaneProject(FVector::UpVector,Preview.ImpactNormal).GetSafeNormal();
 if(Up.IsNearlyZero())Up=FVector::VectorPlaneProject(PC->GetControlRotation().Vector(),Preview.ImpactNormal).GetSafeNormal();
 if(Up.IsNearlyZero())Up=FVector::ForwardVector;
 const FVector Face=AnimationSet ? AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
 const FQuat Model=FRotationMatrix::MakeFromXZ(Visual->GetRelativeTransform().TransformVectorNoScale(Face),Visual->GetRelativeTransform().TransformVectorNoScale(FVector::UpVector)).ToQuat();
 const FQuat Q=FRotationMatrix::MakeFromXZ(Preview.ImpactNormal,Up).ToQuat()*Model.Inverse();
 SurfaceComponent=Preview.GetComponent();SurfacePoint=Preview.ImpactPoint;SurfaceNormal=Preview.ImpactNormal;
 SurfaceLocalFrame=FTransform(Q,SurfacePoint).GetRelativeTransform(SurfaceComponent->GetComponentTransform());
 bSurfaceAttached=true;bAirHovering=false;bSurfaceArrived=false;
}
bool ABBSBBunniarCompanion::HasSurfaceContact(UPrimitiveComponent* Component) const
{
 if(!IsValid(Component))return false;
 const FBox Bounds=RestClearanceBounds();const FVector Scale=GetActorScale3D();
 const FVector Center=GetActorTransform().TransformPosition(Bounds.GetCenter());
 const float Radius=FMath::Max(2.f,float((Bounds.GetExtent()*Scale.GetAbs()).GetMin())*.5f);
 FVector Closest;const float Distance=Component->GetClosestPointOnCollision(Center,Closest);if(Distance<0 || Distance>Radius+3.f)return false;
 FHitResult Other;
 if(TraceSurfaceTravel(Center,Center,Radius,Other,Component) && Other.bStartPenetrating)return false;
 // Contact with this selected body cannot reach through even a thin wall.
 return Distance<=UE_KINDA_SMALL_NUMBER || !TraceSurfaceTravel(Center,Closest,0.f,Other,Component);
}
bool ABBSBBunniarCompanion::AttachToHit(FHitResult Hit,APlayerController* PC)
{
 auto* Player=Cast<ABBSBCharacter>(PC ? PC->GetPawn():nullptr);
 if(!Player || !IsValid(Hit.GetComponent()))return false;
 if (!BBSBBunniarSurface::CanUse(BBSBBunniarSurface::Resolve(Hit.GetActor()), Player)) return false;
 FRotator View=PC->GetControlRotation();
 if(Player->FieldActions->bBunniarShortcutActive && (!IsValid(Hit.GetActor()) || !BBSBBunniarSurface::Resolve(Hit.GetActor())))return false;
 // A moving-base dependency must never point back to its rider through carry/camera ticks.
 if(const auto* Prop=Cast<ABBSBCarryObject>(Hit.GetActor());Prop && !Prop->CanBeCarriedBy(Player))return false;
 UPrimitiveComponent* TargetComponent=Hit.GetComponent();
 const auto* Enemy=Cast<ABBSBInkEnemy>(Hit.GetActor());
 if(Enemy)
 {
  if(Enemy->bDefeated) return false;
  TargetComponent=Enemy->GetCapsuleComponent();
  Hit.ImpactPoint=Enemy->GetActorLocation()+FVector::UpVector*(Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+8.f);
  Hit.ImpactNormal=FVector::UpVector;
 }
 if(auto* NPC=Cast<ABBSBInteractionNPC>(Hit.GetActor())) NPC->ResolveAttachment(TargetComponent,Hit.ImpactPoint,Hit.ImpactNormal,GetActorLocation());
 const FHitResult Requested=Hit;
 if(!ClipSurfaceHit(Hit))
 {
  // A falling selected body can overlap the companion between physics ticks.
  // Genuine contact is a successful catch, while other geometry still blocks it.
  if(!Cast<ABBSBCarryObject>(Requested.GetActor()) || !HasSurfaceContact(TargetComponent))return false;
  Hit=Requested;
 }
 FVector Up=FVector::VectorPlaneProject(FVector::UpVector,Hit.ImpactNormal).GetSafeNormal();
 if (Up.IsNearlyZero()) Up=FVector::VectorPlaneProject(View.Vector(),Hit.ImpactNormal).GetSafeNormal();
 if (Up.IsNearlyZero()) Up=FVector::ForwardVector;
 const FVector Face=AnimationSet ? AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
 const FVector FaceAxis=Visual->GetRelativeTransform().TransformVectorNoScale(Face.IsNearlyZero() ? FVector::RightVector:Face);
 const FQuat ModelFrame=FRotationMatrix::MakeFromXZ(FaceAxis,Visual->GetRelativeTransform().TransformVectorNoScale(FVector::UpVector)).ToQuat();
 const FQuat Rotation=FRotationMatrix::MakeFromXZ(Hit.ImpactNormal,Up).ToQuat()*ModelFrame.Inverse();
 bAimFollowing=false;bAirHovering=false;
 SurfaceComponent=TargetComponent;
 SurfaceLocalFrame=FTransform(Rotation,Hit.ImpactPoint).GetRelativeTransform(SurfaceComponent->GetComponentTransform());
 SurfacePoint=Hit.ImpactPoint; SurfaceNormal=Hit.ImpactNormal; DragPixels=FVector2D::ZeroVector;
 SurfaceTransitFrom=GetActorTransform(); SurfaceTransitAt=GetWorld()->GetTimeSeconds(); SurfaceArrivalAlpha=0; bSurfaceReturning=false; bSurfaceArrived=false; bSurfaceAttached=true;
 Player->FieldActions->SetEquipmentPointerCapture(!Enemy && !Cast<ABBSBCarryObject>(Hit.GetActor()) && !Cast<ABBSBInteractionNPC>(Hit.GetActor()) && (!IsPlainSurfaceAttached() || bEnableSurfaceCameraFocus));
 if(AActor* Target=SurfaceComponent->GetOwner();Target && !Enemy && !BBSBBunniarSurface::Resolve(Target)) LockSurfaceView(PC);
 Player->FieldActions->EquipmentPointerMotion.BindUObject(this,&ABBSBBunniarCompanion::HandleSurfaceDrag);
 SurfaceRecipient=BBSBBunniarSurface::Resolve(SurfaceComponent->GetOwner());
 if (AActor* Target = SurfaceRecipient.Get(); Target && Target->Implements<UBBSBBunniarSurface>())
 {
  IBBSBBunniarSurface::Execute_BunniarAttached(Target, this, SurfacePoint, SurfaceNormal);
 }
 if (auto* Interaction = UBBSBInteractable::Find(SurfaceRecipient.Get()))
 {
  FBBSBInteractContext Context;
  Context.Character = Player;
  Context.Bunniar = this;
  Context.Surface = SurfaceComponent;
  Context.Point = SurfacePoint;
  Context.Normal = SurfaceNormal;
  Context.bHeld = true;
  Interaction->BeginInteraction(Context);
 }
 // Placement never moves Reparado. A separate secondary press commits a wire pull.
 return true;
}
bool ABBSBBunniarCompanion::BeginAimFollowFromCursor(APlayerController* PC)
{
 auto* Player=Cast<ABBSBCharacter>(PC ? PC->GetPawn():nullptr);
 if(!Player || !Player->FieldActions || Player->FieldActions->bBunniarShortcutActive || Player->FieldActions->InteractionEquipment()!=this)return false;
 ReleaseSurfaceView();Player->FieldActions->SetEquipmentPointerCapture(false);
 SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=GetWorld()->GetTimeSeconds();SurfaceArrivalAlpha=0;
 bAimFollowing=true;bSurfaceReturning=bSurfaceArrived=false;DragPixels=FVector2D::ZeroVector;
 UpdateAimFollowing();return bSurfaceAttached || bAirHovering;
}
void ABBSBBunniarCompanion::UpdateAimFollowing()
{
 if(!bAimFollowing)return;
 if(bWireAnchored) { bAimFollowing=false;return; }
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr;
 if(!Player || !PC || Player->FieldActions->InteractionEquipment()!=this) { ReleaseSurface();return; }
 FHitResult Hit;const bool HasHit=Player->FieldActions->TraceEquipmentAim(Hit);bool Reachable=true;
 if(HasHit && (BBSBBunniarSurface::Resolve(Hit.GetActor()) || Cast<ABBSBInkEnemy>(Hit.GetActor())))
 {
  // Drifting from an ordinary surface or open air onto an interactable uses
  // exactly the same saved contact and visible arrival as a direct click.
  if(BeginInteractionFlight(Hit,PC))return;
 }
 if(HasHit)
 {
  const FHitResult Requested=Hit;Reachable=ClipSurfaceHit(Hit);
  // A pending preview may travel around a small obstacle, but target gestures
  // and arrival callbacks stay uncommitted until the real contact is reachable.
  if(!Reachable && CanApproachFromRest(Requested))Hit=Requested;
 }
 if(HasHit)
 {
  if(auto* Loot=Cast<ABBSBLootPickup>(Hit.GetActor());Loot && Loot->IsAvailable() && Reachable)
  {
   if(Player->Hunting->TryBeginCollection(true) && Player->Hunting->bCollectionHeld)
   { bAimFollowing=bSurfaceAttached=bAirHovering=false;SurfaceComponent=nullptr;return; }
  }
 }
 const FVector Face=AnimationSet ? AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
 const FQuat Model=FRotationMatrix::MakeFromXZ(Visual->GetRelativeTransform().TransformVectorNoScale(Face),Visual->GetRelativeTransform().TransformVectorNoScale(FVector::UpVector)).ToQuat();
 if(HasHit && IsValid(Hit.GetComponent()))
 {
  FVector Up=FVector::VectorPlaneProject(FVector::UpVector,Hit.ImpactNormal).GetSafeNormal();
  if(Up.IsNearlyZero())Up=FVector::VectorPlaneProject(PC->GetControlRotation().Vector(),Hit.ImpactNormal).GetSafeNormal();
  if(Up.IsNearlyZero())Up=FVector::ForwardVector;
  const FQuat Q=FRotationMatrix::MakeFromXZ(Hit.ImpactNormal,Up).ToQuat()*Model.Inverse();
  SurfaceComponent=Hit.GetComponent();SurfacePoint=Hit.ImpactPoint;SurfaceNormal=Hit.ImpactNormal;
  SurfaceLocalFrame=FTransform(Q,SurfacePoint).GetRelativeTransform(SurfaceComponent->GetComponentTransform());
  bSurfaceAttached=true;bAirHovering=false;
 }
 else
 {
  FVector Eye,Ray,Point;if(!Player->FieldActions->EquipmentAimRay(Eye,Ray))return;
  Player->FieldActions->ResolveEquipmentAim(Point,Hit);if(Hit.bBlockingHit)Point-=Ray*22.f;
  AirHoverPoint=Point;SurfacePoint=Point;SurfaceNormal=-Ray;SurfaceComponent=nullptr;
  if(!bAirHovering)AirHoverRotation=FRotationMatrix::MakeFromXZ(-Ray,FVector::UpVector).ToQuat()*Model.Inverse();
  bSurfaceAttached=false;bAirHovering=true;
 }
}
bool ABBSBBunniarCompanion::BeginWireAirAnchor(const FVector& Point)
{
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if(!Player || !Player->FieldActions || bGliding || bSurfaceAttached || bAirHovering || AttachedDevice || Point.ContainsNaN()) return false;
 SetOwner(Player);ReleaseSurfaceView();bAimFollowing=false;
 const FVector Ray=(Point-GetActorLocation()).GetSafeNormal();
 const FVector Face=AnimationSet ? AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
 const FQuat ModelFrame=FRotationMatrix::MakeFromXZ(Visual->GetRelativeTransform().TransformVectorNoScale(Face),Visual->GetRelativeTransform().TransformVectorNoScale(FVector::UpVector)).ToQuat();
 AirHoverPoint=Point;AirHoverRotation=FRotationMatrix::MakeFromXZ(-Ray,FVector::UpVector).ToQuat()*ModelFrame.Inverse();
 SurfaceComponent=nullptr;SurfacePoint=Point;SurfaceNormal=-Ray;DragPixels=FVector2D::ZeroVector;
 SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=GetWorld()->GetTimeSeconds();SurfaceArrivalAlpha=0;
 bAirHovering=bWireAnchored=true;bSurfaceReturning=bSurfaceArrived=false;return true;
}

bool ABBSBBunniarCompanion::CanMaintainAirHover() const
{
 const auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 const auto* Controller=Player ? Cast<APlayerController>(Player->GetController()):nullptr;
 return bAirHovering && Player && Controller && Controller->IsLocalController() && Player->FieldActions &&
  (bWireAnchored || FVector::DistSquared(Player->GetActorLocation(),AirHoverPoint)<=FMath::Square(Player->FieldActions->WeaponReach+.1f)) &&
  (Player->FieldActions->InteractionEquipment()==this || bWireAnchored) && (Player->FieldActions->IsGestureChannelHeld(0) || bWireAnchored) &&
  !(Player->Combat && (Player->Combat->bDead || Player->Combat->IsStageLocked())) &&
  (!FSlateApplication::IsInitialized() || FSlateApplication::Get().IsActive());
}
void ABBSBBunniarCompanion::RetargetAttachedSurface(UPrimitiveComponent* Component)
{
 if(!bSurfaceAttached || !IsValid(SurfaceComponent) || !IsValid(Component))return;
 const FTransform Frame=SurfaceLocalFrame*SurfaceComponent->GetComponentTransform();
 SurfaceComponent=Component;SurfaceLocalFrame=Frame.GetRelativeTransform(Component->GetComponentTransform());
}
void ABBSBBunniarCompanion::HandleSurfaceDrag(FVector2D Delta)
{
 if (bAimFollowing || !bSurfaceAttached || !IsValid(SurfaceComponent) || Delta.IsNearlyZero()) return;
 if(!CanMaintainSurfaceAttachment()) { ReleaseSurface();return; }
 if(auto* G=UBBSBGuideSubsystem::Get(this))G->NotifySurfaceMotion(Delta);
 if(auto* Object=Cast<ABBSBCarryObject>(SurfaceComponent->GetOwner())) { Object->MoveFromPointer(Delta);return; }
 DragPixels+=Delta;
 if (AActor* Target = SurfaceRecipient.Get(); IsValid(Target))
 {
  if (auto* Interaction = UBBSBInteractable::Find(Target)) Interaction->ApplyPointerDelta(Delta);
  if (Target->Implements<UBBSBBunniarSurface>()) IBBSBBunniarSurface::Execute_BunniarDragged(Target, this, Delta, DragPixels);
 }
}
void ABBSBBunniarCompanion::ReleaseSurface()
{
 const bool HadPlacement=bSurfaceAttached || bAirHovering || bSurfaceTargetPending;
 AActor* Recipient=SurfaceRecipient.Get();SurfaceRecipient.Reset();ClearInteractionFlight();
 bAimFollowing=false;bWireAnchored=false;bSurfaceArrived=false;ReleaseSurfaceView();
 bSurfaceAttached=false;bAirHovering=false;
 if(HadPlacement)
 {
  SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=GetWorld()->GetTimeSeconds();ReturnTransitSeconds=ReturnTravelSeconds;
  bSurfaceReturning=true;SurfaceArrivalAlpha=0;
 }
 if (IsValid(Recipient))
 {
  if (Recipient->Implements<UBBSBBunniarSurface>()) IBBSBBunniarSurface::Execute_BunniarReleased(Recipient, this);
  if (auto* Interaction = UBBSBInteractable::Find(Recipient)) Interaction->EndInteraction();
 }
 if(AttachedDevice) { AttachedDevice->bPowered=false;AttachedDevice=nullptr; }
 SurfaceComponent=nullptr;
 if(auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))Player->FieldActions->SetEquipmentPointerCapture(false);
}
