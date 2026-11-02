#pragma once
#include "CoreMinimal.h"
class UWorld;
class ACharacter;

// Preserve the low seven bits used by cel shading and interaction outlines.
// The high bit opts a primitive into occluded-only visibility.
namespace BBSBVisibilityStencil
{
 constexpr int32 Occluded=128,Player=129,Projectile=134;
 BBSB_API bool Enabled(UWorld* World);
 // Register held/floating weapons and the companion together so their own
 // opaque surfaces do not produce false occlusion fill on one another.
 BBSB_API void UpdateHandOccluders(ACharacter* Character,bool Active);
 FORCEINLINE int32 Focus(int32 Base,bool Locked,UWorld* World)
 { return Base | ((Locked && Enabled(World)) ? Occluded:0); }
}
