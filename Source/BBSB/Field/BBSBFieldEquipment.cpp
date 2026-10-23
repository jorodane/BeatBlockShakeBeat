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
 if (!Character || !Character->GetController()) return FVector::ZeroVector;
 if (const auto* Buddy=Cast<ABBSBBunniarCompanion>(FieldEquipment); Buddy && Buddy->bSurfaceAttached) return Buddy->SurfacePoint;
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
bool UBBSBFieldActionComponent::TraceEquipmentAim(FHitResult& Hit) const
{
 auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 if(!PC || bEquipmentWheelOpen) return false;
 FVector Eye,Ray;FRotator View;PC->GetPlayerViewPoint(Eye,View);Ray=View.Vector();
 if(PC->bShowMouseCursor) PC->DeprojectMousePositionToWorld(Eye,Ray);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBEquipmentAim),true,Character);Q.AddIgnoredActor(FloatingWeapon);Q.AddIgnoredActor(OtherFloatingWeapon);Q.AddIgnoredActor(BunniarEquipment);
 if(!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+Ray*2500,ECC_Visibility,Q) || !Hit.GetComponent()) return false;
 if(FVector::Dist(Character->GetActorLocation(),Hit.ImpactPoint)>WeaponReach) return false;
 FHitResult Cover;
 return !GetWorld()->LineTraceSingleByChannel(Cover,Character->GetActorLocation()+FVector(0,0,25),Hit.ImpactPoint,ECC_Visibility,Q) || Cover.GetActor()==Hit.GetActor();
}
