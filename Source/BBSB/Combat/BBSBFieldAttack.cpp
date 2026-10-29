#include "Combat/BBSBFieldAttack.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABBSBFieldAttack::ABBSBFieldAttack()
{
 auto* Origin=CreateDefaultSubobject<USceneComponent>(TEXT("ShapeOrigin"));SetRootComponent(Origin);Body->SetupAttachment(Origin);
 Body->SetRelativeScale3D(FVector(1.f));Body->SetVisibility(false);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
 Telegraph=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Telegraph"));
 Energy=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ActiveShape"));
 Stones=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RisingStones"));
 for(auto* Part:{Telegraph.Get(),Energy.Get(),Stones.Get()})
 {
  Part->SetupAttachment(Origin);Part->SetStaticMesh(Cube.Object);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Part->SetGenerateOverlapEvents(false);Part->SetCastShadow(false);Part->SetCanEverAffectNavigation(false);
 }
 Stones->SetStaticMesh(Cone.Object);Pattern=EBBSBInkPattern::Area;bFieldProjectile=true;
}

FCollisionQueryParams ABBSBFieldAttack::WorldQuery() const
{
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBFieldShape),false,this);
 if(const auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0)) Q.AddIgnoredActor(Pawn);
 for(TActorIterator<ABBSBInkEnemy> It(GetWorld());It;++It) Q.AddIgnoredActor(*It);
 return Q;
}

bool ABBSBFieldAttack::FloorAt(const FVector& Point,FVector& Floor) const
{
 FHitResult Hit;
 if(!GetWorld()->LineTraceSingleByObjectType(Hit,Point+FVector(0,0,600),Point-FVector(0,0,1800),BBSBCombatVisuals::GeometryTypes(),WorldQuery()) || Hit.ImpactNormal.Z<.82f) return false;
 Floor=Hit.ImpactPoint;return true;
}

void ABBSBFieldAttack::InitializeField(ABBSBInkEnemy* Enemy,EBBSBFieldAttackKind Kind,FVector Target,float Delay)
{
 Source=Enemy;Attack=Kind;bFieldProjectile=true;Pattern=EBBSBInkPattern::Area;Damage=Enemy ? Enemy->ShotDamage:6.f;
 LaunchOrigin=GetActorLocation();FrozenTarget=Target;Age=0;LaunchDelay=FMath::Max(0.f,Delay);ContactedShapes.Reset();
 AttackSpeed=Enemy ? FMath::Max(100.f,Enemy->FieldShotSpeed):720.f;
 Forward=(Target-LaunchOrigin).GetSafeNormal();if(Forward.IsNearlyZero()) Forward=FVector::ForwardVector;
 Right=FVector::CrossProduct(FVector::UpVector,Forward).GetSafeNormal();if(Right.IsNearlyZero()) Right=FVector::RightVector;
 if(!FloorAt(Target,ImpactPoint)) { Destroy();return; }
 WarningSeconds=.75f+LaunchDelay;ActiveSeconds=.4f;AttackRadius=220.f;AttackHeight=180.f;
 if(Kind==EBBSBFieldAttackKind::GroundSpikes)
 {
  FVector Last;if(!FloorAt(LaunchOrigin,Last)) { Destroy();return; }
  const FVector Direction=Forward.GetSafeNormal2D();
  const int32 Count=FMath::Clamp(FMath::CeilToInt((FVector::Dist2D(LaunchOrigin,Target)+320.f)/160.f),4,11);
  for(int32 Index=0;Index<Count;++Index)
  {
   FVector Floor;const FVector Point=LaunchOrigin+Direction*(160.f*(Index+1));FHitResult Wall;
   if(!FloorAt(Point,Floor) || FMath::Abs(Floor.Z-Last.Z)>65.f ||
    GetWorld()->LineTraceSingleByObjectType(Wall,Last+FVector(0,0,70),Floor+FVector(0,0,70),BBSBCombatVisuals::GeometryTypes(),WorldQuery())) break;
   SpikePoints.Add(Floor);Last=Floor;
  }
  AttackRadius=64.f;AttackHeight=175.f;WarningSeconds=.55f+LaunchDelay;ActiveSeconds=.38f;
  TotalSeconds=WarningSeconds+FMath::Max(0,SpikePoints.Num()-1)*SpikeStepSeconds+ActiveSeconds+.25f;
 }
 else if(Kind==EBBSBFieldAttackKind::SwordWave)
 {
  WarningSeconds=.28f+LaunchDelay;ActiveSeconds=3.2f;AttackRadius=190.f;AttackThickness=16.f;AttackHeight=38.f;
  TotalSeconds=WarningSeconds+ActiveSeconds;
 }
 else if(Kind==EBBSBFieldAttackKind::Artillery)
 {
  FlightSeconds=FMath::Clamp(float(FVector::Dist2D(LaunchOrigin,Target)/AttackSpeed),.95f,1.65f);
  WarningSeconds=LaunchDelay+FlightSeconds;AttackRadius=185.f;AttackHeight=230.f;ArcHeight=FMath::Max(420.f,float(FVector::Dist2D(LaunchOrigin,Target))*.45f);
  TotalSeconds=WarningSeconds+ActiveSeconds+.2f;Damage*=1.2f;
 }
 else if(Kind==EBBSBFieldAttackKind::Donut)
 {
  if(!FloorAt(LaunchOrigin,ImpactPoint)) { Destroy();return; }
  AttackRadius=1150.f;AttackThickness=70.f;AttackHeight=115.f;WarningSeconds=.85f+LaunchDelay;ActiveSeconds=1.65f;
  TotalSeconds=WarningSeconds+ActiveSeconds+.2f;
 }
 else
 {
  WarningSeconds=.8f+LaunchDelay;ActiveSeconds=.28f;AttackRadius=230.f;AttackHeight=200.f;
  Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  TotalSeconds=WarningSeconds+ActiveSeconds+.25f;Damage*=1.5f;
 }
 auto Style=[&](UStaticMeshComponent* Part,FLinearColor Color,float Opacity)
 {
  if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BBSB/VFX/Enemies/M_FieldAttack.M_FieldAttack")))
  {
   auto* MID=UMaterialInstanceDynamic::Create(Base,this);Part->SetMaterial(0,MID);
   MID->SetVectorParameterValue(TEXT("Tint"),Color);MID->SetScalarParameterValue(TEXT("Density"),Opacity);MID->SetScalarParameterValue(TEXT("Brightness"),1.15f);
  }
  else BBSBCombatVisuals::Tint(Part,Color);
 };
 Style(Telegraph,FLinearColor(1.f,.56f,.06f),.78f);Style(Energy,FLinearColor(1.f,.12f,.28f),.92f);
 BBSBCombatVisuals::Tint(Stones,FLinearColor(.25f,.19f,.26f));BBSBCombatVisuals::Tint(Body,Kind==EBBSBFieldAttackKind::Artillery ? FLinearColor(.95f,.32f,.08f):FLinearColor(.24f,.19f,.3f));
 if(Kind==EBBSBFieldAttackKind::Artillery) Style(Body,FLinearColor(1.f,.26f,.05f),.85f);
 if(const auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0)) PreviousHurt=Pawn->GetActorLocation();
 bInitialized=true;SetLifeSpan(TotalSeconds+.5f);RenderShape();
}

FVector ABBSBFieldAttack::ShellAt(float Time) const
{
 const float T=FMath::Clamp((Time-LaunchDelay)/FlightSeconds,0.f,1.f);
 return FMath::Lerp(LaunchOrigin,ImpactPoint+FVector(0,0,22),T)+FVector::UpVector*(4.f*ArcHeight*T*(1.f-T));
}

FVector ABBSBFieldAttack::WavePoint(float Time,int32 Segment) const
{
 const float Angle=FMath::DegreesToRadians(FMath::Lerp(-68.f,68.f,Segment/14.f));
 return LaunchOrigin+Forward*(FMath::Max(0.f,Time-WarningSeconds)*AttackSpeed+AttackRadius*(FMath::Cos(Angle)-1.f))+Right*(AttackRadius*FMath::Sin(Angle));
}

bool ABBSBFieldAttack::ContactAt(float Before,float Now,const FVector& Hurt,float HurtRadius,int32& Shape,FVector& Contact) const
{
 Shape=0;Contact=Hurt;
 auto Column=[&](FVector Base,float Radius)
 {
  return Hurt.Z+HurtRadius>=Base.Z && Hurt.Z-HurtRadius<=Base.Z+AttackHeight && FVector::DistSquared2D(Hurt,Base)<=FMath::Square(Radius+HurtRadius);
 };
 if(Attack==EBBSBFieldAttackKind::GroundSpikes)
 {
  for(int32 Index=0;Index<SpikePoints.Num();++Index)
  {
   const float Start=WarningSeconds+Index*SpikeStepSeconds;
   if(Now>=Start && Before<=Start+ActiveSeconds && !ContactedShapes.Contains(Index) && Column(SpikePoints[Index],AttackRadius)) { Shape=Index;Contact=SpikePoints[Index]+FVector(0,0,80);return true; }
  }
  return false;
 }
 if(Now<WarningSeconds || Before>WarningSeconds+ActiveSeconds || ContactedShapes.Contains(0)) return false;
 if(Attack==EBBSBFieldAttackKind::SwordWave)
 {
  for(int32 Index=0;Index<=14;++Index)
  {
   float Fraction;
   if(BBSBCombatRules::SweptCapsuleContact(WavePoint(FMath::Max(Before,WarningSeconds),Index),WavePoint(Now,Index),Hurt,HurtRadius,HurtRadius,AttackThickness,Contact,Fraction)) return true;
   if(Index<14 && FVector::DistSquared(Hurt,FMath::ClosestPointOnSegment(Hurt,WavePoint(Now,Index),WavePoint(Now,Index+1)))<=FMath::Square(HurtRadius+AttackThickness)) return true;
  }
  return false;
 }
 if(Attack==EBBSBFieldAttackKind::Donut)
 {
  const float R=FMath::Lerp(180.f,AttackRadius,FMath::Clamp((Now-WarningSeconds)/ActiveSeconds,0.f,1.f));
  return Hurt.Z+HurtRadius>=ImpactPoint.Z+25.f && Hurt.Z-HurtRadius<=ImpactPoint.Z+AttackHeight && FMath::Abs(float(FVector::Dist2D(Hurt,ImpactPoint))-R)<=AttackThickness*.5f+HurtRadius;
 }
 Contact=ImpactPoint+FVector(0,0,65);return Column(ImpactPoint,AttackRadius);
}

void ABBSBFieldAttack::Tick(float DT)
{
 // Shape timing and geometry replace the legacy pellet trajectory completely.
 AActor::Tick(DT);if(!bInitialized || bResolved) return;
 auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0);auto* Combat=Pawn ? Pawn->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 const FVector Hurt=Combat ? Pawn->GetActorTransform().TransformPosition(Combat->FieldProjectileHurtOffset):FVector::ZeroVector;
 const float HurtRadius=Combat ? FMath::Clamp(Combat->FieldProjectileHurtRadius,1.f,80.f):18.f;
 const auto Query=WorldQuery();const auto Types=BBSBCombatVisuals::GeometryTypes();
 const int32 Steps=FMath::Clamp(FMath::CeilToInt(DT/.008f),1,64);const float Step=DT/Steps;const float BeforeFrame=Age;
 for(int32 Index=0;Index<Steps;++Index)
 {
  const float Before=Age;Age+=Step;
  if(Attack==EBBSBFieldAttackKind::Artillery && Age>=LaunchDelay && Before<WarningSeconds)
  {
   FHitResult Block;
   if(GetWorld()->SweepSingleByObjectType(Block,ShellAt(Before),ShellAt(FMath::Min(Age,WarningSeconds)),FQuat::Identity,Types,FCollisionShape::MakeSphere(16.f),Query)) { Destroy();return; }
  }
  if(Attack==EBBSBFieldAttackKind::SwordWave && Age>=WarningSeconds)
  {
   for(int32 Segment=0;Segment<=14;++Segment)
   {
    FHitResult Block;
    if(GetWorld()->SweepSingleByObjectType(Block,WavePoint(FMath::Max(Before,WarningSeconds),Segment),WavePoint(Age,Segment),FQuat::Identity,Types,FCollisionShape::MakeSphere(AttackThickness),Query)) { Destroy();return; }
   }
  }
  const FVector AtHurt=FMath::Lerp(PreviousHurt,Hurt,float(Index+1)/Steps);int32 Shape;FVector Contact;
  if(Combat && !Combat->bDead && ContactAt(Before,Age,AtHurt,HurtRadius,Shape,Contact))
  {
   FHitResult Cover;const FVector Origin=Attack==EBBSBFieldAttackKind::SwordWave ? Contact:Attack==EBBSBFieldAttackKind::GroundSpikes ? SpikePoints[Shape]+FVector(0,0,75):ImpactPoint+FVector(0,0,75);
   if(!GetWorld()->LineTraceSingleByObjectType(Cover,Origin,AtHurt,Types,Query))
   {
    ContactedShapes.Add(Shape);++Contacts;Combat->ResolveContact(Contact,INDEX_NONE,Damage,this);
   }
  }
 }
 PreviousHurt=Hurt;
 bDamageActive=Attack==EBBSBFieldAttackKind::GroundSpikes ? Age>=WarningSeconds && Age<=WarningSeconds+FMath::Max(0,SpikePoints.Num()-1)*SpikeStepSeconds+ActiveSeconds:Age>=WarningSeconds && BeforeFrame<=WarningSeconds+ActiveSeconds;
 RenderShape();if(Age>TotalSeconds) { bResolved=true;Destroy(); }
}

void ABBSBFieldAttack::Instance(UInstancedStaticMeshComponent* Mesh,int32& Count,const FVector& Point,const FVector& Size,FRotator Rotation)
{
 const FTransform Pose(Rotation,Point,Size/100.f);
 if(Count<Mesh->GetInstanceCount()) Mesh->UpdateInstanceTransform(Count,Pose,true,false,true);else Mesh->AddInstance(Pose,true);
 ++Count;
}

void ABBSBFieldAttack::Ring(UInstancedStaticMeshComponent* Mesh,int32& Count,const FVector& Point,float Radius,float Width,float Height,int32 Segments)
{
 for(int32 Index=0;Index<Segments;++Index)
 {
  const float Angle=360.f*Index/Segments;const FVector Offset=FRotator(0,Angle,0).Vector()*Radius;
  Instance(Mesh,Count,Point+Offset,FVector(Width,2.f*PI*Radius/Segments+2.f,Height),FRotator(0,Angle,0));
 }
}

void ABBSBFieldAttack::Trim(UInstancedStaticMeshComponent* Mesh,int32 Count)
{
 for(int32 Index=Mesh->GetInstanceCount()-1;Index>=Count;--Index) Mesh->RemoveInstance(Index);
 Mesh->MarkRenderStateDirty();
}

void ABBSBFieldAttack::RenderShape()
{
 MarkerCount=EnergyCount=StoneCount=0;Body->SetVisibility(false);
 if(Attack==EBBSBFieldAttackKind::GroundSpikes)
 {
  for(int32 Index=0;Index<SpikePoints.Num();++Index)
  {
   const float Phase=Age-WarningSeconds-Index*SpikeStepSeconds;const FVector P=SpikePoints[Index];
   if(Phase<0.f)
   {
    Ring(Telegraph,MarkerCount,P+FVector(0,0,3),AttackRadius,8.f,3.f,20);
    const float Progress=FMath::Clamp(1.f+Phase/.55f,0.f,1.f);Ring(Telegraph,MarkerCount,P+FVector(0,0,4),AttackRadius*Progress,4.f,3.f,16);
   }
   else if(Phase<ActiveSeconds+.25f)
   {
    const float Rise=FMath::SmoothStep(0.f,.08f,Phase)*(1.f-FMath::SmoothStep(ActiveSeconds,ActiveSeconds+.25f,Phase));
    Instance(Stones,StoneCount,P+FVector(0,0,AttackHeight*Rise*.5f),FVector(AttackRadius*2,AttackRadius*2,FMath::Max(1.f,AttackHeight*Rise)),FRotator(0,Index*37.f,0));
    if(Phase<=ActiveSeconds) Ring(Energy,EnergyCount,P+FVector(0,0,5),AttackRadius,9.f,6.f,20);
   }
  }
 }
 else if(Attack==EBBSBFieldAttackKind::SwordWave)
 {
  if(Age<WarningSeconds)
  {
   const FVector A=LaunchOrigin-FVector(0,0,65),B=FrozenTarget-FVector(0,0,80);const FVector Delta=B-A;
   Instance(Telegraph,MarkerCount,(A+B)*.5f,FVector(Delta.Size(),8,4),Delta.Rotation());
  }
  else
  {
   for(int32 Index=0;Index<14;++Index)
   {
    const FVector A=WavePoint(Age,Index),B=WavePoint(Age,Index+1),Delta=B-A;
    Instance(Energy,EnergyCount,(A+B)*.5f,FVector(Delta.Size()+4.f,AttackThickness*2,AttackHeight),Delta.Rotation());
   }
  }
 }
 else if(Attack==EBBSBFieldAttackKind::Donut)
 {
  const float Progress=FMath::Clamp((Age-WarningSeconds)/ActiveSeconds,0.f,1.f);const float Radius=FMath::Lerp(180.f,AttackRadius,Progress);
  if(Age<WarningSeconds)
  {
   Ring(Telegraph,MarkerCount,ImpactPoint+FVector(0,0,4),180.f,12,4);
   for(int32 Index=0;Index<48;Index+=2)
   {
    const float Angle=Index*360.f/48.f;Instance(Telegraph,MarkerCount,ImpactPoint+FRotator(0,Angle,0).Vector()*AttackRadius+FVector(0,0,3),FVector(8,60,3),FRotator(0,Angle,0));
   }
  }
  else if(Age<=WarningSeconds+ActiveSeconds)
  {
   Ring(Energy,EnergyCount,ImpactPoint+FVector(0,0,70),Radius,AttackThickness,AttackHeight-25.f,72);
   Ring(Energy,EnergyCount,ImpactPoint+FVector(0,0,5),Radius,AttackThickness,6.f,72);
  }
 }
 else
 {
  const float Phase=Age-WarningSeconds;
  if(Phase<0.f)
  {
   Ring(Telegraph,MarkerCount,ImpactPoint+FVector(0,0,3),AttackRadius,12,4);
   Ring(Telegraph,MarkerCount,ImpactPoint+FVector(0,0,4),AttackRadius*FMath::Clamp(Age/WarningSeconds,0.f,1.f),5,3);
   Instance(Telegraph,MarkerCount,ImpactPoint+FVector(0,0,4),FVector(55,8,3));
   Instance(Telegraph,MarkerCount,ImpactPoint+FVector(0,0,4),FVector(8,55,3));
  }
  else if(Phase<ActiveSeconds)
  {
   Ring(Energy,EnergyCount,ImpactPoint+FVector(0,0,6),AttackRadius,22,12);
   Ring(Energy,EnergyCount,ImpactPoint+FVector(0,0,AttackHeight*.5f),AttackRadius,10,AttackHeight);
  }
  if(Attack==EBBSBFieldAttackKind::Artillery && Age>=LaunchDelay && Phase<0.f)
  {
   Body->SetVisibility(true);Body->SetWorldLocation(ShellAt(Age));Body->SetWorldScale3D(FVector(.4f));
   const FVector Previous=ShellAt(FMath::Max(LaunchDelay,Age-.12f)),Delta=ShellAt(Age)-Previous;
   Instance(Energy,EnergyCount,(ShellAt(Age)+Previous)*.5f,FVector(Delta.Size(),12,12),Delta.Rotation());
  }
  else if(Attack==EBBSBFieldAttackKind::Artillery && Phase>=0.f && Phase<ActiveSeconds)
  {
   Body->SetVisibility(true);Body->SetWorldLocation(ImpactPoint+FVector(0,0,AttackHeight*.5f));
   Body->SetWorldScale3D(FVector(AttackRadius*2.f,AttackRadius*2.f,AttackHeight)/100.f);
   if(auto* MID=Cast<UMaterialInstanceDynamic>(Body->GetMaterial(0))) MID->SetScalarParameterValue(TEXT("Density"),.32f*(1.f-Phase/ActiveSeconds));
  }
  else if(Attack==EBBSBFieldAttackKind::Slam && Phase<ActiveSeconds+.25f)
  {
   const float Drop=FMath::SmoothStep(WarningSeconds-.2f,WarningSeconds,Age);
   Body->SetVisibility(true);Body->SetWorldLocation(ImpactPoint+FVector(0,0,FMath::Lerp(680.f,65.f,Drop)));
   Body->SetWorldScale3D(FVector(1.5f,1.5f,1.3f)*(1.f-FMath::SmoothStep(ActiveSeconds,ActiveSeconds+.25f,Phase)));
   if(Phase<0.f) Instance(Telegraph,MarkerCount,ImpactPoint+FVector(0,0,340),FVector(6,6,600));
  }
 }
 Trim(Telegraph,MarkerCount);Trim(Energy,EnergyCount);Trim(Stones,StoneCount);
}
