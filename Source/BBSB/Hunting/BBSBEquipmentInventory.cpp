#include "Hunting/BBSBHunting.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Character/BBSBTraversal.h"
#include "Combat/BBSBCombat.h"
#include "Field/BBSBFieldActions.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"

const FBBSBInventoryEntry* UBBSBHuntingComponent::EquipmentEntryAt(int32 Slot) const
{ return EquippedItems.FindByPredicate([&](const auto& E){return E.GridY==Slot;}); }
UBBSBItemDefinition* UBBSBHuntingComponent::EquippedItemAt(int32 Slot) const
{ const auto* E=EquipmentEntryAt(Slot);return E ? E->Item.Get():nullptr; }
const FBBSBInventoryEntry* UBBSBHuntingComponent::FindOwnedEntry(FGuid Id) const
{ if(const auto* E=FindEntry(Id))return E;return EquippedItems.FindByPredicate([&](const auto& E){return E.InstanceId==Id;}); }
bool UBBSBHuntingComponent::FitsEquipment(UBBSBItemDefinition* Item,int32 Slot) const
{
 if(!Item || Item->IsBuiltInCompanion())return false;
 if(Slot>=0 && Slot<6)return Slot!=CompanionWheelSlot && Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon && Item->WeaponId>=1 && Item->WeaponId<=6;
 return (Slot==10 && Item->EquipmentKind==EBBSBItemEquipmentKind::Companion) || (Slot==11 && Item->EquipmentKind==EBBSBItemEquipmentKind::Armor) || (Slot==12 && Item->EquipmentKind==EBBSBItemEquipmentKind::Accessory);
}
void UBBSBHuntingComponent::InitializeEquipment()
{
 bEquipmentInitialized=true;CompanionWheelSlot=0;
 for(const auto& Pair:{TPair<const TCHAR*,int32>(TEXT("/Game/BBSB/Hunting/Equipment/DA_Equipment_Sword.DA_Equipment_Sword"),1)})
  if(auto* Item=LoadObject<UBBSBItemDefinition>(nullptr,Pair.Key))
  { FBBSBInventoryEntry E;E.InstanceId=FGuid::NewGuid();E.Item=Item;E.Quantity=1;E.GridX=-1;E.GridY=Pair.Value;E.WeaponLevel=Item->DefaultWeaponLevel;E.NormalizeWeapon();EquippedItems.Add(E); }
 bEquipmentLayoutPending=true;
}
bool UBBSBHuntingComponent::CanPlaceOwnedItem(FGuid Id,int32 Destination) const
{
 const auto* Source=FindOwnedEntry(Id);const int32 BagCount=Columns*Rows;
 if(!Source || !Source->Item || Source->Item->IsBuiltInCompanion() || Destination<0 || Destination>=BagCount+13)return false;
 const bool InBag=FindEntry(Id)!=nullptr;
 if(Destination<BagCount)
 {
  const int32 OtherIndex=EntryAt(Destination%Columns,Destination/Columns);
  if(!Items.IsValidIndex(OtherIndex) || Items[OtherIndex].InstanceId==Id)return true;
  const auto& Other=Items[OtherIndex];
  if(InBag)return Other.Item!=Source->Item || Other.Quantity<Other.Item->StackLimit();
  return FitsEquipment(Other.Item,Source->GridY);
 }
 const int32 Slot=Destination-BagCount;if(!FitsEquipment(Source->Item,Slot) || Source->Quantity!=1)return false;
 const auto* Other=EquipmentEntryAt(Slot);
 return !Other || Other->InstanceId==Id || InBag || FitsEquipment(Other->Item,Source->GridY);
}
bool UBBSBHuntingComponent::PlaceOwnedItem(FGuid Id,int32 Destination)
{
 if(!CanPlaceOwnedItem(Id,Destination))return false;
 const int32 BagCount=Columns*Rows,SourceBag=Items.IndexOfByPredicate([&](const auto& E){return E.InstanceId==Id;});
 const int32 SourceEquipment=EquippedItems.IndexOfByPredicate([&](const auto& E){return E.InstanceId==Id;});
 FBBSBInventoryEntry Moving=SourceBag!=INDEX_NONE ? Items[SourceBag]:EquippedItems[SourceEquipment];
 if(Destination<BagCount)
 {
  const int32 X=Destination%Columns,Y=Destination/Columns,Other=EntryAt(X,Y);
  if(SourceBag!=INDEX_NONE && SourceBag==Other)return true;
  if(SourceBag!=INDEX_NONE)
  {
   if(Other!=INDEX_NONE && Items[Other].Item==Moving.Item)
   {
    const int32 Amount=FMath::Min(Moving.Quantity,Items[Other].Item->StackLimit()-Items[Other].Quantity);
    Items[Other].Quantity+=Amount;Items[SourceBag].Quantity-=Amount;if(Items[SourceBag].Quantity==0)Items.RemoveAt(SourceBag);
   }
   else
   {
    if(Other!=INDEX_NONE) { Items[Other].GridX=Moving.GridX;Items[Other].GridY=Moving.GridY; }
    Items[SourceBag].GridX=X;Items[SourceBag].GridY=Y;Items[SourceBag].bRotated=false;
   }
  }
  else
  {
   if(Other!=INDEX_NONE)
   { auto Replacement=Items[Other];Replacement.GridX=-1;Replacement.GridY=Moving.GridY;EquippedItems[SourceEquipment]=Replacement; }
   else EquippedItems.RemoveAt(SourceEquipment);
   Moving.GridX=X;Moving.GridY=Y;Moving.bRotated=false;
   if(Other!=INDEX_NONE)Items[Other]=Moving;else Items.Add(Moving);
   bEquipmentLayoutPending=true;
  }
 }
 else
 {
  const int32 Slot=Destination-BagCount,Other=EquippedItems.IndexOfByPredicate([&](const auto& E){return E.GridY==Slot;});
  if(SourceEquipment!=INDEX_NONE && SourceEquipment==Other)return true;
  const bool Occupied=Other!=INDEX_NONE;FBBSBInventoryEntry Previous;if(Occupied)Previous=EquippedItems[Other];
  if(SourceBag!=INDEX_NONE)
  {
   if(Occupied) { Previous.GridX=Moving.GridX;Previous.GridY=Moving.GridY;Previous.bRotated=false;Items[SourceBag]=Previous; }
   else Items.RemoveAt(SourceBag);
  }
  else
  {
   if(Occupied) { Previous.GridX=-1;Previous.GridY=Moving.GridY;EquippedItems[SourceEquipment]=Previous; }
   else EquippedItems.RemoveAt(SourceEquipment);
  }
  Moving.GridX=-1;Moving.GridY=Slot;Moving.bRotated=false;
  // The target index may have shifted after removing the source.
  const int32 Target=EquippedItems.IndexOfByPredicate([&](const auto& E){return E.GridY==Slot;});
  if(Target!=INDEX_NONE)EquippedItems[Target]=Moving;else EquippedItems.Add(Moving);
  bEquipmentLayoutPending=true;
 }
 ProgressChanged();return true;
}
bool UBBSBHuntingComponent::MoveCompanionWheelSlot(int32 Destination)
{
 if(Destination<0 || Destination>=6)return false;
 if(Destination==CompanionWheelSlot)return true;
 for(auto& E:EquippedItems)if(E.GridY==Destination)E.GridY=CompanionWheelSlot;
 CompanionWheelSlot=Destination;bEquipmentLayoutPending=true;ProgressChanged();return true;
}
void UBBSBHuntingComponent::TickEquipmentLoadout()
{
 if(!bEquipmentLayoutPending || !Player || !Player->FieldActions || !bEquipmentInitialized || Player->Combat->bDead)return;
 auto* A=Player->FieldActions.Get();if(FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive())return;
 auto* Buddy=Cast<ABBSBBunniarCompanion>(A->BunniarEquipment);if(!Buddy)return;
 A->bInventoryLoadoutManaged=true;A->AvailableWeapons.Reset();A->EquipmentWheelIds.Init(INDEX_NONE,6);A->EquipmentWheelInstances.Init(FGuid(),6);
 for(const auto& E:EquippedItems)if(E.Item && E.Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon && E.GridY>=0 && E.GridY<6)
 { A->AvailableWeapons.AddUnique(EBBSBPrototypeWeapon(E.Item->WeaponId));A->EquipmentWheelIds[E.GridY]=E.Item->WeaponId;A->EquipmentWheelInstances[E.GridY]=E.InstanceId; }
 A->EquipmentWheelIds[CompanionWheelSlot]=0;
 if(A->PendingWeaponInstance.IsValid() && !A->EquipmentWheelInstances.Contains(A->PendingWeaponInstance)) { A->PendingWeaponInstance.Invalidate();A->PendingEquipmentId=INDEX_NONE; }
 if(A->IsEquipmentActionBusy() || Player->Traversal->IsBunniarTraversalActive())return;
 auto* Companion=EquippedItemAt(10);A->bBunniarAvailable=true;
 if(!OriginalCompanionMesh)OriginalCompanionMesh=Buddy->Visual->GetSkeletalMeshAsset();
 auto* Mesh=Companion && !Companion->CompanionMesh.IsNull() ? Companion->CompanionMesh.LoadSynchronous():OriginalCompanionMesh.Get();
 if(Mesh && Buddy->Visual->GetSkeletalMeshAsset()!=Mesh)Buddy->Visual->SetSkeletalMesh(Mesh);
 Buddy->bFollowing=true;Buddy->SetActorHiddenInGame(false);
 if(A->PendingEquipmentId==INDEX_NONE && A->EquippedEquipmentId()!=0)
 {
  if(A->EquippedWeaponInstance.IsValid() && A->EquipmentWheelInstances.Contains(A->EquippedWeaponInstance))A->RequestWeaponInstance(A->EquippedWeaponInstance);
  else
  {
   bool Chosen=false;for(int32 I=0;I<6;++I)if(A->EquipmentWheelIds[I]>0 && A->EquipmentWheelIds[I]==A->EquippedEquipmentId()) { A->RequestEquipmentSlot(I);Chosen=true;break; }
   if(!Chosen)A->RequestEquipment(0);
  }
 }
 bEquipmentLayoutPending=false;
}
