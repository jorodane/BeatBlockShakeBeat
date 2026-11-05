#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

void ABBSBInkEnemy::RestoreHitVibration()
{
 for(const auto& Weak:HitVisualRoots)if(auto* Part=Weak.Get())Part->SetRelativeLocation(Part->GetRelativeLocation()-HitVibrationOffset);
 HitVibrationOffset=FVector::ZeroVector;
}
void ABBSBInkEnemy::BeginHitVibration(bool Critical)
{
 HitVisualRoots.Reset();TArray<UMeshComponent*> Parts;GetComponents(Parts);
 for(auto* Part:Parts)
 {
  if(!Part->IsVisible() || Part==GetRootComponent())continue;
  bool Nested=false;
  for(auto* Parent=Part->GetAttachParent();Parent;Parent=Parent->GetAttachParent())if(Parent->IsA<UMeshComponent>()) { Nested=true;break; }
  if(!Nested)HitVisualRoots.Add(Part);
 }
 HitVibrationAt=GetWorld()->GetTimeSeconds();HitVibrationSeconds=Critical ? .24f:.16f;
 HitVibrationAmplitude=FMath::Clamp(Critical ? CriticalVibrationDistance:HitVibrationDistance,0.f,25.f);
}
void ABBSBInkEnemy::TickHitVibration()
{
 const float Age=GetWorld()->GetTimeSeconds()-HitVibrationAt;
 if(bConcertoSuspended)BBSBCombatVisuals::Tint(Body,Age<.13f ? (bLastHitCritical ? FLinearColor(1.f,.82f,.38f):FLinearColor(.9f,.65f,.7f)):Tint);
 if(bDefeated || Age<0 || Age>=HitVibrationSeconds)return;
 const float Envelope=FMath::Pow(1-Age/HitVibrationSeconds,2.f)*HitVibrationAmplitude;
 HitVibrationOffset=FVector(0,FMath::Cos(Age*130.f)*Envelope,FMath::Sin(Age*100.f)*Envelope*.35f);
 for(const auto& Weak:HitVisualRoots)if(auto* Part=Weak.Get())Part->SetRelativeLocation(Part->GetRelativeLocation()+HitVibrationOffset);
 // Hit flash remains visible even when the enemy's AI/bobbing is suspended.
}
void ABBSBInkEnemy::SetConcertoSuspended(bool Suspend,UObject* PauseSource)
{
 if(Suspend)
 {
  if(bConcertoSuspended || bDefeated)return;
  bConcertoSuspended=true;ConcertoPauseOwner=PauseSource;ConcertoPausedAt=GetWorld()->GetTimeSeconds();
  auto* Move=GetCharacterMovement();bMovementTickBeforeConcerto=Move->IsComponentTickEnabled();VelocityBeforeConcerto=Move->Velocity;
  ConsumeMovementInputVector();Move->StopMovementImmediately();Move->SetComponentTickEnabled(false);
  TArray<USkeletalMeshComponent*> Meshes;GetComponents(Meshes);
  for(auto* VisualMesh:Meshes)if(!VisualMesh->bPauseAnims) { ConcertoPausedMeshes.Add(VisualMesh);VisualMesh->bPauseAnims=true; }
  if(auto* AI=Cast<AAIController>(GetController()))if(auto* Brain=AI->GetBrainComponent())if(!Brain->IsPaused()) { Brain->PauseLogic(TEXT("Concerto"));bBrainPausedByConcerto=true; }
  return;
 }
 if(!bConcertoSuspended || (PauseSource && ConcertoPauseOwner.Get()!=PauseSource))return;
 const double Duration=FMath::Max(0.,GetWorld()->GetTimeSeconds()-ConcertoPausedAt);
 auto Shift=[&](double& Deadline){ if(Deadline>=0)Deadline+=Duration; };
 Shift(NextFieldVolleyAt);Shift(FieldLastSeenAt);Shift(FieldReleaseAt);Shift(FieldRecoveryUntil);Shift(WeaponImpulseUntil);
 Shift(NextFieldWanderAt);Shift(FieldWanderProgressAt);Shift(FieldWanderStartedAt);Shift(BeatFireArmedAt);Shift(LastFieldMajorAt);
 // Resume on the musical grid without replaying beats missed during the cutscene.
 LastFieldBeatTick=-1;
 bConcertoSuspended=false;ConcertoPauseOwner.Reset();
 for(const auto& Weak:ConcertoPausedMeshes)if(auto* VisualMesh=Weak.Get())VisualMesh->bPauseAnims=false;ConcertoPausedMeshes.Reset();
 GetCharacterMovement()->SetComponentTickEnabled(bMovementTickBeforeConcerto);
 if(!bDefeated)
 {
  auto* Move=GetCharacterMovement();Move->Velocity=VelocityBeforeConcerto;
  if(auto* AI=Cast<AAIController>(GetController()))if(auto* Brain=AI->GetBrainComponent())if(bBrainPausedByConcerto)Brain->ResumeLogic(TEXT("Concerto ended"));
 }
 bBrainPausedByConcerto=false;
}
