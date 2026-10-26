#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Engine/DataAsset.h"
#include "BBSBBunniarAnimation.generated.h"

class UAnimSequence;
class ULevelSequence;
class USkeletalMesh;

UENUM(BlueprintType)
enum class EBBSBBunniarMotion : uint8 { Idle, Travel, Equipped, Attached, Gliding, AirDash };

/** Edit LS_Bunniar_* with FK controls; linked A_Bunniar_* clips are the runtime poses. */
UCLASS(BlueprintType)
class BBSB_API UBBSBBunniarAnimationSet : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Model") TObjectPtr<USkeletalMesh> Mesh;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Model", meta=(ClampMin="0.01")) float CompanionScale=.12f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Model", meta=(ClampMin="0.01")) float EquippedScale=.175f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UAnimSequence> Idle;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UAnimSequence> Travel;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UAnimSequence> Equipped;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UAnimSequence> Attached;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Animation") TObjectPtr<UAnimSequence> Gliding;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Animation") TObjectPtr<UAnimSequence> AirDash;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation", meta=(ClampMin="0.01")) float BlendSeconds=.16f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation", meta=(ClampMin="1")) float TravelSpeedThreshold=60;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Authoring") TArray<TObjectPtr<ULevelSequence>> EditableSources;
 /** Mesh-space outward direction through the face, before the visual component's rotation. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface") FVector FaceDirection=FVector(0,1,0);
 /** Mesh-space point on the back, before the visual component's scale/offset. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface") FVector BackContactPoint=FVector(0,-54,70);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0")) float SurfaceGap=2;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0",ClampMax="1")) float AttachTravelSeconds=.24f;
 UAnimSequence* Clip(int32 Index) const;
};

/** Small native player blends ordinary editable animation assets, never writes ear bones in gameplay. */
UCLASS(Transient, BlueprintType)
class BBSB_API UBBSBBunniarAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 virtual void NativeInitializeAnimation() override;
 virtual void NativeUpdateAnimation(float DeltaSeconds) override;
 UPROPERTY(Transient, BlueprintReadOnly, Category="Bunniar") TObjectPtr<UBBSBBunniarAnimationSet> AnimationSet;
 UPROPERTY(Transient, BlueprintReadOnly, Category="Bunniar") EBBSBBunniarMotion Motion=EBBSBBunniarMotion::Idle;
protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
