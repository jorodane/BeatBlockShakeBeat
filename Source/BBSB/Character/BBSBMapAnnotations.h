#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BBSBMapAnnotations.generated.h"

USTRUCT(BlueprintType)
struct FBBSBMapMarker
{
 GENERATED_BODY()
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) FGuid Id;
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) FName Level;
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) FVector Position=FVector::ZeroVector;
};

UCLASS()
class BBSB_API UBBSBMapMarkerSave : public USaveGame
{
 GENERATED_BODY()
public:
 UPROPERTY(SaveGame) int32 Version=1;
 UPROPERTY(SaveGame) TArray<FBBSBMapMarker> Markers;
};

namespace BBSBMapMarkerRules
{
 inline bool Valid(const TArray<FBBSBMapMarker>& Markers)
 {
  if(Markers.Num()>4096)return false;TSet<FGuid> Seen;
  for(const auto& M:Markers)
  {
   if(!M.Id.IsValid() || M.Level.IsNone() || M.Position.ContainsNaN() || !FMath::IsFinite(M.Position.X) || !FMath::IsFinite(M.Position.Y) || !FMath::IsFinite(M.Position.Z) || Seen.Contains(M.Id))return false;
   Seen.Add(M.Id);
  }
  return true;
 }
}
