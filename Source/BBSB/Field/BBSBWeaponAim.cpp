#include "BBSBFieldActions.h"
#include "Combat/BBSBCombat.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

bool UBBSBFieldActionComponent::RequiresMouseGesture(int32 Channel) const
{
 if (FieldEquipment) return false;
 return PrototypeWeapon==EBBSBPrototypeWeapon::Legacy || PrototypeWeapon==EBBSBPrototypeWeapon::Greatsword
  || PrototypeWeapon==EBBSBPrototypeWeapon::Spear || (PrototypeWeapon==EBBSBPrototypeWeapon::Hammer && Channel==1);
}

void UBBSBFieldActionComponent::AcquireWeaponAim(int32 Channel)
{
 if (bWeaponAimLocked) return;
 AActor* Target=nullptr;
 const UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
 if (PrototypeWeapon==EBBSBPrototypeWeapon::Legacy && Combat && Combat->IsStageLocked() && IsValid(Combat->SelectedEnemy))
 {
  // Preserve the baseline weapon's selected-target input, captured once per chain.
  LockedAimPoint=FindAimPoint(); Target=Combat->SelectedEnemy;
 }
 else LockedAimPoint=FindPrototypeCursorPoint(&Target);
 LockedAimActor=Target;
 LockedAimLocalPoint=Target ? Target->GetActorTransform().InverseTransformPosition(LockedAimPoint) : LockedAimPoint;
 LockedApproach=(LockedAimPoint-Character->GetActorLocation()).GetSafeNormal2D();
 if (LockedApproach.IsNearlyZero()) LockedApproach=Character->GetActorForwardVector();
 AimLockChannel=Channel; AimLockStartedAt=GetWorld()->GetTimeSeconds(); bWeaponAimLocked=true; bAimCameraNearAnchor=false;
 AimLockWeaponFrom=FloatingWeapon->GetActorTransform();
}

void UBBSBFieldActionComponent::ClearWeaponAim()
{
 ReleaseSpearEmbed();
 bWeaponAimLocked=false; LockedAimActor=nullptr; AimLockChannel=-1; bAimCameraNearAnchor=false;
}

void UBBSBFieldActionComponent::ReleaseSpearEmbed(bool bReturnToRest)
{
 if (bSpearEmbedded && IsValid(FloatingWeapon))
 {
  if (bReturnToRest)
  {
   FollowThroughPose[0]=FloatingWeapon->GetActorTransform(); bHasFollowThrough[0]=true;
   FollowThroughUntil[0]=GetWorld()->GetTimeSeconds()+WeaponReturnSeconds;
  }
  FloatingWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
 }
 bSpearEmbedded=false; SpearPiercedActor.Reset();
}

void UBBSBFieldActionComponent::UpdateWeaponAim()
{
 if (!bWeaponAimLocked) return;
 if (IsValid(LockedAimActor))
 {
  const ABBSBInkEnemy* Enemy=Cast<ABBSBInkEnemy>(LockedAimActor);
  const ABBSBBreakableProp* Prop=Cast<ABBSBBreakableProp>(LockedAimActor);
  if ((Enemy && Enemy->bDefeated) || (Prop && Prop->bBroken)) { ReleaseSpearEmbed(true); LockedAimActor=nullptr; }
  else LockedAimPoint=LockedAimActor->GetActorTransform().TransformPosition(LockedAimLocalPoint);
 }
 if (bSpearEmbedded && !SpearPiercedActor.IsValid()) ReleaseSpearEmbed(true);
 if ((AimLockChannel<0 || !Channels[AimLockChannel].bHeld) && !CurrentAction.IsSet() && !OtherWeaponAction.IsSet())
 { ClearWeaponAim(); return; }
 const UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
 if (Combat && Combat->IsStageLocked()) return; // The combat frame always has camera priority.
 APlayerController* PC=Cast<APlayerController>(Character->GetController());
 if (!PC) return;
 FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
 const FVector LocalEye=PC->GetControlRotation().UnrotateVector(Eye-Character->GetActorLocation());
 // A shoulder camera cannot center an anchor inside its lateral orbit radius.
 // Keep the last view there; rotating the camera to chase that impossible solution
 // makes it orbit the player indefinitely. Hysteresis also prevents edge chatter.
 const float NearRadius=FMath::Max(200.f,float(FMath::Abs(LocalEye.Y))+80.f);
 const float Distance=FVector::Dist2D(Character->GetActorLocation(),LockedAimPoint);
 const FVector ToTarget=LockedAimPoint-Eye;
 if (Distance<=NearRadius) bAimCameraNearAnchor=true;
 if (bAimCameraNearAnchor)
 {
  if (Distance<NearRadius+80.f || FVector::DotProduct(ToTarget.GetSafeNormal(),View.Vector())<=0.1f) return;
  bAimCameraNearAnchor=false;
 }
 if (ToTarget.IsNearlyZero()) return;
 // One bounded correction from the actual view, never a recursive orbit solve.
 // Preserve the captured actor/point even while the close-range camera is held.
 const FRotator Current=PC->GetControlRotation();
 FRotator Goal=Current+(ToTarget.Rotation()-View).GetNormalized();
 Goal.Pitch=FMath::Clamp(FRotator::NormalizeAxis(Goal.Pitch),-65.f,65.f); Goal.Roll=0;
 PC->SetControlRotation(FMath::RInterpConstantTo(Current,Goal,GetWorld()->GetDeltaSeconds(),90.f));
}

void UBBSBFieldActionComponent::DrawAimFeedback(AHUD* HUD) const
{
 if (!HUD || !Character) return;
 APlayerController* PC=Cast<APlayerController>(Character->GetController());
 if (!PC) return;
 const FLinearColor Color(0.62f,0.42f,0.54f,0.95f);
 if (bWeaponAimLocked && !IsValid(LockedAimActor))
 {
  DrawDebugSphere(GetWorld(),LockedAimPoint,AimMarkerRadius,24,Color.ToFColor(true),false,0,0,1.5f);
  return;
 }
}
