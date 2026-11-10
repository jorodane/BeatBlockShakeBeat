#include "Caravan/BBSBCaravan.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
void ABBSBBunniarCompanion::BeginOpeningRest(FTransform Pose)
{ bOpeningRest=true;OpeningRestTransform=Pose;SetActorTransform(Pose);BandageA->SetVisibility(true);BandageB->SetVisibility(true); }
void ABBSBBunniarCompanion::EndOpeningRest()
{
 if(!bOpeningRest)return;bOpeningRest=false;BandageA->SetVisibility(false);BandageB->SetVisibility(false);
 SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=GetWorld()->GetTimeSeconds();ReturnTransitSeconds=.5f;bSurfaceReturning=true;
}
