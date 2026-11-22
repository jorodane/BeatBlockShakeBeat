#include "Caravan/BBSBBunniarSurface.h"
#include "BBSBCharacter.h"
#include "Interaction/BBSBInteraction.h"
#include "Interaction/BBSBInteractable.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AActor* BBSBBunniarSurface::Resolve(AActor* PhysicalActor)
{
 if(!IsValid(PhysicalActor))return nullptr;
 if(PhysicalActor->Implements<UBBSBBunniarSurface>() || UBBSBInteractable::Find(PhysicalActor))return PhysicalActor;
 const auto* Binding=PhysicalActor->FindComponentByClass<UBBSBBunniarSurfaceBinding>();
 auto* Handler=Binding ? Binding->Handler.Get():nullptr;
 return IsValid(Handler) && (Handler->Implements<UBBSBBunniarSurface>() || UBBSBInteractable::Find(Handler)) ? Handler:nullptr;
}

bool BBSBBunniarSurface::CanUse(AActor* Handler, ACharacter* Character)
{
 const auto* Interaction = UBBSBInteractable::Find(Handler);
 return !Interaction || Interaction->CanStartInteraction(Character);
}

bool BBSBBunniarSurface::TryQuickUse(AActor* Handler, ACharacter* Character, ABBSBBunniarCompanion* Bunniar, const FHitResult& Hit)
{
 if (!IsValid(Handler)) return false;
 auto* Interaction = UBBSBInteractable::Find(Handler);
 if (Interaction && !Interaction->CanStartInteraction(Character)) return true;
 const bool bLegacyResult = Handler->Implements<UBBSBBunniarSurface>() && IBBSBBunniarSurface::Execute_BunniarQuickUse(Handler, Bunniar);
 if (!IsValid(Interaction)) return bLegacyResult;
 if (!bLegacyResult && Interaction->RequiresAttachment()) return false;

 FBBSBInteractContext Context;
 Context.Character = Character;
 Context.Bunniar = Bunniar;
 Context.Surface = Hit.GetComponent();
 Context.Point = Hit.ImpactPoint;
 Context.Normal = Hit.ImpactNormal;
 Interaction->BeginInteraction(Context);
 return true;
}
void BBSBBunniarSurface::Bind(AActor* PhysicalActor,AActor* Handler)
{
 if(!IsValid(PhysicalActor) || !IsValid(Handler) || !PhysicalActor->GetWorld() || !PhysicalActor->GetWorld()->IsGameWorld())return;
 auto* Binding=PhysicalActor->FindComponentByClass<UBBSBBunniarSurfaceBinding>();
 if(!Binding) { Binding=NewObject<UBBSBBunniarSurfaceBinding>(PhysicalActor,NAME_None,RF_Transient);PhysicalActor->AddOwnedComponent(Binding);Binding->RegisterComponent(); }
 Binding->Handler=Handler;PhysicalActor->Tags.AddUnique(TEXT("BBSBPhysicalInteraction"));
 if(auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(PhysicalActor,0));Player && Player->Interaction)
  Player->Interaction->RegisterInteractionCandidate(PhysicalActor);
}
