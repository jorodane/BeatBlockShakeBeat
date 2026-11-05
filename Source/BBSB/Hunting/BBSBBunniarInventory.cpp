#include "Hunting/BBSBEquipmentGrowth.h"
#include "Hunting/BBSBHunting.h"
#include "BBSBCharacter.h"

namespace
{
 FString ModuleKey(FName Id) { return TEXT("M:")+Id.ToString(); }
 FString ItemKey(FGuid Id) { return TEXT("I:")+Id.ToString(EGuidFormats::Digits); }
}
FName UBBSBEquipmentGrowthComponent::RequestedModule(EBBSBModuleSlot Slot) const
{
 if(Slot==EBBSBModuleSlot::Aerial)return bPendingAerial ? PendingAerialModule:AerialModule;
 return bPendingDirectional ? PendingDirectionalModule:DirectionalModule;
}
void UBBSBEquipmentGrowthComponent::EnsureBunniarBag()
{
 TArray<FString> Keys;
 for(const auto& M:OwnedModules)if(M.Definition)Keys.AddUnique(ModuleKey(M.Definition->ModuleId));
 if(Player && Player->Hunting)
 {
  for(const auto& E:Player->Hunting->Items)if(E.Item && E.Item->EquipmentKind==EBBSBItemEquipmentKind::Companion)Keys.AddUnique(ItemKey(E.InstanceId));
  for(const auto& E:Player->Hunting->EquippedItems)if(E.Item && E.Item->EquipmentKind==EBBSBItemEquipmentKind::Companion)Keys.AddUnique(ItemKey(E.InstanceId));
 }
 const int32 Capacity=FMath::Max(40,FMath::DivideAndRoundUp(Keys.Num(),5)*5);TSet<int32> Used;TMap<FString,int32> Clean;
 for(const auto& Key:Keys)if(const int32* Cell=BunniarBagPositions.Find(Key);Cell && *Cell>=0 && *Cell<Capacity && !Used.Contains(*Cell)) { Clean.Add(Key,*Cell);Used.Add(*Cell); }
 for(const auto& Key:Keys)if(!Clean.Contains(Key)) { int32 Cell=0;while(Used.Contains(Cell))++Cell;Clean.Add(Key,Cell);Used.Add(Cell); }
 bool Different=Clean.Num()!=BunniarBagPositions.Num();
 if(!Different)for(const auto& Pair:Clean)if(const auto* Old=BunniarBagPositions.Find(Pair.Key);!Old || *Old!=Pair.Value) { Different=true;break; }
 if(Different) { BunniarBagPositions=MoveTemp(Clean);Changed(); }
}
int32 UBBSBEquipmentGrowthComponent::BunniarBagCapacity() const
{ return FMath::Max(40,FMath::DivideAndRoundUp(BunniarBagPositions.Num(),5)*5); }
FName UBBSBEquipmentGrowthComponent::ModuleAtBunniarCell(int32 Cell) const
{
 for(const auto& M:OwnedModules)if(M.Definition && RequestedModule(M.Definition->Slot)!=M.Definition->ModuleId)
  if(const auto* At=BunniarBagPositions.Find(ModuleKey(M.Definition->ModuleId));At && *At==Cell)return M.Definition->ModuleId;
 return NAME_None;
}
FGuid UBBSBEquipmentGrowthComponent::ItemAtBunniarCell(int32 Cell) const
{
 if(Player && Player->Hunting)for(const auto& E:Player->Hunting->Items)if(E.Item && E.Item->EquipmentKind==EBBSBItemEquipmentKind::Companion)
  if(const auto* At=BunniarBagPositions.Find(ItemKey(E.InstanceId));At && *At==Cell)return E.InstanceId;
 return FGuid();
}
bool UBBSBEquipmentGrowthComponent::MoveBunniarKey(const FString& Key,int32 Cell)
{
 EnsureBunniarBag();auto* Source=BunniarBagPositions.Find(Key);if(!Source || Cell<0 || Cell>=BunniarBagCapacity())return false;
 const int32 Before=*Source;
 for(auto& Pair:BunniarBagPositions)if(Pair.Key!=Key && Pair.Value==Cell) { Pair.Value=Before;break; }
 BunniarBagPositions[Key]=Cell;Changed();return true;
}
bool UBBSBEquipmentGrowthComponent::MoveBunniarModule(FName Id,int32 Cell)
{ return ModuleDefinition(Id) && MoveBunniarKey(ModuleKey(Id),Cell); }
bool UBBSBEquipmentGrowthComponent::MoveBunniarItem(FGuid Id,int32 Cell)
{
 const auto* E=Player && Player->Hunting ? Player->Hunting->FindOwnedEntry(Id):nullptr;
 return E && E->Item && E->Item->EquipmentKind==EBBSBItemEquipmentKind::Companion && MoveBunniarKey(ItemKey(Id),Cell);
}
bool UBBSBEquipmentGrowthComponent::SortBunniarInventory()
{
 EnsureBunniarBag();TArray<FString> Keys;BunniarBagPositions.GetKeys(Keys);
 auto Entry=[&](const FString& Key)->const FBBSBInventoryEntry* { FGuid Id;return Key.StartsWith(TEXT("I:")) && FGuid::ParseExact(Key.Mid(2),EGuidFormats::Digits,Id) && Player && Player->Hunting ? Player->Hunting->FindOwnedEntry(Id):nullptr; };
 Keys.StableSort([&](const FString& A,const FString& B)
 {
  const auto* EA=Entry(A);const auto* EB=Entry(B);
  if(bool(EA)!=bool(EB))return EA!=nullptr;
  if(EA && EB)
  {
   if(EA->Item->SortCategory()!=EB->Item->SortCategory())return EA->Item->SortCategory()<EB->Item->SortCategory();
   if(EA->Item->SortType()!=EB->Item->SortType())return EA->Item->SortType().LexicalLess(EB->Item->SortType());
   if(EA->Quantity!=EB->Quantity)return EA->Quantity>EB->Quantity;
  }
  else
  {
   const auto* MA=ModuleDefinition(FName(*A.Mid(2)));const auto* MB=ModuleDefinition(FName(*B.Mid(2)));
   if(MA && MB && MA->Slot!=MB->Slot)return MA->Slot<MB->Slot;
  }
  return A<B;
 });
 for(int32 I=0;I<Keys.Num();++I)BunniarBagPositions[Keys[I]]=I;
 Changed();return true;
}
