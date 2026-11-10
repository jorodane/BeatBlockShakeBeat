#include "BBSBFieldActions.h"
#include "Character/BBSBVisibilityStencil.h"
#include "BBSBCharacter.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Hunting/BBSBItemIcon.h"
#include "Character/BBSBTraversal.h"
#include "BBSB.h"
#include "Caravan/BBSBCaravan.h"
#include "Combat/BBSBCombat.h"
#include "Interaction/BBSBInteraction.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "RenderUtils.h"

void UBBSBFieldActionComponent::EnsureBunniarEquipment()
{
 if (!Character || IsValid(BunniarEquipment)) return;
 ABBSBBunniarCompanion* Buddy=nullptr;
 for (TActorIterator<ABBSBBunniarCompanion> It(GetWorld());It;++It)
  if (!It->GetOwner() || It->GetOwner()==Character) { Buddy=*It; break; }
 if (!Buddy)
 {
  FActorSpawnParameters Params; Params.Owner=Character; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  Buddy=GetWorld()->SpawnActor<ABBSBBunniarCompanion>(Character->GetActorLocation(),FRotator::ZeroRotator,Params);
 }
 if (Buddy)
 {
  Buddy->SetOwner(Character); Buddy->bFollowing=true; BunniarEquipment=Buddy;
  BBSBVisibilityStencil::UpdateHandOccluders(Character,BBSBVisibilityStencil::Enabled(GetWorld()));
  EquipmentGesture.BindUObject(Buddy,&ABBSBBunniarCompanion::HandleEquipmentGesture);
 }
}

int32 UBBSBFieldActionComponent::EquippedEquipmentId() const { return FieldEquipment ? 0:bInventoryLoadoutManaged && PrototypeWeapon==EBBSBPrototypeWeapon::Legacy ? INDEX_NONE:int32(PrototypeWeapon); }

FString UBBSBFieldActionComponent::EquipmentLabel(int32 Id) const
{
 if(Id<0)return TEXT("장비 없음");
 const TCHAR* Names[]={TEXT("버니아"),TEXT("대검"),TEXT("창"),TEXT("해머"),TEXT("한손검"),TEXT("단검"),TEXT("크레센트")};
 return Names[FMath::Clamp(Id,0,6)];
}

int32 UBBSBFieldActionComponent::WheelEquipmentId(int32 Index) const
{
 if (Index<0 || Index>=EquipmentWheelSlotCount) return INDEX_NONE;
 if(bInventoryLoadoutManaged)return EquipmentWheelIds.IsValidIndex(Index) && EquipmentWheelIds[Index]>0 ? EquipmentWheelIds[Index]:INDEX_NONE;
 return AvailableWeapons.IsValidIndex(Index) ? int32(AvailableWeapons[Index]):INDEX_NONE;
}

int32 UBBSBFieldActionComponent::EquipmentPreviewSlot() const
{
 const FGuid Target=PendingWeaponInstance.IsValid() ? PendingWeaponInstance:EquippedWeaponInstance;
 for(int32 I=0;I<EquipmentWheelSlotCount;++I)
  if(bInventoryLoadoutManaged ? Target.IsValid() && EquipmentWheelInstances.IsValidIndex(I) && EquipmentWheelInstances[I]==Target:
    WheelEquipmentId(I)==(PendingEquipmentId>0 ? PendingEquipmentId:EquippedEquipmentId()))return I;
 return INDEX_NONE;
}

bool UBBSBFieldActionComponent::IsEquipmentActionBusy() const
{
 if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->EquipmentGrowth && P->EquipmentGrowth->bConcertoActive)return true;
 if (const auto* Player=Cast<ABBSBCharacter>(Character);Player && Player->Traversal && (Player->Traversal->bGlideDashing || Player->Traversal->IsWireTraversalActive() || Player->Traversal->IsParkourActive())) return true;
 if (!Character || bDodging || IsGestureCaptured() || CurrentAction.IsSet() || OtherWeaponAction.IsSet() || IsWeaponReturning() || Arbiter.IsBusy(GetWorld()->GetTimeSeconds())) return true;
 const auto* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
 return Combat && !Combat->CanAttack();
}

bool UBBSBFieldActionComponent::RequestEquipment(int32 Id)
{
 if(bTutorialEquipmentGate && Id>0 && !TutorialWeaponInstance.IsValid())return false;
 if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && P->Traversal->IsWireTraversalActive()) return false;
 EnsureBunniarEquipment();
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 if (!Character || bEquipmentInputBlocked || (Combat && Combat->bDead)) return false;
 if (Id==0 ? (!IsValid(BunniarEquipment) || !bBunniarAvailable):Id<1 || Id>6 || !AvailableWeapons.Contains(EBBSBPrototypeWeapon(Id))) return false;
 if(Id>0 && bInventoryLoadoutManaged)
 {
  const auto* H=Character->FindComponentByClass<UBBSBHuntingComponent>();
  if(H)
  {
   const auto* Previous=H->FindOwnedEntry(LastCombatWeaponInstance);
   if(Previous && Previous->Item && Previous->Item->WeaponId==Id && EquipmentWheelInstances.Contains(Previous->InstanceId))return RequestWeaponInstance(Previous->InstanceId);
   for(int32 I=0;I<EquipmentWheelIds.Num();++I)if(EquipmentWheelIds[I]==Id)return RequestEquipmentSlot(I);
  }
 }
 ResetQueuedWheelTravel();PendingWeaponInstance.Invalidate();
 // Choosing the equipped item cancels an earlier request. Requests never accumulate.
 PendingEquipmentId=Id==EquippedEquipmentId() ? INDEX_NONE:Id;
 UE_LOG(LogBBSB,Display,TEXT("Equipment request id=%d pending=%d busy=%d"),Id,PendingEquipmentId,IsEquipmentActionBusy());
 return true;
}

bool UBBSBFieldActionComponent::RequestWeaponInstance(FGuid Id)
{
 if(bTutorialEquipmentGate && Id!=TutorialWeaponInstance)return false;
 const auto* H=Character ? Character->FindComponentByClass<UBBSBHuntingComponent>():nullptr;
 const auto* E=H ? H->FindOwnedEntry(Id):nullptr;
 const auto* P=Cast<ABBSBCharacter>(Character);
 if(!E || !E->Item || E->Item->EquipmentKind!=EBBSBItemEquipmentKind::Weapon || !EquipmentWheelInstances.Contains(Id) || bEquipmentInputBlocked || (P && P->Combat->bDead))return false;
 ResetQueuedWheelTravel();
 const bool Already=Id==EquippedWeaponInstance && Id==PresentedWeaponInstance && !FieldEquipment && int32(PrototypeWeapon)==E->Item->WeaponId;
 PendingEquipmentId=Already ? INDEX_NONE:E->Item->WeaponId;PendingWeaponInstance=Already ? FGuid():Id;return true;
}
bool UBBSBFieldActionComponent::RequestEquipmentSlot(int32 Index)
{
 if(bInventoryLoadoutManaged)return EquipmentWheelInstances.IsValidIndex(Index) ? RequestWeaponInstance(EquipmentWheelInstances[Index]):false;
 return WheelEquipmentId(Index)!=INDEX_NONE && RequestEquipment(WheelEquipmentId(Index));
}
bool UBBSBFieldActionComponent::IsWheelSlotEquipped(int32 Index) const
{
 if(WheelEquipmentId(Index)==INDEX_NONE)return false;
 return bInventoryLoadoutManaged ? !FieldEquipment && EquipmentWheelInstances.IsValidIndex(Index) && EquippedWeaponInstance.IsValid() && EquipmentWheelInstances[Index]==EquippedWeaponInstance:WheelEquipmentId(Index)==EquippedEquipmentId();
}

bool UBBSBFieldActionComponent::CanCycleEquipmentWheel() const
{
 const auto* Player=Cast<ABBSBCharacter>(Character);
 return Player && !bEquipmentInputBlocked && !bEquipmentWheelOpen && !Player->Combat->bDead &&
    !Player->Hunting->bInventoryOpen && !Player->Interaction->bDialogueActive && !Player->Traversal->IsWireTraversalActive() && EquippedEquipmentId()>0;
}
void UBBSBFieldActionComponent::ResetEquipmentScrollBurst()
{ LastEquipmentScrollAt=-1;EquipmentScrollDirection=0;bEquipmentScrollHalf=false; }
bool UBBSBFieldActionComponent::ScrollEquipmentWheel(int32 Direction)
{
 if(Direction==0 || !CanCycleEquipmentWheel()) { ResetEquipmentScrollBurst();return false; }
 const double Now=FPlatformTime::Seconds();const int32 Step=Direction>0 ? 1:-1;
 const bool First=LastEquipmentScrollAt<0 || Now-LastEquipmentScrollAt>=.35 || Step!=EquipmentScrollDirection;
 LastEquipmentScrollAt=Now;EquipmentScrollDirection=Step;
 if(First)bEquipmentScrollHalf=false;
 else if(!bEquipmentScrollHalf) { bEquipmentScrollHalf=true;return true; }
 else bEquipmentScrollHalf=false;
 const bool Accepted=CycleEquipmentWheel(Step);if(!Accepted)ResetEquipmentScrollBurst();return Accepted;
}
bool UBBSBFieldActionComponent::CycleEquipmentWheel(int32 Direction)
{
 if(Direction==0 || !CanCycleEquipmentWheel())return false;
 // Repeated notches continue from the latest request while a busy action finishes.
 int32 Start=INDEX_NONE;
 for(int32 Index=0;Index<EquipmentWheelSlotCount;++Index)
 {
  const bool Pending=PendingEquipmentId!=INDEX_NONE && (PendingEquipmentId==0 ? WheelEquipmentId(Index)==0:
   bInventoryLoadoutManaged ? EquipmentWheelInstances.IsValidIndex(Index) && EquipmentWheelInstances[Index]==PendingWeaponInstance:WheelEquipmentId(Index)==PendingEquipmentId);
  if(Pending) { Start=Index;break; }
  if(PendingEquipmentId==INDEX_NONE && IsWheelSlotEquipped(Index)) { Start=Index;break; }
 }
 if(Start==INDEX_NONE)Start=Direction>0 ? EquipmentWheelSlotCount-1:0;
 const int32 Step=Direction>0 ? 1:-1;
 for(int32 Offset=1;Offset<EquipmentWheelSlotCount;++Offset)
 {
  const int32 Index=(Start+Step*Offset+EquipmentWheelSlotCount)%EquipmentWheelSlotCount;
  if(WheelEquipmentId(Index)!=INDEX_NONE)
  {
   const int64 PreviousTravel=QueuedWheelTarget==Start ? QueuedWheelTravel:0;
   if(!RequestEquipmentSlot(Index))return false;
   // Requests may coalesce while an action is busy, but the visible wheel must
   // retain every skipped slot and full revolution in the requested direction.
   QueuedWheelTravel=PreviousTravel+Step*Offset;QueuedWheelTarget=Index;QueuedWheelDirection=Step;
   QueuedWheelInstance=bInventoryLoadoutManaged && WheelEquipmentId(Index)>0 && EquipmentWheelInstances.IsValidIndex(Index) ? EquipmentWheelInstances[Index]:FGuid();
   return true;
  }
 }
 return false;
}

void UBBSBFieldActionComponent::ResetQueuedWheelTravel()
{ QueuedWheelTravel=0;QueuedWheelTarget=INDEX_NONE;QueuedWheelDirection=0;QueuedWheelInstance.Invalidate(); }
void UBBSBFieldActionComponent::CommitQueuedWheelTravel()
{
 const bool SameInstance=!QueuedWheelInstance.IsValid() || (EquipmentWheelInstances.IsValidIndex(QueuedWheelTarget) && EquipmentWheelInstances[QueuedWheelTarget]==QueuedWheelInstance);
 if(QueuedWheelTravel!=0 && SameInstance && IsWheelSlotEquipped(QueuedWheelTarget))
 {
  EquipmentWheelTravel+=QueuedWheelTravel;EquipmentWheelTravelDirection=QueuedWheelDirection;++EquipmentWheelTravelRevision;
 }
 ResetQueuedWheelTravel();
}

void UBBSBFieldActionComponent::EquipmentTabDown()
{
 if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && P->Traversal->IsWireTraversalActive())return;
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 if(!Character || bEquipmentTabHeld || bEquipmentInputBlocked || (Combat && Combat->bDead))return;
 bEquipmentTabHeld=true;bEquipmentTabCancelled=false;ResetEquipmentScrollBurst();
 const int32 Intended=PendingEquipmentId!=INDEX_NONE ? PendingEquipmentId:EquippedEquipmentId();
 if(Intended==0)
 {
  if(LastCombatWeaponInstance.IsValid() && EquipmentWheelInstances.Contains(LastCombatWeaponInstance))RequestWeaponInstance(LastCombatWeaponInstance);
  else if(!AvailableWeapons.IsEmpty())RequestEquipment(int32(AvailableWeapons[0]));
 }
 else if(bBunniarAvailable)RequestEquipment(0);
}
void UBBSBFieldActionComponent::EquipmentTabUp()
{ bEquipmentTabHeld=false; }

void UBBSBFieldActionComponent::CloseEquipmentWheel()
{
 if (!bEquipmentWheelOpen) return;
 bEquipmentWheelOpen=false; EquipmentWheelIndex=INDEX_NONE;
 if (GetWorld() && FMath::IsNearlyEqual(GetWorld()->GetWorldSettings()->TimeDilation,WheelAppliedTimeDilation,.00001f))
  UGameplayStatics::SetGlobalTimeDilation(this,WheelSavedTimeDilation);
 if (auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr)
 {
  const auto* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
  const bool Locked=Combat && Combat->IsStageLocked();
  const bool Cursor=Locked || (bWheelOpenedInCombat ? PC->bShowMouseCursor:bWheelOriginalCursor);
  PC->bShowMouseCursor=Cursor;
  if (Cursor) PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen));
  else PC->SetInputMode(FInputModeGameOnly().SetConsumeCaptureMouseDown(false));
  if (Cursor && bWheelSavedMousePosition) PC->SetMouseLocation(FMath::RoundToInt(WheelSavedMousePosition.X),FMath::RoundToInt(WheelSavedMousePosition.Y));
  PC->RotationInput=FRotator::ZeroRotator;
  if(auto* H=Character->FindComponentByClass<UBBSBHuntingComponent>();H && H->bInventoryOpen)H->RefreshInventoryInput();
 }
 // Cursor travel chooses a menu item; it must never become a weapon Flick or Shake.
 SyntheticMotion=FVector2D::ZeroVector; SampledMouseFrame=GFrameCounter;
 for (auto& Channel:Channels)
 {
  Channel.Samples.Reset(); Channel.Reversals.Reset(); Channel.ReleaseMotion=FVector2D::ZeroVector;
  Channel.StrokeDirection=FVector2D::ZeroVector; Channel.StrokeTravel=0;
  Channel.PreviousTime=GetWorld() ? GetWorld()->GetTimeSeconds():0;
 }
 EquipmentLookResumeAt=FPlatformTime::Seconds()+.05;
 UE_LOG(LogBBSB,Display,TEXT("Equipment wheel closed"));
}

void UBBSBFieldActionComponent::CancelEquipmentSelection()
{
 ResetEquipmentScrollBurst();ResetQueuedWheelTravel();bEquipmentTabCancelled=true; bEquipmentTabHeld=false; PendingEquipmentId=INDEX_NONE;PendingWeaponInstance.Invalidate(); CloseEquipmentWheel();
}

// Retained for existing Blueprint callers; the hold-to-open menu has been retired.
bool UBBSBFieldActionComponent::ConfirmEquipmentWheelClick() { return false; }

void UBBSBFieldActionComponent::TickEquipmentSelection()
{
 if(!CanCycleEquipmentWheel())ResetEquipmentScrollBurst();
 if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && P->Traversal->IsWireTraversalActive()) { bEquipmentTabHeld=false;bEquipmentTabCancelled=true;CloseEquipmentWheel();return; }
 EnsureBunniarEquipment();
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 if (!Character || bEquipmentInputBlocked || (Combat && Combat->bDead) || (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive()))
 { CancelEquipmentSelection(); return; }
 if(IsEquipmentActionBusy())return;
 // A full turn can finish on the already-equipped item, so there may be no
 // gameplay request left to apply even though a complete visual lap is pending.
 if(PendingEquipmentId==INDEX_NONE) { CommitQueuedWheelTravel();return; }
 if(PendingWeaponInstance.IsValid() && !EquipmentWheelInstances.Contains(PendingWeaponInstance)) { ResetQueuedWheelTravel();PendingWeaponInstance.Invalidate();PendingEquipmentId=INDEX_NONE;return; }
 const int32 Choice=PendingEquipmentId;const FGuid Previous=EquippedWeaponInstance;
 if(PendingWeaponInstance.IsValid())EquippedWeaponInstance=PendingWeaponInstance;
 const bool Applied=Choice==0 ? EquipFieldEquipment(BunniarEquipment,FName(TEXT("Bunniar"))):SelectPrototypeWeapon(EBBSBPrototypeWeapon(Choice));
 if(!Applied)EquippedWeaponInstance=Previous;
 if (Applied) { if(Choice>0)LastCombatWeaponInstance=EquippedWeaponInstance;PendingWeaponInstance.Invalidate();PendingEquipmentId=INDEX_NONE;CommitQueuedWheelTravel();if(auto* H=Character->FindComponentByClass<UBBSBHuntingComponent>())H->NotifyGrowthChanged(); UE_LOG(LogBBSB,Display,TEXT("Equipment applied id=%d"),Choice); }
}

