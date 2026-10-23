#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BBSBAimGaze.generated.h"

/** Caches bounded head intent; movement, camera and the authored body graph keep their authority. */
UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBAimGazeComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UBBSBAimGazeComponent();
 virtual void BeginPlay() override;
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Gaze") bool bEnableHeadGaze=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Gaze",meta=(ClampMin="0",ClampMax="80")) float MaximumYaw=50;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Gaze",meta=(ClampMin="0",ClampMax="50")) float MaximumPitch=25;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Gaze",meta=(ClampMin="0",ClampMax="1")) float AimWeight=.7f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Gaze",meta=(ClampMin="1")) float InterpSpeed=10;
 UPROPERTY(EditAnywhere,Category="Gaze") TSoftClassPtr<UAnimInstance> HeadPostProcess;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Gaze") FVector AimPoint;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Gaze") FRotator HeadAimRotation;
 FQuat ComponentAimRotation=FQuat::Identity;
};

/** Passes through the entire incoming body pose, adding only bounded neck/head rotation. */
UCLASS(Transient,Blueprintable)
class BBSB_API UBBSBHeadGazeAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};

/** Adds the engine's linked input pose to this project's dedicated gaze post-process asset. */
UCLASS()
class BBSB_API UBBSBGazeAuthoringTools : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="BBSB|Gaze") static bool ConfigurePostProcessInput(UObject* BlueprintAsset);
};
