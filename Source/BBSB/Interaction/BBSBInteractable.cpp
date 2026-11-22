#include "Interaction/BBSBInteractable.h"

#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Interaction/BBSBConditions.h"
#include "Interaction/BBSBInteraction.h"
#include "Kismet/GameplayStatics.h"

UBBSBInteractable::UBBSBInteractable()
{
 PrimaryComponentTick.bCanEverTick = true;
 PrimaryComponentTick.bStartWithTickEnabled = false;
 UnavailableText = NSLOCTEXT("BBSBInteraction", "Unavailable", "조건 미충족");
}

void UBBSBInteractable::BeginPlay()
{
 Super::BeginPlay();
 if (auto* Player = Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)); Player && Player->Interaction)
 {
  Player->Interaction->RegisterInteractionCandidate(GetOwner());
 }
 UpdateInteractionTick();
}

void UBBSBInteractable::EndPlay(EEndPlayReason::Type Reason)
{
 EndInteraction();
 Super::EndPlay(Reason);
}

UBBSBInteractable* UBBSBInteractable::Find(const AActor* Actor)
{
 if (!IsValid(Actor)) return nullptr;
 TInlineComponentArray<UBBSBInteractable*> Components(Actor);
 for (auto* Component : Components)
 {
  if (Component->bPrimaryInteraction) return Component;
 }
 return nullptr;
}

bool UBBSBInteractable::CanInteract_Implementation(ACharacter* Character) const
{
 return true;
}

bool UBBSBInteractable::CanStartInteraction(ACharacter* Character) const
{
 if (!bInteractionEnabled || !IsValid(GetOwner()) || GetOwner()->IsHidden()) return false;
 for (const auto& Requirement : RequiredConditions)
 {
  const AActor* Source = Requirement.Source ? Requirement.Source.Get() : GetOwner();
  const auto* Receiver = IsValid(Source) ? Source->FindComponentByClass<UBBSBConditionReceiver>() : nullptr;
  if (!Receiver || !Receiver->IsConditionMet(Requirement.Condition, Requirement.MinimumValue)) return false;
 }
 return CanInteract(Character);
}

void UBBSBInteractable::SetInteractionEnabled(bool bEnabled)
{
 bInteractionEnabled = bEnabled;
 if (!bEnabled && bInteracting)
 {
  if (IsValid(ActiveContext.Bunniar) && ActiveContext.bHeld) ActiveContext.Bunniar->ReleaseSurface();
  EndInteraction();
 }
}

const UBBSBInteractTypes* UBBSBInteractable::GetTypes() const
{
 return InteractTypes ? InteractTypes.Get() : UBBSBInteractTypes::DefaultCatalog();
}

const UBBSBInteractTypes* UBBSBInteractTypes::DefaultCatalog()
{
 static TWeakObjectPtr<UBBSBInteractTypes> DefaultTypes;
 if (!DefaultTypes.IsValid())
 {
  DefaultTypes = LoadObject<UBBSBInteractTypes>(nullptr, TEXT("/Game/BBSB/Placeables/Interactions/DA_InteractTypes.DA_InteractTypes"), nullptr, LOAD_NoWarn);
 }
 return DefaultTypes.Get();
}

FBBSBInteractPrompt UBBSBInteractable::GetPrompt() const
{
 FBBSBInteractPrompt Result = PromptOverride;
 if (const auto* Types = GetTypes())
 {
  if (const auto* Entry = Types->Types.Find(InteractType))
  {
   if (Result.ActionText.IsEmpty()) Result.ActionText = Entry->ActionText;
   if (!Result.Icon) Result.Icon = Entry->Icon;
  }
 }
 return Result;
}

TArray<FString> UBBSBInteractable::GetInteractTypeOptions() const
{
 TArray<FString> Options{TEXT("None")};
 if (const auto* Types = GetTypes())
 {
  for (const auto& Entry : Types->Types) Options.Add(Entry.Key.ToString());
 }
 else Options.Add(InteractType.ToString());
 Options.Sort();
 return Options;
}

bool UBBSBInteractable::BeginInteraction(const FBBSBInteractContext& Context)
{
 if (bInteracting || !CanStartInteraction(Context.Character)) return false;
 ActiveContext = Context;
 bInteracting = true;
 bCompanionInteraction = Context.bHeld && IsValid(Context.Bunniar);
 OnInteractEnter.Broadcast(Context);
 if (bInteracting) InteractionStarted();
 if (!Context.bHeld) EndInteraction();
 UpdateInteractionTick();
 return true;
}

void UBBSBInteractable::EndInteraction()
{
 if (!bInteracting) return;
 const auto Context = ActiveContext;
 ActiveContext = {};
 bInteracting = false;
 bCompanionInteraction = false;
 InteractionEnded(Context);
 OnInteractEnd.Broadcast(Context);
 UpdateInteractionTick();
}

void UBBSBInteractable::UpdateInteractionTick()
{
 SetComponentTickEnabled(bInteracting || NeedsIdleTick());
}

void UBBSBInteractable::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
 Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
 if (!bInteracting) return;
 if ((bCompanionInteraction && !IsValid(ActiveContext.Bunniar)) || !IsValid(ActiveContext.Character) || !CanStartInteraction(ActiveContext.Character))
 {
  if (IsValid(ActiveContext.Bunniar)) ActiveContext.Bunniar->ReleaseSurface();
  EndInteraction();
  return;
 }
 if (bCompanionInteraction)
 {
  ActiveContext.Surface = ActiveContext.Bunniar->SurfaceComponent;
  ActiveContext.Point = ActiveContext.Bunniar->SurfacePoint;
  ActiveContext.Normal = ActiveContext.Bunniar->SurfaceNormal;
 }
 const auto Context = ActiveContext;
 OnInteractStay.Broadcast(Context, DeltaSeconds);
 if (bInteracting) InteractionTick(DeltaSeconds);
}

UBBSBGrabbable::UBBSBGrabbable()
{
 bUseBunniarAttachment = true;
 InteractType = TEXT("Grab");
 UnavailableText = NSLOCTEXT("BBSBInteraction", "GrabUnavailable", "조금 떨어져서 옮기기");
}

bool UBBSBGrabbable::CanInteract_Implementation(ACharacter* Character) const
{
 const auto* Carry = Cast<ABBSBCarryObject>(GetOwner());
 return Carry && (Carry->bCarried ? Carry->CarryingCharacter() == Character : Carry->CanBeCarriedBy(Character));
}

void UBBSBGrabbable::BeginGrab()
{
 if (!bGrabbing && IsInteracting())
 {
  bGrabbing = true;
  const auto Context = ActiveContext;
  OnGrabEnter.Broadcast(Context);
 }
}

void UBBSBGrabbable::EndGrab()
{
 if (!bGrabbing) return;
 bGrabbing = false;
 const auto Context = ActiveContext;
 OnGrabEnd.Broadcast(Context);
}

void UBBSBGrabbable::InteractionTick(float DeltaSeconds)
{
 if (!bGrabbing) return;
 const auto Context = ActiveContext;
 OnGrabStay.Broadcast(Context, DeltaSeconds);
}

void UBBSBGrabbable::InteractionEnded(const FBBSBInteractContext& Context)
{
 if (!bGrabbing) return;
 bGrabbing = false;
 OnGrabEnd.Broadcast(Context);
}
