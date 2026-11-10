#include "Character/BBSBRespawn.h"
#include "BBSBCharacter.h"
#include "Character/BBSBViewOcclusion.h"
#include "Caravan/BBSBCaravan.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/MeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

float UBBSBRespawnComponent::ClearLiftHeight(FVector Position) const
{
 const float Height=FMath::Clamp(WarpLiftHeight,10.f,150.f);FHitResult Hit;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBWarpLift),false,Player);Query.AddIgnoredActor(Buddy);
 for(AActor* Gear:HiddenTravelWeapons)Query.AddIgnoredActor(Gear);
 const auto* Capsule=Player->GetCapsuleComponent();
 const bool Blocked=GetWorld()->SweepSingleByObjectType(Hit,Position,Position+FVector::UpVector*Height,FQuat::Identity,
  BBSBCombatVisuals::GeometryTypes(),FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query);
 return Blocked ? FMath::Clamp(Hit.Distance-3.f,0.f,Height):Height;
}
void UBBSBRespawnComponent::BeginTravelFade()
{
 RestoreTravelFade();
 if(auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr)TravelFadeCamera=PC->PlayerCameraManager;
 SetTravelOpacity(1.f);
}
void UBBSBRespawnComponent::SetTravelOpacity(float Opacity)
{
 TravelOpacity=FMath::Clamp(Opacity,0.f,1.f);
 if(auto* Camera=TravelFadeCamera.Get())Camera->SetManualCameraFade(1.f-TravelOpacity,FLinearColor::Black,false);
}
void UBBSBRespawnComponent::RestoreTravelFade()
{
 if(auto* Camera=TravelFadeCamera.Get())Camera->StopCameraFade();
 TravelFadeCamera.Reset();TravelOpacity=1.f;
}
