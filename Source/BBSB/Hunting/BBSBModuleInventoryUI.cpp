#include "Hunting/BBSBInventoryWidget.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "BBSBCharacter.h"

void UBBSBInventoryWidget::ActivateModuleCell(int32 Cell)
{
 auto* Player=Cast<ABBSBCharacter>(GetOwningPlayerPawn());auto* Growth=Player ? Player->EquipmentGrowth.Get():nullptr;
 if(!Growth || !Hunting || bHoldingCompanionPosition)return;Growth->EnsureBunniarBag();
 if(!HeldModule.IsNone())
 {
  const auto* D=Growth->ModuleDefinition(HeldModule);if(!D || Cell<0 || Cell>=Growth->BunniarBagCapacity())return;
  if(Growth->RequestedModule(D->Slot)==HeldModule && !Growth->EquipModule(D->Slot,NAME_None))return;
  if(Growth->MoveBunniarModule(HeldModule,Cell))HeldModule=NAME_None;
 }
 else if(HeldId.IsValid())
 {
  const auto* E=Hunting->FindOwnedEntry(HeldId);if(!E || !E->Item || E->Item->EquipmentKind!=EBBSBItemEquipmentKind::Companion)return;
  if(!Hunting->FindEntry(HeldId))
  {
   int32 Empty=INDEX_NONE;for(int32 I=0;I<Hunting->Columns*Hunting->Rows;++I)if(Hunting->EntryAt(I%Hunting->Columns,I/Hunting->Columns)==INDEX_NONE) { Empty=I;break; }
   if(Empty==INDEX_NONE || !Hunting->PlaceOwnedItem(HeldId,Empty)) { Growth->Feedback=TEXT("추가 장비를 넣을 가방 빈칸이 필요해요");return; }
  }
  if(Growth->MoveBunniarItem(HeldId,Cell))HeldId.Invalidate();
 }
 else
 {
  HeldModule=Growth->ModuleAtBunniarCell(Cell);if(!HeldModule.IsNone())SelectedModule=HeldModule;
  else HeldId=Growth->ItemAtBunniarCell(Cell);
 }
 UpdatePointerSurface();InvalidateLayoutAndVolatility();
}
void UBBSBInventoryWidget::ActivateModuleSlot(int32 ModuleSlotIndex)
{
 auto* Player=Cast<ABBSBCharacter>(GetOwningPlayerPawn());auto* Growth=Player ? Player->EquipmentGrowth.Get():nullptr;
 if(!Growth || HeldId.IsValid() || bHoldingCompanionPosition || ModuleSlotIndex<0 || ModuleSlotIndex>1)return;
 if(!HeldModule.IsNone())
 {
  const auto* D=Growth->ModuleDefinition(HeldModule);if(!D || int32(D->Slot)!=ModuleSlotIndex)return;
  if(Growth->EquipModule(D->Slot,HeldModule)) { SelectedModule=HeldModule;HeldModule=NAME_None; }
 }
 else { HeldModule=Growth->RequestedModule(EBBSBModuleSlot(ModuleSlotIndex));if(!HeldModule.IsNone())SelectedModule=HeldModule; }
 UpdatePointerSurface();InvalidateLayoutAndVolatility();
}
