#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"
#include "Hunting/BBSBHunting.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBRewardSaveMigrationTest,"BBSB.Rewards.LegacyAndWalletMigration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBRewardSaveMigrationTest::RunTest(const FString&)
{
 auto* Sword=LoadObject<UBBSBItemDefinition>(nullptr,TEXT("/Game/BBSB/Hunting/Equipment/DA_Equipment_Sword.DA_Equipment_Sword"));
 auto* Orb=LoadObject<UBBSBItemDefinition>(nullptr,TEXT("/Game/BBSB/Growth/Orbs/DA_Orb_Power.DA_Orb_Power"));
 if(!TestNotNull(TEXT("Existing sword definition"),Sword) || !TestNotNull(TEXT("Existing orb definition"),Orb))return false;
 const FString Slot=TEXT("BBSB_RewardMigration_QA_")+FGuid::NewGuid().ToString();
 if(!TestFalse(TEXT("Never overwrite an existing profile"),UGameplayStatics::DoesSaveGameExist(Slot,0)))return false;
 ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot,0); };
 TStrongObjectPtr<UBBSBHuntingSave> Old(NewObject<UBBSBHuntingSave>());Old->Version=6;
 FBBSBInventoryEntry E;E.Item=Sword;E.InstanceId=FGuid::NewGuid();E.WeaponLevel=7;E.SocketCount=2;
 FBBSBOrbSocket O;O.Item=Orb;O.InstanceId=FGuid::NewGuid();E.Orbs.Add(O);E.Orbs.Add(FBBSBOrbSocket());Old->Items.Add(E);
 TestTrue(TEXT("Seed an isolated version-six profile"),UGameplayStatics::SaveGameToSlot(Old.Get(),Slot,0));
 TStrongObjectPtr<UBBSBHuntingComponent> H(NewObject<UBBSBHuntingComponent>());H->SaveSlotName=Slot;H->bPersistProgress=false;
 if(!TestTrue(TEXT("Load a legacy profile through the real migration"),H->LoadProgress()))return false;
 const auto Legacy=H->WeaponInstance(E.InstanceId);
 TestEqual(TEXT("Legacy level is retained"),Legacy.WeaponLevel,7);
 TestEqual(TEXT("Legacy starting experience is inferred"),Legacy.WeaponEcstatic,Sword->WeaponExperienceForLevel(7));
 TestEqual(TEXT("Legacy starting level creates no refundable investment"),Legacy.InvestedEcstatic,int64(0));
 TestEqual(TEXT("Legacy socket identity is retained"),Legacy.Orbs[0].InstanceId,O.InstanceId);
 TestEqual(TEXT("Legacy wallet starts empty"),H->Notes,int64(0));
 TestTrue(TEXT("Notes enter the wallet"),H->AddNotes(1234));
 H->TotalEcstaticCollected=4567;
 auto* Weapon=H->MutableOwnedEntry(E.InstanceId);Weapon->WeaponEcstatic+=55;Weapon->InvestedEcstatic=55;Weapon->NormalizeWeapon();
 H->bPersistProgress=true;const bool Saved=H->SaveProgress();H->bPersistProgress=false;
 if(!TestTrue(TEXT("Save the migrated schema"),Saved))return false;
 TStrongObjectPtr<UBBSBHuntingSave> New(Cast<UBBSBHuntingSave>(UGameplayStatics::LoadGameFromSlot(Slot,0)));
 if(!TestNotNull(TEXT("Deserialize the migrated SaveGame"),New.Get()))return false;
 TestEqual(TEXT("Version eight is written explicitly"),New->Version,8);
 TestEqual(TEXT("Wallet is serialized exactly"),New->Notes,int64(1234));
 TestEqual(TEXT("Collected total is serialized exactly"),New->TotalEcstaticCollected,int64(4567));
 TStrongObjectPtr<UBBSBHuntingComponent> Fresh(NewObject<UBBSBHuntingComponent>());Fresh->SaveSlotName=Slot;Fresh->bPersistProgress=false;
 TestTrue(TEXT("Reload version seven"),Fresh->LoadProgress());
 const auto RoundTrip=Fresh->WeaponInstance(E.InstanceId);
 TestEqual(TEXT("Investment survives round trip"),RoundTrip.InvestedEcstatic,int64(55));
 TestEqual(TEXT("Experience survives round trip"),RoundTrip.WeaponEcstatic,Legacy.WeaponEcstatic+55);
 TestEqual(TEXT("Socket survives round trip"),RoundTrip.Orbs[0].InstanceId,O.InstanceId);
 TestFalse(TEXT("Negative spending is rejected"),Fresh->SpendNotes(-1));
 TestFalse(TEXT("Overdraft is rejected"),Fresh->SpendNotes(1235));
 TestTrue(TEXT("Valid spending is exact"),Fresh->SpendNotes(34));TestEqual(TEXT("Wallet balance after spending"),Fresh->Notes,int64(1200));
 New->Notes=-1;TestTrue(TEXT("Seed a malformed QA wallet"),UGameplayStatics::SaveGameToSlot(New.Get(),Slot,0));
 TestFalse(TEXT("Malformed wallet refuses to load"),Fresh->LoadProgress());
 TestEqual(TEXT("Refused load retains the live wallet"),Fresh->Notes,int64(1200));
 TestEqual(TEXT("Refused load retains the live investment"),Fresh->WeaponInstance(E.InstanceId).InvestedEcstatic,int64(55));
 Fresh->bPersistProgress=true;TestFalse(TEXT("A refused load cannot overwrite the profile"),Fresh->SaveProgress());Fresh->bPersistProgress=false;
 return true;
}
#endif
