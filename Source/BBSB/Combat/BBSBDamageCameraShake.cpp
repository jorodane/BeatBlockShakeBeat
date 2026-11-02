#include "Combat/BBSBDamageCameraShake.h"

UBBSBDamageCameraShake::UBBSBDamageCameraShake(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
 bSingleInstance=true;
 SetRootShakePattern(CreateDefaultSubobject<UBBSBDamageShakePattern>(TEXT("DamageMotion")));
}

void UBBSBDamageShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
 OutInfo.Duration=FCameraShakeDuration(.24f);OutInfo.BlendIn=.015f;OutInfo.BlendOut=.14f;
}
void UBBSBDamageShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
 State.Start(this,Params);
}
void UBBSBDamageShakePattern::Sample(float Weight,FCameraShakePatternUpdateResult& OutResult) const
{
 const float T=State.GetElapsedTime();
 OutResult.Location=FVector(0,FMath::Sin(T*83.f)*LocationAmplitude.Y,FMath::Sin(T*61.f)*LocationAmplitude.Z)*Weight;
 OutResult.Rotation=FRotator::ZeroRotator;OutResult.FOV=0.f;
}
void UBBSBDamageShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params,FCameraShakePatternUpdateResult& OutResult)
{
 Sample(State.Update(Params.DeltaTime),OutResult);
}
void UBBSBDamageShakePattern::ScrubShakePatternImpl(const FCameraShakePatternScrubParams& Params,FCameraShakePatternUpdateResult& OutResult)
{
 Sample(State.Scrub(Params.AbsoluteTime),OutResult);
}
