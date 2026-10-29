#include "BBSBCombat.h"
#include "BBSBCombatVisuals.h"
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
  bFieldAggro=false;FieldTarget.Reset();UpdateLabel();
 }
 if(bFieldAggro && Target)
 {
  const FRotator Facing=(Target->GetActorLocation()-GetActorLocation()).Rotation();
  SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,Facing.Yaw,0),DT,5.f));
  if(Visible && Now>=NextFieldVolleyAt)
  {
   FireFieldVolley(Target);
   // Never replay missed volleys after a pause or editor hitch.
   NextFieldVolleyAt=Now+FMath::Max(.25f,FieldVolleyInterval);
  }
 }
 // These test opponents defend their pocket, then walk home after a weapon impulse.
 const FVector Delta=Home-GetActorLocation();
 if(Now<WeaponImpulseUntil) ConsumeMovementInputVector();
 else if(Delta.Size2D()>24.f) AddMovementInput(Delta.GetSafeNormal2D());
 Body->SetRelativeLocation(FVector(0,0,-6.f+1.5f*FMath::Sin(Now*2.f)));
}

void ABBSBInkEnemy::FireFieldVolley(APawn* Target)
{
 if(!IsValid(Target) || bDefeated) return;
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
