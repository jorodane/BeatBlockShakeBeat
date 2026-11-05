#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "BBSBInteractionDoor.generated.h"
class UStaticMeshComponent;
UCLASS(Blueprintable)
class BBSB_API ABBSBInteractionDoor : public AActor,public IBBSBBunniarSurface
{
 GENERATED_BODY()
public:
 ABBSBInteractionDoor();
 virtual void Tick(float DT) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USceneComponent> Hinge;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Leaf;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door") FText DoorName;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door") bool bLocked=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Door") float OpenDegrees=95;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Door") bool bOpen=false;
 UFUNCTION(BlueprintCallable,Category="Door") bool ToggleDoor();
 virtual void BunniarAttached_Implementation(ABBSBBunniarCompanion*,FVector,FVector) override { ToggleDoor(); }
 virtual FText BunniarInteractionText_Implementation() const override;
};
