#include "Character/BBSBTraversal.h"
#include "BBSBCharacter.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Caravan/BBSBCaravan.h"
#include "Field/BBSBFieldActions.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

FVector UBBSBTraversalComponent::EnemyVaultHead(const ABBSBInkEnemy* Enemy) const
{
 return Enemy->GetActorLocation()+FVector::UpVector*(Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+8.f);
}

FVector UBBSBTraversalComponent::EnemyVaultApproach(const ABBSBInkEnemy* Enemy,const FVector& Forward) const
{
 const auto* Capsule=Character->GetCapsuleComponent();
 const float StandOff=Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius()+Capsule->GetScaledCapsuleRadius()+75.f;
 return EnemyVaultHead(Enemy)-Forward*StandOff+FVector::UpVector*(Capsule->GetScaledCapsuleHalfHeight()+44.f);
}

bool UBBSBTraversalComponent::CanEnemyVaultPath(const ABBSBInkEnemy* Enemy) const
{
 if(!Character || !IsValid(Enemy) || Enemy->bDefeated) return false;
 const auto* Capsule=Character->GetCapsuleComponent();const FVector Start=Character->GetActorLocation(),Head=EnemyVaultHead(Enemy);
 FVector Forward=(Head-Start).GetSafeNormal2D();if(Forward.IsNearlyZero()) Forward=Character->GetActorForwardVector().GetSafeNormal2D();
 const FVector Approach=Start.Z-Capsule->GetScaledCapsuleHalfHeight()<Head.Z+20.f ? EnemyVaultApproach(Enemy,Forward):Start;
 const FVector End=Head+FVector::UpVector*(Capsule->GetScaledCapsuleHalfHeight()+2.f);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BunniarEnemyVaultRoute),false,Character);Q.AddIgnoredActor(Enemy);
 Q.AddIgnoredActor(Character->FieldActions ? Character->FieldActions->BunniarEquipment.Get():nullptr);
 const auto Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),FMath::Max(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-2.f));
 FHitResult Hit;
 if(!Approach.Equals(Start,.1f) && GetWorld()->SweepSingleByObjectType(Hit,Start,Approach,FQuat::Identity,BBSBCombatVisuals::GeometryTypes(),Shape,Q)) return false;
 return !GetWorld()->SweepSingleByObjectType(Hit,Approach,End,FQuat::Identity,BBSBCombatVisuals::GeometryTypes(),Shape,Q);
}

void UBBSBTraversalComponent::LaunchEnemyVault()
{
 auto* Enemy=WireVaultEnemy.Get();
 if(!IsValid(Enemy) || Enemy->bDefeated) { CancelWireRecall();return; }
 const FVector Contact=EnemyVaultHead(Enemy)-FVector::UpVector*8.f;
 const float PullSpeed=FMath::Max(GlideSpeed,WireCurrentSpeed),ReferenceRise=FMath::Clamp(EnemyVaultRiseSpeed,0.f,300.f);
 const float Horizontal=PullSpeed*FMath::Clamp(EnemyVaultForwardSpeedScale,.1f,1.f);
 const float AddedPitch=FMath::DegreesToRadians(FMath::Clamp(EnemyVaultPitchOffsetDegrees,0.f,30.f));
 const float Pitch=FMath::Min(FMath::Atan2(ReferenceRise,PullSpeed)+AddedPitch,FMath::DegreesToRadians(60.f));
 // Halve the forward travel and lift the old launch bearing by ten degrees.
 // A short approach must still gain height rather than losing its small hop.
 const float Rise=FMath::Max(ReferenceRise*(AddedPitch>0.f ? 1.25f:1.f),Horizontal*FMath::Tan(Pitch));
 EnemyVaultExitVelocity=EnemyVaultForward*Horizontal+FVector::UpVector*Rise;
 bWireArrival=false;WireSurface.Reset();WireVaultEnemy.Reset();bEnemyVaultApproach=false;
 RestoreWireMovement(false);auto* Move=Character->GetCharacterMovement();Move->Velocity=EnemyVaultExitVelocity;
 // A small push redirects the pull into forward travel, with no extra double-jump lift.
 bAirJumpUsed=true;BeginGlide(false);DoubleJumpAge=-1;DoubleJumpYaw=0;
 bWireRecovering=true;WireRecoveryAge=0;WireRecoveryStartedAt=GetWorld()->GetTimeSeconds();
 WireRecoveryVelocity=EnemyVaultExitVelocity;WireRecoveryYaw=WireCatchAlpha=0;GlideMomentumSpeed=EnemyVaultExitVelocity.Size2D();
 Move->SetMovementMode(MOVE_Flying);Move->GravityScale=0;Move->AirControl=0;Move->BrakingDecelerationFlying=0;Move->MaxFlySpeed=WireSpeed;Move->Velocity=EnemyVaultExitVelocity;
 if(Presentation) SpawnSmoke(Presentation->WireLandingSmoke ? Presentation->WireLandingSmoke.Get():Presentation->LandingSmoke.Get(),Contact,FVector::UpVector,.7f);
 if(EnemyVaultDamage>0.f)
 {
  FHitResult Hit(Enemy,Enemy->GetCapsuleComponent(),Contact,FVector::UpVector);Hit.bBlockingHit=true;
  UGameplayStatics::ApplyPointDamage(Enemy,EnemyVaultDamage,-FVector::UpVector,Hit,Character->GetController(),Character,UDamageType::StaticClass());
 }
}
