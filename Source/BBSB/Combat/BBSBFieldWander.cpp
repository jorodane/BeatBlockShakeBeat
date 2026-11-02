#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void ABBSBInkEnemy::ResetFieldWander()
{
 FieldWanderRandom.Initialize(FMath::RoundToInt(Home.X*3.f+Home.Y*7.f)+EncounterID*131);
 bFieldWanderReady=true;bFieldWanderMoving=false;FieldWanderGoal=Home;
 NextFieldWanderAt=GetWorld()->GetTimeSeconds()+FieldWanderRandom.FRandRange(.4f,3.f);
 FieldWanderProgressAt=FieldWanderStartedAt=0;FieldWanderProgress=Home;
}

bool ABBSBInkEnemy::ChooseFieldWanderGoal()
{
 const FVector Position=GetActorLocation();
 const float Radius=FMath::Clamp(FieldWanderRadius,100.f,FMath::Max(100.f,FieldHomeRadius));
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBFieldWander),false,this);
 const float HalfHeight=GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 for(int32 Attempt=0;Attempt<12;++Attempt)
 {
  const float Angle=FieldWanderRandom.FRandRange(0.f,2.f*PI);
  FVector Goal=Home+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0.f)*Radius*FieldWanderRandom.FRandRange(.3f,1.f);
  if(FVector::Dist2D(Position,Goal)<120.f) continue;
  bool bReachable=true;FVector Previous=Position;
  const int32 Samples=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Position,Goal)/100.f));
  for(int32 Index=1;Index<=Samples;++Index)
  {
   const FVector Point=FMath::Lerp(Position,Goal,float(Index)/Samples);
   FHitResult Floor;
   if(!GetWorld()->LineTraceSingleByObjectType(Floor,Point+FVector(0,0,280),Point-FVector(0,0,600),BBSBCombatVisuals::GeometryTypes(),Query) || Floor.ImpactNormal.Z<.8f)
   { bReachable=false;break; }
   const FVector Step=Floor.ImpactPoint+FVector(0,0,HalfHeight+2.f);
   FHitResult Obstacle;
   if(FMath::Abs(Step.Z-Previous.Z)>55.f || GetWorld()->SweepSingleByObjectType(Obstacle,Previous,Step,FQuat::Identity,BBSBCombatVisuals::GeometryTypes(),FCollisionShape::MakeSphere(40.f),Query))
   { bReachable=false;break; }
   Previous=Step;
  }
  if(!bReachable) continue;
  FieldWanderGoal=Previous;bFieldWanderMoving=true;
  FieldWanderProgress=Position;FieldWanderProgressAt=FieldWanderStartedAt=GetWorld()->GetTimeSeconds();return true;
 }
 return false;
}

FVector ABBSBInkEnemy::IdleFieldGoal()
{
 if(!bFieldWanderReady) ResetFieldWander();
 const double Now=GetWorld()->GetTimeSeconds();const FVector Position=GetActorLocation();
 // Combat and knockback may leave the idle area; come home before choosing another stroll.
 if(FVector::Dist2D(Position,Home)>FMath::Max(100.f,FieldHomeRadius)+60.f)
 { bFieldWanderMoving=false;NextFieldWanderAt=Now+1.f;return Home; }
 if(bFieldWanderMoving)
 {
  bool bStop=FVector::Dist2D(Position,FieldWanderGoal)<30.f || Now-FieldWanderStartedAt>16.f;
  if(Now-FieldWanderProgressAt>.7)
  {
   bStop|=FVector::Dist2D(Position,FieldWanderProgress)<12.f;
   FieldWanderProgress=Position;FieldWanderProgressAt=Now;
  }
  if(!bStop) return FieldWanderGoal;
  bFieldWanderMoving=false;
  const float Min=FMath::Max(.2f,FieldWanderPauseMin),Max=FMath::Max(Min,FieldWanderPauseMax);
  NextFieldWanderAt=Now+FieldWanderRandom.FRandRange(Min,Max);
 }
 if(Now>=NextFieldWanderAt)
 {
  if(ChooseFieldWanderGoal()) return FieldWanderGoal;
  NextFieldWanderAt=Now+FieldWanderRandom.FRandRange(1.f,2.f);
 }
 return Position;
}
