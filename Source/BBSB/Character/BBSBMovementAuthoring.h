#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BBSBMovementAuthoring.generated.h"
class UAnimInstance;
class UBlendSpace;
class UPoseSearchDatabase;
class UPoseSearchSchema;

USTRUCT(BlueprintType)
struct FBBSBMotionMatchDebug
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool bValid=false;
 UPROPERTY(BlueprintReadOnly) FString Animation;
 UPROPERTY(BlueprintReadOnly) FString Database;
 UPROPERTY(BlueprintReadOnly) FVector BlendParameters=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) float Time=0;
 UPROPERTY(BlueprintReadOnly) float Cost=0;
 UPROPERTY(BlueprintReadOnly) int32 IndexedPoses=0;
 UPROPERTY(BlueprintReadOnly) int32 HistoryEntries=0;
};

/** Small editor authoring helpers; gameplay still owns capsule movement and action timing. */
UCLASS()
class BBSB_API UBBSBMovementAuthoring : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="BBSB|Movement|Authoring") static bool RefreshBlendSpace(UBlendSpace* BlendSpace);
 UFUNCTION(BlueprintPure,Category="BBSB|Movement|Authoring") static TArray<float> BlendWeights(UBlendSpace* BlendSpace,FVector Input);
 UFUNCTION(BlueprintCallable,Category="BBSB|Movement|Authoring") static bool ConfigureMotionDatabase(UPoseSearchSchema* Schema,UPoseSearchDatabase* Database,UBlendSpace* BlendSpace,bool bDash);
 UFUNCTION(BlueprintCallable,Category="BBSB|Movement|Authoring") static bool InstallMotionMatching(UObject* BlueprintAsset,UPoseSearchDatabase* Database);
 UFUNCTION(BlueprintPure,Category="BBSB|Movement") static FBBSBMotionMatchDebug MotionMatchingDebug(UAnimInstance* Instance);
 static void MatchAfterDash(UAnimInstance* Instance);
};
