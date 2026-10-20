#pragma once

#include "CoreMinimal.h"
#include "BBSBFieldGesture.generated.h"

UENUM(BlueprintType)
enum class EBBSBFieldGesture : uint8
{
	Beat, Hold, Shake, Flick, Dive
};

/** Relative mouse motion is measured in pixels, independent of camera sensitivity. */
struct FBBSBFieldGestureSettings
{
	float HoldSeconds = 0.35f;
	float FlickPixelsPerSecond = 900.f;
	float FlickMinimumPixels = 26.f;
	float FlickWindowSeconds = 0.085f;
	float ShakeStrokePixels = 16.f;
	float ShakeWindowSeconds = 0.65f;
	float ShakeCooldownSeconds = 0.22f;
	int32 ShakeReversals = 3;
};

/** One independent recognizer per mouse button. Releasing always produces exactly one event. */
struct FBBSBFieldGestureRecognizer
{
	struct FSample { double Start; double End; FVector2D Delta; };
	bool bHeld = false;
	bool bHoldEmitted = false;
	double PressedAt = 0.0;
	double PreviousTime = 0.0;
	double LastShakeAt = -100.0;
	FVector2D StrokeDirection = FVector2D::ZeroVector;
	FVector2D ReleaseMotion = FVector2D::ZeroVector;
	float StrokeTravel = 0.f;
	TArray<double> Reversals;
	TArray<FSample> Samples;

	void Begin(double Now)
	{
		*this = FBBSBFieldGestureRecognizer();
		bHeld = true;
		PressedAt = PreviousTime = Now;
	}

	void Cancel() { *this = FBBSBFieldGestureRecognizer(); }

	TArray<EBBSBFieldGesture> Update(double Now, const FVector2D& Delta, const FBBSBFieldGestureSettings& Settings)
	{
		TArray<EBBSBFieldGesture> Events;
		if (!bHeld || Now <= PreviousTime) return Events;
		Samples.Add({PreviousTime, Now, Delta});
		PreviousTime = Now;
		Samples.RemoveAll([&](const FSample& S) { return S.End < Now - Settings.FlickWindowSeconds; });
		Reversals.RemoveAll([&](double Time) { return Time < Now - Settings.ShakeWindowSeconds; });
		if (!bHoldEmitted && Now - PressedAt >= Settings.HoldSeconds)
		{
			bHoldEmitted = true;
			Events.Add(EBBSBFieldGesture::Hold);
		}
		if (Delta.SizeSquared() >= 0.25f)
		{
			const FVector2D Direction = Delta.GetSafeNormal();
			if (StrokeDirection.IsNearlyZero()) StrokeDirection = Direction;
			if (FVector2D::DotProduct(Direction, StrokeDirection) < -0.45f)
			{
				if (StrokeTravel >= Settings.ShakeStrokePixels) Reversals.Add(Now);
				StrokeDirection = Direction;
				StrokeTravel = Delta.Size();
			}
			else StrokeTravel += Delta.Size();
			if (Reversals.Num() >= Settings.ShakeReversals && Now - LastShakeAt >= Settings.ShakeCooldownSeconds)
			{
				Events.Add(EBBSBFieldGesture::Shake);
				Reversals.Reset();
				LastShakeAt = Now;
			}
		}
		return Events;
	}

	EBBSBFieldGesture End(double Now, const FBBSBFieldGestureSettings& Settings)
	{
		FVector2D Travel = FVector2D::ZeroVector;
		const double WindowStart = FMath::Max(PressedAt, Now - Settings.FlickWindowSeconds);
		for (const FSample& Sample : Samples)
		{
			const double Overlap = FMath::Max(0.0, FMath::Min(Now, Sample.End) - FMath::Max(WindowStart, Sample.Start));
			Travel += Sample.Delta * (Overlap / FMath::Max(0.000001, Sample.End - Sample.Start));
		}
		const double Elapsed = FMath::Max(0.001, Now - WindowStart);
		ReleaseMotion=Travel;
		const bool bFlick = Travel.Size() >= Settings.FlickMinimumPixels && Travel.Size() / Elapsed >= Settings.FlickPixelsPerSecond;
		bHeld = false;
		return bFlick ? EBBSBFieldGesture::Flick : EBBSBFieldGesture::Dive;
	}
};
