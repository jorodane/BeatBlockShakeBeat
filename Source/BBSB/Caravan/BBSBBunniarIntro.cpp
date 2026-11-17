#include "Caravan/BBSBCaravan.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
void ABBSBBunniarCompanion::BeginOpeningRest(FTransform Pose)
{ bOpeningRest=true;OpeningRestTransform=Pose;SetActorTransform(Pose);BandageA->SetVisibility(true);BandageB->SetVisibility(true); }
void ABBSBBunniarCompanion::SetOpeningBandaged(bool Visible)
{ BandageA->SetVisibility(Visible);BandageB->SetVisibility(Visible); }
void ABBSBBunniarCompanion::EndOpeningRest()
{
 // Leaving the desk changes the pose, not the mouth-bandage story state.
 // The opening explicitly clears it at the start of each new playthrough.
 if(!bOpeningRest)return;bOpeningRest=false;
 SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=GetWorld()->GetTimeSeconds();ReturnTransitSeconds=.5f;bSurfaceReturning=true;
}
