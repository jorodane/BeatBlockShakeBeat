#include "Caravan/BBSBBunniarSurface.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "BBSBCharacter.h"
#include "Character/BBSBTraversal.h"
#include "Interaction/BBSBInteraction.h"
#include "Camera/CameraTypes.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
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
 if(bSurfaceViewLocked || !Controller) return;
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
  if(Player && !Player->Combat->IsStageLocked() && SurfaceLookController->PlayerCameraManager && SurfaceLookController->GetViewTarget()==Player)
  {
   FQuat ContinueView=SurfaceLookController->PlayerCameraManager->GetCameraRotation().Quaternion();
   if(const UCameraComponent* Camera=Player->GetFollowCamera();Camera && !Camera->bUsePawnControlRotation)
    ContinueView=ContinueView*Camera->GetRelativeRotation().Quaternion().Inverse();
   SurfaceLookController->SetControlRotation(ContinueView.Rotator());
   BeginSurfaceCameraBlend(SurfaceLookController.Get());
  }
  SurfaceLookController->SetIgnoreLookInput(false);SurfaceLookController->RotationInput=FRotator::ZeroRotator;
 }
 bSurfaceViewLocked=false;SurfaceLookController.Reset();SurfaceRetreatGoal=0;
}
void ABBSBBunniarCompanion::BeginSurfaceCameraBlend(APlayerController* Controller)
{
 const APawn* Pawn=Controller ? Controller->GetPawn():nullptr;
 if(!Pawn || !Controller->PlayerCameraManager) return;
 // Preserve the complete rendered eye, including the old boom basis and extra distance.
 // Store it around the pawn in the current control frame so walking and fresh mouse input
 // continue immediately instead of leaving a world-fixed camera behind during recovery.
 SurfaceBlendController=Controller;
 SurfaceBlendLocalEye=Controller->GetControlRotation().Quaternion().UnrotateVector(Controller->PlayerCameraManager->GetCameraLocation()-Pawn->GetActorLocation());
 SurfaceBlendElapsed=0;SurfaceBlendRetreat=SurfaceCameraRetreat;bSurfaceCameraBlending=true;
}
void ABBSBBunniarCompanion::ApplySurfaceCameraBlend(float DeltaTime,FMinimalViewInfo& View)
{
 if(!bSurfaceCameraBlending) { if(!bSurfaceViewLocked) SurfaceCameraRetreat=SurfaceRetreatVelocity=0;return; }
 auto* PC=SurfaceBlendController.Get();
 const auto* Player=PC ? Cast<ABBSBCharacter>(PC->GetPawn()):nullptr;
 if(!Player || Player->Combat->IsStageLocked() || Player->Combat->bDead ||
  Player->Interaction->bDialogueActive || PC->GetViewTarget()!=Player)
 {
  bSurfaceCameraBlending=false;SurfaceBlendController.Reset();SurfaceCameraRetreat=SurfaceRetreatVelocity=0;return;
 }
 const float T=FMath::Clamp(SurfaceBlendElapsed/FMath::Max(.1f,SurfaceCameraReturnSeconds),0.f,1.f);
 // Quintic easing starts and finishes with zero speed and acceleration; there is no long tail.
 const float Weight=1.f-T*T*T*(T*(T*6.f-15.f)+10.f);
 const FQuat Control=PC->GetControlRotation().Quaternion();
 if(!bSurfaceViewLocked)
 {
  // Release may happen after the boom tick. Do not show its stale orbit rotation for a frame.
  const UCameraComponent* Camera=Player->GetFollowCamera();
  View.Rotation=(Camera && !Camera->bUsePawnControlRotation ? Control*Camera->GetRelativeRotation().Quaternion():Control).Rotator();
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
 const auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if(!bWireAnchored && Player && IsValid(SurfaceComponent) && !SurfaceComponent->GetOwner()->Implements<UBBSBBunniarSurface>() &&
  FVector::DistSquared(Player->GetActorLocation(),(SurfaceLocalFrame*SurfaceComponent->GetComponentTransform()).GetLocation())>FMath::Square(Player->FieldActions->WeaponReach)) return false;
 return bSurfaceAttached && IsValid(SurfaceComponent) && Player && Player->FieldActions->FieldEquipment==this &&
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
void ABBSBBunniarCompanion::ApplySurfaceFocus(float DeltaTime,FMinimalViewInfo& View)
{
 if(!bSurfaceViewLocked) { ApplySurfaceCameraBlend(DeltaTime,View);return; }
 if(!CanMaintainSurfaceAttachment() || !SurfaceLookController.IsValid()) { ReleaseSurface();ApplySurfaceCameraBlend(DeltaTime,View);return; }
 auto* Player=Cast<ABBSBCharacter>(SurfaceLookController->GetPawn());
 if(!Player) { ReleaseSurface();return; }
 const FMinimalViewInfo UnlockedView=View;
 SurfacePoint=(SurfaceLocalFrame*SurfaceComponent->GetComponentTransform()).GetLocation();
 // Drive the boom from a player-relative bearing, not from its own offset eye.
 // Feeding the eye's look-at back into the boom causes an endless orbit near the contact.
 const FVector FromPawn=SurfacePoint-SurfaceLookController->GetPawn()->GetActorLocation();
 FRotator Orbit=(FromPawn.Rotation()+SurfaceOrbitOffset).GetNormalized();
 if(FromPawn.SizeSquared2D()<1.f) Orbit.Yaw=SurfaceLookController->GetControlRotation().Yaw;
 Orbit.Pitch=FMath::ClampAngle(Orbit.Pitch,-85.f,85.f);Orbit.Roll=0;
 SurfaceLookController->SetControlRotation(FMath::RInterpTo(SurfaceLookController->GetControlRotation(),Orbit,DeltaTime,12.f));
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
 SurfaceLookController->RotationInput=FRotator::ZeroRotator;
}
bool ABBSBBunniarCompanion::HandleEquipmentGesture(EBBSBFieldGesture Gesture,int32 Channel,float Charge,bool Initial)
{
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if (!Player || Player->FieldActions->FieldEquipment!=this || Player->Combat->IsStageLocked() || Player->FieldActions->bEquipmentInputBlocked) return false;
 if(Player->Traversal && Player->Traversal->IsWireTraversalActive()) return true;
 auto* Object=bSurfaceAttached && IsValid(SurfaceComponent) ? Cast<ABBSBCarryObject>(SurfaceComponent->GetOwner()):nullptr;
 if(Channel==1 && Object && Object->bCarried)
 {
  if(Initial && Gesture==EBBSBFieldGesture::Beat && Object->BeginManipulation())
  {
   Player->FieldActions->EquipmentPointerMotion.BindUObject(this,&ABBSBBunniarCompanion::HandleSurfaceDrag);
   Player->FieldActions->SetEquipmentPointerCapture(true);
  }
  else if(Gesture==EBBSBFieldGesture::Flick || Gesture==EBBSBFieldGesture::Dive)
  {
   Object->EndManipulation(Gesture==EBBSBFieldGesture::Flick);Player->FieldActions->SetEquipmentPointerCapture(false);
  }
  return true;
 }
 if (Initial && Gesture==EBBSBFieldGesture::Beat)
 {
  if (Channel==0) AttachFromCursor();
  else if(!Object)
  {
   const AActor* SurfaceOwner=IsValid(SurfaceComponent) ? SurfaceComponent->GetOwner():nullptr;
   const bool FreeAnchor=bAirHovering || (bSurfaceAttached && SurfaceOwner && !SurfaceOwner->Implements<UBBSBBunniarSurface>());
   if(FreeAnchor && Player->Traversal && Player->Traversal->TryWireRecall(this)) return true;
   if(!FreeAnchor) ReleaseSurface();
  }
 }
 if (Channel==0 && bSurfaceAttached && IsValid(SurfaceComponent))
  if (AActor* Target=SurfaceComponent->GetOwner();Target && Target->Implements<UBBSBBunniarSurface>())
   IBBSBBunniarSurface::Execute_BunniarGesture(Target,this,Gesture,Player->FieldActions->GestureReleaseDirection(Channel),Initial ? 0.f:Player->FieldActions->GestureHeldSeconds(Channel));
 return true;
}
bool ABBSBBunniarCompanion::AttachFromCursor()
{
 if (bGliding) return false;
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr;
 if (!PC || Player->FieldActions->FieldEquipment!=this || bSurfaceAttached || bAirHovering) return false;
 FRotator View=PC->GetControlRotation();
 FHitResult Hit;if(!Player->FieldActions->TraceEquipmentAim(Hit)) return BeginAirHoverFromCursor(PC);
 UPrimitiveComponent* TargetComponent=Hit.GetComponent();
 if(auto* NPC=Cast<ABBSBInteractionNPC>(Hit.GetActor())) NPC->ResolveAttachment(TargetComponent,Hit.ImpactPoint,Hit.ImpactNormal);
 FVector Up=FVector::VectorPlaneProject(FVector::UpVector,Hit.ImpactNormal).GetSafeNormal();
 if (Up.IsNearlyZero()) Up=FVector::VectorPlaneProject(View.Vector(),Hit.ImpactNormal).GetSafeNormal();
 if (Up.IsNearlyZero()) Up=FVector::ForwardVector;
 const FVector Face=AnimationSet ? AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
 const FVector FaceAxis=Visual->GetRelativeTransform().TransformVectorNoScale(Face.IsNearlyZero() ? FVector::RightVector:Face);
 const FQuat ModelFrame=FRotationMatrix::MakeFromXZ(FaceAxis,Visual->GetRelativeTransform().TransformVectorNoScale(FVector::UpVector)).ToQuat();
 const FQuat Rotation=FRotationMatrix::MakeFromXZ(Hit.ImpactNormal,Up).ToQuat()*ModelFrame.Inverse();
 SurfaceComponent=TargetComponent;
 SurfaceLocalFrame=FTransform(Rotation,Hit.ImpactPoint).GetRelativeTransform(SurfaceComponent->GetComponentTransform());
 SurfacePoint=Hit.ImpactPoint; SurfaceNormal=Hit.ImpactNormal; DragPixels=FVector2D::ZeroVector;
 SurfaceTransitFrom=GetActorTransform(); SurfaceTransitAt=GetWorld()->GetTimeSeconds(); SurfaceArrivalAlpha=0; bSurfaceReturning=false; bSurfaceArrived=false; bSurfaceAttached=true;
 Player->FieldActions->SetEquipmentPointerCapture(!Cast<ABBSBCarryObject>(Hit.GetActor()) && !Cast<ABBSBInteractionNPC>(Hit.GetActor()));
 if(AActor* Target=SurfaceComponent->GetOwner();Target && !Target->Implements<UBBSBBunniarSurface>()) LockSurfaceView(PC);
 Player->FieldActions->EquipmentPointerMotion.BindUObject(this,&ABBSBBunniarCompanion::HandleSurfaceDrag);
 if (AActor* Target=SurfaceComponent->GetOwner(); Target && Target->Implements<UBBSBBunniarSurface>())
  IBBSBBunniarSurface::Execute_BunniarAttached(Target,this,SurfacePoint,SurfaceNormal);
 return true;
}
bool ABBSBBunniarCompanion::BeginAirHoverFromCursor(APlayerController* Controller)
{
 auto* Player=Cast<ABBSBCharacter>(Controller ? Controller->GetPawn():nullptr);
 if(!Player || !Player->FieldActions || Player->FieldActions->FieldEquipment!=this) return false;
 FVector Eye,Ray,Point;FHitResult Hit;
 if(!Player->FieldActions->EquipmentAimRay(Eye,Ray)) return false;
 Player->FieldActions->ResolveEquipmentAim(Point,Hit);
 const FVector Face=AnimationSet ? AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
 const FVector FaceAxis=Visual->GetRelativeTransform().TransformVectorNoScale(Face.IsNearlyZero() ? FVector::RightVector:Face);
 const FQuat ModelFrame=FRotationMatrix::MakeFromXZ(FaceAxis,Visual->GetRelativeTransform().TransformVectorNoScale(FVector::UpVector)).ToQuat();
 AirHoverPoint=Point;AirHoverRotation=FRotationMatrix::MakeFromXZ(-Ray,FVector::UpVector).ToQuat()*ModelFrame.Inverse();
 SurfaceComponent=nullptr;SurfacePoint=Point;SurfaceNormal=-Ray;DragPixels=FVector2D::ZeroVector;
 SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=GetWorld()->GetTimeSeconds();SurfaceArrivalAlpha=0;
 bAirHovering=true;bSurfaceReturning=false;bSurfaceArrived=false;
 Player->FieldActions->SetEquipmentPointerCapture(false);
 return true;
}
bool ABBSBBunniarCompanion::BeginWireAirAnchor(const FVector& Point)
{
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if(!Player || !Player->FieldActions || bGliding || bSurfaceAttached || bAirHovering || AttachedDevice || Point.ContainsNaN()) return false;
 SetOwner(Player);ReleaseSurfaceView();
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
  (Player->FieldActions->FieldEquipment==this || bWireAnchored) && (Player->FieldActions->IsGestureChannelHeld(0) || bWireAnchored) &&
  !(Player->Combat && (Player->Combat->bDead || Player->Combat->IsStageLocked())) &&
  (!FSlateApplication::IsInitialized() || FSlateApplication::Get().IsActive());
}
void ABBSBBunniarCompanion::HandleSurfaceDrag(FVector2D Delta)
{
 if (!bSurfaceAttached || !IsValid(SurfaceComponent) || Delta.IsNearlyZero()) return;
 if(auto* Object=Cast<ABBSBCarryObject>(SurfaceComponent->GetOwner())) { Object->MoveFromPointer(Delta);return; }
 DragPixels+=Delta;
 if (AActor* Target=SurfaceComponent->GetOwner(); Target && Target->Implements<UBBSBBunniarSurface>())
  IBBSBBunniarSurface::Execute_BunniarDragged(Target,this,Delta,DragPixels);
}
void ABBSBBunniarCompanion::ReleaseSurface()
{
 bWireAnchored=false;ReleaseSurfaceView();
 if (!bSurfaceAttached && !bAirHovering) return;
 SurfaceTransitFrom=GetActorTransform(); SurfaceTransitAt=GetWorld()->GetTimeSeconds(); ReturnTransitSeconds=ReturnTravelSeconds; bSurfaceReturning=true; SurfaceArrivalAlpha=0;
 bSurfaceAttached=false;bAirHovering=false;
 if (AActor* Target=IsValid(SurfaceComponent) ? SurfaceComponent->GetOwner():nullptr; IsValid(Target) && Target->Implements<UBBSBBunniarSurface>())
  IBBSBBunniarSurface::Execute_BunniarReleased(Target,this);
 if (AttachedDevice) { AttachedDevice->bPowered=false; AttachedDevice=nullptr; }
 SurfaceComponent=nullptr;
 if (auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0))) Player->FieldActions->SetEquipmentPointerCapture(false);
}
