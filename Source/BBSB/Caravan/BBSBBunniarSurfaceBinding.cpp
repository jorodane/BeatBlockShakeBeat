#include "Caravan/BBSBBunniarSurface.h"
#include "BBSBCharacter.h"
#include "Interaction/BBSBInteraction.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AActor* BBSBBunniarSurface::Resolve(AActor* PhysicalActor)
{
 if(!IsValid(PhysicalActor))return nullptr;
 if(PhysicalActor->Implements<UBBSBBunniarSurface>())return PhysicalActor;
 const auto* Binding=PhysicalActor->FindComponentByClass<UBBSBBunniarSurfaceBinding>();
 auto* Handler=Binding ? Binding->Handler.Get():nullptr;
 return IsValid(Handler) && Handler->Implements<UBBSBBunniarSurface>() ? Handler:nullptr;
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
