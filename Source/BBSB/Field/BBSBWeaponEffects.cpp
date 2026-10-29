#include "BBSBFieldActions.h"
#include "BBSBBladeTrail.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"

void UBBSBFieldActionComponent::FinishTravelTrail(FWeaponAction& A)
{
 if(auto* Trail=A.TravelTrail.Get()) Trail->Finish();
 A.TravelTrail.Reset();A.bTravelTrailFinished=true;
}

void UBBSBFieldActionComponent::UpdateTravelTrail(FWeaponAction& A,float PreviousT,float T)
{
 const auto* Set=ActiveWeaponAnimationSet.Get();
 if(!Set || !Set->bEnableTravelTrail || !Set->TravelTrailMaterial || A.bTravelTrailFinished) return;
 const float End=A.bAuthoredMotion ? FMath::Clamp(A.Motion.ActiveStart,0.f,1.f):.2f;
 if(End<=0 || PreviousT>=End) { FinishTravelTrail(A);return; }
 AActor* Visual=PrototypeWeapon==EBBSBPrototypeWeapon::Crescent && A.Channel==1 ? OtherFloatingWeapon.Get():FloatingWeapon.Get();
 UStaticMeshComponent* Model=A.Channel==1 && PrototypeWeapon==EBBSBPrototypeWeapon::Crescent ? OtherBlade.Get():Blade.Get();
 if(!Visual || !Model || !Model->GetStaticMesh()) return;
 if(!A.TravelTrail.IsValid())
 {
  auto* Trail=NewObject<UBBSBBladeTrailComponent>(Visual,MakeUniqueObjectName(Visual,UBBSBBladeTrailComponent::StaticClass(),TEXT("WeaponTravelTrail")));
  Visual->AddInstanceComponent(Trail);Trail->SetupAttachment(Visual->GetRootComponent());Trail->SetAbsolute(true,true,true);
  Trail->SetWorldLocation(Visual->GetActorLocation());Trail->TrailMaterial=Set->TravelTrailMaterial;
  Trail->Lifetime=Set->TravelTrailLifetime;Trail->Color=Set->TravelTrailColor;Trail->RegisterComponent();A.TravelTrail=Trail;
 }
 FVector Root=Set->BladeTrailRoot,Tip=Set->BladeTrailTip;
 if(PrototypeWeapon!=EBBSBPrototypeWeapon::Sword)
 {
  const FBox Bounds=Model->GetStaticMesh()->GetBoundingBox();const FVector E=Bounds.GetExtent();
  const FVector Axis=E.X>=E.Y && E.X>=E.Z ? FVector(E.X,0,0):E.Y>=E.Z ? FVector(0,E.Y,0):FVector(0,0,E.Z);
  Root=Bounds.GetCenter()-Axis;Tip=Bounds.GetCenter()+Axis;
 }
 const float To=FMath::Min(T,End);
 // Flight may spin a full turn in 0.1 seconds; sample the authored path, independently of damage.
 const int32 Steps=FMath::Clamp(FMath::CeilToInt((To-PreviousT)/FMath::Max(.001f,A.Motion.BlendIn)*30.f),1,32);
 for(int32 Step=0;Step<=Steps;++Step)
 {
  const FTransform MeshPose=Model->GetRelativeTransform()*PrototypeAttackPose(A,FMath::Lerp(PreviousT,To,float(Step)/Steps));
  const FVector P=MeshPose.TransformPosition(Root),Q=MeshPose.TransformPosition(Tip),Center=(P+Q)*.5;
  A.TravelTrail->AddBladeSample(FMath::Lerp(Center,P,Set->TravelTrailWidth),FMath::Lerp(Center,Q,Set->TravelTrailWidth));
 }
 if(T>=End) FinishTravelTrail(A);
}

void UBBSBFieldActionComponent::UpdateAttackHighlight(FWeaponAction& A,float T)
{
 const auto* Set=ActiveWeaponAnimationSet.Get();
 const float End=A.bAuthoredMotion ? A.Motion.ActiveEnd:.64f;
 if(!Set || !Set->AttackHighlightMesh || !Set->AttackHighlightMaterial || A.bImpactCuePlayed || T>=End)
 { if(auto* Marker=A.TargetHighlight.Get()) Marker->DestroyComponent();A.TargetHighlight.Reset();return; }
 if(!A.TargetHighlight.IsValid())
 {
  auto* Marker=NewObject<UStaticMeshComponent>(Character,MakeUniqueObjectName(Character,UStaticMeshComponent::StaticClass(),TEXT("AttackTargetHighlight")));
  Character->AddInstanceComponent(Marker);Marker->SetupAttachment(Character->GetRootComponent());Marker->SetAbsolute(true,true,true);
  Marker->SetStaticMesh(Set->AttackHighlightMesh);Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);Marker->SetCastShadow(false);Marker->SetReceivesDecals(false);
  Marker->SetMaterial(0,UMaterialInstanceDynamic::Create(Set->AttackHighlightMaterial,Marker));Marker->RegisterComponent();A.TargetHighlight=Marker;
 }
 FVector Normal=A.bSwordThrow ? -A.SwordThrowFrame.GetAxisZ():-A.Direction;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBAttackMarker),false,Character);Query.AddIgnoredActor(FloatingWeapon);Query.AddIgnoredActor(OtherFloatingWeapon);
 FHitResult Surface;
 if(GetWorld()->LineTraceSingleByChannel(Surface,A.Target+Normal*18.f,A.Target-Normal*18.f,ECC_Visibility,Query)) Normal=Surface.ImpactNormal;
 auto* Marker=A.TargetHighlight.Get();Marker->SetWorldLocationAndRotation(A.Target+Normal*1.2f,FRotationMatrix::MakeFromZ(Normal).ToQuat());
 const float Lock=A.bAuthoredMotion ? FMath::Max(.001f,A.Motion.ActiveStart):.2f;
 const float Pulse=.86f+.14f*FMath::Sin(GetWorld()->GetTimeSeconds()*22.f);
 Marker->SetWorldScale3D(FVector(Set->AttackHighlightRadius/50.f*(1.18f-.18f*FMath::Clamp(T/Lock,0.f,1.f))));
 if(auto* MID=Cast<UMaterialInstanceDynamic>(Marker->GetMaterial(0)))
 { MID->SetVectorParameterValue(TEXT("Tint"),Set->AttackHighlightColor);MID->SetScalarParameterValue(TEXT("Density"),Pulse); }
}

void UBBSBFieldActionComponent::EmitAttackImpact(FWeaponAction& A,const FVector& Point,const FVector& Normal,bool bEnemyHit)
{
 A.bImpactCuePlayed=true;
 if(auto* Marker=A.TargetHighlight.Get()) Marker->DestroyComponent();A.TargetHighlight.Reset();
 const auto* Set=ActiveWeaponAnimationSet.Get();
 if(Set && Set->AttackImpactScale>0) for(auto* System:{Set->AttackImpactBurst.Get(),Set->AttackImpactRing.Get()}) if(System)
  UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),System,Point+Normal*2.f,FRotationMatrix::MakeFromZ(Normal.IsNearlyZero() ? FVector::UpVector:Normal).Rotator(),FVector(Set->AttackImpactScale*(bEnemyHit ? FMath::Clamp(Set->EnemyImpactScaleMultiplier,1.f,3.f):1.f)),true,true,ENCPoolMethod::AutoRelease,true);
}

void UBBSBFieldActionComponent::EmitSurfaceImpact(FWeaponAction& A)
{
 FVector Normal=A.bSwordThrow ? -A.SwordThrowFrame.GetAxisZ():-A.Direction;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBAttackSurfaceImpact),false,Character);Query.AddIgnoredActor(FloatingWeapon);Query.AddIgnoredActor(OtherFloatingWeapon);
 FHitResult Surface;
 if(GetWorld()->LineTraceSingleByChannel(Surface,A.Target+Normal*18.f,A.Target-Normal*18.f,ECC_Visibility,Query)) EmitAttackImpact(A,Surface.ImpactPoint,Surface.ImpactNormal);
}
