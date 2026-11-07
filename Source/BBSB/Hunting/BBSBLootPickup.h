#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "Hunting/BBSBItem.h"
#include "BBSBLootPickup.generated.h"

class UBBSBItemDefinition;
class UBBSBHuntingComponent;
class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UBBSBBladeTrailComponent;

UCLASS(Blueprintable)
class BBSB_API ABBSBLootPickup : public AActor,public IBBSBBunniarSurface
{
 GENERATED_BODY()
public:
 ABBSBLootPickup();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void Tick(float DT) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USphereComponent> PickupQuery;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Presentation") TObjectPtr<UStaticMeshComponent> RarityBeam;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Presentation") TObjectPtr<UBBSBBladeTrailComponent> DropTrail;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Presentation") TObjectPtr<UBBSBBladeTrailComponent> CollectionTrail;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Presentation") FLinearColor CollectionColor=FLinearColor::White;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Presentation") FVector CollectionDestination=FVector::ZeroVector;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Presentation") bool bCollectionArrived=false;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Presentation") bool bCollectionVisualOnly=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation",meta=(ClampMin=".15",ClampMax="1",Units="s")) float CollectionFlightSeconds=.42f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation") TSoftObjectPtr<UMaterialInterface> BeamMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation") TSoftObjectPtr<UMaterialInterface> TrailMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation") TSoftObjectPtr<UMaterialInterface> ResourceGlowMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation",meta=(ClampMin="1",ClampMax="8")) float ResourceGlowStrength=2.2f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation|Resources") TSoftObjectPtr<UTexture2D> EcstaticImage;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation|Resources") TSoftObjectPtr<UTexture2D> NoteImage;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation|Resources",meta=(ClampMin="30",ClampMax="160",Units="cm")) float ResourceHoverHeight=90.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation|Resources",meta=(ClampMin="20",ClampMax="80",Units="cm")) float ResourceSpriteSize=48.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation",meta=(ClampMin=".2",ClampMax="2")) float DropSeconds=.82f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation",meta=(ClampMin="20",ClampMax="400",Units="cm")) float DropArcHeight=155.f;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Presentation") FVector DropVelocity=FVector::ZeroVector;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Presentation") bool bNameVisible=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Item") TObjectPtr<UBBSBItemDefinition> Item;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Item",meta=(ClampMin="1")) int32 Quantity=1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Item") bool bClaimed=false;
 // Retained on the drop so an interrupted concerto cannot later recharge itself.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Rewards") bool bConcertoReward=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Item") FBBSBInventoryEntry WeaponPayload;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Item") bool bDropping=false;
 UFUNCTION(BlueprintCallable,Category="Item") void BeginDrop(FVector RestPoint);
 int32 TakeQuantity(int32 Requested);
 void BeginCollectionFlight(UBBSBHuntingComponent* Collector);
 void PlayCollectionFlight(UBBSBHuntingComponent* Collector);
 UFUNCTION(BlueprintPure,Category="Presentation") FLinearColor BeamColor() const { return Item ? Item->IsFieldResource() ? Item->Color:Item->RarityColor():FLinearColor::White; }
 UFUNCTION(BlueprintPure,Category="Item") bool IsAvailable() const { return !bClaimed && Item && Quantity>0; }
private:
 TWeakObjectPtr<UBBSBHuntingComponent> RegisteredWith,CollectingWith;
 FVector FlightFrom=FVector::ZeroVector,Rest=FVector::ZeroVector;
 FVector MeshScale=FVector(.22f);
 double FlightStartedAt=0;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BeamMID;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ResourceGlowMID;
 bool bLargeResource=false;
 double NameQueryAt=0;
 float DropGravity=1200.f;
 bool bDropDeflected=false;
 void TickDrop(float DT);
 void FinishDrop(const FVector& Point);
 void AddTrailSample();
 void UpdateNameVisibility();
 void UpdateResourceFacing();
};
