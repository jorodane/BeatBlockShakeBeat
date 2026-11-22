#include "Interaction/BBSBConditions.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/BBSBInteraction.h"

namespace
{
float ConditionValue(float Value)
{
 return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : 0.f;
}
}

float UBBSBConditionReceiver::GetConditionValue(FName Condition) const
{
 const float* Value = Values.Find(Condition);
 float Result = Value ? *Value : ConditionValue(InitialConditions.FindRef(Condition));
 if (const auto* Sources = Contributions.Find(Condition))
 {
  for (const auto& Entry : *Sources)
  {
   if (Entry.Key.IsValid()) Result = FMath::Max(Result, Entry.Value);
  }
 }
 return Result;
}

bool UBBSBConditionReceiver::IsConditionMet(FName Condition, float MinimumValue) const
{
 return !Condition.IsNone() && GetConditionValue(Condition) >= ConditionValue(MinimumValue);
}

void UBBSBConditionReceiver::NotifyChange(FName Condition, float Previous)
{
 const float Current = GetConditionValue(Condition);
 if (!FMath::IsNearlyEqual(Previous, Current, KINDA_SMALL_NUMBER)) OnConditionChanged.Broadcast(Condition, Current);
}

void UBBSBConditionReceiver::SetConditionValue(FName Condition, float Value)
{
 if (Condition.IsNone()) return;
 const float Previous = GetConditionValue(Condition);
 Values.Add(Condition, ConditionValue(Value));
 NotifyChange(Condition, Previous);
}

void UBBSBConditionReceiver::SetSourceValue(FName Condition, UObject* Source, float Value)
{
 if (Condition.IsNone() || !Source) return;
 const float Previous = GetConditionValue(Condition);
 Value = ConditionValue(Value);
 if (Value > 0.f) Contributions.FindOrAdd(Condition).Add(Source, Value);
 else if (auto* Sources = Contributions.Find(Condition))
 {
  Sources->Remove(Source);
  if (Sources->IsEmpty()) Contributions.Remove(Condition);
 }
 NotifyChange(Condition, Previous);
}

UBBSBConditionSwitch::UBBSBConditionSwitch()
{
 InteractType = TEXT("Switch");
 PressureVolume.ComponentProperty = TEXT("PressureArea");
 PrimaryComponentTick.TickInterval = .05f;
}

bool UBBSBConditionSwitch::IsPressureMode() const
{
 return Mode == EBBSBConditionSwitchMode::Pressure || Mode == EBBSBConditionSwitchMode::PressureToggle;
}

bool UBBSBConditionSwitch::CanInteract_Implementation(ACharacter* Character) const
{
 return !IsPressureMode() && Super::CanInteract_Implementation(Character);
}

bool UBBSBConditionSwitch::NeedsIdleTick() const
{
 return IsPressureMode();
}

void UBBSBConditionSwitch::InteractionStarted()
{
 switch (Mode)
 {
 case EBBSBConditionSwitchMode::SetOn: Publish(1.f); break;
 case EBBSBConditionSwitchMode::Toggle: Publish(SignalValue > .5f ? 0.f : 1.f); break;
 case EBBSBConditionSwitchMode::Hold: Publish(HoldFillSeconds > 0.f ? 0.f : 1.f); break;
 default: break;
 }
}

void UBBSBConditionSwitch::InteractionTick(float DeltaSeconds)
{
 if (Mode == EBBSBConditionSwitchMode::Hold && HoldFillSeconds > 0.f) Publish(SignalValue + DeltaSeconds / HoldFillSeconds);
}

void UBBSBConditionSwitch::InteractionEnded(const FBBSBInteractContext& Context)
{
 if (Mode == EBBSBConditionSwitchMode::Hold) Publish(0.f);
}

void UBBSBConditionSwitch::ResetSignal()
{
 Publish(0.f);
}

void UBBSBConditionSwitch::EndPlay(EEndPlayReason::Type Reason)
{
 Publish(0.f);
 Super::EndPlay(Reason);
}

void UBBSBConditionSwitch::Publish(float Value)
{
 Value = ConditionValue(Value);
 const bool bChanged = !FMath::IsNearlyEqual(SignalValue, Value, KINDA_SMALL_NUMBER);
 SignalValue = Value;
 TArray<FOutput> Next;
 if (Value > 0.f)
 {
  for (const auto& Entry : Targets)
  {
   if (!IsValid(Entry.Target) || Entry.Condition.IsNone()) continue;
   if (auto* Receiver = Entry.Target->FindComponentByClass<UBBSBConditionReceiver>())
   {
    Next.AddUnique(FOutput(Receiver, Entry.Condition));
   }
  }
 }
 const auto Previous = MoveTemp(Driven);
 Driven = Next;
 for (const auto& Output : Previous)
 {
  if (auto* Receiver = Output.Key.Get(); Receiver && !Next.Contains(Output)) Receiver->SetSourceValue(Output.Value, this, 0.f);
 }
 for (const auto& Output : Next)
 {
  if (auto* Receiver = Output.Key.Get()) Receiver->SetSourceValue(Output.Value, this, Value);
 }
 if (bChanged) OnSignalChanged.Broadcast(SignalValue);
}

bool UBBSBConditionSwitch::HasPressure() const
{
 const auto* Area = Cast<UBoxComponent>(PressureVolume.GetComponent(GetOwner()));
 if (!Area || !GetWorld()) return false;

 FCollisionObjectQueryParams Channels;
 Channels.AddObjectTypesToQuery(ECC_Pawn);
 Channels.AddObjectTypesToQuery(ECC_PhysicsBody);
 Channels.AddObjectTypesToQuery(ECC_WorldDynamic);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBPressureSwitch), false, GetOwner());
 TArray<FOverlapResult> Overlaps;
 const FVector Extent = Area->GetScaledBoxExtent();
 GetWorld()->OverlapMultiByObjectType(Overlaps, Area->GetComponentLocation(), Area->GetComponentQuat(), Channels, FCollisionShape::MakeBox(Extent), Query);

 const FVector Up = Area->GetUpVector();
 TSet<const AActor*> Counted;
 float Pressure = 0.f;
 for (const auto& Hit : Overlaps)
 {
  const auto* Part = Hit.GetComponent();
  const auto* Actor = Hit.GetActor();
  if (!IsValid(Actor) || !IsValid(Part) || Counted.Contains(Actor)) continue;
  const auto* Character = Cast<ACharacter>(Actor);
  if (Character)
  {
   if (Part != Character->GetRootComponent() || !Character->GetCharacterMovement()->IsMovingOnGround()) continue;
  }
  else if (!Part->IsSimulatingPhysics()) continue;
  if (const auto* Carry = Cast<ABBSBCarryObject>(Actor); Carry && (Carry->bCarried || Carry->bBroken)) continue;

  // Only feet or the underside of a supported body count, including when the plate is rotated.
  const FVector Center = Part->Bounds.Origin;
  const double Bottom = FVector::DotProduct(Center - Area->GetComponentLocation(), Up) - FVector::DotProduct(Part->Bounds.BoxExtent, Up.GetAbs());
  if (Bottom < -Extent.Z - 4.f || Bottom > -Extent.Z + 16.f || FMath::Abs(FVector::DotProduct(Part->GetComponentVelocity(), Up)) > 100.f) continue;
  Counted.Add(Actor);
  Pressure += Character ? CharacterPressure : Part->GetMass();
 }
 return !Counted.IsEmpty() && Pressure >= MinimumPressure;
}

void UBBSBConditionSwitch::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
 Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
 if (!IsPressureMode()) return;
 const bool bPressed = bInteractionEnabled && HasPressure();
 if (Mode == EBBSBConditionSwitchMode::Pressure && bPressed != bWasPressed) Publish(bPressed ? 1.f : 0.f);
 if (Mode == EBBSBConditionSwitchMode::PressureToggle && bPressed && !bWasPressed) Publish(SignalValue > .5f ? 0.f : 1.f);
 if (!bInteractionEnabled && SignalValue > 0.f) Publish(0.f);
 bWasPressed = bPressed;
}
