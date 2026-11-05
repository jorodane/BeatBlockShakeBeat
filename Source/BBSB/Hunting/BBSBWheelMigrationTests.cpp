#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"
#include "Hunting/BBSBHunting.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBSixSlotMigrationTest,"BBSB.Inventory.SixSlotMigration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBSixSlotMigrationTest::RunTest(const FString&)
{
 auto* Sword=LoadObject<UBBSBItemDefinition>(nullptr,TEXT("/Game/BBSB/Hunting/Equipment/DA_Equipment_Sword.DA_Equipment_Sword"));
 auto* Ink=LoadObject<UBBSBItemDefinition>(nullptr,TEXT("/Game/BBSB/Hunting/Items/DA_Item_InkDrop.DA_Item_InkDrop"));
 auto* Orb=LoadObject<UBBSBItemDefinition>(nullptr,TEXT("/Game/BBSB/Growth/Orbs/DA_Orb_Power.DA_Orb_Power"));
 if(!TestNotNull(TEXT("Existing sword asset"),Sword) || !TestNotNull(TEXT("Material asset"),Ink) || !TestNotNull(TEXT("Orb asset"),Orb))return false;
 const FString Slot=TEXT("BBSB_SixSlotMigration_QA_")+FGuid::NewGuid().ToString();
 if(!TestFalse(TEXT("Test never overwrites any existing save"),UGameplayStatics::DoesSaveGameExist(Slot,0)))return false;
 ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot,0); };
 TStrongObjectPtr<UBBSBHuntingSave> Old(NewObject<UBBSBHuntingSave>());Old->Version=4;Old->CompanionWheelSlot=8;
 for(int32 I=0;I<40;++I) { FBBSBInventoryEntry E;E.Item=Ink;E.InstanceId=FGuid::NewGuid();E.GridX=I%5;E.GridY=I/5;Old->Items.Add(E); }
 TSet<FGuid> WeaponIds;FGuid SocketOrb;
 for(int32 I=0;I<10;++I)if(I!=8)
 {
  FBBSBInventoryEntry E;E.Item=Sword;E.InstanceId=FGuid::NewGuid();E.GridX=-1;E.GridY=I;E.WeaponLevel=I+1;E.SocketCount=3;E.NormalizeWeapon();
  if(I==9) { E.Orbs[0].Item=Orb;SocketOrb=E.Orbs[0].InstanceId=FGuid::NewGuid();Old->SelectedWeaponInstance=E.InstanceId; }
  WeaponIds.Add(E.InstanceId);Old->EquippedItems.Add(E);
 }
 TestTrue(TEXT("Seed the isolated ten-sector legacy profile"),UGameplayStatics::SaveGameToSlot(Old.Get(),Slot,0));
 TStrongObjectPtr<UBBSBHuntingComponent> H(NewObject<UBBSBHuntingComponent>());H->SaveSlotName=Slot;H->bPersistProgress=false;
 if(!TestTrue(TEXT("Load and migrate a full legacy bag and wheel"),H->LoadProgress()))return false;
 TestEqual(TEXT("Only five weapons plus permanent Bunniar remain in wheel"),H->EquippedItems.Num(),5);
 TestEqual(TEXT("Four excess weapons return to bag"),H->Items.Num(),44);
 TestEqual(TEXT("A full legacy bag expands by only one row to preserve items"),H->Rows,9);
 TestEqual(TEXT("An out-of-range companion position moves into six sectors"),H->CompanionWheelSlot,0);
 TestTrue(TEXT("The chosen legacy weapon stays equipped"),H->EquippedItems.ContainsByPredicate([&](const auto& E){return E.InstanceId==Old->SelectedWeaponInstance;}));
 TSet<FGuid> Restored;
 for(const auto* List:{&H->Items,&H->EquippedItems})for(const auto& E:*List)if(E.Item==Sword)Restored.Add(E.InstanceId);
 TestEqual(TEXT("All original weapon identities survive migration"),Restored.Num(),WeaponIds.Num());for(const auto& Id:WeaponIds)TestTrue(TEXT("Original weapon is retained"),Restored.Contains(Id));
 for(const auto& E:H->EquippedItems)TestTrue(TEXT("Every active weapon occupies a legal sector"),E.GridY>=0 && E.GridY<6 && E.GridY!=H->CompanionWheelSlot);
 const auto* Chosen=H->FindOwnedEntry(Old->SelectedWeaponInstance);
 TestTrue(TEXT("Level and socket identity are lossless"),Chosen && Chosen->WeaponLevel==10 && Chosen->Orbs[0].InstanceId==SocketOrb && Chosen->Orbs[0].Item==Orb);
 H->bPersistProgress=true;TestTrue(TEXT("Save the migrated version-five profile"),H->SaveProgress());H->bPersistProgress=false;
 TStrongObjectPtr<UBBSBHuntingComponent> Fresh(NewObject<UBBSBHuntingComponent>());Fresh->SaveSlotName=Slot;Fresh->bPersistProgress=false;
 TestTrue(TEXT("Reload the migrated six-sector profile"),Fresh->LoadProgress());TestEqual(TEXT("Migration rows are persisted"),Fresh->Rows,9);TestEqual(TEXT("No inventory loss after round trip"),Fresh->Items.Num(),44);TestEqual(TEXT("No wheel loss after round trip"),Fresh->EquippedItems.Num(),5);
 return true;
}
#endif
