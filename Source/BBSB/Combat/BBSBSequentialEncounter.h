#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/BBSBSequentialEncounterData.h"
#include "BBSBSequentialEncounter.generated.h"
class USplineComponent;class UPCGComponent;class UDecalComponent;class UBoxComponent;
class ABBSBCharacter;class ABBSBInkEnemy;class ABBSBStage01Director;class UPCGGraph;

/** Editor-only PCG samples. The encounter bakes them into its cooked sample array. */
UCLASS()
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
 double ReadyAt=0;
 bool bPrepared=false;
 FVector Position=FVector::ZeroVector;
 UPROPERTY(Transient) TObjectPtr<UDecalComponent> Warning;
};

/** Opt-in free-field respawns. This actor never changes player input, camera, rewards or existing shots. */
UCLASS(Blueprintable)
class BBSB_API ABBSBSequentialEncounter : public AActor
{
 GENERATED_BODY()
public:
 ABBSBSequentialEncounter();
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Tick(float DT) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USplineComponent> Path;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UPCGComponent> SampleGenerator;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> GenerationBounds;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Encounter",meta=(DisplayName="사용")) bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wave",meta=(ClampMin="1",ClampMax="8",DisplayName="등장 수")) int32 InitialCount=2;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wave",meta=(ClampMin=".65",Units="s",DisplayName="딜레이")) float SpawnDelay=1.4f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wave",meta=(ClampMin="50",Units="cm",DisplayName="이동량")) float AdvanceDistance=450.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Path",meta=(DisplayName="닫힌 경로: 시계 방향")) bool bClockwise=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Path",meta=(DisplayName="열린 경로: 역방향 시작")) bool bReverseOpenPath=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Path",meta=(Units="cm")) float StartDistance=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Encounter") TArray<TSubclassOf<ABBSBInkEnemy>> EnemyClasses;
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
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sampling") TSoftObjectPtr<UPCGGraph> SamplingGraph;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Sampling") TArray<FBBSBEncounterSpawnSample> SpawnSamples;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") TArray<TObjectPtr<ABBSBInkEnemy>> ActiveEnemies;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") bool bStarted=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") int32 Defeats=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") int32 SpawnSerial=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") int32 PendingCount=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") double CursorDistance=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") int32 TravelDirection=1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Runtime") FString Status;
 UFUNCTION(CallInEditor,BlueprintCallable,Category="Sampling",meta=(DisplayName="스폰 지점 갱신")) void RebuildSpawnSamples();
 UFUNCTION(BlueprintPure,Category="Encounter") bool CanSpawnNow() const;
 UFUNCTION(BlueprintCallable,Category="Encounter") void ResetEncounter();
 FBBSBEncounterSave CaptureState() const;
 void RestoreState(const FBBSBEncounterSave& State);
private:
 UPROPERTY(Transient) TArray<FBBSBPendingEncounterSpawn> Pending;
 void BakeSamples(UPCGComponent* Component);
 void QueueSpawn(float Delay);
 void ClearRuntime();
 void SetInitialDirection();
 double NextDistance(double Step,int32& Direction) const;
 bool FindSpawn(FVector& Position,double& Distance,int32& Direction) const;
 bool ValidateSpawn(FVector Point,FVector& Result,const ABBSBInkEnemy* Ignore=nullptr) const;
 ABBSBInkEnemy* CreateEnemy(UClass* Class,FVector Position,float Warmup);
};
