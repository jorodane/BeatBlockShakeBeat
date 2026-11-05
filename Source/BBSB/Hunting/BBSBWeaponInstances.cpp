#include "Hunting/BBSBHunting.h"
#include "BBSBCharacter.h"
#include "Field/BBSBFieldActions.h"

bool UBBSBHuntingComponent::EquipOwnedWeapon(FGuid Id)
{
 const auto* E=FindOwnedEntry(Id);if(!Player || !E || !E->Item || E->Item->EquipmentKind!=EBBSBItemEquipmentKind::Weapon)return false;
 if(FindEntry(Id))
 {
  int32 Slot=INDEX_NONE;for(int32 I=0;I<6;++I)if(I!=CompanionWheelSlot && !EquipmentEntryAt(I)) { Slot=I;break; }
  if(Slot==INDEX_NONE || !PlaceOwnedItem(Id,Columns*Rows+Slot))return false;
 }
 TickEquipmentLoadout();return Player->FieldActions->RequestWeaponInstance(Id);
}

FBBSBInventoryEntry* UBBSBHuntingComponent::MutableOwnedEntry(FGuid Id)
{
 if(auto* Entry=Items.FindByPredicate([&](const auto& E){return E.InstanceId==Id;}))return Entry;
 return EquippedItems.FindByPredicate([&](const auto& E){return E.InstanceId==Id;});
}
FBBSBInventoryEntry UBBSBHuntingComponent::WeaponInstance(FGuid Id) const
{
 const auto* E=FindOwnedEntry(Id);return E && E->Item && E->Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon ? *E:FBBSBInventoryEntry();
}
bool UBBSBHuntingComponent::ConsumeItem(UBBSBItemDefinition* Item,int32 Quantity)
{
 if(!Item || Quantity<0 || ItemCount(Item)<Quantity || (Item->EquipmentKind!=EBBSBItemEquipmentKind::None && Item->EquipmentKind!=EBBSBItemEquipmentKind::Orb))return false;
 int32 Left=Quantity;
 for(int32 I=Items.Num()-1;I>=0 && Left>0;--I)if(Items[I].Item==Item)
 { const int32 Used=FMath::Min(Left,Items[I].Quantity);Left-=Used;Items[I].Quantity-=Used;if(Items[I].Quantity==0)Items.RemoveAt(I); }
 if(Quantity>0)ProgressChanged();return true;
}
bool UBBSBHuntingComponent::AddWeaponEntry(const FBBSBInventoryEntry& Source)
{
 if(!Source.Item || Source.Item->EquipmentKind!=EBBSBItemEquipmentKind::Weapon || Source.Quantity!=1 || !Source.InstanceId.IsValid() || FindOwnedEntry(Source.InstanceId))return false;
 TSet<FGuid> Seen;for(const auto& E:Items)E.RegisterIdentity(Seen);for(const auto& E:EquippedItems)E.RegisterIdentity(Seen);if(!Source.RegisterIdentity(Seen))return false;
 FBBSBInventoryEntry Entry=Source;bool Rotated=false;
 if(!FindSpace(Entry.Item,Entry.GridX,Entry.GridY,Rotated))return false;
 Entry.bRotated=false;Entry.NormalizeWeapon();Items.Add(Entry);ShowPickup(Entry.Item,1);ProgressChanged();return true;
}
FGuid UBBSBHuntingComponent::AddWeaponInstance(UBBSBItemDefinition* Item,int32 WeaponLevel,int32 SocketCount)
{
 FBBSBInventoryEntry Entry;Entry.Item=Item;Entry.InstanceId=FGuid::NewGuid();Entry.Quantity=1;Entry.WeaponLevel=FMath::Max(1,WeaponLevel);Entry.SocketCount=FMath::Max(0,SocketCount);
 return AddWeaponEntry(Entry) ? Entry.InstanceId:FGuid();
}

namespace
{
bool ReturnOrb(UBBSBHuntingComponent* H,TArray<FBBSBInventoryEntry>& Bag,const FBBSBOrbSocket& Orb)
{
 if(!Orb.Item)return true;
 for(auto& E:Bag)if(E.Item==Orb.Item && E.Quantity<Orb.Item->StackLimit()) { ++E.Quantity;return true; }
 for(int32 Y=0;Y<H->Rows;++Y)for(int32 X=0;X<H->Columns;++X)if(H->CanPlace(Orb.Item,X,Y,false,FGuid()))
 { FBBSBInventoryEntry E;E.InstanceId=Orb.InstanceId;E.Item=Orb.Item;E.Quantity=1;E.GridX=X;E.GridY=Y;E.NormalizeWeapon();Bag.Add(E);return true; }
 return false;
}
}
bool UBBSBHuntingComponent::SocketOrb(FGuid WeaponId,int32 Socket,FGuid OrbId)
{
 const auto* W=FindOwnedEntry(WeaponId);const auto* O=FindEntry(OrbId);
 if(!W || !W->Item || W->Item->EquipmentKind!=EBBSBItemEquipmentKind::Weapon || Socket<0 || Socket>=W->SocketCount || !O || !O->Item || O->Item->EquipmentKind!=EBBSBItemEquipmentKind::Orb)return false;
 const auto BagBefore=Items,GearBefore=EquippedItems;const FBBSBInventoryEntry Incoming=*O;const FBBSBOrbSocket Previous=W->Orbs.IsValidIndex(Socket) ? W->Orbs[Socket]:FBBSBOrbSocket();
 const int32 SourceIndex=Items.IndexOfByPredicate([&](const auto& E){return E.InstanceId==OrbId;});
 if(--Items[SourceIndex].Quantity==0)Items.RemoveAt(SourceIndex);
 auto* Weapon=MutableOwnedEntry(WeaponId);Weapon->NormalizeWeapon();Weapon->Orbs[Socket].Item=Incoming.Item;Weapon->Orbs[Socket].InstanceId=Incoming.Quantity==1 ? Incoming.InstanceId:FGuid::NewGuid();
 if(!ReturnOrb(this,Items,Previous)) { Items=BagBefore;EquippedItems=GearBefore;return false; }
 ProgressChanged();return true;
}
bool UBBSBHuntingComponent::RemoveOrb(FGuid WeaponId,int32 Socket)
{
 auto* W=MutableOwnedEntry(WeaponId);if(!W || !W->Item || W->Item->EquipmentKind!=EBBSBItemEquipmentKind::Weapon || !W->Orbs.IsValidIndex(Socket) || !W->Orbs[Socket].Item)return false;
 const auto BagBefore=Items,GearBefore=EquippedItems;const auto Orb=W->Orbs[Socket];W->Orbs[Socket]=FBBSBOrbSocket();
 if(!ReturnOrb(this,Items,Orb)) { Items=BagBefore;EquippedItems=GearBefore;return false; }
 ProgressChanged();return true;
}
