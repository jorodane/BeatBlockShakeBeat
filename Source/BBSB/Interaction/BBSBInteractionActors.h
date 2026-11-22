#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BBSBInteractionActors.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class USplineComponent;
class UStaticMeshComponent;
class UBBSBConditionReceiver;
class UBBSBInteractable;

/** World authoring defaults. The capability components also work on custom Blueprint actors. */
UCLASS(Blueprintable)
class BBSB_API ABBSBInteractableActor : public AActor
{
 GENERATED_BODY()
public:
 ABBSBInteractableActor(const FObjectInitializer& ObjectInitializer);

 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interaction") TObjectPtr<UBBSBInteractable> Interaction;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Condition") TObjectPtr<UBBSBConditionReceiver> Conditions;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Placement") TObjectPtr<UStaticMeshComponent> MotionBody;
};

UCLASS(Blueprintable)
class BBSB_API ABBSBDraggableActor : public ABBSBInteractableActor
{
 GENERATED_BODY()
public:
 ABBSBDraggableActor(const FObjectInitializer& ObjectInitializer);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drag") TObjectPtr<USplineComponent> DragPath;
};

UCLASS(Blueprintable)
class BBSB_API ABBSBClimbableActor : public ABBSBInteractableActor
{
 GENERATED_BODY()
public:
 ABBSBClimbableActor(const FObjectInitializer& ObjectInitializer);
 virtual void OnConstruction(const FTransform& Transform) override;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb") bool bGenerateRungs = true;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climb") TObjectPtr<UInstancedStaticMeshComponent> Rungs;
};

UCLASS(Blueprintable)
class BBSB_API ABBSBConditionSwitchActor : public ABBSBInteractableActor
{
 GENERATED_BODY()
public:
 ABBSBConditionSwitchActor(const FObjectInitializer& ObjectInitializer);
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Switch") TObjectPtr<UBoxComponent> PressureArea;
};
