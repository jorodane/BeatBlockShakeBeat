#pragma once
#include "CoreMinimal.h"
#include "UI/BBSBPaperMotion.h"
#include "Camera/CameraTypes.h"
#include "Components/ActorComponent.h"
#include "Hunting/BBSBGrowthData.h"
#include "Hunting/BBSBResourceGain.h"
#include "BBSBEquipmentGrowth.generated.h"

class ABBSBCharacter;
class ABBSBInkEnemy;
class UBBSBHuntingSave;
class UStaticMeshComponent;
class UAnimMontage;
class UBBSBBulletClearFXComponent;
class AHUD;
class UCanvas;

UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBEquipmentGrowthComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UBBSBEquipmentGrowthComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Growth") TSoftObjectPtr<UBBSBGrowthSettings> SettingsAsset;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Growth") TObjectPtr<UBBSBGrowthSettings> Settings;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Modules") TArray<FBBSBOwnedModule> OwnedModules;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Modules") FName AerialModule;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Modules") FName DirectionalModule;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Modules") bool bModuleChangePending=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Modules") FName PendingAerialModule;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Modules") FName PendingDirectionalModule;
 UFUNCTION(BlueprintCallable,Category="Modules") bool GrantModule(UBBSBModuleDefinition* Definition,bool EquipIfEmpty=false);
 UFUNCTION(BlueprintCallable,Category="Modules") bool EquipModule(EBBSBModuleSlot Slot,FName Id);
 UFUNCTION(BlueprintCallable,Category="Modules") bool UpgradeModule(FName Id);
 UFUNCTION(BlueprintPure,Category="Modules") int32 ModuleLevel(FName Id) const;
 UFUNCTION(BlueprintPure,Category="Modules") bool HasModuleAction(EBBSBModuleAction Action) const;
 UFUNCTION(BlueprintPure,Category="Modules") bool CanUpgradeModule(FName Id) const;
 UFUNCTION(BlueprintPure,Category="Modules") UBBSBModuleDefinition* ModuleDefinition(FName Id) const;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Modules") TMap<FString,int32> BunniarBagPositions;
 void EnsureBunniarBag();
 UFUNCTION(BlueprintPure,Category="Modules") FName RequestedModule(EBBSBModuleSlot Slot) const;
 UFUNCTION(BlueprintPure,Category="Modules") int32 BunniarBagCapacity() const;
 UFUNCTION(BlueprintPure,Category="Modules") FName ModuleAtBunniarCell(int32 Cell) const;
 UFUNCTION(BlueprintPure,Category="Modules") FGuid ItemAtBunniarCell(int32 Cell) const;
 UFUNCTION(BlueprintCallable,Category="Modules") bool MoveBunniarModule(FName Id,int32 Cell);
 UFUNCTION(BlueprintCallable,Category="Modules") bool MoveBunniarItem(FGuid Id,int32 Cell);
 UFUNCTION(BlueprintCallable,Category="Modules") bool SortBunniarInventory();
 UFUNCTION(BlueprintPure,Category="Weapons") FBBSBWeaponSnapshot WeaponSnapshot(FGuid InstanceId) const;
 UFUNCTION(BlueprintPure,Category="Weapons") float ConditionalDamageMultiplier(const FBBSBWeaponSnapshot& Snapshot) const;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Concerto") float ConcertoCharge=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Concerto") bool bConcertoActive=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Concerto") FGuid ConcertoMainWeapon;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Concerto") int32 ConcertoActivations=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Concerto") int32 BulletsCleared=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Ensemble") int32 EnsembleAttacks=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Performances") int32 MainPerformances=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Performances") int32 SubPerformances=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Growth") FString Feedback;
 UFUNCTION(BlueprintCallable,Category="Concerto") bool TryConcerto();
 UFUNCTION(BlueprintCallable,Category="Concerto") void AddConcertoCharge(float Amount);
 UFUNCTION(BlueprintPure,Category="Concerto|Presentation") float DisplayedConcertoCharge() const;
 UFUNCTION(BlueprintPure,Category="Concerto") bool IsConcertoInvulnerable() const { return bConcertoActive; }
 void EnsureInitialized();
 void InitializeProfile(bool NewOrLegacy);
 void LoadFromSave(const UBBSBHuntingSave* Save);
 void StoreInSave(UBBSBHuntingSave* Save) const;
 void NotifyWeaponHit(const FBBSBWeaponSnapshot& Snapshot,AActor* Target,float Damage,const FVector& Point,bool Support=false);
 void DrawGrowthHUD(AHUD* HUD,UCanvas* Canvas) const;
 mutable FBBSBHUDReveal GrowthHUDReveal;
 void CancelConcerto();
 void ApplyConcertoCamera(float DT,FMinimalViewInfo& View);
 UFUNCTION(BlueprintPure,Category="Concerto") bool IsConcertoCameraActive() const { return bConcertoCameraActive; }
 UFUNCTION(BlueprintPure,Category="Concerto") float ConcertoProgress() const;
 void SuspendConcertoEnemy(ABBSBInkEnemy* Enemy);
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Concerto") float ConcertoElapsed=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Concerto") float ConcertoPlayRate=1;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Concerto") float FaceSlowElapsed=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Concerto") bool bConcertoCameraActive=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Concerto") FVector WhirlwindBladePosition=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Concerto") float WhirlwindRadius=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Ensemble") int32 SupportBeatEvents=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Ensemble") int32 LastSupportBeat=INDEX_NONE;
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<UAnimMontage> ConcertoMontage;
 bool bInitialized=false,bProfileReady=false,bStatsPending=false;
 FBBSBResourceGain ConcertoGain;
 bool bPendingAerial=false,bPendingDirectional=false;
 float BaseGlideSpeed=550,BaseGlideFall=140,BaseWireSpeed=1800,BaseSkyWireDistance=700,BaseWireRecharge=2;
 double ConcertoStartedAt=-100,NextConcertoMainAt=0,NextEnsembleAt=-100;
 struct FPerformance;
 double ConcertoDuration=3.5,CameraReturnAt=-100;
 int64 LastMusicBeat=MIN_int64,NextConcertoSupportBeat=0;
 FVector ConcertoOrigin;FQuat ConcertoFrame=FQuat::Identity;
 UPROPERTY(Transient) TObjectPtr<UBBSBConcertoCameraPreset> CameraPreset;
 FMinimalViewInfo LastCinematicView,CameraReturnFrom;
 bool bCameraReturning=false,bCameraSampled=false;
 struct FPendingSupport { FBBSBWeaponSnapshot Weapon;FVector Target;int64 Beat=0; };
 TArray<FPendingSupport> PendingSupport;
 double MusicBeat() const;
 FBBSBPerformanceRole NormalBeatRole(const FBBSBWeaponSnapshot& Weapon) const;
 FVector SpiralPosition(const FPerformance& Performance,float Progress) const;
 void SweepSpiral(FPerformance& Performance,float Previous,float Current);
 TArray<FBBSBWeaponSnapshot> ConcertoWeapons;
 TArray<TWeakObjectPtr<ABBSBInkEnemy>> SuspendedEnemies;
 int32 ConcertoClearStart=0;
 UPROPERTY(Transient) TObjectPtr<UBBSBBulletClearFXComponent> BulletClearFX;
 void EmitBulletClear(const FVector& Point,const FLinearColor& Tint);
 double ConcertoMusicStart=0;
 struct FPerformance
 {
  FBBSBWeaponSnapshot Weapon;FBBSBPerformanceRole Role;FVector Origin,Target;double StartedAt=0,LastDamageAt=-100;
  TWeakObjectPtr<AActor> Visual;bool Main=false,Ultimate=false;int32 Seat=0;float PreviousSpiral=0;
  TMap<TWeakObjectPtr<AActor>,double> LastSpiralHit;
 };
 TArray<FPerformance> Performances;
 struct FStatus
 {
  float BurnDPS=0,Slow=1,OriginalSpeed=0;double Until=0,NextBurn=0;
 };
 TMap<TWeakObjectPtr<ABBSBInkEnemy>,FStatus> Statuses;
 bool IsModuleSwapSafe() const;
 bool MoveBunniarKey(const FString& Key,int32 Cell);
 void ApplyModuleStats();
 void Changed();
 void TickPerformances(float DT);
 void TickStatuses(float DT);
 void EmitPerformance(const FBBSBWeaponSnapshot& Weapon,const FBBSBPerformanceRole& Role,const FVector& Point,bool Ultimate,bool Main,int32 Seat=0);
 void PerformanceDamage(FPerformance& Performance);
 void ClearNearbyBullets();
};
