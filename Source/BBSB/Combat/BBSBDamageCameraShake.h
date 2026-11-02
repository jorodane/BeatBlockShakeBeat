#pragma once

#include "Camera/CameraShakeBase.h"
#include "BBSBDamageCameraShake.generated.h"

/** Small additive view-space motion. It never re-aims or rotates the camera. */
UCLASS()
class BBSB_API UBBSBDamageShakePattern : public UCameraShakePattern
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category="Shake") FVector LocationAmplitude=FVector(0,2.5,1.7);
private:
 FCameraShakeState State;
 virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
 virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
 virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params,FCameraShakePatternUpdateResult& OutResult) override;
 virtual void ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params,FCameraShakePatternUpdateResult& OutResult) override;
 virtual bool IsFinishedImpl() const override { return !State.IsPlaying(); }
 virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override { State.Stop(Params.bImmediately); }
 virtual void TeardownShakePatternImpl() override { State=FCameraShakeState(); }
 void Sample(float Weight,FCameraShakePatternUpdateResult& OutResult) const;
};

UCLASS()
class BBSB_API UBBSBDamageCameraShake : public UCameraShakeBase
{
 GENERATED_BODY()
public:
 UBBSBDamageCameraShake(const FObjectInitializer& ObjectInitializer);
};
