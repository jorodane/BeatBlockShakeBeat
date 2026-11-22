#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BBSBPlacement.generated.h"
class UStaticMesh;class UMaterialInterface;

/** Geometry belongs to the placed object, never to an unrelated world position. */
USTRUCT(BlueprintType)
struct FBBSBDeviceSurface
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UStaticMesh> Mesh;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(MakeEditWidget)) FTransform Transform;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<TObjectPtr<UMaterialInterface>> Materials;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bBlockMovement=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bVisible=true;
};

/** Native fallback -> designer-authored Blueprint, also used by runtime spawners. */
UCLASS(BlueprintType)
class BBSB_API UBBSBPlacementClasses : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TMap<FName,TSoftClassPtr<AActor>> Classes;
};

UCLASS()
class BBSB_API UBBSBPlacementAuthoringTools : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure,Category="BBSB|Placement") static UClass* ResolvePlacementClass(UClass* Requested);
 UFUNCTION(BlueprintPure,Category="BBSB|Placement") static UClass* ResolvePlacementPreset(FName Key,UClass* Requested);
 /** Editor migration retains the actor name/path, editable values and references. */
 UFUNCTION(BlueprintCallable,Category="BBSB|Placement") static AActor* ReplacePlacedActor(AActor* Original,TSubclassOf<AActor> BlueprintClass);
 UFUNCTION(BlueprintCallable,Category="BBSB|Placement") static int32 LoadPlacedActorsForEditing(UWorld* World);
};
