#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Puzzle/BBSBPuzzleField.h"
#include "Interaction/BBSBInteraction.h"
#include "Combat/BBSBCombat.h"
#include "BBSBStage01.generated.h"
class ABBSBCharacter;class ABBSBStage01Director;class ABBSBStage01Singer;class ABBSBStage01CarrySinger;class ABBSBRespawnPoint;
class UBBSBItemDefinition;class UAudioComponent;class USoundWaveProcedural;class USpotLightComponent;class ACameraActor;
UENUM(BlueprintType)
enum class EBBSBStage01Phase : uint8 { Opening, Outside, FindTools, ReturnToMerchant, ShopBattle, Cargo, Rooftops, Trio, Restock, FindLight, ReturnForReady, ApproachCaptain, Captain, Complete };
UENUM(BlueprintType)
enum class EBBSBStage01Node : uint8 { CabinDoor, LowerLadder, ClimbLadder, ToolCache, UtilityDoor, CargoButton, CargoLever, CargoLift, Trapdoor, TrioExit, CaptainNote, LightLever, BuyTea, BuyParts, TuneReward, ReadyGate, Departure };
UENUM(BlueprintType)
enum class EBBSBStage01Voice : uint8 { Ambient, Guard, Utility, Roof, Trio, Captain };
UCLASS()
class BBSB_API UBBSBStage01Save : public USaveGame
{
 GENERATED_BODY()
public:
 UPROPERTY(SaveGame) int32 Version=1;
 UPROPERTY(SaveGame) EBBSBStage01Phase Phase=EBBSBStage01Phase::Opening;
 UPROPERTY(SaveGame) TSet<FName> Solved;
 UPROPERTY(SaveGame) TSet<FName> Quieted;
 UPROPERTY(SaveGame) bool bRewardGranted=false;
 UPROPERTY(SaveGame) int32 Tea=0;
};
/** Editable authored action node. Target meshes retain their closed edit-time transforms. */
UCLASS(Blueprintable)
class BBSB_API ABBSBStage01Device : public AActor,public IBBSBBunniarSurface
{
 GENERATED_BODY()
public:
 ABBSBStage01Device();
 virtual void BeginPlay() override;
 virtual void Tick(float DT) override;
 virtual void OnConstruction(const FTransform& Transform) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UInstancedStaticMeshComponent> LadderGeometry;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UInstancedStaticMeshComponent> LadderExtension;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> DoorLeft;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> DoorRight;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<UStaticMeshComponent>> DeviceDetails;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="Stage01|Physical presentation") void RebuildPhysicalPresentation();
 void RestoreSessionState(float Value,bool Solved);
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Stage01|Physical presentation") float Painted=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Label;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TObjectPtr<ABBSBStage01Director> Director;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") EBBSBStage01Node Kind=EBBSBStage01Node::CabinDoor;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FName NodeId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FText Title;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TArray<TObjectPtr<AActor>> Targets;
 /** Fixed structure containing the sliding-door pockets. This exception applies
  * only to the door leaves, never to characters or props pushed by the door. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01|Collision") TArray<TObjectPtr<AActor>> DoorTrackHousing;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FVector OpenOffset=FVector(0,400,0);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FVector Destination;
 /** Capsule-centre endpoints. Zero uses the authored device/destination fallback. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ladder") FVector LadderBottom;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ladder") FVector LadderTop;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ladder") FVector LadderOutward;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ladder",meta=(ClampMin="20",ClampMax="50",Units="cm")) float RungSpacing=35.f;
 UFUNCTION(BlueprintCallable) bool MountLadder();
 UFUNCTION(BlueprintPure) FVector LadderBottomPoint() const;
 UFUNCTION(BlueprintPure) FVector LadderTopPoint() const;
 UFUNCTION(BlueprintPure) FVector LadderNormal() const;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="Ladder") void RebuildLadderGeometry();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float RequiredDrag=110;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float MotionSeconds=.6f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float Progress=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bSolved=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bHeld=false;
 UFUNCTION(BlueprintCallable) bool Use();
 UFUNCTION(BlueprintCallable) void SetSolved(bool Value);
 virtual FText BunniarInteractionText_Implementation() const override;
 virtual bool BunniarQuickUse_Implementation(ABBSBBunniarCompanion* Buddy) override;
 virtual void BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector Point,FVector Normal) override;
 virtual void BunniarDragged_Implementation(ABBSBBunniarCompanion* Buddy,FVector2D Delta,FVector2D Total) override;
 virtual void BunniarGesture_Implementation(ABBSBBunniarCompanion* Buddy,EBBSBFieldGesture G,FVector2D Direction,float Seconds) override;
 virtual void BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy) override;
private:
 TArray<FTransform> Closed;double HeldAt=0;
 FTransform DoorFrame;
 FVector DoorAxis=FVector::RightVector,DragMotion=FVector::ZeroVector;
 float DoorTravel=200,CapturedProgress=0;
 bool bSurfacesRegistered=false;
 bool IsDirectDrag() const;
 void UpdatePhysicalPresentation(bool bSweepMotion=false,float PreviousPainted=0);
 void RegisterPhysicalSurfaces();
 float LadderRetractDistance() const;
 float ProjectedDragProgress(ABBSBBunniarCompanion* Buddy,FVector2D Delta,FVector2D Total) const;
};
UCLASS(Blueprintable)
class BBSB_API ABBSBStage01NPC : public ABBSBInteractionNPC
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TObjectPtr<ABBSBStage01Director> Director;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") int32 CarNumber=6;
 virtual bool BunniarQuickUse_Implementation(ABBSBBunniarCompanion* Buddy) override;
 virtual void BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector P,FVector N) override;
};
/** Ordinary song has no chase. Its individual shots keep their launch trajectories. */
UCLASS(Blueprintable)
class BBSB_API ABBSBStage01Singer : public ABBSBInkEnemy
{
 GENERATED_BODY()
public:
 ABBSBStage01Singer();
 virtual void BeginPlay() override;
 virtual void Tick(float DT) override;
 virtual float TakeDamage(float Amount,const FDamageEvent& Event,AController* Controller,AActor* Causer) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> SingerMesh;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TObjectPtr<ABBSBStage01Director> Director;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TObjectPtr<ABBSBStage01CarrySinger> Carrier;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") EBBSBStage01Voice Voice=EBBSBStage01Voice::Ambient;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FName SingerId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") int32 VoiceIndex=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float SongBPM=88;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float QuietSecondsAfterCarry=7;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float SongDamage=4;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float SongSpeed=450;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float HearingRadius=1700;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bSilenced=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) double QuietUntil=-100;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 SongShots=0;
 UFUNCTION(BlueprintCallable) void SilenceFor(float Seconds);
 UFUNCTION(BlueprintCallable) void Rearm();
 void FireAt(FVector Point,float Speed,float Damage,int32 Count=1,float Spread=0);
 bool IsSongEnabled() const;
 void UpdateSongPresentation();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Stage01|Song") bool bSongAttached=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Stage01|Song") FVector SongSurfacePoint=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Stage01|Song") FVector SongSurfaceNormal=FVector::UpVector;
private:
 TWeakObjectPtr<UPrimitiveComponent> SongSurface;
 FVector SongLocalPoint,SongLocalNormal;
 double NextSurfaceProbe=0,LastSongPoseAt=-1;
 FVector PreviousSongOrigin=FVector::ZeroVector,PreviousSupportPoint=FVector::ZeroVector;
 TWeakObjectPtr<UPrimitiveComponent> PreviousSongSurface;
public:
private:
 int64 LastBeat=MIN_int64;bool bReportedQuiet=false;
 FTransform MeshRest;
};
/** Uses the same pickup, camera-plane dragging, wheel rotation and throwing as other carry objects. */
UCLASS(Blueprintable)
class BBSB_API ABBSBStage01CarrySinger : public ABBSBCarryObject
{
 GENERATED_BODY()
public:
 ABBSBStage01CarrySinger();
 virtual void BeginPlay() override;
 virtual void Tick(float DT) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TObjectPtr<ABBSBStage01Singer> Singer;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> BunniarMesh;
 virtual FText BunniarInteractionText_Implementation() const override;
 virtual void BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector P,FVector N) override;
 virtual void BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy) override;
private:
 FVector SongHome;
};
UCLASS(Blueprintable)
class BBSB_API ABBSBStage01Director : public AActor
{
 GENERATED_BODY()
 friend class UBBSBSessionSubsystem;
public:
 ABBSBStage01Director();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void Tick(float DT) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") bool bPersistProgress=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FString SaveSlotName=TEXT("BBSB_Stage01Story_v1");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") bool bEnforceTutorialLoadout=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float TrioBPM=88;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float CaptainBPM=96;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float GrabWindow=.18f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float GrabDamage=6;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") float RevealSeconds=2.2f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01|Recovery") bool bRecoverFromCliffFall=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01|Recovery") float RecoveryFloorZ=5600;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TSoftObjectPtr<UBBSBItemDefinition> RepairTool;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TSoftObjectPtr<UBBSBItemDefinition> TrioReward;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TSoftObjectPtr<UBBSBItemDefinition> HarmonyOrb;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TObjectPtr<ABBSBRespawnPoint> CabinCheckpoint;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TObjectPtr<ABBSBRespawnPoint> UtilityCheckpoint;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TObjectPtr<ABBSBRespawnPoint> ArtCheckpoint;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") TObjectPtr<AActor> CaptainLight;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FVector TrioCenter;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FRotator TrioFrame;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FVector CaptainCenter;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FVector RoofEntry;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FVector TrioEntry;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FVector CargoLanding;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stage01") FTransform BookstandPose;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) EBBSBStage01Phase Phase=EBBSBStage01Phase::Opening;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TSet<FName> Solved;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TSet<FName> Quieted;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<ABBSBStage01Device>> Devices;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<TObjectPtr<ABBSBStage01Singer>> Singers;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bRewardGranted=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Tea=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 GrabNoteIndex=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 GrabSuccesses=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bGrabActive=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bGrabWarning=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bLaneWarning=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 WarningLane=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FVector GrabPoint;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float GrabWarningRadius=230;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Caption;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FGuid ToolInstance;
 UFUNCTION(BlueprintCallable) bool UseDevice(ABBSBStage01Device* Device);
 UFUNCTION(BlueprintCallable) void TalkTo(ABBSBStage01NPC* NPC);
 UFUNCTION(BlueprintCallable) void UseTea();
 UFUNCTION(BlueprintCallable) void ResetDraft();
 UFUNCTION(BlueprintCallable) bool BeginGrab(ABBSBStage01Singer* Source);
 UFUNCTION(BlueprintCallable) void EndGrab();
 UFUNCTION(BlueprintPure) FString Objective() const;
 UFUNCTION(BlueprintPure) FVector ObjectivePoint() const;
 UFUNCTION(BlueprintPure) uint8 GrabMask(int32 Index) const;
 UFUNCTION(BlueprintPure) float GrabNoteSeconds(int32 Index) const;
 UFUNCTION(BlueprintPure) ABBSBCharacter* PlayerCharacter() const { return Player; }
 UFUNCTION(BlueprintPure) bool HasTool() const { return Phase>=EBBSBStage01Phase::ReturnToMerchant; }
 void ReportQuiet(ABBSBStage01Singer* Singer);
 void Say(const FString& Text,float Seconds=5);
 bool MoveAlongLadder(FVector Destination,float Seconds=1.2f,float Clearance=60,FVector Access=FVector::ZeroVector);
 UFUNCTION() void RefillAtCheckpoint(ABBSBCharacter* Character);
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<UInputComponent> Controls;
 UPROPERTY(Transient) TObjectPtr<UAudioComponent> CaptainAudio;
 UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> CaptainMelody;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> WarningRing;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LaneWarningMesh;
 TWeakObjectPtr<ABBSBStage01Singer> GrabSource;
 double StartedAt=0,CaptionUntil=0,NextGrabAt=0,GrabWarningAt=0,GrabEpoch=0,LastMelodyAt=-100;
 int64 SongBeat=MIN_int64;bool bBound=false,bSavedHUD=false,bMoveIgnored=false,bPlayerWasDead=false,bLadderMoving=false,bToolEquipPending=false;
 int32 GrabCount=6;FVector GrabLocation,LadderFrom,LadderTo,LadderAccess;double LadderStarted=0;float LadderDuration=1,LadderClearance=60;double NextLaneAt=0,LaneImpactAt=0;
 TArray<bool> GrabResolved;
 UPROPERTY(Transient) TObjectPtr<ACameraActor> RevealCamera;
 TWeakObjectPtr<AActor> RevealPrevious;double RevealUntil=0,LastRewardTry=-100;
 void BeginReveal();void TickReveal();void FinishReveal();void TryGrantReward();
 void BindPlayer();
 void Advance(EBBSBStage01Phase NewPhase);
 void SaveProgress();
 void ApplyProgress();
 void TickCombat(float DT);
 void TickGrab(float DT);
 void TickMusic();
 void UpdateTutorialGate();
 bool GiveRepairTool();
 bool AllQuiet(EBBSBStage01Voice Voice) const;
 void ClearEncounterShots();
 ABBSBStage01Device* FindNode(EBBSBStage01Node Kind) const;
};
UCLASS()
class BBSB_API ABBSBStage01HUD : public ABBSBPuzzleFieldHUD
{
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
};
