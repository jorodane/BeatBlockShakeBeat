#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Interaction/BBSBInteractable.h"
#include "BBSBDraggable.generated.h"

class APlayerController;
class USceneComponent;
class USplineComponent;

UENUM(BlueprintType)
enum class EBBSBDragConstraint : uint8
{
 Axis,
 Area,
 Spline
};

USTRUCT(BlueprintType)
struct BBSB_API FBBSBDragState
{
 GENERATED_BODY()

 UPROPERTY(BlueprintReadOnly, Category="Drag") float Progress = 0.f;
 UPROPERTY(BlueprintReadOnly, Category="Drag") FVector LocalOffset = FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly, Category="Drag") bool bBlocked = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBBSBDragEvent, const FBBSBInteractContext&, Context, const FBBSBDragState&, State);

UCLASS(Blueprintable, ClassGroup=(BBSB), meta=(BlueprintSpawnableComponent, DisplayName="Draggable"))
class BBSB_API UBBSBDraggable : public UBBSBInteractable
{
 GENERATED_BODY()
public:
 UBBSBDraggable();

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag") EBBSBDragConstraint Constraint = EBBSBDragConstraint::Axis;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.SceneComponent")) FComponentReference MovedComponent;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag|Axis", meta=(MakeEditWidget, EditCondition="Constraint == EBBSBDragConstraint::Axis", EditConditionHides)) FVector LocalTravel = FVector(0, 240, 0);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag|Area", meta=(MakeEditWidget, EditCondition="Constraint == EBBSBDragConstraint::Area", EditConditionHides)) FVector AreaMin = FVector(0, -200, 0);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag|Area", meta=(MakeEditWidget, EditCondition="Constraint == EBBSBDragConstraint::Area", EditConditionHides)) FVector AreaMax = FVector(0, 200, 200);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag|Spline", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.SplineComponent", EditCondition="Constraint == EBBSBDragConstraint::Spline", EditConditionHides)) FComponentReference Spline;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag", meta=(ClampMin="0.05", Units="cm")) float UnitsPerPixel = 1.3f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag", meta=(ClampMin="1", Units="cm/s")) float Speed = 850.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag", meta=(ClampMin="0", ClampMax="1")) float InitialProgress = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag|Conditions") FName DriveCondition;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drag") FText DragHintText;
 UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Drag") FBBSBDragState DragState;
 UPROPERTY(BlueprintAssignable, Category="Drag") FBBSBDragEvent OnDragEnter;
 UPROPERTY(BlueprintAssignable, Category="Drag") FBBSBDragEvent OnDragStay;
 UPROPERTY(BlueprintAssignable, Category="Drag") FBBSBDragEvent OnDragEnd;

 UFUNCTION(BlueprintCallable, Category="Drag") void SetDragProgress(float Progress);
 UFUNCTION(BlueprintPure, Category="Drag") bool IsDragging() const { return bDragging; }
 virtual bool CanInteract_Implementation(ACharacter* Character) const override;
 virtual void ApplyPointerDelta(FVector2D Delta) override;
 bool GetDragHint(FVector2D& ScreenAxis, FText& Text) const;

 // Story devices supply their own moving parts but share the lifecycle and input projection.
 void SetExternalDrag(bool bAvailable, float Progress, const FVector& LocalOffset, bool bBlocked = false);
 bool bExternalMotion = false;
 static float ProjectLinearDrag(const APlayerController* Controller, const FVector& PointAtZero, const FVector& Travel,
  float Progress, FVector2D Delta, float PixelRange, bool bFallbackHorizontal);

protected:
 virtual void BeginPlay() override;
 virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
 virtual bool NeedsIdleTick() const override;
 virtual void InteractionStarted() override;
 virtual void InteractionTick(float DeltaSeconds) override;
 virtual void InteractionEnded(const FBBSBInteractContext& Context) override;

private:
 TWeakObjectPtr<USceneComponent> Moving;
 TWeakObjectPtr<USplineComponent> Path;
 FVector Home = FVector::ZeroVector;
 FVector TargetOffset = FVector::ZeroVector;
 float TargetProgress = 0.f;
 bool bExternalDragAvailable = false;
 bool bDragging = false;

 FVector RouteOffset(float Progress) const;
 float RouteLength() const;
 void MovePart(float DeltaSeconds);
};
