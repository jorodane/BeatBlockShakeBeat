#pragma once
#include "CoreMinimal.h"
#include "Combat/BBSBCombat.h"
#include "BBSBDuelPracticeArea.generated.h"

class UMaterialInterface;

/** Spacious, editable duel pockets. Each opponent owns an isolated encounter. */
UCLASS()
class BBSB_API ABBSBDuelPracticeArea : public ABBSBCombatPracticeArea
{
 GENERATED_BODY()
public:
 ABBSBDuelPracticeArea();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void SpawnPracticeEnemies() override;
 virtual FVector EntryLocation() const override;
 UFUNCTION(CallInEditor,BlueprintCallable,Category="Duel field") void RebuildField();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Duel field") FVector2D FieldSize=FVector2D(10000,7600);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Duel field") TArray<FVector> DuelPositions;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Duel field") TSoftObjectPtr<UMaterialInterface> GroundMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Duel field") TSoftObjectPtr<UMaterialInterface> PadMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Duel field") TSoftObjectPtr<UMaterialInterface> PathMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Duel field") TSoftObjectPtr<UMaterialInterface> BoundaryMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Duel field") TSoftObjectPtr<UMaterialInterface> AccentMaterial;
private:
 UPROPERTY() TArray<TObjectPtr<USceneComponent>> FieldParts;
};
