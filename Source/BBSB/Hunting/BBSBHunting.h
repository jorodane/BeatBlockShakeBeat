#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Hunting/BBSBItem.h"
#include "BBSBHunting.generated.h"

class ABBSBCharacter;
class ABBSBInkEnemy;
class ABBSBLootPickup;
class UBBSBInventoryWidget;
class USceneComponent;
class USplineMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AHUD;
class APlayerController;
class UCanvas;
class USkeletalMesh;
struct FCollisionQueryParams;

USTRUCT()
struct FBBSBPickupNotice
{
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<UBBSBItemDefinition> Item;
 int32 Quantity=0;
 double At=0;
};

UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBHuntingComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UBBSBHuntingComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Progression") int32 Level=1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Progression") int32 Experience=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Progression",meta=(ClampMin="1")) int32 BaseExperienceToLevel=100;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Progression",meta=(ClampMin="0")) int32 ExperienceIncreasePerLevel=50;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Progression",meta=(ClampMin="2",ClampMax="999")) int32 MaximumLevel=99;
 UFUNCTION(BlueprintPure,Category="Progression") int32 ExperienceToNextLevel() const;
 UFUNCTION(BlueprintCallable,Category="Progression") void AddExperience(int32 Amount);
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Rewards") int64 Notes=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Rewards") int64 TotalEcstaticCollected=0;
 UFUNCTION(BlueprintPure,Category="Rewards") FGuid RewardWeaponInstance() const;
 UFUNCTION(BlueprintCallable,Category="Rewards") bool CollectEcstatic(int64 Amount,bool ChargeConcerto=true);
 UFUNCTION(BlueprintCallable,Category="Rewards") bool AddNotes(int64 Amount);
 UFUNCTION(BlueprintCallable,Category="Rewards") bool SpendNotes(int64 Amount);
 UFUNCTION(BlueprintCallable,Category="Rewards") int32 CollectLoot(ABBSBLootPickup* Loot) { return Collect(Loot); }
 UFUNCTION(BlueprintCallable,Category="Weapons|Growth") int64 AddWeaponEcstatic(FGuid WeaponId,int64 Amount);
 UFUNCTION(BlueprintPure,Category="Weapons|Growth") bool IsEcstasyWeapon(FGuid WeaponId) const;
 UFUNCTION(BlueprintPure,Category="Weapons|Growth") int64 WeaponExperienceToNextLevel(FGuid WeaponId) const;
 UFUNCTION(BlueprintPure,Category="Weapons|Growth") int64 WeaponExperienceInLevel(FGuid WeaponId) const;
 UFUNCTION(BlueprintPure,Category="Weapons|Growth") FBBSBWeaponDismantlePreview PreviewDismantleWeapon(FGuid WeaponId) const;
 UFUNCTION(BlueprintCallable,Category="Weapons|Growth") bool DismantleWeapon(FGuid WeaponId);
 UFUNCTION(BlueprintCallable,Category="Weapons|Growth") bool UseEcstaticCrystals(FGuid WeaponId,int32 Count=1);
 UFUNCTION(BlueprintPure,Category="Weapons|Growth") UBBSBItemDefinition* EcstaticCrystalDefinition() const;
 void GrantEnemyRewards(ABBSBInkEnemy* Enemy);
 void GrantEnemyDamageRewards(ABBSBInkEnemy* Enemy);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Loot") TSoftObjectPtr<UBBSBLootTable> DefaultLootTable;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Loot",meta=(ClampMin="200",Units="cm")) float LootLabelDistance=2400.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Inventory") TArray<FBBSBInventoryEntry> Items;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Inventory",meta=(ClampMin="1",ClampMax="24")) int32 Columns=5;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Inventory",meta=(ClampMin="1",ClampMax="24")) int32 Rows=8;
 /** Equipment indices 0..9 are the wheel; 10 companion, 11 armor, 12 accessory. */
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Equipment") TArray<FBBSBInventoryEntry> EquippedItems;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Equipment") int32 CompanionWheelSlot=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Equipment") bool bEquipmentLayoutPending=false;
 UFUNCTION(BlueprintCallable,Category="Equipment") bool PlaceOwnedItem(FGuid Id,int32 Destination);
 UFUNCTION(BlueprintPure,Category="Equipment") bool CanPlaceOwnedItem(FGuid Id,int32 Destination) const;
 UFUNCTION(BlueprintCallable,Category="Equipment") bool MoveCompanionWheelSlot(int32 Destination);
 UFUNCTION(BlueprintPure,Category="Equipment") UBBSBItemDefinition* EquippedItemAt(int32 Slot) const;
 const FBBSBInventoryEntry* EquipmentEntryAt(int32 Slot) const;
 const FBBSBInventoryEntry* FindOwnedEntry(FGuid Id) const;
 UFUNCTION(BlueprintCallable,Category="Inventory") bool SortInventory();
 UFUNCTION(BlueprintCallable,Category="Inventory") bool ConsumeItem(UBBSBItemDefinition* Item,int32 Quantity);
 UFUNCTION(BlueprintCallable,Category="Weapons") FGuid AddWeaponInstance(UBBSBItemDefinition* Item,int32 WeaponLevel,int32 SocketCount);
 bool AddWeaponEntry(const FBBSBInventoryEntry& Entry);
 UFUNCTION(BlueprintCallable,Category="Weapons") bool SocketOrb(FGuid WeaponId,int32 Socket,FGuid OrbId);
 UFUNCTION(BlueprintCallable,Category="Weapons") bool RemoveOrb(FGuid WeaponId,int32 Socket);
 UFUNCTION(BlueprintCallable,Category="Weapons") bool EquipOwnedWeapon(FGuid Id);
 UFUNCTION(BlueprintPure,Category="Weapons") FBBSBInventoryEntry WeaponInstance(FGuid Id) const;
 UFUNCTION(BlueprintCallable,Category="Growth") void NotifyGrowthChanged() { ProgressChanged(); }
 FBBSBInventoryEntry* MutableOwnedEntry(FGuid Id);
 UFUNCTION(BlueprintCallable,Category="Inventory") int32 AddItem(UBBSBItemDefinition* Item,int32 Quantity);
 UFUNCTION(BlueprintPure,Category="Inventory") int32 ItemCount(UBBSBItemDefinition* Item) const;
 UFUNCTION(BlueprintPure,Category="Inventory") bool CanPlace(UBBSBItemDefinition* Item,int32 X,int32 Y,bool Rotated,FGuid Ignore) const;
 UFUNCTION(BlueprintCallable,Category="Inventory") bool MoveItem(FGuid InstanceId,int32 X,int32 Y,bool Rotated);
 UFUNCTION(BlueprintPure,Category="Inventory") int32 EntryAt(int32 X,int32 Y) const;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Inventory") bool bInventoryOpen=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Inventory") bool bInventoryPending=false;
 UPROPERTY(EditDefaultsOnly,Category="Inventory") TSubclassOf<UBBSBInventoryWidget> InventoryWidgetClass;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Inventory") TObjectPtr<UBBSBInventoryWidget> InventoryWidget;
 UFUNCTION(BlueprintCallable,Category="Inventory") void ToggleInventory();
 UFUNCTION(BlueprintCallable,Category="Inventory") bool OpenInventory();
 UFUNCTION(BlueprintCallable,Category="Inventory") void CloseInventory();
 void RefreshInventoryInput();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Save") bool bPersistProgress=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Save") FString SaveSlotName=TEXT("BBSB_HuntingInventory_v1");
 UFUNCTION(BlueprintCallable,Category="Save") bool SaveProgress();
 UFUNCTION(BlueprintCallable,Category="Save") bool LoadProgress();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Collection") bool bCollectionHeld=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Collection") bool bVacuuming=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Collection") bool bQuickCollection=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Collection",meta=(ClampMin="40",ClampMax="180",Units="cm")) float BuddyVacuumOrbitRadius=100.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Collection",meta=(ClampMin=".5",Units="s")) float BuddyVacuumOrbitSeconds=1.2f;
 UFUNCTION(BlueprintCallable,Category="Collection") void QuickCollectDown();
 UFUNCTION(BlueprintCallable,Category="Collection") void QuickCollectUp();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Collection") FVector VacuumPoint=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Collection") FVector VacuumNormal=FVector::UpVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Collection",meta=(ClampMin=".1",ClampMax="1")) float VacuumHoldSeconds=.25f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Collection",meta=(ClampMin="50",ClampMax="600",Units="cm")) float VacuumRadius=220.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Collection",meta=(ClampMin=".04",Units="s")) float VacuumScanSeconds=.08f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Collection") TSoftObjectPtr<UMaterialInterface> VacuumMaterial;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Collection") TArray<TObjectPtr<USplineMeshComponent>> VacuumRibbons;
 bool TryBeginCollection();
 bool TryBeginQuickCollection(ABBSBLootPickup* Loot);
 bool HasQuickCollectionInput() const { return bQuickCollection || bQuickSuctionPending; }
 UFUNCTION(BlueprintPure,Category="Collection") bool IsQuickSuctionPending() const { return bQuickSuctionPending; }
 UFUNCTION(BlueprintPure,Category="Collection") bool IsBunniarBusyForCollection() const;
 UFUNCTION(BlueprintCallable,Category="Collection") void StopCollection();
 void RegisterLoot(ABBSBLootPickup* Loot);
 void UnregisterLoot(ABBSBLootPickup* Loot);
 void IgnoreLoot(FCollisionQueryParams& Query) const;
 FVector CollectionMouth() const;
 bool TickBuddyPresentation(float DT);
 void DrawFeedback(AHUD* HUD,UCanvas* Canvas) const;
 void DrawLootNames(AHUD* HUD,UCanvas* Canvas) const;
 const FBBSBInventoryEntry* FindEntry(FGuid Id) const;
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<USceneComponent> VacuumRoot;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> VacuumMID;
 UPROPERTY(Transient) TObjectPtr<UBBSBLootTable> LoadedDefaultLoot;
 UPROPERTY(Transient) TObjectPtr<USkeletalMesh> OriginalCompanionMesh;
 bool bEquipmentInitialized=false;
 void InitializeEquipment();
 void TickEquipmentLoadout();
 bool FitsEquipment(UBBSBItemDefinition* Item,int32 Slot) const;
 UPROPERTY(Transient) TArray<FBBSBPickupNotice> Notices;
 TSet<TWeakObjectPtr<ABBSBLootPickup>> LootActors;
 double CollectionStartedAt=-100,NextVacuumScanAt=0,SaveAt=-100,LastFullNoticeAt=-100;
 double LastExperienceAt=-100,LastLevelUpAt=-100;
 int32 LastExperienceGain=0;
 FString StatusNotice;
 double StatusUntil=-100;
 bool bDirtyProgress=false,bMenuOriginalCursor=false,bMenuOwnLook=false,bSaveLoadFailed=false;
 bool bBuddyLootReturning=false;
 bool bQuickInputHeld=false,bQuickSuctionPending=false;
 double QuickInputStartedAt=-100;
 bool BeginQuickCollection(ABBSBLootPickup* Loot,bool FromHold=false);
 void TickQuickCollectionIntent();
 TWeakObjectPtr<ABBSBLootPickup> QuickPendingLoot;
 FTransform QuickTravelFrom;
 float QuickTravelSeconds=.24f;
 bool TraceCollectionHit(FHitResult& Hit) const;
 void TickQuickCollection(float DT);
 bool bHuntingInitialized=false;
 TWeakObjectPtr<APlayerController> MenuController;
 FTransform BuddyLootReturnFrom;
 double BuddyLootReturnAt=-100;
 FVector2D MenuMouse=FVector2D::ZeroVector;
 bool bMenuSavedMouse=false;
 bool FindSpace(UBBSBItemDefinition* Item,int32& X,int32& Y,bool& Rotated) const;
 void ProgressChanged();
 void ShowPickup(UBBSBItemDefinition* Item,int32 Quantity);
 int32 Collect(ABBSBLootPickup* Loot);
 const class UBBSBGrowthSettings* RewardSettings() const;
 UBBSBItemDefinition* ResourceDefinition(EBBSBRewardResource Resource) const;
 void SpawnBasicRewards(ABBSBInkEnemy* Enemy,int32& Index);
 int32 SpawnResourceAmount(ABBSBInkEnemy* Enemy,EBBSBRewardResource Resource,int32 Amount,bool FinalBurst,int32& Index);
 void TickResourcePickups();
 double NextResourceScanAt=0;
 FGuid LastExperienceWeapon;
 bool ResolveVacuumAim(FVector& Point,FVector& Normal) const;
 bool LootVisible(const ABBSBLootPickup* Loot,const FVector& From) const;
 void UpdateVacuum(float DT);
 void UpdateVacuumVisual();
 void SetVacuumVisible(bool Visible);
};
