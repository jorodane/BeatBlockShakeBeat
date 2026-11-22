#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/BBSBSequentialEncounterData.h"
#include "BBSBSequentialEncounter.generated.h"
class USplineComponent;class UDecalComponent;
class ABBSBInkEnemy;class ABBSBStage01Director;

/** Retained to load older levels and their retired PCG graph. */
UCLASS(NotPlaceable)
class BBSB_API ABBSBEncounterSamplePoint : public AActor
{
 GENERATED_BODY()
public:
 ABBSBEncounterSamplePoint();
 UPROPERTY(EditAnywhere,BlueprintReadWrite) double DistanceAlongPath=0;
};
USTRUCT()
struct FBBSBPendingEncounterSpawn
{
 GENERATED_BODY()
 double Distance=0.;
 FVector Position=FVector::ZeroVector;
 UPROPERTY(Transient) TObjectPtr<UDecalComponent> Warning;
};

/** Opt-in free-field waves. This actor never changes player input, camera, rewards or existing shots. */
UCLASS(Blueprintable)
class BBSB_API ABBSBSequentialEncounter : public AActor
{
 GENERATED_BODY()
public:
 ABBSBSequentialEncounter();
 virtual void Serialize(FArchive& Ar) override;
 virtual void PostLoad() override;
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Tick(float DT) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Monster Spawning|Path") TObjectPtr<USplineComponent> Path;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Encounter",meta=(DisplayName="사용")) bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Monster Spawning",meta=(DisplayName="웨이브")) TArray<FBBSBEncounterWave> Waves;
 /** Start the full wave list again after the final wave clears. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Monster Spawning",meta=(DisplayName="웨이브 무한 반복")) bool bRepeatWave=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Monster Spawning|Path",meta=(ClampMin="0",Units="cm",DisplayName="첫 등장 위치 (스플라인 거리)")) float StartDistance=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Monster Spawning|Path",meta=(DisplayName="닫힌 경로: 시계 방향")) bool bClockwise=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Monster Spawning|Path",meta=(DisplayName="열린 경로: 역방향 시작")) bool bReverseOpenPath=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TObjectPtr<ABBSBStage01Director> Story;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") bool bRequireFirstWeapon=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Safety",meta=(Units="cm")) float ActivationRadius=1400.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Safety",meta=(Units="cm")) float MinimumPlayerDistance=500.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Safety",meta=(Units="cm")) float MaximumPlayerDistance=2200.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Safety",meta=(Units="cm")) float MinimumEnemySpacing=260.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Safety",meta=(ClampMin=".25",Units="s")) float AttackGraceSeconds=.85f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Safety",meta=(Units="s")) float WarningSeconds=.65f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Safety",meta=(Units="cm")) float GroundProbeUp=250.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Safety",meta=(Units="cm")) float GroundProbeDown=700.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") TArray<TObjectPtr<ABBSBInkEnemy>> ActiveEnemies;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") bool bStarted=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") bool bCompleted=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") int32 WaveIndex=INDEX_NONE;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") int32 Defeats=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") int32 SpawnSerial=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") int32 PendingCount=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") double CursorDistance=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") int32 TravelDirection=1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") FString Status;
 UFUNCTION(BlueprintPure,Category="Encounter") bool CanSpawnNow() const;
 UFUNCTION(BlueprintCallable,Category="Encounter") void ResetEncounter();
 FBBSBEncounterSave CaptureState() const;
 void RestoreState(const FBBSBEncounterSave& State);
private:
 // Serialized names from pre-wave level and Blueprint packages.
 UPROPERTY() TArray<TSubclassOf<ABBSBInkEnemy>> EnemyClasses;
 UPROPERTY() float SpawnDelay=1.4f;
 UPROPERTY() float AdvanceDistance=450.f;
 bool bReadLegacyWaves=false;

 UPROPERTY(Transient) TArray<FBBSBPendingEncounterSpawn> Pending;
 UPROPERTY(Transient) TSubclassOf<ABBSBInkEnemy> BatchClass;
 UPROPERTY(Transient) TArray<FBBSBEncounterEnemySave> RestoreQueue;
 FRandomStream WaveRandom;
 double NextActionAt=0.;
 double NextRestoreAt=0.;
 int32 EntryIndex=0;
 int32 RepeatIndex=0;

 void ClearSpawnWarnings();
 void ClearRuntime();
 void SetInitialDirection();
 const FBBSBEncounterSpawnEntry* CurrentEntry() const;
 void StartWave(int32 Index,double Now,bool bMoveAnchor);
 void ScheduleEntry(double BaseTime);
 void FinishBurst(double Now);
 bool AdvanceWaveIfCleared(double Now);
 void TickBatch(double Now,float WarningTime);
 double AdvanceAlongPath(double From,double Step,int32& Direction) const;
 void FindSpawnPositions(const ABBSBInkEnemy& Default,int32 Limit,TArray<FBBSBPendingEncounterSpawn>& Plans) const;
 bool ValidateSpawn(FVector Point,const ABBSBInkEnemy& Default,FVector& Result) const;
 ABBSBInkEnemy* CreateEnemy(UClass* RequestedClass,FVector Position,float Warmup,int32 SavedVoice=INDEX_NONE,int32 SavedVoiceIndex=INDEX_NONE);
 bool TryRestoreEnemy(const FBBSBEncounterEnemySave& Saved,bool bAllowNearby);
 void RestoreLegacySchedule(const FBBSBEncounterSave& Saved,double Now);
};
