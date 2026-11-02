#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Combat/BBSBFieldAttack.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool ABBSBInkEnemy::FireFieldSecondaryPattern(const FVector& Origin,const FVector& Aim)
{
 FVector Forward=(Aim-Origin).GetSafeNormal2D();if(Forward.IsNearlyZero()) Forward=GetActorForwardVector();
 const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
 auto Shape=[&](EBBSBFieldAttackKind Kind,FVector Start,FVector Target,float Delay,float Scale=1.f)
 {
  // Remote wave launch points must be reachable from the emitter; side shots
  // cannot appear through nearby cover. Already launched shapes keep their course.
  FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBSecondaryLaunch),false,this);
  for(TActorIterator<ABBSBInkEnemy> It(GetWorld());It;++It) Q.AddIgnoredActor(*It);
  FHitResult Wall;
  if(GetWorld()->LineTraceSingleByObjectType(Wall,Origin,Start,BBSBCombatVisuals::GeometryTypes(),Q)) return;
  FActorSpawnParameters P;P.Owner=this;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  if(auto* Shot=GetWorld()->SpawnActor<ABBSBFieldAttack>(ABBSBFieldAttack::StaticClass(),Start,FRotator::ZeroRotator,P))
  { Shot->InitializeField(this,Kind,Target,Delay,Scale);++FieldShotsFired; }
 };
 switch(FieldAttack)
 {
  case EBBSBFieldAttackKind::GroundSpikes:
  {
   const float Reach=FMath::Clamp(float(FVector::Dist2D(Origin,Aim)),350.f,1400.f);
   Shape(FieldAttack,Origin,Origin+Forward.RotateAngleAxis(-28.f,FVector::UpVector)*Reach,0.f);
   Shape(FieldAttack,Origin,Origin+Forward.RotateAngleAxis(28.f,FVector::UpVector)*Reach,.18f);
   Shape(FieldAttack,Origin,Aim,.36f);
   return true;
  }
  case EBBSBFieldAttackKind::SwordWave:
   // A V-shaped pair converges on the committed point, then continues past it.
   Shape(FieldAttack,Origin-Right*180.f,Aim,.06f);
   Shape(FieldAttack,Origin+Right*180.f,Aim,.06f);
   return true;
  case EBBSBFieldAttackKind::Slam:
   Shape(EBBSBFieldAttackKind::Donut,Origin,Aim,0.f,.57f);
   Shape(EBBSBFieldAttackKind::Donut,Origin,Aim,.55f,.57f);
   return true;
  case EBBSBFieldAttackKind::Artillery:
   // Four smaller blasts leave escape lanes; the delayed center discourages camping.
   for(int32 Index=0;Index<4;++Index)
    Shape(FieldAttack,Origin,Aim+Forward.RotateAngleAxis(Index*90.f,FVector::UpVector)*300.f,0.f,.7f);
   Shape(FieldAttack,Origin,Aim,.6f,.7f);
   return true;
  case EBBSBFieldAttackKind::Donut:
   Shape(EBBSBFieldAttackKind::InwardRing,Origin,Aim,0.f);
   return true;
  default:return false;
 }
}

void ABBSBInkEnemy::TickFieldBeatFire(APawn* Target,bool bVisible)
{
 const auto* Combat=Target ? Target->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 if(!Combat) return;
 const double Now=GetWorld()->GetTimeSeconds();const float BPM=FMath::Clamp(Combat->FieldBPM,40.f,240.f);
 const double BeatTime=BBSBCombatRules::BeatAt(Now,0,BPM);const int64 Beat=FMath::FloorToInt64(BeatTime);
 if(Beat==LastFieldBeatTick) return;
 LastFieldBeatTick=Beat;
 // One current beat only. Missed/invisible beats are discarded, never queued.
 if(!bEnableFieldBeatFire || FieldAttack==EBBSBFieldAttackKind::Fan || !bFieldAggro || bDefeated || !bVisible || Combat->bDead ||
  Now<BeatFireArmedAt || Now<WeaponImpulseUntil || bFieldWindingUp || Now<LastFieldMajorAt+.3 || (BeatTime-Beat)*60.0/BPM>.12) return;
 int32 Live=0;for(TActorIterator<ABBSBInkProjectile> It(GetWorld());It;++It) if(It->bBeatProjectile && !It->bResolved) ++Live;
 constexpr int32 GlobalBeatShotLimit=192;
 if(Live>=GlobalBeatShotLimit) return;
 const FVector Aim=(Target->GetActorLocation()-GetAimPoint()).GetSafeNormal();if(Aim.IsNearlyZero()) return;
 const FRotator Frame=Aim.Rotation();const int32 CountBefore=FieldBeatShotsFired;
 const float ShotRange=FVector::Dist(GetAimPoint(),Target->GetActorLocation());
 auto Fire=[&](float Yaw,float Pitch=0.f)
 {
  if(Live>=GlobalBeatShotLimit) return;
  const FVector Direction=FRotator(FMath::Clamp(float(Frame.Pitch)+Pitch,-65.f,65.f),Frame.Yaw+Yaw,0).Vector();
  FActorSpawnParameters P;P.Owner=this;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  if(auto* Shot=GetWorld()->SpawnActor<ABBSBInkProjectile>(ABBSBInkProjectile::StaticClass(),GetAimPoint(),FRotator::ZeroRotator,P))
  { Shot->InitializeBeat(this,Direction,ShotRange);++Live;++FieldBeatShotsFired;++FieldShotsFired; }
 };
 const float Phase=float(Beat%24)*15.f;
 switch(FieldAttack)
 {
  case EBBSBFieldAttackKind::GroundSpikes:
   for(float Angle:{-30.f,0.f,30.f}) Fire(Angle+(Beat%2 ? 10.f:-10.f));break;
  case EBBSBFieldAttackKind::SwordWave:
   for(int32 Index=0;Index<4;++Index) Fire(Index*90.f+Phase);break;
  case EBBSBFieldAttackKind::Slam:
   for(float Angle:{-80.f,-40.f,0.f,40.f,80.f}) Fire(Angle);break;
  case EBBSBFieldAttackKind::Artillery:
   Fire(-18.f);Fire(18.f);Fire(-100.f,4.f);Fire(100.f,4.f);break;
  default:
   for(int32 Index=0;Index<6;++Index) Fire(Index*60.f+Phase);break;
 }
 if(FieldBeatShotsFired>CountBefore) { ++FieldBeatBurstsFired;FieldLastFiredBeat=Beat;FieldLastBeatShotAt=Now; }
}

