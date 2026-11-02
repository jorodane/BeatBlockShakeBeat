#include "Combat/BBSBCombat.h"
#include "Character/BBSBVisibilityStencil.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

void ABBSBInkProjectile::InitializeBeat(ABBSBInkEnemy* Enemy,FVector Direction,float TargetDistance)
{
 if(!Enemy) { Destroy();return; }
 const float Life=FMath::Clamp(Enemy->FieldBeatShotLifetime,1.f,6.f);
 const float Speed=FMath::Clamp(Enemy->FieldBeatShotSpeed,100.f,1000.f);
 Initialize(nullptr,Enemy,GetActorLocation()+Direction.GetSafeNormal()*(Speed*Life),EBBSBInkPattern::Tap,Life,INDEX_NONE);
 bBeatProjectile=true;BeatStyle=Enemy->FieldAttack;Damage=FMath::Max(0.f,Enemy->FieldBeatShotDamage);
 CollisionRadius=FMath::Clamp(Enemy->FieldBeatShotRadius,2.f,12.f);
 BeatCurveSign=Enemy->FieldBeatShotsFired%2 ? -1.f:1.f;
 BeatFlightSeconds=FMath::Clamp(TargetDistance/Speed,.65f,3.3f);
 BeatArcHeight=FMath::Clamp(TargetDistance*.12f,50.f,160.f);
 InitializeBeatVisual();UpdateBeatVisual();SetLifeSpan(Life);
}

FVector ABBSBInkProjectile::BeatFlightPosition(float AtAge) const
{
 const float T=FMath::Max(0.f,AtAge);
 FVector Position=LaunchOrigin+FlightVelocity*T;
 switch(BeatStyle)
 {
  case EBBSBFieldAttackKind::GroundSpikes:
   // A thin shard weaves narrowly, without chasing a moving target.
   Position+=Side*(12.f*FMath::Sin(T*18.f)*FMath::SmoothStep(0.f,.12f,T));break;
  case EBBSBFieldAttackKind::SwordWave:
   Position+=Side*(BeatCurveSign*100.f*FMath::SmoothStep(0.f,.95f,T));break;
  case EBBSBFieldAttackKind::Slam:
   Position.Z+=38.f*FMath::Abs(FMath::Sin(PI*T/.7f))*FMath::Exp(-T*.7f);break;
  case EBBSBFieldAttackKind::Artillery:
  {
   const float ArcT=T/BeatFlightSeconds;
   Position.Z+=4.f*BeatArcHeight*ArcT*(1.f-ArcT);break;
  }
  case EBBSBFieldAttackKind::Donut:
  {
   const FVector Axis=FlightVelocity.GetSafeNormal();
   const FVector Up=FVector::CrossProduct(Axis,Side).GetSafeNormal();
   const float Radius=22.f*FMath::SmoothStep(0.f,.25f,T),Angle=T*7.f*BeatCurveSign;
   Position+=Side*(Radius*(FMath::Cos(Angle)-1.f))+Up*(Radius*FMath::Sin(Angle));break;
  }
  default:break;
 }
 return Position;
}

float ABBSBInkProjectile::BeatFlightReach(float Horizon) const
{
 if(!bBeatProjectile || BeatStyle!=EBBSBFieldAttackKind::Artillery) return 0.f;
 const float T=FMath::Max(.1f,BeatFlightSeconds),Step=Horizon/T;
 // Include vertical travel of late, falling shells in the forecast broad phase.
 return 4.f*BeatArcHeight*Step*(1.f+2.f*Age/T+Step);
}

void ABBSBInkProjectile::InitializeBeatVisual()
{
 Body->SetWorldScale3D(FVector(1.f));Body->SetVisibility(false);
 BeatVisual=NewObject<UInstancedStaticMeshComponent>(this,TEXT("BeatShape"));AddInstanceComponent(BeatVisual);
 BeatVisual->SetupAttachment(Body);BeatVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 BeatVisual->SetGenerateOverlapEvents(false);BeatVisual->SetCastShadow(false);BeatVisual->SetCanEverAffectNavigation(false);
 BeatVisual->SetMobility(EComponentMobility::Movable);
 BeatVisual->SetRenderCustomDepth(BBSBVisibilityStencil::Enabled(GetWorld()));BeatVisual->SetCustomDepthStencilValue(BBSBVisibilityStencil::Projectile);
 const TCHAR* Mesh=BeatStyle==EBBSBFieldAttackKind::GroundSpikes ? TEXT("/Engine/BasicShapes/Cone.Cone"):
  BeatStyle==EBBSBFieldAttackKind::Artillery ? TEXT("/Engine/BasicShapes/Sphere.Sphere"):TEXT("/Engine/BasicShapes/Cube.Cube");
 BeatVisual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Mesh));BeatVisual->RegisterComponent();BeatVisual->SetVisibility(true);
 const float R=CollisionRadius;
 auto Part=[&](FVector Point,FVector Size,FRotator Rotation=FRotator::ZeroRotator)
 { BeatVisual->AddInstance(FTransform(Rotation,Point,Size/100.f)); };
 auto Arc=[&](int32 Segments,float Start,float End)
 {
  for(int32 Index=0;Index<Segments;++Index)
  {
   const float A=FMath::DegreesToRadians(FMath::Lerp(Start,End,float(Index)/Segments));
   const float B=FMath::DegreesToRadians(FMath::Lerp(Start,End,float(Index+1)/Segments));
   const FVector From(0,FMath::Cos(A)*R*.82f,FMath::Sin(A)*R*.82f),To(0,FMath::Cos(B)*R*.82f,FMath::Sin(B)*R*.82f);
   const FVector Delta=To-From;Part((From+To)*.5f,FVector(Delta.Size()+.2f,R*.2f,R*.2f),Delta.Rotation());
  }
 };
 FLinearColor Color(.95f,.58f,.28f);
 switch(BeatStyle)
 {
  case EBBSBFieldAttackKind::GroundSpikes:
   Part(FVector::ZeroVector,FVector(R*1.2f,R*1.2f,R*1.6f),FRotator(-90,0,0));Color=FLinearColor(.92f,.68f,.35f);break;
  case EBBSBFieldAttackKind::SwordWave:
   Arc(14,-110.f,110.f);Color=FLinearColor(.25f,.86f,1.f);break;
  case EBBSBFieldAttackKind::Slam:
   Part(FVector::ZeroVector,FVector(R*1.12f),FRotator(30,45,20));Color=FLinearColor(.82f,.38f,.5f);break;
  case EBBSBFieldAttackKind::Artillery:
   Part(FVector::ZeroVector,FVector(R*1.5f));
   Part(FVector(-R*.65f,R*.16f,0),FVector(R*.5f));
   Part(FVector(-R*.65f,-R*.16f,0),FVector(R*.5f));Color=FLinearColor(1.f,.51f,.13f);break;
  default:Arc(18,0.f,360.f);Color=FLinearColor(.73f,.5f,1.f);break;
 }
 BBSBCombatVisuals::Tint(BeatVisual,Color);
 auto* TrailBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BBSB/VFX/Enemies/M_BeatBulletTrail.M_BeatBulletTrail"));
 auto* TrailMaterial=TrailBase ? UMaterialInstanceDynamic::Create(TrailBase,this):nullptr;
 if(TrailMaterial) TrailMaterial->SetVectorParameterValue(TEXT("WireColor"),Color);
 auto* Ribbon=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
 for(int32 Index=0;Index<3;++Index)
 {
  auto* Trail=NewObject<USplineMeshComponent>(this,MakeUniqueObjectName(this,USplineMeshComponent::StaticClass(),TEXT("BeatTrail")));
  AddInstanceComponent(Trail);Trail->SetupAttachment(Body);Trail->SetMobility(EComponentMobility::Movable);
  Trail->SetStaticMesh(Ribbon);Trail->SetForwardAxis(ESplineMeshAxis::X,false);Trail->SetSplineUpDir(FVector::UpVector,false);
  Trail->bSmoothInterpRollScale=false;Trail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Trail->SetbNeverNeedsCookedCollisionData(true);Trail->SetGenerateOverlapEvents(false);
  Trail->SetCastShadow(false);Trail->SetCanEverAffectNavigation(false);Trail->bReceivesDecals=false;
  if(TrailMaterial) Trail->SetMaterial(0,TrailMaterial);
  Trail->RegisterComponent();Trail->SetVisibility(false);BeatTrails.Add(Trail);
 }
}

void ABBSBInkProjectile::UpdateBeatVisual()
{
 if(!BeatVisual || bResolved) return;
 FVector Tangent=BeatFlightPosition(Age+.005f)-BeatFlightPosition(FMath::Max(0.f,Age-.005f));
 if(Tangent.IsNearlyZero()) Tangent=FlightVelocity;
 const float Spin=BeatStyle==EBBSBFieldAttackKind::Slam ? Age*220.f:BeatStyle==EBBSBFieldAttackKind::SwordWave ? Age*120.f:0.f;
 FRotator Facing=Tangent.Rotation();Facing.Roll=Spin;SetActorRotation(Facing);
 const FTransform Frame=GetActorTransform();
 FVector RibbonNormal=FVector::UpVector;
 if(const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))
 {
  RibbonNormal=Frame.InverseTransformVectorNoScale(-Camera->GetCameraRotation().Vector());
  if(FMath::Abs(FVector::DotProduct(RibbonNormal,FVector::ForwardVector))>.98f)
   RibbonNormal=Frame.InverseTransformVectorNoScale(Camera->GetCameraRotation().Quaternion().GetUpVector());
 }
 constexpr float Step=.04f,Lifetime=Step*3.f;
 auto VelocityAt=[&](float Time)
 {
  const float A=FMath::Max(0.f,Time-.002f),B=Time+.002f;
  return (BeatFlightPosition(B)-BeatFlightPosition(A))/(B-A);
 };
 for(int32 Index=0;Index<BeatTrails.Num();++Index)
 {
  auto* Trail=BeatTrails[Index].Get();if(!Trail) continue;
  const float NewTime=Age-Index*Step,OldTime=FMath::Max(0.f,Age-(Index+1)*Step);
  const bool Visible=NewTime>0.f && NewTime>OldTime;Trail->SetVisibility(Visible);
  if(!Visible) continue;
  const float Span=NewTime-OldTime;
  const FVector Start=Frame.InverseTransformPosition(BeatFlightPosition(NewTime)),End=Frame.InverseTransformPosition(BeatFlightPosition(OldTime));
  const FVector StartTangent=Frame.InverseTransformVectorNoScale(-VelocityAt(NewTime)*Span);
  const FVector EndTangent=Frame.InverseTransformVectorNoScale(-VelocityAt(OldTime)*Span);
  // The leading cross-section spans the complete bullet diameter. Adjacent
  // ribbon widths match, then taper continuously to zero at the end of its wake.
  const float Width=CollisionRadius*2.f;
  Trail->SetSplineUpDir(RibbonNormal,false);
  Trail->SetStartScale(FVector2D(Width*FMath::Clamp(1.f-(Age-NewTime)/Lifetime,0.f,1.f)/100.f,1.f),false);
  Trail->SetEndScale(FVector2D(Width*FMath::Clamp(1.f-(Age-OldTime)/Lifetime,0.f,1.f)/100.f,1.f),false);
  Trail->SetStartAndEnd(Start,StartTangent,End,EndTangent,true);
 }
}
