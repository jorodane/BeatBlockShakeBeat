#include "BBSBDuelPracticeArea.h"
#include "BBSBCombatVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"

ABBSBDuelPracticeArea::ABBSBDuelPracticeArea()
{
 EnemyCount=5;EncounterID=1000;EnemyHealth=300.f;bFieldBulletHell=true;
 DuelPositions={FVector(-2800,-1500,0),FVector(0,-1500,0),FVector(2800,-1500,0),FVector(-1400,1800,0),FVector(1800,1800,0)};
 GroundMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/CombatField/MI_FieldGround.MI_FieldGround")));
 PadMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/CombatField/MI_DuelPad.MI_DuelPad")));
 PathMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/CombatField/MI_FieldPath.MI_FieldPath")));
 BoundaryMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/CombatField/MI_FieldBoundary.MI_FieldBoundary")));
 AccentMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/CombatField/MI_FieldAccent.MI_FieldAccent")));
 RootComponent->SetMobility(EComponentMobility::Static);
#if WITH_EDITOR
 SetIsSpatiallyLoaded(false);
#endif
}
void ABBSBDuelPracticeArea::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);RebuildField();
}
void ABBSBDuelPracticeArea::RebuildField()
{
 for(USceneComponent* Part:FieldParts) if(IsValid(Part)) Part->DestroyComponent();FieldParts.Reset();
 auto Mesh=[&](const TCHAR* Shape,FVector Position,FVector Dimensions,const TSoftObjectPtr<UMaterialInterface>& Style,bool Solid=true)
 {
  auto* Part=NewObject<UStaticMeshComponent>(this,NAME_None,RF_Transactional);AddInstanceComponent(Part);Part->SetupAttachment(RootComponent);
  Part->SetMobility(EComponentMobility::Static);Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape)));
  Part->SetRelativeLocation(Position);Part->SetRelativeScale3D(Dimensions/100.f);
  Part->SetCollisionProfileName(Solid ? TEXT("BlockAll"):TEXT("NoCollision"));Part->SetGenerateOverlapEvents(false);
  if(auto* Material=Style.LoadSynchronous()) Part->SetMaterial(0,Material);
  Part->RegisterComponent();FieldParts.Add(Part);return Part;
 };
 auto Label=[&](FVector Position,const FString& Text,float Size)
 {
  auto* Part=NewObject<UTextRenderComponent>(this,NAME_None,RF_Transactional);AddInstanceComponent(Part);Part->SetupAttachment(RootComponent);
  Part->SetRelativeLocation(Position);Part->SetRelativeRotation(FRotator(0,180,0));Part->SetText(FText::FromString(Text));Part->SetWorldSize(Size);
  Part->SetHorizontalAlignment(EHTA_Center);Part->SetTextRenderColor(FColor(76,53,71));Part->SetCastShadow(false);Part->RegisterComponent();FieldParts.Add(Part);
 };
 const float X=FMath::Max(8000.f,float(FieldSize.X)),Y=FMath::Max(6000.f,float(FieldSize.Y));
 Mesh(TEXT("Cube"),FVector(0,0,-50),FVector(X,Y,100),GroundMaterial);
 Mesh(TEXT("Cube"),FVector(0,-Y*.5f,100),FVector(X+100,100,200),BoundaryMaterial);
 Mesh(TEXT("Cube"),FVector(0,Y*.5f,100),FVector(X+100,100,200),BoundaryMaterial);
 Mesh(TEXT("Cube"),FVector(-X*.5f,0,100),FVector(100,Y,200),BoundaryMaterial);
 Mesh(TEXT("Cube"),FVector(X*.5f,0,100),FVector(100,Y,200),BoundaryMaterial);
 Mesh(TEXT("Cube"),FVector(0,0,1),FVector(X-500,360,2),PathMaterial);
 for(const FVector& Point:DuelPositions) Mesh(TEXT("Cube"),FVector(Point.X,Point.Y*.5f,1.2f),FVector(280,FMath::Abs(Point.Y),2),PathMaterial);
 const TCHAR* Names[]={TEXT("QUILL / UP > RIGHT > LEFT"),TEXT("RIBBON / LEFT > HOLD UP > RIGHT"),TEXT("WAVER / RIGHT > DOWN > UP"),TEXT("SKIPPER / LEFT > RIGHT > DOWN"),TEXT("SPLASH / TWO GROUND MARKS")};
 const TCHAR* BulletNames[]={TEXT("QUILL / AIMED FAN"),TEXT("RIBBON / QUICK BURSTS"),TEXT("WAVER / SWEEPING FAN"),TEXT("SKIPPER / STAGGERED FAN"),TEXT("SPLASH / RING + AIMED SHOTS")};
 for(int32 Index=0;Index<DuelPositions.Num();++Index)
 {
  const FVector P=DuelPositions[Index];
  Mesh(TEXT("Cylinder"),P+FVector(0,0,1.5f),FVector(2000,2000,3),PadMaterial);
  for(int32 Segment=0;Segment<48;++Segment)
  {
   const float A=360.f*Segment/48.f;auto* Line=Mesh(TEXT("Cube"),P+FRotator(0,A,0).RotateVector(FVector(1000,0,3.5f)),FVector(128,12,2),AccentMaterial,false);
   Line->SetRelativeRotation(FRotator(0,A+90,0));
  }
  // A placement guide is visible in the editor and cannot become another game enemy.
  auto* Preview=Mesh(TEXT("Sphere"),P+FVector(0,0,72),FVector(75,65,125),AccentMaterial,false);Preview->SetHiddenInGame(true);
  Label(P+FVector(-750,-850,90),FString::Printf(TEXT("%d  %s"),Index+1,(bFieldBulletHell ? BulletNames:Names)[FMath::Min(Index,4)]),20.f);
 }
 for(float WallX:{-1400.f,1400.f}) Mesh(TEXT("Cube"),FVector(WallX,-1500,120),FVector(120,1150,240),BoundaryMaterial);
 for(float WallX:{-2900.f,2900.f}) Mesh(TEXT("Cube"),FVector(WallX,350,120),FVector(1500,120,240),BoundaryMaterial);
 Label(FVector(-4000,-1500,240),bFieldBulletHell ? TEXT("BULLET FIELD\nApproach or hit / move and dodge\nFree camera / no combat stance"):TEXT("RHYTHM DUELS\nRead the phrase / defend / counter\nOne opponent per pocket"),32.f);
}
void ABBSBDuelPracticeArea::SpawnPracticeEnemies()
{
 const EBBSBInkPattern Patterns[]={EBBSBInkPattern::Tap,EBBSBInkPattern::Hold,EBBSBInkPattern::Curve,EBBSBInkPattern::Zigzag,EBBSBInkPattern::Area};
 const TCHAR* Names[]={TEXT("Quill"),TEXT("Ribbon"),TEXT("Waver"),TEXT("Skipper"),TEXT("Splash")};
 const TArray<EBBSBParryDirection> Directions[]={
  {EBBSBParryDirection::Up,EBBSBParryDirection::Right,EBBSBParryDirection::Left},
  {EBBSBParryDirection::Left,EBBSBParryDirection::Up,EBBSBParryDirection::Right},
  {EBBSBParryDirection::Right,EBBSBParryDirection::Down,EBBSBParryDirection::Up},
  {EBBSBParryDirection::Left,EBBSBParryDirection::Right,EBBSBParryDirection::Down},
  {EBBSBParryDirection::Up,EBBSBParryDirection::Right,EBBSBParryDirection::Down}};
 for(int32 Index=0;Index<FMath::Min(EnemyCount,DuelPositions.Num());++Index)
 {
  FVector Floor;if(!BBSBCombatVisuals::Ground(GetWorld(),GetActorTransform().TransformPosition(DuelPositions[Index]),Floor)) continue;
  const FTransform Pose(GetActorRotation()+FRotator(0,180,0),Floor+FVector(0,0,73));
  auto* Enemy=GetWorld()->SpawnActorDeferred<ABBSBInkEnemy>(ABBSBInkEnemy::StaticClass(),Pose,this,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
  if(!Enemy) continue;
  Enemy->PracticeArea=this;Enemy->EncounterID=EncounterID+Index;Enemy->EnemyName=Names[Index%5];Enemy->MaximumHealth=EnemyHealth;
  Enemy->MainPattern=Patterns[Index%5];Enemy->AttackDirections=Directions[Index%5];
  Enemy->bFieldBulletHell=bFieldBulletHell;
  if(bFieldBulletHell)
  {
   Enemy->ShotDamage=6.f;
   Enemy->FieldBulletsPerVolley=Index==1 ? 7:Index==4 ? 18:9;
   Enemy->FieldVolleyInterval=Index==1 ? .75f:Index==4 ? 1.5f:1.05f;
   Enemy->FieldShotSpeed=Index==4 ? 540.f:650.f;
  }
  Enemy->Tint=Index%2==0 ? FLinearColor(.46f,.34f,.52f):FLinearColor(.35f,.51f,.51f);
  UGameplayStatics::FinishSpawningActor(Enemy,Pose);
 }
}
FVector ABBSBDuelPracticeArea::EntryLocation() const
{
 const FVector Local=DuelPositions.IsEmpty() ? FVector(-3650,-1500,0):DuelPositions[0]-FVector(850,0,0);
 const FVector Point=GetActorTransform().TransformPosition(Local);FVector Floor;
 return BBSBCombatVisuals::Ground(GetWorld(),Point,Floor) ? Floor+FVector(0,0,96):Point+FVector(0,0,96);
}
