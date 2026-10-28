#include "BBSBFieldActions.h"
#include "Combat/BBSBCombat.h"
#include "Caravan/BBSBCaravan.h"
#include "Interaction/BBSBInteraction.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

bool UBBSBFieldActionComponent::EquipFieldEquipment(AActor* Equipment,FName Name)
{
 if (!Character || !IsValid(Equipment) || bDodging || IsGestureCaptured() || CurrentAction.IsSet() || OtherWeaponAction.IsSet() || IsWeaponReturning() || Arbiter.IsBusy(GetWorld()->GetTimeSeconds())) return false;
 const auto* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
 if (Combat && !Combat->CanAttack()) return false;
 CancelCurrentAction(true); Arbiter.Clear(); ActiveState=EBBSBActionState::Idle;
 FieldEquipment=Equipment; FieldEquipmentName=Name;
 if (FieldMetronome) FieldMetronome->Stop();
 UpdateWeaponPresentation();
 return true;
}

void UBBSBFieldActionComponent::ClearFieldEquipment()
{
 if (!FieldEquipment) return;
 if (auto* Buddy=Cast<ABBSBBunniarCompanion>(FieldEquipment)) Buddy->ReleaseSurface();
 FieldEquipment=nullptr; FieldEquipmentName=NAME_None;
 for (AActor* Visual:{FloatingWeapon.Get(),OtherFloatingWeapon.Get(),ConductorWeapon.Get(),OtherConductorWeapon.Get()}) if (Visual) Visual->SetActorHiddenInGame(false);
}

void UBBSBFieldActionComponent::SetEquipmentPointerCapture(bool Capture)
{
 if (Capture==bEquipmentPointerCaptured) return;
 // This flag routes the shared weapon motion to equipment and holds the view.
 // Changing input mode here releases viewport capture during the held gesture.
 bEquipmentPointerCaptured=Capture;
 if (!Capture)
 {
  EquipmentPointerMotion.Unbind();
  EquipmentLookResumeAt=FPlatformTime::Seconds()+.08;
 }
 if (auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr) PC->RotationInput=FRotator::ZeroRotator;
}

FVector UBBSBFieldActionComponent::VisualAimPoint() const
{
 const APlayerController* PC=IsValid(Character) ? Cast<APlayerController>(Character->GetController()):nullptr;
 if (!IsValid(PC) || !PC->IsLocalController()) return FindPrototypeCursorPoint();
 if (const auto* Buddy=Cast<ABBSBBunniarCompanion>(FieldEquipment))
 {
  if(Buddy->bSurfaceAttached) return Buddy->SurfacePoint;
  FVector Point;FHitResult Hit;ResolveEquipmentAim(Point,Hit);return Point;
 }
 return bWeaponAimLocked ? LockedAimPoint:FindPrototypeCursorPoint();
}

FTransform UBBSBFieldActionComponent::FieldEquipmentRestTransform() const
{
 // Keep the requested anchor independent of the previous collision correction.
 // The companion resolves its actual size/orientation once, after choosing this target.
 return GetWeaponRestTransform();
}

bool UBBSBFieldActionComponent::ResolveRestEquipmentPose(FTransform& Pose,const FBox& Bounds,AActor* Equipment,bool bInterpolate,bool bWaiting) const
{
 if(!Character || !GetWorld() || !Bounds.IsValid || !IsValid(Equipment)) return false;
 const float Gap=FMath::Max(2.f,RestEquipmentGap);
 const FVector Scale=Pose.GetScale3D(),HalfSize=Bounds.GetExtent()*Scale.GetAbs(),Extents=HalfSize+FVector(Gap);
 // A turning, round companion needs a stable clearance volume. Rotating an
 // ear-sized box makes a previously safe gap disappear from one frame to the next.
 const FCollisionShape Shape=bInterpolate ? FCollisionShape::MakeSphere(HalfSize.Size()+Gap):FCollisionShape::MakeBox(Extents);
 const FQuat Rotation=Pose.GetRotation();
 const FVector LocalCenter=Bounds.GetCenter()*Scale;
 const FVector Offset=bInterpolate ? FVector(0,0,LocalCenter.Z):Rotation.RotateVector(LocalCenter),Origin=Character->GetActorLocation();
 const FVector Desired=Pose.GetLocation()+Offset;
 const float MaximumDistance=FMath::Max(RestEquipmentDistance+35.f,float(Character->GetCapsuleComponent()->GetScaledCapsuleRadius()+Extents.Size()+Gap));
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBEquipmentRestClearance),false,Character);
 Query.bFindInitialOverlaps=true;
 for(const AActor* Ignored:{Equipment,FloatingWeapon.Get(),OtherFloatingWeapon.Get(),FieldEquipment.Get(),BunniarEquipment.Get()}) if(Ignored) Query.AddIgnoredActor(Ignored);
 auto Clear=[&](const FVector& Center)
 {
  if(Character->GetCapsuleComponent()->OverlapComponent(Center,Rotation,Shape)) return false;
  if(!bInterpolate && Character->GetMesh() && Character->GetMesh()->OverlapComponent(Center,Rotation,Shape)) return false;
  if(GetWorld()->OverlapBlockingTestByChannel(Center,Rotation,ECC_Camera,Shape,Query)) return false;
  FHitResult Obstruction;
  return !bWaiting || (FVector::DistSquared(Center,Origin)<=FMath::Square(MaximumDistance)
   && !GetWorld()->LineTraceSingleByChannel(Obstruction,Origin,Center,ECC_Camera,Query));
 };
 // Resolve local penetration rather than orbiting the equipment around the pawn.
 auto Project=[&](FVector& Center)
 {
  for(int32 Iteration=0;Iteration<8;++Iteration)
  {
   bool Changed=false;
   auto PushOut=[&](UPrimitiveComponent* Body)
   {
    FMTDResult MTD;
    if(Body && Body->OverlapComponent(Center,Rotation,Shape) && Body->ComputePenetration(MTD,Shape,Center,Rotation))
    { Center+=MTD.Direction*(MTD.Distance+.5f);Changed=true; }
   };
   PushOut(Character->GetCapsuleComponent());if(!bInterpolate) PushOut(Character->GetMesh());
   FHitResult Wall;
   if(GetWorld()->SweepSingleByChannel(Wall,Center,Center,Rotation,ECC_Camera,Shape,Query) && Wall.bStartPenetrating)
   { Center+=Wall.Normal*(Wall.PenetrationDepth+.5f);Changed=true; }
   if(Clear(Center)) return true;
   if(!Changed) break;
  }
  return false;
 };
 if(!bWaiting)
 {
  FVector Center=Desired;if(!Project(Center)) return false;
  Pose.SetLocation(Center-Offset);return true;
 }
 const double Now=GetWorld()->GetTimeSeconds();
 if(RestEquipmentPoses.Num()>16) for(auto It=RestEquipmentPoses.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
 auto& State=RestEquipmentPoses.FindOrAdd(Equipment);
 const bool HasHistory=State.UpdatedAt>=0 && Now-State.UpdatedAt<.25;
 const FVector Previous=Origin+State.OwnerOffset+Offset;
 // Sweep continuously along the obstacle instead of pinning the previous safe goal.
 auto Slide=[&](FVector From,const FVector& To,FVector& Result)
 {
  if(!Project(From)) return false;
  FVector End=To;
  for(int32 I=0;I<3;++I)
  {
   FHitResult Hit;
   bool Blocked=GetWorld()->SweepSingleByChannel(Hit,From,End,Rotation,ECC_Camera,Shape,Query);
   for(UPrimitiveComponent* Body:{static_cast<UPrimitiveComponent*>(Character->GetCapsuleComponent()),static_cast<UPrimitiveComponent*>(Character->GetMesh())})
   {
    FHitResult OwnerHit;
    if(Body && (!bInterpolate || Body!=Character->GetMesh()) && Body->SweepComponent(OwnerHit,From,End,Rotation,Shape) && (!Blocked || OwnerHit.Time<Hit.Time)) { Hit=OwnerHit;Blocked=true; }
   }
   if(!Blocked) { From=End;break; }
   if(Hit.bStartPenetrating) { if(!Project(From)) return false;break; }
   From=Hit.Location+Hit.Normal*.5f;
   End=From+FVector::VectorPlaneProject(End-From,Hit.Normal);
  }
  Result=From;return Project(Result);
 };
 FVector Center=Desired;bool Found=Project(Center);
 if(!Found && HasHistory) Found=Slide(Previous,Desired,Center);
 if(!Found && HasHistory) { Center=Previous;Found=Project(Center); }
 if(!Found)
 {
  double Best=MAX_dbl;
  auto Consider=[&](FVector Candidate)
  {
   if(!Project(Candidate)) return;
   const double Score=FVector::DistSquared(Candidate,Desired)+(HasHistory ? 2.*FVector::DistSquared(Candidate,Previous):0);
   if(Score<Best) { Best=Score;Center=Candidate;Found=true; }
  };
  // Prefer a small slide near the last pose before considering another side.
  if(HasHistory) for(float Step:{10.f,20.f,40.f}) for(const FVector& Direction:{FVector::ForwardVector,-FVector::ForwardVector,FVector::RightVector,-FVector::RightVector,FVector::UpVector}) Consider(Previous+Direction*Step);
  FVector Side=(Desired-Origin).GetSafeNormal2D();if(Side.IsNearlyZero()) Side=Character->GetActorRightVector();
  for(float Radius:{RestEquipmentDistance,FMath::Min(MaximumDistance,RestEquipmentDistance+25.f)})
   for(float Height:{float(Desired.Z-Origin.Z),25.f,55.f}) for(int32 Angle=0;Angle<360;Angle+=15)
  {
   Consider(Origin+FRotator(0,Angle,0).RotateVector(Side)*Radius+FVector::UpVector*Height);
  }
 }
 if(!Found) return false;
 const FVector Goal=Center;
 const float DT=FMath::Max(0.f,GetWorld()->GetDeltaSeconds());
 if(bInterpolate || HasHistory)
 {
  const FVector From=HasHistory ? Previous:Equipment->GetActorLocation()+Offset;
  const float Alpha=1.f-FMath::Exp(-(bInterpolate ? 18.f:24.f)*DT);
  // The sword follows the current camera immediately in free space. Only its
  // collision correction eases out; the small companion eases its actual pose.
  FVector Smoothed=Desired+FMath::Lerp(State.CollisionOffset,Goal-Desired,Alpha);
  if(bInterpolate)
  {
   // A companion circles the pawn; a straight chord cuts through the body
   // and makes penetration correction repeatedly stop and release it.
   const FVector A=From-Offset-Origin,B=Goal-Offset-Origin;
   const float Angle=FMath::Atan2(A.Y,A.X)+FMath::FindDeltaAngleRadians(FMath::Atan2(A.Y,A.X),FMath::Atan2(B.Y,B.X))*Alpha;
   const float Radius=FMath::Lerp(A.Size2D(),B.Size2D(),Alpha);
   Smoothed=Origin+Offset+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,FMath::Lerp(A.Z,B.Z,Alpha));
  }
  if((!bInterpolate && Clear(Smoothed)) || Slide(From,Smoothed,Smoothed)) Center=Smoothed;
  else { FVector Safe=From;if(Project(Safe)) Center=Safe; }
 }
 Pose.SetLocation(Center-Offset);
 // Keep the rendered position as history, never an unreached future goal.
 State.OwnerOffset=Center-Offset-Origin;State.CollisionOffset=Center-Desired;State.UpdatedAt=Now;
 return true;
}

bool UBBSBFieldActionComponent::IsEquipmentTarget(AActor* Actor) const
{
 if(!IsValid(Actor)) return false;
 if(const auto* Object=Cast<ABBSBCarryObject>(Actor);Object && Object->bBroken) return false;
 if(FieldEquipment) return FieldEquipment==BunniarEquipment && Actor->Implements<UBBSBBunniarSurface>();
 if(const auto* Enemy=Cast<ABBSBInkEnemy>(Actor)) return !Enemy->bDefeated;
 if(const auto* Prop=Cast<ABBSBBreakableProp>(Actor)) return !Prop->bBroken;
 return Actor->ActorHasTag(TEXT("BBSBAttackable"));
}
bool UBBSBFieldActionComponent::EquipmentAimRay(FVector& Origin,FVector& Direction) const
{
 auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 if(!PC || !PC->IsLocalController() || !GetWorld()) return false;
 FRotator View;PC->GetPlayerViewPoint(Origin,View);Direction=View.Vector();
 if(PC->bShowMouseCursor && PC->DeprojectMousePositionToWorld(Origin,Direction)) { Direction.Normalize();return true; }
 int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
 if(Width>0 && Height>0) PC->DeprojectScreenPositionToWorld(Width*.5f,Height*.5f,Origin,Direction);
 Direction.Normalize();return true;
}
bool UBBSBFieldActionComponent::ResolveEquipmentAim(FVector& Point,FHitResult& Hit) const
{
 Hit=FHitResult();Point=Character ? Character->GetActorLocation():FVector::ZeroVector;
 FVector Origin,Direction;
 if(!EquipmentAimRay(Origin,Direction)) return false;
 const float Reach=FMath::Max(50.f,WeaponReach);
 const FVector Offset=Origin-Character->GetActorLocation();const float B=FVector::DotProduct(Offset,Direction);
 const float Discriminant=B*B-Offset.SizeSquared()+FMath::Square(Reach);
 const float RayDistance=FMath::Max(0.f,-B+(Discriminant>=0 ? FMath::Sqrt(Discriminant):0.f));
 Point=Origin+Direction*RayDistance;
 if(bEquipmentWheelOpen) return false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBEquipmentAim),true,Character);
 for(AActor* Ignored:{FloatingWeapon.Get(),OtherFloatingWeapon.Get(),ConductorWeapon.Get(),OtherConductorWeapon.Get(),BunniarEquipment.Get()}) Q.AddIgnoredActor(Ignored);
 FHitResult CameraHit;
 if(!GetWorld()->LineTraceSingleByChannel(CameraHit,Origin,Origin+Direction*FMath::Max(2500.f,RayDistance),ECC_Visibility,Q) || !CameraHit.GetComponent()) return false;
 if(FVector::DistSquared(Character->GetActorLocation(),CameraHit.ImpactPoint)<=FMath::Square(Reach))
 {
  Hit=CameraHit;Point=Hit.ImpactPoint;return true;
 }
 // An unreachable visible surface may stop the free-air point along this same
 // screen ray. A character-to-point obstruction never substitutes its own point.
 if(CameraHit.Distance<RayDistance) Point=Origin+Direction*FMath::Max(0.f,CameraHit.Distance-22.f);
 return false;
}
bool UBBSBFieldActionComponent::TraceEquipmentAim(FHitResult& Hit) const
{
 FVector Point;
 if(!ResolveEquipmentAim(Point,Hit)) return false;
 if(IsValid(FieldEquipment) && FieldEquipment==BunniarEquipment) return true;
 // Keep the existing character cover check for combat weapons.
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBWeaponCover),true,Character);Q.AddIgnoredActor(FloatingWeapon);Q.AddIgnoredActor(OtherFloatingWeapon);Q.AddIgnoredActor(BunniarEquipment);
 FHitResult Cover;
 return !GetWorld()->LineTraceSingleByChannel(Cover,Character->GetActorLocation()+FVector(0,0,25),Hit.ImpactPoint,ECC_Visibility,Q) || Cover.GetActor()==Hit.GetActor();
}
