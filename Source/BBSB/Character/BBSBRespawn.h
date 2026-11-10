#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraTypes.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "BBSBRespawn.generated.h"

class ABBSBCharacter;
class ABBSBBunniarCompanion;
class UBBSBRespawnComponent;
class UAnimSequence;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTextBlock;
class UButton;
class USceneComponent;
class UBBSBHuntingSave;
class UBBSBViewOcclusionMaterials;
class UMeshComponent;
class UMaterialInterface;

USTRUCT()
struct FBBSBTravelFadeMesh
{
 GENERATED_BODY()
 UPROPERTY() TWeakObjectPtr<UMeshComponent> Mesh;
 UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Originals;
 UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Replacements;
 float Weight=0,Kind=0,InstanceKind=0;
 bool bHidden=false;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBBSBCheckpointActivated,ABBSBCharacter*,Player);

UENUM(BlueprintType)
enum class EBBSBRespawnPhase : uint8 { Alive,Downed,AwaitingChoice,Returning };

/** Place on the ground. The landing marker is editable independently of the beacon. */
UCLASS(Blueprintable)
class BBSB_API ABBSBRespawnPoint : public AActor,public IBBSBBunniarSurface
{
 GENERATED_BODY()
public:
 ABBSBRespawnPoint();
 virtual void BeginPlay() override;
 virtual void Tick(float DT) override;
 virtual void OnConstruction(const FTransform& Transform) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Base;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Beacon;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USceneComponent> LandingPoint;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Label;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn") FText PointName=NSLOCTEXT("BBSB","RespawnPointName","휴식의 표식");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn",meta=(ClampMin="100",Units="cm")) float ActivationRange=700;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn") FName PointId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn") bool bTravelAvailable=true;
 UFUNCTION(BlueprintPure,Category="Respawn") FString TravelId() const;
 UFUNCTION(BlueprintCallable,Category="Respawn") bool ActivateFor(ABBSBCharacter* Player);
 UPROPERTY(BlueprintAssignable,Category="Respawn") FBBSBCheckpointActivated OnActivated;
 UFUNCTION(BlueprintPure,Category="Respawn") bool IsActivePoint() const;
 FTransform LandingTransform(const ABBSBCharacter* Player) const;
 virtual FText BunniarInteractionText_Implementation() const override;
 virtual bool BunniarQuickUse_Implementation(ABBSBBunniarCompanion* Bunniar) override;
 virtual void BunniarAttached_Implementation(ABBSBBunniarCompanion* Bunniar,FVector Point,FVector Normal) override;
private:
 bool bWasActive=false;
};

/** A focusable modal, with mouse, keyboard and gamepad confirmation. */
UCLASS(Blueprintable)
class BBSB_API UBBSBRespawnWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UPROPERTY(Transient,BlueprintReadOnly) TObjectPtr<UBBSBRespawnComponent> Respawn;
 void FocusConfirm();
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
 UPROPERTY(Transient) TObjectPtr<UTextBlock> Message;
 UPROPERTY(Transient) TObjectPtr<UButton> ConfirmButton;
 UFUNCTION() void Confirm();
};

/** Owns one death/recovery sequence; inventory and defeated enemies keep their state. */
UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBRespawnComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UBBSBRespawnComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Respawn") EBBSBRespawnPhase Phase=EBBSBRespawnPhase::Alive;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Respawn") TObjectPtr<ABBSBRespawnPoint> ActivePoint;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn|Animation") TSoftObjectPtr<UAnimSequence> DeathAnimation;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn|Animation") TSoftObjectPtr<UAnimSequence> CarryAnimation;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn|Presentation",meta=(ClampMin=".5",Units="s")) float DeathPresentationSeconds=1.8f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn|Presentation",meta=(ClampMin="1.5",Units="s")) float ReturnSeconds=3.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn|Presentation",meta=(ClampMin="100",Units="cm")) float DeliveryHeight=260.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn|Presentation",meta=(ClampMin="0",Units="s")) float ProtectionSeconds=2.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn|UI") TSubclassOf<UBBSBRespawnWidget> PromptClass;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Respawn|UI") TObjectPtr<UBBSBRespawnWidget> Prompt;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Respawn|Travel") TSet<FString> DiscoveredPoints;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Respawn|Travel") bool bCarryingPlayer=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Respawn|Travel") float CarryBlend=0;
 /** Retained for existing assets; warp timing is now independent of map distance. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn|Travel",meta=(ClampMin="300")) float CarrySpeed=2200;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Respawn|Travel",meta=(ClampMin="10",ClampMax="150",Units="cm")) float WarpLiftHeight=75.f;
 UPROPERTY(EditAnywhere,Category="Respawn|Travel") TSoftObjectPtr<UBBSBViewOcclusionMaterials> TravelFadeLibrary;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Respawn|Travel") float TravelOpacity=1.f;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Respawn|Travel") bool bTravelRelocated=false;
 UFUNCTION(BlueprintPure,Category="Respawn|Travel") bool IsPointDiscovered(const ABBSBRespawnPoint* Point) const;
 UFUNCTION(BlueprintCallable,Category="Respawn|Travel") bool RequestFastTravel(ABBSBRespawnPoint* Point);
 void LoadTravel(const UBBSBHuntingSave* Save);
 void StoreTravel(UBBSBHuntingSave* Save) const;
 void GetCarryPose(UAnimSequence*& Carry,UAnimSequence*& Fallen,float& Alpha) const;
 FVector CarryGrip(bool Left) const;
 UFUNCTION(BlueprintCallable,Category="Respawn") bool SetCheckpoint(ABBSBRespawnPoint* Point);
 UFUNCTION(BlueprintCallable,Category="Respawn") bool TryActivateFromAim();
 UFUNCTION(BlueprintCallable,Category="Respawn") void BeginDeath();
 UFUNCTION(BlueprintCallable,Category="Respawn") bool RequestRespawn();
 UFUNCTION(BlueprintPure,Category="Respawn") bool IsSequenceActive() const { return Phase!=EBBSBRespawnPhase::Alive; }
 UFUNCTION(BlueprintPure,Category="Respawn") bool IsDamageProtected() const;
 UFUNCTION(BlueprintPure,Category="Respawn") FTransform RespawnTransform() const;
 void ResetPresentation();
 void ApplyCamera(float DT,FMinimalViewInfo& View) const;
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<ABBSBBunniarCompanion> Buddy;
 UPROPERTY(Transient) TObjectPtr<UClass> SavedAnimClass;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> DeathClip;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> CarryClip;
 struct FDroppedVisual { TWeakObjectPtr<AActor> Actor;FVector Velocity;bool Landed=false; };
 TArray<FDroppedVisual> Drops;
 FTransform InitialSpawn,ReturnTarget;
 FTransform TravelFrom;
 FVector BuddyPickupFrom;
 float TravelDepartureLift=0,TravelArrivalLift=0;
 TWeakObjectPtr<class APlayerCameraManager> TravelFadeCamera;
 UPROPERTY(Transient) TObjectPtr<UBBSBViewOcclusionMaterials> FadeLibrary;
 void BeginTravelFade();
 void SetTravelOpacity(float Opacity);
 void RestoreTravelFade();
 float ClearLiftHeight(FVector Position) const;
 bool bTravelFromDeath=true,bCarryPoseStarted=false;
 FString PendingPointId;
 UPROPERTY(Transient) TArray<TObjectPtr<AActor>> HiddenTravelWeapons;
 bool bInitialSpawnSaved=false,bMovedToReturn=false,bOwnMoveLock=false,bOwnLookLock=false,bOriginalCursor=false,bOriginalEquipmentBlock=false;
 bool bOriginalPostProcess=false;
 bool bOriginalHUD=true;
 bool bSavedCameraLag=false,bOriginalCameraLag=false,bOriginalCameraRotationLag=false;
 float SavedGravityScale=1;
 uint8 SavedAnimationMode=0;
 double StartedAt=0,ProtectedUntil=-100;
 float PromptDelay=1.8f;
 void ShowPrompt();
 void TickDrops(float DT);
 void TickBuddy(float Age,bool Returning);
 bool ResolveReturnTransform(FTransform& Out) const;
 bool PrepareTravel(const FTransform& Target,bool FromDeath);
 void BeginCarryPose();
 void SavePresentation();
 void FinishRespawn();
 void RestoreAnimation();
 void ReleaseInput();
};
