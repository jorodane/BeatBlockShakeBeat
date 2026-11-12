#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "GameFramework/SaveGame.h"
#include "UI/BBSBPaperWidget.h"
#include "Hunting/BBSBItem.h"
#include "BBSBSession.generated.h"

class ABBSBCharacter;class UBBSBSessionMenu;class UInputComponent;class UButton;class UVerticalBox;class UTextBlock;

UENUM(BlueprintType)
enum class EBBSBSavedMovement : uint8 { Ground, Falling, Glide, Ladder, WallSlide, LedgeHang };

USTRUCT(BlueprintType)
struct FBBSBSessionActor
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FString Path;
 UPROPERTY(SaveGame) FTransform Transform;
 UPROPERTY(SaveGame) FVector Velocity=FVector::ZeroVector;
 UPROPERTY(SaveGame) FVector AngularVelocity=FVector::ZeroVector;
 UPROPERTY(SaveGame) uint8 Kind=0;
 UPROPERTY(SaveGame) float Value=0;
 UPROPERTY(SaveGame) bool bFlag=false;
 UPROPERTY(SaveGame) bool bSimulating=false;
};
USTRUCT()
struct FBBSBSessionLoot
{
 GENERATED_BODY()
 UPROPERTY(SaveGame) FBBSBInventoryEntry Entry;
 UPROPERTY(SaveGame) FVector Location=FVector::ZeroVector;
 UPROPERTY(SaveGame) bool bConcertoReward=false;
};

/** A complete record, not a reference to mutable inventory/story save files. */
UCLASS()
class BBSB_API UBBSBSessionSave : public USaveGame
{
 GENERATED_BODY()
public:
 UPROPERTY(SaveGame) int32 Version=1;
 UPROPERTY(SaveGame) int64 Sequence=0;
 UPROPERTY(SaveGame) FDateTime SavedAt;
 UPROPERTY(SaveGame) FString Reason;
 UPROPERTY(SaveGame) FString MapPackage;
 UPROPERTY(SaveGame) FTransform PlayerTransform;
 UPROPERTY(SaveGame) FRotator ViewRotation;
 UPROPERTY(SaveGame) FVector Velocity=FVector::ZeroVector;
 UPROPERTY(SaveGame) float Health=100;
 UPROPERTY(SaveGame) EBBSBSavedMovement Movement=EBBSBSavedMovement::Ground;
 UPROPERTY(SaveGame) bool bAirJumpUsed=false;
 UPROPERTY(SaveGame) int32 WireCharges=2;
 UPROPERTY(SaveGame) float WireRecharge=0;
 UPROPERTY(SaveGame) bool bWireRechargeArmed=false;
 UPROPERTY(SaveGame) FString SurfaceActor;
 UPROPERTY(SaveGame) FString SurfaceComponent;
 UPROPERTY(SaveGame) FVector LadderBottom=FVector::ZeroVector;
 UPROPERTY(SaveGame) FVector LadderTop=FVector::ZeroVector;
 UPROPERTY(SaveGame) FVector LadderExit=FVector::ZeroVector;
 UPROPERTY(SaveGame) float LadderRungSpacing=35;
 UPROPERTY(SaveGame) FVector SurfacePoint=FVector::ZeroVector;
 UPROPERTY(SaveGame) FVector SurfaceNormal=FVector::ForwardVector;
 UPROPERTY(SaveGame) FVector LedgeStand=FVector::ZeroVector;
 UPROPERTY(SaveGame) FVector LedgeTopNormal=FVector::UpVector;
 UPROPERTY(SaveGame) TArray<uint8> Inventory;
 UPROPERTY(SaveGame) TArray<uint8> Story;
 UPROPERTY(SaveGame) FGuid StoryTool;
 UPROPERTY(SaveGame) bool bTutorialLoadoutEnforced=true;
 UPROPERTY(SaveGame) bool bBunniarOpeningRest=false;
 UPROPERTY(SaveGame) TArray<FBBSBSessionActor> Actors;
 UPROPERTY(SaveGame) TArray<FBBSBSessionLoot> Loot;
 bool IsValidRecord() const;
 FString StateHash() const;
};

USTRUCT(BlueprintType)
struct FBBSBSessionSlot
{
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Index=-1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int64 Sequence=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Title;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Details;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bValid=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bOccupied=false;
};

/** The same owner spans OpenLevel; pending records never depend on old actors. */
UCLASS()
class BBSB_API UBBSBSessionSubsystem : public UGameInstanceSubsystem,public FTickableGameObject
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual void Tick(float DeltaTime) override;
 virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBBSBSessionSubsystem,STATGROUP_Tickables); }
 virtual bool IsTickable() const override { return !IsTemplate() && bEnabled; }
 virtual bool IsTickableWhenPaused() const override { return true; }
 virtual UWorld* GetTickableGameObjectWorld() const override;
 virtual UWorld* GetWorld() const override { return GetTickableGameObjectWorld(); }
 static UBBSBSessionSubsystem* Get(const UObject* Context);
 static bool Owns(const UObject* Context);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Save") bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Save") FString SaveNamespace=TEXT("BBSB_Session_v1");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Save",meta=(ClampMin="60",Units="s")) float AutoSaveIntervalSeconds=300.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Save") bool bGameStarted=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Save") bool bMenuOpen=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Save") bool bLoading=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Save") FString StatusText;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Save") TArray<FBBSBSessionSlot> Slots;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Save") TObjectPtr<UBBSBSessionMenu> Menu;
 UFUNCTION(BlueprintCallable,Category="Save") void RefreshSlots();
 UFUNCTION(BlueprintCallable,Category="Save") bool ContinueGame();
 UFUNCTION(BlueprintCallable,Category="Save") bool StartNewGame();
 UFUNCTION(BlueprintCallable,Category="Save") bool LoadSlot(int32 Index);
 UFUNCTION(BlueprintCallable,Category="Save") bool SaveNow(FString Reason,bool Force=false);
 UFUNCTION(BlueprintCallable,Category="Save") void RequestAutoSave(FString Reason);
 UFUNCTION(BlueprintCallable,Category="Save") void CheckAutoSave();
 UFUNCTION(BlueprintCallable,Category="Save") void OpenMenu();
 UFUNCTION(BlueprintCallable,Category="Save") void CloseMenu();
 UFUNCTION(BlueprintPure,Category="Save") bool CanContinue() const;
 UFUNCTION(BlueprintPure,Category="Save") UBBSBSessionSave* Capture() const;
 UFUNCTION(BlueprintCallable,Category="Save") bool Restore(UBBSBSessionSave* Record);
 /** Used by the same runtime path and the isolated disk tests. */
 bool WriteRecord(UBBSBSessionSave* Record);
 UBBSBSessionSave* ReadRecord(int32 Index) const;
 FString SlotFile(int32 Index) const;
 void MarkChanged() { bChanged=true; }
private:
 UPROPERTY(Transient) TObjectPtr<UBBSBSessionSave> Pending;
 UPROPERTY(Transient) TObjectPtr<UInputComponent> Controls;
 TWeakObjectPtr<ABBSBCharacter> Player;
 TWeakObjectPtr<UWorld> BoundWorld;
 FDelegateHandle TearDownHandle;
 FString LastStateHash,PendingReason;
 double NextCheck=0,SaveRequestedAt=-1,BindAfter=0;
 bool bApplying=false,bChanged=false,bSwitching=false,bNewGamePending=false,bOwnPause=false,bOwnMoveIgnore=false,bOwnLookIgnore=false,bPreviousCursor=false;
 void OnWorldTearDown(UWorld* World);
 bool BindPlayer();
 void BeginMap(const FString& Map);
 bool ImportLegacy();
 bool HasLegacy() const;
 bool IsStable() const;
};

UCLASS()
class BBSB_API UBBSBSessionSlotClick : public UObject
{
 GENERATED_BODY()
public:
 UPROPERTY() TObjectPtr<UBBSBSessionSubsystem> Session;
 int32 Index=-1;
 UFUNCTION() void Click();
};

UCLASS()
class BBSB_API UBBSBSessionMenu : public UBBSBPaperWidget
{
 GENERATED_BODY()
public:
 UPROPERTY() TObjectPtr<UBBSBSessionSubsystem> Session;
 void Refresh();
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
 UPROPERTY() TObjectPtr<UVerticalBox> Rows;
 UPROPERTY() TObjectPtr<UTextBlock> Message;
 UPROPERTY() TArray<TObjectPtr<UBBSBSessionSlotClick>> Clicks;
 bool bLoadList=false;
 UFUNCTION() void Continue();
 UFUNCTION() void NewGame();
 UFUNCTION() void LoadList();
 UFUNCTION() void Back();
 UFUNCTION() void Save();
 UButton* AddButton(const FString& Label,bool Enabled=true);
};
