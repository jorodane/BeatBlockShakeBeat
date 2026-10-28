#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "Combat/BBSBCombat.h"
#include "Field/BBSBFieldActions.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "BBSBCaravan.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USkeletalMeshComponent;
class UPrimitiveComponent;
class UBBSBBunniarAnimationSet;
class ABBSBCharacter;
class ABBSBCaravanStage;
class APlayerController;
struct FMinimalViewInfo;

UENUM(BlueprintType)
enum class EBBSBCaravanStep : uint8 { Rest, Detour, Cargo, Rooftops, Miniboss, SearchCaptain, CaptainDuel, Complete };
UENUM(BlueprintType)
enum class EBBSBBunniarDevice : uint8 { Bookstand, Shutter, Lift, Winch, ArtShop, UtilityShop, CaptainDesk, Departure };

UCLASS()
class BBSB_API UBBSBCaravanSave : public USaveGame
{
 GENERATED_BODY()
public:
 UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) int32 Coins=100;
 UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) int32 Tea=0;
 UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) int32 Postcards=0;
 UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) bool bSupplyUnlocked=false;
 UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly) bool bRibbonOwned=false;
};

UCLASS()
class BBSB_API ABBSBBunniarDevice : public AActor, public IBBSBBunniarSurface
{
 GENERATED_BODY()
public:
 ABBSBBunniarDevice();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void Tick(float DT) override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Label;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) EBBSBBunniarDevice Device=EBBSBBunniarDevice::Bookstand;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DeviceLabel;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bPowered=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bLatched=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Activation=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ABBSBCaravanStage> Director;
 FVector StartPosition;
 double PoweredAt=0;
 UFUNCTION(BlueprintCallable) bool Use();
 UFUNCTION(BlueprintCallable) FVector AttachmentPoint() const;
 virtual void BunniarAttached_Implementation(ABBSBBunniarCompanion* Bunniar,FVector WorldPoint,FVector WorldNormal) override;
 virtual void BunniarDragged_Implementation(ABBSBBunniarCompanion* Bunniar,FVector2D DeltaPixels,FVector2D TotalPixels) override;
 virtual void BunniarReleased_Implementation(ABBSBBunniarCompanion* Bunniar) override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar") FVector2D SurfaceDragPixels=FVector2D::ZeroVector;
 void SetFocused(bool bFocused);
};

UCLASS()
class BBSB_API ABBSBBunniarCompanion : public AActor
{
 GENERATED_BODY()
public:
 ABBSBBunniarCompanion();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void Tick(float DT) override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Visual;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UBBSBBunniarAnimationSet> AnimationSet;
 UPROPERTY(EditDefaultsOnly, Category="Animation") TSoftObjectPtr<UBBSBBunniarAnimationSet> AnimationLibrary;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> BandageA;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> BandageB;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ABBSBCaravanStage> Director;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ABBSBBunniarDevice> AttachedDevice;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFollowing=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bEquipped=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Bunniar|Presentation",meta=(ClampMin="0.05",Units="s")) float ReturnTravelSeconds=.40f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Bunniar|Presentation",meta=(ClampMin="0.05",Units="s")) float EquipmentTravelSeconds=.32f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Bunniar|Presentation",meta=(ClampMin="1")) float AirGazeInterpSpeed=5.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bunniar|Glide") bool bGliding=false;
 void BeginGlidePresentation();
 void EndGlidePresentation();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar|Surface") bool bSurfaceAttached=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar|Surface") bool bAirHovering=false;
 /** The wire takes ownership of an anchor until arrival, independently of the held mouse button. */
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bunniar|Surface") bool bWireAnchored=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar|Surface") FVector AirHoverPoint=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar|Surface") bool bSurfaceViewLocked=false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bunniar|Surface", meta=(ClampMin="0.02",ClampMax="0.2")) float SurfaceFocusScreenMargin=.08f;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar|Surface") float SurfaceCameraRetreat=0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bunniar|Surface", meta=(ClampMin="0.1",Units="s")) float SurfaceCameraSmoothSeconds=.35f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bunniar|Surface", meta=(ClampMin="10",Units="cm/s")) float SurfaceCameraMaxSpeed=180.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bunniar|Surface", meta=(ClampMin="10",Units="cm/s^2")) float SurfaceCameraMaxAcceleration=600.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bunniar|Surface", meta=(ClampMin="0.1",Units="s")) float SurfaceCameraReturnSeconds=.75f;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar|Surface") TObjectPtr<UPrimitiveComponent> SurfaceComponent;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar|Surface") FVector SurfacePoint=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar|Surface") FVector SurfaceNormal=FVector::UpVector;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar|Surface") FVector2D DragPixels=FVector2D::ZeroVector;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Bunniar|Surface") float SurfaceArrivalAlpha=1;
 UFUNCTION(BlueprintCallable, Category="Bunniar|Surface") bool AttachFromCursor();
 UFUNCTION(BlueprintCallable, Category="Bunniar|Surface") void ReleaseSurface();
 void ApplySurfaceFocus(float DeltaTime,FMinimalViewInfo& View);
 // Release only camera ownership; surface and wire attachment remain intact.
 void ReleaseSurfaceView();
 bool HandleEquipmentGesture(EBBSBFieldGesture Gesture,int32 Channel,float Charge,bool bInitialPress);
 void HandleSurfaceDrag(FVector2D Delta);
private:
 FTransform SurfaceLocalFrame;
 FTransform SurfaceTransitFrom;
 double SurfaceTransitAt=0;
 float ReturnTransitSeconds=.40f;
 bool bLastEquipped=false;
 bool bRestOccluded=false;
 bool bSurfaceReturning=false;
 bool bSurfaceArrived=false;
 TWeakObjectPtr<APlayerController> SurfaceLookController;
 FRotator SurfaceOrbitOffset=FRotator::ZeroRotator;
 bool CanMaintainSurfaceAttachment() const;
 bool BeginAirHoverFromCursor(APlayerController* Controller);
 bool CanMaintainAirHover() const;
 FQuat AirHoverRotation=FQuat::Identity;
 float SurfaceRetreatGoal=0,SurfaceRetreatVelocity=0;
 double SurfaceFramingBlockedAt=-100;
 TWeakObjectPtr<APlayerController> SurfaceBlendController;
 FVector SurfaceBlendLocalEye=FVector::ZeroVector;
 float SurfaceBlendElapsed=0,SurfaceBlendRetreat=0;
 bool bSurfaceCameraBlending=false;
 void BeginSurfaceCameraBlend(APlayerController* Controller);
 void ApplySurfaceCameraBlend(float DeltaTime,FMinimalViewInfo& View);
 bool ApplySurfaceRetreat(float DeltaTime,float Required,FMinimalViewInfo& View);
 void LockSurfaceView(APlayerController* Controller);
};

UCLASS()
class BBSB_API ABBSBCaravanPerformer : public ABBSBInkEnemy
{
 GENERATED_BODY()
public:
 ABBSBCaravanPerformer();
 virtual void BeginPlay() override;
 virtual void Tick(float DT) override;
 virtual float TakeDamage(float Amount,const FDamageEvent& Event,AController* InstigatorController,AActor* Causer) override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> BunniarVisual;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCaptain=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bEncounterEnabled=false;
};

/** Self-contained, editable blockout. Existing maps, character assets and combat rules are untouched. */
UCLASS()
class BBSB_API ABBSBCaravanStage : public AActor
{
 GENERATED_BODY()
public:
 ABBSBCaravanStage();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void Tick(float DT) override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EBBSBCaravanStep Step=EBBSBCaravanStep::Rest;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ABBSBBunniarCompanion> Buddy;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<ABBSBBunniarDevice>> Devices;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ABBSBCaravanPerformer> Miniboss;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ABBSBCaravanPerformer> Captain;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ABBSBCombatStage> Encounter;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBBSBCaravanSave> Inventory;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ABBSBBunniarDevice> Focus;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ABBSBBunniarDevice> OpenShop;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bShutterOpen=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bBridgeOpen=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bLiftRidden=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bReadCaptainNote=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bVisitedArtCar=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ShopSelection=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FallsRecovered=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Checkpoint;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Dialogue;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double DialogueUntil=0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prototype") float InteractionDistance=270.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prototype") float StageBPM=88.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prototype") bool bPersistProgress=true;
 UFUNCTION(BlueprintCallable) bool InteractWith(ABBSBBunniarDevice* Device);
 UFUNCTION(BlueprintCallable) void Interact();
 UFUNCTION(BlueprintCallable) void RecallBuddy();
 UFUNCTION(BlueprintCallable) bool EquipBunniar();
 UFUNCTION(BlueprintCallable) void RetryCheckpoint();
 UFUNCTION(BlueprintCallable) void ConsumeTea();
 UFUNCTION(BlueprintCallable) bool Purchase(int32 Index);
 UFUNCTION(BlueprintCallable) FString Objective() const;
 UFUNCTION(BlueprintCallable) FVector ObjectiveLocation() const;
 UFUNCTION(BlueprintCallable) FString Status() const;
 UFUNCTION(BlueprintCallable) ABBSBBunniarDevice* FindDevice(EBBSBBunniarDevice Kind) const;
 UFUNCTION(BlueprintCallable) bool StartEncounter(bool bFinal);
 void Say(const FString& Text,float Seconds=6.f);
 void CloseShop();
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<UInputComponent> Controls;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> CargoGate;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RoofBridge;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ForwardGate;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> NorthGate;
 double StartedAt=0, DeathAt=-1;
 bool bBound=false,bIntroCue=false;
 bool bShopLookOwned=false,bShopMoveOwned=false;
 FVector P(FVector Local) const { return GetActorLocation()+Local; }
 UStaticMeshComponent* Shape(const FString& Name,FVector Position,FVector Size,FLinearColor Color,const TCHAR* Mesh=TEXT("Cube"),FRotator Rotation=FRotator::ZeroRotator,bool bSolid=true);
 void Text(const FString& Value,FVector Position,float Size=34.f,FColor Color=FColor(225,194,130),FRotator Rotation=FRotator(0,-90,0));
 void BuildLayout();
 void BindPlayer();
 void UpdateFocus();
 void UpdateProgress();
 ABBSBBunniarDevice* AimDevice() const;
 void EquipBunniarKey();
 void SaveProgress();
 void ShopUp(); void ShopDown(); void ShopBuy();
};

UCLASS()
class BBSB_API ABBSBCaravanHUD : public ABBSBFieldPracticeHUD
{
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
};
