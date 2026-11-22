#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Interaction/BBSBInteractable.h"
#include "BBSBConditions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBBSBConditionChanged, FName, Condition, float, Value);

/** Named 0–1 values. Each sender owns its contribution, so one release cannot clear another source. */
UCLASS(Blueprintable, ClassGroup=(BBSB), meta=(BlueprintSpawnableComponent, DisplayName="Condition Receiver"))
class BBSB_API UBBSBConditionReceiver : public UActorComponent
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Condition") TMap<FName, float> InitialConditions;
 UPROPERTY(BlueprintAssignable, Category="Condition") FBBSBConditionChanged OnConditionChanged;

 UFUNCTION(BlueprintPure, Category="Condition") float GetConditionValue(FName Condition) const;
 UFUNCTION(BlueprintPure, Category="Condition") bool IsConditionMet(FName Condition, float MinimumValue = .99f) const;
 UFUNCTION(BlueprintCallable, Category="Condition") void SetConditionValue(FName Condition, float Value);

 void SetSourceValue(FName Condition, UObject* Source, float Value);

private:
 TMap<FName, float> Values;
 TMap<FName, TMap<TWeakObjectPtr<UObject>, float>> Contributions;
 void NotifyChange(FName Condition, float Previous);
};

UENUM(BlueprintType)
enum class EBBSBConditionSwitchMode : uint8
{
 SetOn,
 Toggle,
 Hold,
 Pressure,
 PressureToggle
};

USTRUCT(BlueprintType)
struct BBSB_API FBBSBConditionTarget
{
 GENERATED_BODY()

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition") TObjectPtr<AActor> Target;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition") FName Condition;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBBSBSignalChanged, float, Value);

UCLASS(Blueprintable, ClassGroup=(BBSB), meta=(BlueprintSpawnableComponent, DisplayName="Condition Switch"))
class BBSB_API UBBSBConditionSwitch : public UBBSBInteractable
{
 GENERATED_BODY()
public:
 UBBSBConditionSwitch();

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Switch") EBBSBConditionSwitchMode Mode = EBBSBConditionSwitchMode::Toggle;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Switch") TArray<FBBSBConditionTarget> Targets;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Switch|Hold", meta=(ClampMin="0", Units="s")) float HoldFillSeconds = 0.f;
 /** The bottom of this box is just below the top of the solid pressure plate. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Switch|Pressure", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.BoxComponent")) FComponentReference PressureVolume;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Switch|Pressure", meta=(ClampMin="0", Units="kg")) float MinimumPressure = 1.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Switch|Pressure", meta=(ClampMin="0", Units="kg")) float CharacterPressure = 70.f;
 UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Switch") float SignalValue = 0.f;
 UPROPERTY(BlueprintAssignable, Category="Switch") FBBSBSignalChanged OnSignalChanged;

 UFUNCTION(BlueprintCallable, Category="Switch") void ResetSignal();
 virtual bool CanInteract_Implementation(ACharacter* Character) const override;
 virtual bool RequiresAttachment() const override { return Mode == EBBSBConditionSwitchMode::Hold; }

protected:
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
 virtual bool NeedsIdleTick() const override;
 virtual void InteractionStarted() override;
 virtual void InteractionTick(float DeltaSeconds) override;
 virtual void InteractionEnded(const FBBSBInteractContext& Context) override;

private:
 using FOutput = TPair<TWeakObjectPtr<UBBSBConditionReceiver>, FName>;
 TArray<FOutput> Driven;
 bool bWasPressed = false;
 bool IsPressureMode() const;
 bool HasPressure() const;
 void Publish(float Value);
};
