#include "Hunting/BBSBLootBurst.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ABBSBLootBurst::ABBSBLootBurst()
{
 PrimaryActorTick.bCanEverTick=true;
 Puffs=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DefeatInkPuffs"));SetRootComponent(Puffs);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));Puffs->SetStaticMesh(Sphere.Object);
 Puffs->SetCollisionEnabled(ECollisionEnabled::NoCollision);Puffs->SetGenerateOverlapEvents(false);
 Puffs->SetCastShadow(false);Puffs->SetReceivesDecals(false);Puffs->SetCanEverAffectNavigation(false);
 Tags.Add(TEXT("BBSBDefeatBurst"));
}
void ABBSBLootBurst::StartBurst(const FLinearColor& Color)
{
 StartedAt=GetWorld()->GetTimeSeconds();SetLifeSpan(FMath::Max(.2f,Lifetime));
 BBSBCombatVisuals::Tint(Puffs,FMath::Lerp(Color,FLinearColor(.96f,.89f,.91f),.45f));
 Puffs->ClearInstances();Particles.Reset();
 for(int32 I=0;I<FMath::Clamp(PuffCount,4,48);++I)
 {
  const float Angle=I*2.399963f,Speed=FMath::FRandRange(110.f,270.f);
  FPuff P;P.Velocity=FVector(FMath::Cos(Angle)*Speed,FMath::Sin(Angle)*Speed,FMath::FRandRange(70.f,260.f));
  P.Radius=FMath::FRandRange(10.f,23.f);P.Rotation=FRotator::MakeFromEuler(FVector(I*23.f,I*47.f,I*71.f)).Quaternion();Particles.Add(P);
  Puffs->AddInstance(FTransform(P.Rotation,FVector::ZeroVector,FVector(.01f)));
 }
}
void ABBSBLootBurst::Tick(float DT)
{
 Super::Tick(DT);const float Age=GetWorld()->GetTimeSeconds()-StartedAt;
 const float Expansion=FMath::SmoothStep(0.f,.09f,Age),Fade=1.f-FMath::SmoothStep(Lifetime*.36f,Lifetime,Age);
 for(int32 I=0;I<Particles.Num();++I)
 {
  const auto& P=Particles[I];const FVector Position=P.Velocity*Age-FVector::UpVector*(Age*Age*210.f);
  const float Scale=FMath::Max(.001f,P.Radius/50.f*Expansion*Fade);
  Puffs->UpdateInstanceTransform(I,FTransform(P.Rotation,Position,FVector(Scale,Scale*.85f,Scale*1.12f)),false,I==Particles.Num()-1,false);
 }
}
