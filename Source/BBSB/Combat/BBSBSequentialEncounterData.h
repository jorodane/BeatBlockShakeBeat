#pragma once
#include "CoreMinimal.h"
#include "BBSBSequentialEncounterData.generated.h"
class ABBSBInkEnemy;
USTRUCT(BlueprintType)
struct FBBSBEncounterSpawnSample
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) double Distance=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector Position=FVector::ZeroVector;
};
USTRUCT()
struct FBBSBEncounterEnemySave
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) TSoftClassPtr<ABBSBInkEnemy> Class;
 UPROPERTY(SaveGame) FTransform Transform;
 UPROPERTY(SaveGame) float Health=1;
 UPROPERTY(SaveGame) float WarmupLeft=0;
 UPROPERTY(SaveGame) int32 EcstaticBudget=-1;
 UPROPERTY(SaveGame) int32 NoteBudget=-1;
 UPROPERTY(SaveGame) int32 EcstaticDropped=0;
 UPROPERTY(SaveGame) int32 NotesDropped=0;
 UPROPERTY(SaveGame) int32 DropSerial=0;
 UPROPERTY(SaveGame) float LowestHealthFraction=1;
};
USTRUCT()
struct FBBSBEncounterSave
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FString Path;
 UPROPERTY(SaveGame) double Cursor=0;
 UPROPERTY(SaveGame) int32 Direction=1;
 UPROPERTY(SaveGame) int32 Defeats=0;
 UPROPERTY(SaveGame) int32 Serial=0;
 UPROPERTY(SaveGame) bool bStarted=false;
 UPROPERTY(SaveGame) TArray<FBBSBEncounterEnemySave> Living;
 UPROPERTY(SaveGame) TArray<float> PendingDelays;
};
