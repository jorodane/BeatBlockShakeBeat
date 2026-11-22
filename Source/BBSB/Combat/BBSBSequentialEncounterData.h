#pragma once
#include "CoreMinimal.h"
#include "BBSBSequentialEncounterData.generated.h"
class ABBSBInkEnemy;

/** A timed batch. Each repeat makes one probability draw for the whole batch. */
USTRUCT(BlueprintType)
struct FBBSBEncounterSpawnEntry
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Spawn",meta=(DisplayName="몬스터")) TSubclassOf<ABBSBInkEnemy> MonsterClass;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Spawn",meta=(ClampMin="1",DisplayName="마릿수")) int32 Count=1;
 /** Total number of attempts, including the first one. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Spawn",meta=(ClampMin="1",DisplayName="반복 횟수")) int32 Repeat=1;
 /** Wait before the first batch and before each repetition. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Spawn",meta=(ClampMin="0",Units="s",DisplayName="등장 딜레이")) float Delay=0.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Spawn",meta=(ClampMin="0",ClampMax="1",DisplayName="등장 확률")) float Probability=1.f;
};

USTRUCT(BlueprintType)
struct FBBSBEncounterWave
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wave",meta=(TitleProperty="MonsterClass",DisplayName="등장 항목")) TArray<FBBSBEncounterSpawnEntry> Entries;
 /** Wait after activation or after the preceding wave clears. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wave",meta=(ClampMin="0",Units="s",DisplayName="웨이브 딜레이")) float Delay=1.4f;
 /** Travel from the preceding wave anchor. The first wave uses StartDistance. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wave",meta=(ClampMin="0",Units="cm",DisplayName="스플라인 이동 거리")) float AdvanceDistance=450.f;
};

USTRUCT()
struct FBBSBEncounterEnemySave
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) TSoftClassPtr<ABBSBInkEnemy> Class;
 UPROPERTY(SaveGame) FTransform Transform;
 UPROPERTY(SaveGame) int32 SingerVoice=INDEX_NONE;
 UPROPERTY(SaveGame) int32 SingerVoiceIndex=INDEX_NONE;
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
 // Older records used a list of queued respawn delays.
 UPROPERTY(SaveGame) TArray<float> PendingDelays;
 UPROPERTY(SaveGame) int32 WaveFormat=0;
 UPROPERTY(SaveGame) int32 WaveIndex=INDEX_NONE;
 UPROPERTY(SaveGame) int32 EntryIndex=0;
 UPROPERTY(SaveGame) int32 RepeatIndex=0;
 UPROPERTY(SaveGame) int32 BatchRemaining=0;
 UPROPERTY(SaveGame) TSoftClassPtr<ABBSBInkEnemy> BatchClass;
 UPROPERTY(SaveGame) float DelayLeft=0.f;
 UPROPERTY(SaveGame) int32 RandomSeed=0;
 UPROPERTY(SaveGame) bool bCompleted=false;
};
