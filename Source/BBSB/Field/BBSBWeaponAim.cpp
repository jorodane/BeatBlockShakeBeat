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
 return PrototypeWeapon==EBBSBPrototypeWeapon::Legacy || PrototypeWeapon==EBBSBPrototypeWeapon::Greatsword
  || PrototypeWeapon==EBBSBPrototypeWeapon::Spear || (PrototypeWeapon==EBBSBPrototypeWeapon::Hammer && Channel==1);
}

void UBBSBFieldActionComponent::AcquireWeaponAim(int32 Channel)
{
 if (bWeaponAimLocked) return;
 AActor* Target=nullptr;
 LockedAimPoint=FindPrototypeCursorPoint(&Target); LockedAimActor=Target;
 LockedAimLocalPoint=Target ? Target->GetActorTransform().InverseTransformPosition(LockedAimPoint) : LockedAimPoint;
 LockedApproach=(LockedAimPoint-Character->GetActorLocation()).GetSafeNormal2D();
 if (LockedApproach.IsNearlyZero()) LockedApproach=Character->GetActorForwardVector();
 AimLockChannel=Channel; AimLockStartedAt=GetWorld()->GetTimeSeconds(); bWeaponAimLocked=true;
 AimLockWeaponFrom=FloatingWeapon->GetActorTransform();
}

void UBBSBFieldActionComponent::ClearWeaponAim()
{
 bWeaponAimLocked=false; LockedAimActor=nullptr; AimLockChannel=-1;
}

void UBBSBFieldActionComponent::UpdateWeaponAim()
{
 if (!bWeaponAimLocked) return;
 if (IsValid(LockedAimActor))
 {
  const ABBSBInkEnemy* Enemy=Cast<ABBSBInkEnemy>(LockedAimActor);
  const ABBSBBreakableProp* Prop=Cast<ABBSBBreakableProp>(LockedAimActor);
  if ((Enemy && Enemy->bDefeated) || (Prop && Prop->bBroken)) LockedAimActor=nullptr;
  else LockedAimPoint=LockedAimActor->GetActorTransform().TransformPosition(LockedAimLocalPoint);
 }
 if ((AimLockChannel<0 || !Channels[AimLockChannel].bHeld) && !CurrentAction.IsSet() && !OtherWeaponAction.IsSet())
 { ClearWeaponAim(); return; }
 const UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
 if (Combat && Combat->IsStageLocked()) return; // The combat frame always has camera priority.
 APlayerController* PC=Cast<APlayerController>(Character->GetController());
 if (!PC) return;
 FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
 const FVector LocalEye=PC->GetControlRotation().UnrotateVector(Eye-Character->GetActorLocation());
 const FQuat RelativeView=PC->GetControlRotation().Quaternion().Inverse()*View.Quaternion();
 FRotator Goal=PC->GetControlRotation();
 // Recompute the view about the same world anchor when the player moves.
 for (int32 I=0;I<4;++I)
 {
  const FVector NewEye=Character->GetActorLocation()+Goal.RotateVector(LocalEye);
  Goal=((LockedAimPoint-NewEye).Rotation().Quaternion()*RelativeView.Inverse()).Rotator();
  Goal.Pitch=FMath::Clamp(FRotator::NormalizeAxis(Goal.Pitch),-65.f,65.f); Goal.Roll=0;
 }
 PC->SetControlRotation(Goal);
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
