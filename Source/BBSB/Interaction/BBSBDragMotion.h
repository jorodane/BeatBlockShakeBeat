#pragma once
#include "CoreMinimal.h"
class USceneComponent;
class UPrimitiveComponent;
struct FHitResult;

/** Shared translation for Bunniar-driven props. Uses actual collision geometry,
 * pushes movable blockers with their own swept path, and returns the reached fraction.
 * Frames in one group keep their authored relative progress (e.g. two door leaves).
 */
namespace BBSBDragMotion
{
 struct FMove
 {
  USceneComponent* Frame=nullptr;
  FVector Destination=FVector::ZeroVector;
 };
 /** Only the requested mechanism may enter its explicitly authored static housing.
  * Pushed pawns/props still collide with that housing; dynamic bodies cannot be ignored. */
 BBSB_API float Move(TConstArrayView<FMove> Moves,TConstArrayView<UPrimitiveComponent*> TrackHousing={});
 BBSB_API float Move(USceneComponent* Frame,const FVector& Destination,FHitResult* BlockingHit=nullptr);
}
