#pragma once

#include "CoreMinimal.h"
#include "Interaction/BBSBInteractable.h"
#include "BBSBClimbable.generated.h"

class ABBSBCharacter;
class UAnimSequence;
class UInstancedStaticMeshComponent;

UENUM(BlueprintType)
enum class EBBSBClimbMotion : uint8
{
 Hold,
 Step,
 Hop,
 Slide,
 Mantle
};

USTRUCT(BlueprintType)
struct BBSB_API FBBSBClimbFrame
{
 GENERATED_BODY()

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb", meta=(MakeEditWidget)) FVector Bottom = FVector(0, 0, 96);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb", meta=(MakeEditWidget)) FVector Top = FVector(0, 0, 596);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb", meta=(MakeEditWidget)) FVector Exit = FVector(-125, 0, 600);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb") FVector Outward = FVector::ForwardVector;
};

USTRUCT(BlueprintType)
struct BBSB_API FBBSBClimbAnimations
{
 GENERATED_BODY()

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UAnimSequence> Hold;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UAnimSequence> Step;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UAnimSequence> Hop;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UAnimSequence> Slide;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UAnimSequence> Mantle;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBBSBClimbEnterEvent, ABBSBCharacter*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBBSBClimbStayEvent, ABBSBCharacter*, Character, float, Progress, EBBSBClimbMotion, Motion);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBBSBClimbEndEvent, ABBSBCharacter*, Character, bool, bReachedTop);

UCLASS(Blueprintable, ClassGroup=(BBSB), meta=(BlueprintSpawnableComponent, DisplayName="Climbable"))
class BBSB_API UBBSBClimbable : public UBBSBInteractable
{
 GENERATED_BODY()
public:
 UBBSBClimbable();

 /** Capsule-center route, in the owning actor's coordinate space. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb") FBBSBClimbFrame LocalFrame;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb", meta=(ClampMin="20", ClampMax="50", Units="cm")) float RungSpacing = 35.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb", meta=(ClampMin="0", Units="cm")) float GripDepth = 32.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb") bool bAutoGrab = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb") bool bClimbEnabled = true;
 /** Unassigned clips use the character's existing traversal set. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climb|Animation") FBBSBClimbAnimations Animations;

 UPROPERTY(BlueprintAssignable, Category="Climb") FBBSBClimbEnterEvent OnClimbEnter;
 UPROPERTY(BlueprintAssignable, Category="Climb") FBBSBClimbStayEvent OnClimbStay;
 UPROPERTY(BlueprintAssignable, Category="Climb") FBBSBClimbEndEvent OnClimbEnd;

 UFUNCTION(BlueprintPure, Category="Climb") FBBSBClimbFrame WorldFrame() const;
 UFUNCTION(BlueprintPure, Category="Climb") UAnimSequence* AnimationFor(EBBSBClimbMotion Motion) const;
 UFUNCTION(BlueprintCallable, Category="Climb") bool TryMount(ABBSBCharacter* Character);
 virtual bool CanInteract_Implementation(ACharacter* Character) const override;

 void NotifyClimbEnter(ABBSBCharacter* Character);
 void NotifyClimbStay(ABBSBCharacter* Character, float Progress, EBBSBClimbMotion Motion);
 void NotifyClimbEnd(ABBSBCharacter* Character, bool bReachedTop);
 void SetRouteReady(bool bReady) { bRouteReady = bReady; }
 bool IsRouteReady() const { return bClimbEnabled && bRouteReady; }
 static void AddRungs(UInstancedStaticMeshComponent* Mesh, const FBBSBClimbFrame& Frame, float Spacing, float From, float To, FVector Layer = FVector::ZeroVector);

private:
 TWeakObjectPtr<ABBSBCharacter> Climber;
 bool bRouteReady = true;
};
