#include "Field/BBSBFieldActions.h"
#include "Combat/BBSBCombat.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Framework/Application/SlateApplication.h"

bool UBBSBFieldActionComponent::IsPrecisionWalking() const
{
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 return PC && PC->IsLocalController() && !PC->IsMoveInputIgnored() && !bEquipmentWheelOpen && !(Combat && Combat->bDead) &&
  (!FSlateApplication::IsInitialized() || FSlateApplication::Get().IsActive()) &&
  (PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl));
}

void UBBSBFieldActionComponent::UpdateHurtSphereGuide()
{
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 const bool Show=IsPrecisionWalking() && Combat;
 if(Show && !HurtSphereGuide)
 {
  auto* Material=HurtSphereGuideMaterial.LoadSynchronous();if(!Material) return;
  HurtSphereGuide=NewObject<UStaticMeshComponent>(Character,TEXT("PlayerHurtSphereGuide"),RF_Transient);
  Character->AddInstanceComponent(HurtSphereGuide);HurtSphereGuide->SetupAttachment(Character->GetRootComponent());
  // Local center follows CharacterMovement in the same frame. Radius stays in world centimetres.
  HurtSphereGuide->SetAbsolute(false,false,true);HurtSphereGuide->SetMobility(EComponentMobility::Movable);
  HurtSphereGuide->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
  HurtSphereGuide->SetMaterial(0,Material);HurtSphereGuide->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  HurtSphereGuide->SetCollisionResponseToAllChannels(ECR_Ignore);HurtSphereGuide->SetGenerateOverlapEvents(false);
  HurtSphereGuide->SetCanEverAffectNavigation(false);HurtSphereGuide->SetCastShadow(false);HurtSphereGuide->bReceivesDecals=false;
  HurtSphereGuide->SetRenderCustomDepth(false);HurtSphereGuide->RegisterComponent();
 }
 if(HurtSphereGuide)
 {
  HurtSphereGuide->SetVisibility(Show);
  if(Show)
  {
   HurtSphereGuide->SetRelativeLocation(Combat->FieldProjectileHurtOffset);
   HurtSphereGuide->SetWorldScale3D(FVector(FMath::Clamp(Combat->FieldProjectileHurtRadius,1.f,80.f)*.02f));
  }
 }
}
