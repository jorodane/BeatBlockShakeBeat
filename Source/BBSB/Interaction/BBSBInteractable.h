#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "BBSBInteractable.generated.h"

class ACharacter;
class ABBSBBunniarCompanion;
class UPrimitiveComponent;
class UTexture2D;

USTRUCT(BlueprintType)
struct BBSB_API FBBSBInteractPrompt
{
 GENERATED_BODY()

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") FText ActionText;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") TObjectPtr<UTexture2D> Icon;
};

UCLASS(BlueprintType)
class BBSB_API UBBSBInteractTypes : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction") TMap<FName, FBBSBInteractPrompt> Types;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hints") TMap<FName, TObjectPtr<UTexture2D>> HintIcons;

 static const UBBSBInteractTypes* DefaultCatalog();
};

USTRUCT(BlueprintType)
struct BBSB_API FBBSBConditionRequirement
{
 GENERATED_BODY()

 /** Empty means the condition lives on this actor. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition") TObjectPtr<AActor> Source;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition") FName Condition;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition", meta=(ClampMin="0", ClampMax="1")) float MinimumValue = .99f;
};

USTRUCT(BlueprintType)
struct BBSB_API FBBSBInteractContext
{
 GENERATED_BODY()

 UPROPERTY(BlueprintReadOnly, Category="Interaction") TObjectPtr<ACharacter> Character;
 UPROPERTY(BlueprintReadOnly, Category="Interaction") TObjectPtr<ABBSBBunniarCompanion> Bunniar;
 UPROPERTY(BlueprintReadOnly, Category="Interaction") TObjectPtr<UPrimitiveComponent> Surface;
 UPROPERTY(BlueprintReadOnly, Category="Interaction") FVector Point = FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly, Category="Interaction") FVector Normal = FVector::UpVector;
 UPROPERTY(BlueprintReadOnly, Category="Interaction") bool bHeld = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBBSBInteractEvent, const FBBSBInteractContext&, Context);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBBSBInteractStayEvent, const FBBSBInteractContext&, Context, float, DeltaSeconds);

/** Selectable by the existing F and Bunniar input paths. Add a derived component for a specific behavior. */
UCLASS(Blueprintable, ClassGroup=(BBSB), meta=(BlueprintSpawnableComponent, DisplayName="Interactable"))
class BBSB_API UBBSBInteractable : public UActorComponent
{
 GENERATED_BODY()
public:
 UBBSBInteractable();

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") FText DisplayName;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(GetOptions="GetInteractTypeOptions")) FName InteractType = TEXT("Use");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") TObjectPtr<UBBSBInteractTypes> InteractTypes;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") FBBSBInteractPrompt PromptOverride;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") FText UnavailableText;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") bool bInteractionEnabled = true;
 /** Disable on an auxiliary capability when a different component owns the actor's prompt and Bunniar input. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction") bool bPrimaryInteraction = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction") bool bUseBunniarAttachment = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Conditions") TArray<FBBSBConditionRequirement> RequiredConditions;

 UPROPERTY(BlueprintAssignable, Category="Interaction") FBBSBInteractEvent OnInteractEnter;
 UPROPERTY(BlueprintAssignable, Category="Interaction") FBBSBInteractStayEvent OnInteractStay;
 UPROPERTY(BlueprintAssignable, Category="Interaction") FBBSBInteractEvent OnInteractEnd;

 UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Interaction") bool CanInteract(ACharacter* Character) const;
 virtual bool CanInteract_Implementation(ACharacter* Character) const;
 UFUNCTION(BlueprintPure, Category="Interaction") bool CanStartInteraction(ACharacter* Character) const;
 UFUNCTION(BlueprintPure, Category="Interaction") FBBSBInteractPrompt GetPrompt() const;
 UFUNCTION(BlueprintPure, Category="Interaction") bool IsInteracting() const { return bInteracting; }
 UFUNCTION(BlueprintCallable, Category="Interaction") void SetInteractionEnabled(bool bEnabled);
 UFUNCTION() TArray<FString> GetInteractTypeOptions() const;

 virtual bool RequiresAttachment() const { return bUseBunniarAttachment; }
 bool BeginInteraction(const FBBSBInteractContext& Context);
 void EndInteraction();
 virtual void ApplyPointerDelta(FVector2D Delta) {}
 static UBBSBInteractable* Find(const AActor* Actor);

protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
 virtual void InteractionStarted() {}
 virtual void InteractionTick(float DeltaSeconds) {}
 virtual void InteractionEnded(const FBBSBInteractContext& Context) {}
 void UpdateInteractionTick();
 virtual bool NeedsIdleTick() const { return false; }

 UPROPERTY(Transient) FBBSBInteractContext ActiveContext;

private:
 const UBBSBInteractTypes* GetTypes() const;
 bool bInteracting = false;
 bool bCompanionInteraction = false;
};

/** Exposes the existing physical carry action in Blueprint, including takeoff and actual release. */
UCLASS(Blueprintable, ClassGroup=(BBSB), meta=(BlueprintSpawnableComponent, DisplayName="Grabbable"))
class BBSB_API UBBSBGrabbable : public UBBSBInteractable
{
 GENERATED_BODY()
public:
 UBBSBGrabbable();
 virtual bool CanInteract_Implementation(ACharacter* Character) const override;

 UPROPERTY(BlueprintAssignable, Category="Grab") FBBSBInteractEvent OnGrabEnter;
 UPROPERTY(BlueprintAssignable, Category="Grab") FBBSBInteractStayEvent OnGrabStay;
 UPROPERTY(BlueprintAssignable, Category="Grab") FBBSBInteractEvent OnGrabEnd;

 void BeginGrab();
 void EndGrab();

protected:
 virtual void InteractionTick(float DeltaSeconds) override;
 virtual void InteractionEnded(const FBBSBInteractContext& Context) override;

private:
 bool bGrabbing = false;
};
