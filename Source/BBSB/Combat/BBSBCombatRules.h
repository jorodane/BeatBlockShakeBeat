#pragma once

#include "CoreMinimal.h"
#include "Field/BBSBFieldGesture.h"
#include "BBSBCombatRules.generated.h"

UENUM(BlueprintType)
enum class EBBSBActionState : uint8 { Idle, Attack, Dodge, Hit, Transition, Dead };

UENUM(BlueprintType)
enum class EBBSBWeaponCommitment : uint8 { Authored, Agile, Heavy };

UENUM(BlueprintType)
enum class EBBSBParryDirection : uint8 { Up, Right, Down, Left };

UENUM(BlueprintType)
enum class EBBSBParryResult : uint8 { Block, Early, Miss, Evade };

UENUM(BlueprintType)
enum class EBBSBTimingKind : uint8 { Waiting, FreshPress, HoldTick, NoRecentPress, Unavailable };

UENUM(BlueprintType)
enum class EBBSBInkPattern : uint8 { Tap, Curve, Zigzag, Bounce, Hold, Area };

UENUM(BlueprintType)
enum class EBBSBStagePhase : uint8 { Forming, CountIn, Fighting, Replacing, Dissolved, Victory };

/** Weapon authors can tune commitment without changing the input recognizer. No action queue exists. */
USTRUCT(BlueprintType)
struct FBBSBWeaponActionRule
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Duration = 0.32f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Priority = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLockMovement = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOccupyConductor = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDodgeCancelable = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1")) float DodgeCancelOpen = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1")) float ChainCancelOpen = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAllowGestureChain = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DamageMultiplier = 1.f;
};

/** The same arbitration is used by runtime actions and automation tests. */
struct FBBSBActionArbiter
{
	EBBSBActionState State = EBBSBActionState::Idle;
	FBBSBWeaponActionRule Rule;
	double StartedAt = 0;
	int32 Channel = -1;
	uint32 Revision = 0;

	bool IsBusy(double Now) const { return State != EBBSBActionState::Idle && (State == EBBSBActionState::Dead || Now < StartedAt + Rule.Duration); }
	float Progress(double Now) const { return FMath::Clamp(float((Now-StartedAt)/FMath::Max(Rule.Duration,0.001f)),0.f,1.f); }
	bool CanAttack(const FBBSBWeaponActionRule& Next, int32 NextChannel, double Now, bool bGestureChain=true) const
	{
		if (!IsBusy(Now)) return true;
		return bGestureChain && State == EBBSBActionState::Attack && Rule.bAllowGestureChain && NextChannel == Channel
			&& Progress(Now) >= Rule.ChainCancelOpen && Next.Priority >= Rule.Priority;
	}
	bool CanDodge(double Now) const
	{
		if (!IsBusy(Now)) return true;
		return State == EBBSBActionState::Attack && Rule.bDodgeCancelable && Progress(Now) >= Rule.DodgeCancelOpen;
	}
	void Start(EBBSBActionState Next, const FBBSBWeaponActionRule& NextRule, int32 NextChannel, double Now)
	{
		State=Next; Rule=NextRule; Channel=NextChannel; StartedAt=Now; ++Revision;
	}
	void Clear() { State=EBBSBActionState::Idle; Channel=-1; ++Revision; }
};

struct FBBSBParryInput
{
	bool bHeld = false;
	double PressedAt = -100;
	uint32 PressSerial = 0;
	uint32 ConsumedSerial = 0;
	int32 HoldingGroup = INDEX_NONE;
	void Down(double Now) { if (!bHeld) { bHeld=true; PressedAt=Now; ++PressSerial; HoldingGroup=INDEX_NONE; } }
	void Up() { bHeld=false; HoldingGroup=INDEX_NONE; }
	EBBSBParryResult Resolve(double Now, int32 HoldGroup, float BlockWindow, float EarlyWindow)
	{
		if (HoldGroup != INDEX_NONE && bHeld && HoldingGroup == HoldGroup) return EBBSBParryResult::Block;
		const double Age = Now - PressedAt;
		if (PressSerial != ConsumedSerial && Age >= 0 && Age <= BlockWindow)
		{
			ConsumedSerial=PressSerial; HoldingGroup=bHeld ? HoldGroup : INDEX_NONE; return EBBSBParryResult::Block;
		}
		HoldingGroup=INDEX_NONE;
		if (PressSerial != ConsumedSerial && Age > BlockWindow && Age <= EarlyWindow)
		{
			ConsumedSerial=PressSerial; return EBBSBParryResult::Early;
		}
		return EBBSBParryResult::Miss;
	}
};

/** Observes input before contact resolution. Never consumes a press or changes a judgement. */
struct FBBSBParryTimingHistory
{
	static constexpr int32 Capacity=24;
	TArray<float> Samples;
	EBBSBTimingKind Kind=EBBSBTimingKind::Waiting;
	float LastLeadMs=-1.f;
	void Observe(const FBBSBParryInput& Input,double Now,int32 HoldGroup,bool bAvailable,float RecentSeconds)
	{
		LastLeadMs=-1.f; Kind=EBBSBTimingKind::Unavailable;
		if (!bAvailable) return;
		if (HoldGroup!=INDEX_NONE && Input.bHeld && Input.HoldingGroup==HoldGroup) { Kind=EBBSBTimingKind::HoldTick; return; }
		const double Age=Now-Input.PressedAt;
		Kind=EBBSBTimingKind::NoRecentPress;
		if (Input.PressSerial==Input.ConsumedSerial || !FMath::IsFinite(Age) || Age<0 || Age>RecentSeconds) return;
		LastLeadMs=float(Age*1000.0); Kind=EBBSBTimingKind::FreshPress;
		if (Samples.Num()>=Capacity) Samples.RemoveAt(0);
		Samples.Add(LastLeadMs);
	}
	float MedianMs() const
	{
		if (Samples.IsEmpty()) return 0.f;
		TArray<float> Sorted=Samples; Sorted.Sort();
		const int32 Middle=Sorted.Num()/2;
		return Sorted.Num()%2 ? Sorted[Middle] : (Sorted[Middle-1]+Sorted[Middle])*0.5f;
	}
	float MinMs() const { float Value=Samples.IsEmpty() ? 0.f : Samples[0]; for (float Sample:Samples) Value=FMath::Min(Value,Sample); return Value; }
	float MaxMs() const { float Value=0.f; for (float Sample:Samples) Value=FMath::Max(Value,Sample); return Value; }
	void Reset() { Samples.Reset(); Kind=EBBSBTimingKind::Waiting; LastLeadMs=-1.f; }
};

namespace BBSBCombatRules
{
	/** Runtime comparison only. Authored arrays and movement/conductor permissions stay intact. */
	inline FBBSBWeaponActionRule WeaponRuleForPreset(const FBBSBWeaponActionRule& Authored, EBBSBWeaponCommitment Preset)
	{
		FBBSBWeaponActionRule Rule=Authored;
		if (Preset==EBBSBWeaponCommitment::Agile)
		{
			Rule.Duration=FMath::Max(0.08f,Authored.Duration*0.8f);
			Rule.DamageMultiplier=Authored.DamageMultiplier*0.85f;
			Rule.DodgeCancelOpen=Authored.DodgeCancelOpen*0.35f;
			Rule.ChainCancelOpen=Authored.ChainCancelOpen*0.6f;
		}
		else if (Preset==EBBSBWeaponCommitment::Heavy)
		{
			Rule.Duration=FMath::Max(0.08f,Authored.Duration*1.5f);
			Rule.DamageMultiplier=Authored.DamageMultiplier*1.35f;
			Rule.DodgeCancelOpen=FMath::Max(Authored.DodgeCancelOpen,0.45f);
			Rule.ChainCancelOpen=FMath::Max(Authored.ChainCancelOpen,0.65f);
		}
		return Rule;
	}
	inline const TCHAR* WeaponCommitmentName(EBBSBWeaponCommitment Preset)
	{
		return Preset==EBBSBWeaponCommitment::Agile ? TEXT("Agile") : Preset==EBBSBWeaponCommitment::Heavy ? TEXT("Heavy") : TEXT("Baseline");
	}
	inline EBBSBParryDirection DirectionAtPlane(const FVector2D& Contact, const FVector2D& HalfSize)
	{
		const FVector2D P(Contact.X/FMath::Max(HalfSize.X,1.0),Contact.Y/FMath::Max(HalfSize.Y,1.0));
		if (FMath::Abs(P.Y) >= FMath::Abs(P.X)) return P.Y >= 0 ? EBBSBParryDirection::Up : EBBSBParryDirection::Down;
		return P.X >= 0 ? EBBSBParryDirection::Right : EBBSBParryDirection::Left;
	}
	inline bool PlaneIntersection(const FVector& From, const FVector& To, const FVector& Origin, const FVector& Normal, FVector& Out)
	{
		const double A=FVector::DotProduct(From-Origin,Normal), B=FVector::DotProduct(To-Origin,Normal);
		if (A < 0 || B > 0 || FMath::Abs(A-B)<UE_SMALL_NUMBER) return false;
		Out=FMath::Lerp(From,To,A/(A-B)); return true;
	}
 /** Earliest swept projectile contact with the physical character capsule, not a remote plane. */
 inline bool SweptCapsuleContact(const FVector& From,const FVector& To,const FVector& Center,float Radius,float HalfHeight,float ProjectileRadius,FVector& Contact,float& Fraction)
 {
  const float Shaft=FMath::Max(0.f,HalfHeight-Radius),R=Radius+ProjectileRadius;
  const FVector A=Center-FVector::UpVector*Shaft,B=Center+FVector::UpVector*Shaft;
  FVector OnShot,OnCapsule;FMath::SegmentDistToSegmentSafe(From,To,A,B,OnShot,OnCapsule);
  if(FVector::DistSquared(OnShot,OnCapsule)>R*R) return false;
  const FVector Delta=To-From;
  auto Inside=[&](float T) { const FVector P=From+Delta*T;return FVector::DistSquared(P,FMath::ClosestPointOnSegment(P,A,B))<=R*R; };
  if(Inside(0.f)) { Fraction=0;Contact=From;return true; }
  float Lo=0,Hi=Delta.SizeSquared()>UE_SMALL_NUMBER ? FMath::Clamp(float(FVector::DotProduct(OnShot-From,Delta)/Delta.SizeSquared()),0.f,1.f):0;
  for(int32 I=0;I<16;++I) { const float Mid=(Lo+Hi)*.5f;if(Inside(Mid)) Hi=Mid;else Lo=Mid; }
  Fraction=Hi;Contact=From+Delta*Hi;return true;
 }
	inline double BeatAt(double Now, double Epoch, float BPM) { return (Now-Epoch)*FMath::Max(BPM,1.f)/60.0; }
	inline float EcstaticDamageMultiplier(float Progress) { return 1.f+0.15f*FMath::Clamp(FMath::FloorToInt(Progress/100.f),0,5); }

	/** Quantized impact sockets. At most two directions, with held directions reserved across their duration. */
	struct FSocketLedger
	{
		TMap<int32,uint8> Used;
		bool Reserve(float Beat, uint8 Directions, float DurationBeats=0.f)
		{
			const int32 First=FMath::RoundToInt(Beat*4.f), Last=FMath::RoundToInt((Beat+DurationBeats)*4.f);
			for (int32 Slot=First; Slot<=Last; ++Slot)
			{
				const uint8 Existing=Used.FindRef(Slot), Combined=Existing|Directions;
				if ((Existing&Directions)!=0 || FMath::CountBits(uint32(Combined))>2) return false;
			}
			for (int32 Slot=First; Slot<=Last; ++Slot) Used.FindOrAdd(Slot)|=Directions;
			return true;
		}
	};
}
