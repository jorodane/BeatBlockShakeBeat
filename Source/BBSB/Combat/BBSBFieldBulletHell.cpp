#include "BBSBCombat.h"
#include "BBSBCombatVisuals.h"
#include "BBSBFieldAttack.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
 bool FieldSight(ABBSBInkEnemy* Enemy,APawn* Target)
 {
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBFieldAggroSight),false,Enemy);
  Query.AddIgnoredActor(Target);
  for(TActorIterator<ABBSBInkEnemy> It(Enemy->GetWorld());It;++It) Query.AddIgnoredActor(*It);
  FHitResult Hit;
  return !Enemy->GetWorld()->LineTraceSingleByObjectType(Hit,Enemy->GetAimPoint(),Target->GetActorLocation(),BBSBCombatVisuals::GeometryTypes(),Query);
 }
}

void ABBSBInkEnemy::AlertFieldPlayer(APawn* Target)
{
 if(!bFieldBulletHell || bDefeated || !IsValid(Target) || !Target->IsPlayerControlled()) return;
 const auto* Combat=Target->FindComponentByClass<UBBSBCombatComponent>();
 if(!Combat || Combat->bDead) return;
 if(!bFieldAggro)
 {
  bFieldAggro=true;NextFieldVolleyAt=GetWorld()->GetTimeSeconds()+.65;UpdateLabel();
 }
 FieldTarget=Target;FieldLastSeenAt=GetWorld()->GetTimeSeconds();
}

void ABBSBInkEnemy::TickFieldBulletHell(float DT)
{
 const double Now=GetWorld()->GetTimeSeconds();
 APawn* Target=FieldTarget.Get();
 if(!bFieldAggro)
 {
  Target=UGameplayStatics::GetPlayerPawn(this,0);
  if(Target && FVector::DistSquared(Target->GetActorLocation(),GetActorLocation())<=FMath::Square(FMath::Max(100.f,FieldAggroRadius)) && FieldSight(this,Target)) AlertFieldPlayer(Target);
 }
 Target=FieldTarget.Get();
 const auto* Combat=Target ? Target->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 const bool InRange=Target && FVector::DistSquared(Target->GetActorLocation(),GetActorLocation())<=FMath::Square(FMath::Max(FieldAggroRadius+100.f,FieldLoseAggroRadius));
 const bool Visible=InRange && Combat && !Combat->bDead && FieldSight(this,Target);
 if(Visible) FieldLastSeenAt=Now;
 if(bFieldAggro && (!Combat || Combat->bDead || Now-FieldLastSeenAt>FMath::Max(.1f,FieldLoseAggroSeconds)))
 {
  bFieldAggro=false;bFieldWindingUp=false;FieldTarget.Reset();UpdateLabel();
 }
 bFieldRecovering=bFieldAggro && Now<FieldRecoveryUntil;
 if(bFieldAggro && Target)
 {
  const FRotator Facing=(Target->GetActorLocation()-GetActorLocation()).Rotation();
  SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,Facing.Yaw,0),DT,5.f));
  if(Now<WeaponImpulseUntil && bFieldWindingUp) { bFieldWindingUp=false;NextFieldVolleyAt=Now+.65; }
  if(bFieldWindingUp)
  {
   // The last 200 ms commit to a visible dodgeable position, including airborne targets.
   if(Now<FieldReleaseAt-.2) FieldCommittedAim=Target->GetActorLocation();
   if(Now>=FieldReleaseAt)
   {
    if(Visible) { FireFieldVolley(Target);FieldRecoveryUntil=Now+FMath::Max(.2f,FieldRecoverySeconds); }
    bFieldWindingUp=false;NextFieldVolleyAt=Now+FMath::Max(.25f,FieldVolleyInterval);
   }
  }
  else if(Visible && !bFieldRecovering && Now>=NextFieldVolleyAt && Now>=WeaponImpulseUntil &&
   (FieldAttack!=EBBSBFieldAttackKind::Slam || FVector::Dist2D(GetActorLocation(),Target->GetActorLocation())<700.f))
  {
   if(FieldAttack==EBBSBFieldAttackKind::Fan)
   {
    FireFieldVolley(Target);NextFieldVolleyAt=Now+FMath::Max(.25f,FieldVolleyInterval);
   }
   else { bFieldWindingUp=true;FieldReleaseAt=Now+FMath::Max(.2f,FieldWindupSeconds);FieldCommittedAim=Target->GetActorLocation(); }
  }
 }
 MoveInField(DT,bFieldAggro ? Target:nullptr);
 const float Bob=GetVelocity().Size2D()>20.f ? 4.f*FMath::Sin(Now*10.f):1.5f*FMath::Sin(Now*2.f);
 Body->SetRelativeLocation(FVector(0,0,-6.f+Bob));
 const float Prepare=bFieldWindingUp ? FMath::Clamp(float((FieldReleaseAt-Now)/FMath::Max(.2f,FieldWindupSeconds)),0.f,1.f):0.f;
 Body->SetRelativeScale3D(FieldRestScale*FVector(1.f+.12f*Prepare,1.f+.12f*Prepare,1.f-.18f*Prepare));
}

void ABBSBInkEnemy::MoveInField(float DT,APawn* Target)
{
 const double Now=GetWorld()->GetTimeSeconds();auto* Movement=GetCharacterMovement();
 Movement->bOrientRotationToMovement=!Target;
 if(Now<WeaponImpulseUntil) { ConsumeMovementInputVector();return; }
 if(Target && (bFieldWindingUp || bFieldRecovering)) { ConsumeMovementInputVector();Movement->StopMovementImmediately();return; }
 const FVector Position=GetActorLocation();FVector Goal=Home;
 if(Target)
 {
  const float Range=FVector::Dist2D(Position,Target->GetActorLocation());
  FVector Toward=(Target->GetActorLocation()-Position).GetSafeNormal2D();if(Toward.IsNearlyZero()) Toward=GetActorForwardVector();
  const FVector Side=FVector::CrossProduct(FVector::UpVector,Toward)*(FieldVolleysFired%2 ? -1.f:1.f);
  const float Radial=FMath::Clamp(Range-FieldPreferredRange,-220.f,220.f);
  switch(FieldMotion)
  {
   case EBBSBFieldMotion::Guard:Goal=Home+Side*(180.f*FMath::Sin(Now*.7f));break;
   case EBBSBFieldMotion::Strafe:Goal=Position+Side*300.f+Toward*Radial;break;
   case EBBSBFieldMotion::Pursue:Goal=Target->GetActorLocation()-Toward*220.f;break;
   case EBBSBFieldMotion::Skirmish:Goal=Position+Toward*(Radial*1.5f)+Side*140.f;break;
   case EBBSBFieldMotion::Orbit:Goal=Position+Side*380.f+Toward*Radial;break;
  }
  Goal=Home+(Goal-Home).GetClampedToMaxSize2D(FMath::Max(100.f,FieldHomeRadius));
 }
 const FVector Delta=Goal-Position;
 if(Delta.Size2D()>24.f)
 {
  // Stay on reachable ground; CharacterMovement still sweeps walls and other pawns.
  FHitResult Floor;FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBFieldStep),false,this);if(Target) Query.AddIgnoredActor(Target);
  const FVector Ahead=Position+Delta.GetSafeNormal2D()*85.f;
  if(GetWorld()->LineTraceSingleByObjectType(Floor,Ahead+FVector(0,0,40),Ahead-FVector(0,0,190),BBSBCombatVisuals::GeometryTypes(),Query) && Floor.ImpactNormal.Z>.75f) AddMovementInput(Delta.GetSafeNormal2D());
 }
}

void ABBSBInkEnemy::FireFieldVolley(APawn* Target)
{
 if(!IsValid(Target) || bDefeated) return;
 if(FieldAttack!=EBBSBFieldAttackKind::Fan)
 {
  const FVector Origin=GetAimPoint();const FVector Side=FVector::CrossProduct(FVector::UpVector,(FieldCommittedAim-Origin).GetSafeNormal2D());
  auto Shape=[&](FVector Aim,float Delay)
  {
   FActorSpawnParameters Spawn;Spawn.Owner=this;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   if(auto* Shot=GetWorld()->SpawnActor<ABBSBFieldAttack>(ABBSBFieldAttack::StaticClass(),Origin,FRotator::ZeroRotator,Spawn))
   { Shot->InitializeField(this,FieldAttack,Aim,Delay);++FieldShotsFired; }
  };
  if(FieldAttack==EBBSBFieldAttackKind::Artillery)
  {
   Shape(FieldCommittedAim,0.f);Shape(FieldCommittedAim+Side*320.f,.28f);Shape(FieldCommittedAim-Side*320.f,.56f);
  }
  else if(FieldAttack==EBBSBFieldAttackKind::SwordWave)
  {
   Shape(FieldCommittedAim-Side*100.f,0.f);Shape(FieldCommittedAim+Side*100.f,.28f);
  }
  else Shape(FieldCommittedAim,0.f);
  ++FieldVolleysFired;return;
 }
 // Every pellet gets an independent world-space launch. No target tracking remains in flight.
 const FVector Origin=GetAimPoint();
 const FVector Aim=(Target->GetActorLocation()-Origin).GetSafeNormal();
 if(Aim.IsNearlyZero()) return;
 const FRotator AimRotation=Aim.Rotation();
 const float Speed=FMath::Clamp(FieldShotSpeed,100.f,2000.f);
 const float Travel=FMath::Clamp(float(FVector::Dist(Origin,Target->GetActorLocation())/Speed),.2f,3.f);
 auto Fire=[&](float Yaw,float Pitch)
 {
  FActorSpawnParameters Spawn;Spawn.Owner=this;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Shot=GetWorld()->SpawnActor<ABBSBInkProjectile>(ABBSBInkProjectile::StaticClass(),Origin,FRotator::ZeroRotator,Spawn);
  if(!Shot) return;
  const FRotator Direction(FMath::Clamp(float(AimRotation.Pitch)+Pitch,-80.f,80.f),float(AimRotation.Yaw)+Yaw,0);
  Shot->Initialize(nullptr,this,Origin+Direction.Vector()*(Speed*Travel),EBBSBInkPattern::Tap,Travel,INDEX_NONE);
  ++FieldShotsFired;
 };
 const int32 Count=FMath::Clamp(FieldBulletsPerVolley,3,32);
 if(MainPattern==EBBSBInkPattern::Area)
 {
  for(int32 Index=0;Index<Count;++Index) Fire(Index*360.f/Count+FMath::Fmod(FieldVolleysFired*11.f,360.f),0.f);
  Fire(-10.f,0.f);Fire(0.f,0.f);Fire(10.f,0.f);
 }
 else
 {
  const float HalfSpread=MainPattern==EBBSBInkPattern::Hold ? 20.f:32.f;
  const float Shift=MainPattern==EBBSBInkPattern::Curve ? 12.f*FMath::Sin(FieldVolleysFired*.85f):MainPattern==EBBSBInkPattern::Zigzag ? (FieldVolleysFired%2 ? 4.f:-4.f):0.f;
  for(int32 Index=0;Index<Count;++Index)
  {
   const float Alpha=float(Index)/float(Count-1);
   // A slightly staggered second row makes a 3D fan while keeping a readable escape gap.
   const float Pitch=(MainPattern==EBBSBInkPattern::Hold && FieldVolleysFired%2) ? (Index%2 ? 3.f:-3.f):0.f;
   Fire(FMath::Lerp(-HalfSpread,HalfSpread,Alpha)+Shift,Pitch);
  }
 }
 ++FieldVolleysFired;
}
