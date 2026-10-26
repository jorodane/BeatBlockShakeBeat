#include "BBSBFieldActions.h"
#include "Combat/BBSBCombat.h"
#include "Caravan/BBSBCaravan.h"
#include "Interaction/BBSBInteraction.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

bool UBBSBFieldActionComponent::EquipFieldEquipment(AActor* Equipment,FName Name)
{
 if (!Character || !IsValid(Equipment) || bDodging || IsGestureCaptured() || CurrentAction.IsSet() || OtherWeaponAction.IsSet() || Arbiter.IsBusy(GetWorld()->GetTimeSeconds())) return false;
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
 FTransform Pose=GetWeaponRestTransform();
 if (!Character || !GetWorld()) return Pose;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(FieldEquipmentWall),false,Character);
 Q.AddIgnoredActor(FieldEquipment); Q.AddIgnoredActor(FloatingWeapon); Q.AddIgnoredActor(OtherFloatingWeapon);
 if(const auto* Buddy=Cast<ABBSBBunniarCompanion>(FieldEquipment);Buddy && Buddy->bSurfaceAttached && IsValid(Buddy->SurfaceComponent))
  if(Cast<ABBSBCarryObject>(Buddy->SurfaceComponent->GetOwner())) Q.AddIgnoredActor(Buddy->SurfaceComponent->GetOwner());
 FHitResult Wall;
 if (GetWorld()->SweepSingleByChannel(Wall,Character->GetActorLocation()+FVector(0,0,25),Pose.GetLocation(),FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(35),Q))
  Pose.SetLocation(Wall.bStartPenetrating ? Character->GetActorLocation()+Wall.Normal*(Wall.PenetrationDepth+36):Wall.Location+Wall.Normal*2);
 return Pose;
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
