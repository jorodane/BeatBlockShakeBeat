#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"

void ABBSBInkEnemy::RebuildFieldAppearance()
{
 for(UStaticMeshComponent* Part:FieldOrnaments) if(IsValid(Part)) Part->DestroyComponent();FieldOrnaments.Reset();
 FieldRestScale=FVector(.75f,.65f,1.25f);
 if(!bFieldBulletHell || FieldAttack==EBBSBFieldAttackKind::Fan) { Body->SetRelativeScale3D(FieldRestScale);return; }
 auto Part=[&](const TCHAR* Shape,FVector Location,FVector Size,FRotator Rotation,FLinearColor Color)
 {
  auto* Mesh=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(Mesh);Mesh->SetupAttachment(GetRootComponent());
  Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape)));
  Mesh->SetRelativeLocation(Location);Mesh->SetRelativeRotation(Rotation);Mesh->SetRelativeScale3D(Size/100.f);
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetGenerateOverlapEvents(false);Mesh->SetCanEverAffectNavigation(false);
  Mesh->RegisterComponent();BBSBCombatVisuals::Tint(Mesh,Color);FieldOrnaments.Add(Mesh);
 };
 const FLinearColor Dark(.15f,.12f,.2f),Accent=Tint*1.35f;
 if(FieldAttack==EBBSBFieldAttackKind::GroundSpikes)
 {
  FieldRestScale=FVector(1.05f,.9f,.85f);
  for(int32 Index=-1;Index<=1;++Index) Part(TEXT("Cone"),FVector(-10,Index*28.f,45.f),FVector(26,26,65.f-FMath::Abs(Index)*15.f),FRotator(0,0,Index*15.f),Dark);
 }
 else if(FieldAttack==EBBSBFieldAttackKind::SwordWave)
 {
  FieldRestScale=FVector(.65f,.58f,1.35f);
  for(float Side:{-1.f,1.f}) Part(TEXT("Cube"),FVector(18,Side*48,0),FVector(12,18,120),FRotator(0,0,Side*28.f),Accent);
 }
 else if(FieldAttack==EBBSBFieldAttackKind::Slam)
 {
  FieldRestScale=FVector(1.12f,1.05f,1.2f);
  Part(TEXT("Cube"),FVector(10,70,15),FVector(22,22,115),FRotator(0,0,-18),Dark);
  Part(TEXT("Cube"),FVector(10,88,65),FVector(85,65,52),FRotator(0,0,-18),Accent);
 }
 else if(FieldAttack==EBBSBFieldAttackKind::Artillery)
 {
  FieldRestScale=FVector(.95f,.9f,.85f);
  for(float Side:{-1.f,1.f}) Part(TEXT("Cylinder"),FVector(-25,Side*30,40),FVector(27,27,100),FRotator(-35,0,0),Dark);
 }
 else if(FieldAttack==EBBSBFieldAttackKind::Donut)
 {
  FieldRestScale=FVector(.7f,.7f,1.3f);
  for(int32 Index=0;Index<16;++Index)
  {
   const float Yaw=Index*360.f/16.f;Part(TEXT("Cube"),FRotator(0,Yaw,0).Vector()*65.f+FVector(0,0,25),FVector(12,27,14),FRotator(0,Yaw,0),Accent);
  }
 }
 Body->SetRelativeScale3D(FieldRestScale);
}
