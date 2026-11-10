#if WITH_DEV_AUTOMATION_TESTS
#include "Save/BBSBSession.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBSessionQueueTest,"BBSB.Save.RollingTenAtomicRecords",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBSessionQueueTest::RunTest(const FString& Parameters)
{
 auto* GI=NewObject<UGameInstance>();auto* Store=NewObject<UBBSBSessionSubsystem>(GI);
 Store->SaveNamespace=TEXT("BBSB_SessionStorageTest_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 ON_SCOPE_EXIT { for(int32 I=0;I<10;++I)for(const FString& Suffix:{FString(),FString(TEXT(".bak")),FString(TEXT(".tmp"))})IFileManager::Get().Delete(*(Store->SlotFile(I)+Suffix),false,true,true); };
 auto* Record=NewObject<UBBSBSessionSave>();Record->MapPackage=TEXT("/Game/BBSB/Levels/Stage01_BunniarHalfStepCaravan");
 auto* Profile=NewObject<UBBSBHuntingSave>();Profile->Version=8;UGameplayStatics::SaveGameToMemory(Profile,Record->Inventory);
 for(int32 I=1;I<=12;++I)
 {
  Record->PlayerTransform.SetLocation(FVector(I*100,12,90));Record->Reason=FString::Printf(TEXT("record %d"),I);
  if(!TestTrue(TEXT("Write a verified rolling record"),Store->WriteRecord(Record)))return false;
 }
 Store->RefreshSlots();int32 Count=0;int64 Minimum=MAX_int64,Maximum=0;int32 Latest=-1;
 for(const auto& Slot:Store->Slots)if(Slot.bValid) { ++Count;Minimum=FMath::Min(Minimum,Slot.Sequence);Maximum=FMath::Max(Maximum,Slot.Sequence);if(Slot.Sequence==12)Latest=Slot.Index; }
 TestEqual(TEXT("Exactly ten selectable saves"),Count,10);TestEqual(TEXT("Only the oldest records leave the queue"),Minimum,int64(3));TestEqual(TEXT("Newest sequence is last write"),Maximum,int64(12));
 auto* Read=Store->ReadRecord(Latest);if(!TestNotNull(TEXT("Newest record reads"),Read))return false;
 TestEqual(TEXT("The snapshot owns its position"),Read->PlayerTransform.GetLocation().X,1200.0);
 const FString Hash=Read->StateHash();Read->Sequence=999;Read->SavedAt=FDateTime::Now();Read->Reason=TEXT("metadata only");TestEqual(TEXT("Metadata does not make an unchanged game dirty"),Read->StateHash(),Hash);
 Read->Velocity.X=51;TestNotEqual(TEXT("Movement changes are detected"),Read->StateHash(),Hash);
 const FString LatestFile=Store->SlotFile(Latest);const TArray<uint8> Broken={0,1,2};TestTrue(TEXT("Create an isolated corrupt primary"),FFileHelper::SaveArrayToFile(Broken,*LatestFile));
 TestNotNull(TEXT("A valid previous version recovers from backup"),Store->ReadRecord(Latest));
 auto* Future=NewObject<UBBSBSessionSave>();Future->MapPackage=Record->MapPackage;Future->Inventory=Record->Inventory;Future->Version=2;
 TArray<uint8> FutureBytes;UGameplayStatics::SaveGameToMemory(Future,FutureBytes);const FString FutureFile=Store->SlotFile(0);FFileHelper::SaveArrayToFile(FutureBytes,*FutureFile);
 TestNull(TEXT("Unknown future versions are not loaded as old data"),Store->ReadRecord(0));
 Record->PlayerTransform.SetLocation(FVector(1500,0,90));TestTrue(TEXT("Other slots can still accept a new save"),Store->WriteRecord(Record));
 TArray<uint8> Preserved;FFileHelper::LoadFileToArray(Preserved,*FutureFile);TestTrue(TEXT("An unsupported record is not overwritten"),Preserved==FutureBytes);
 const FString SafeNamespace=Store->SaveNamespace;Store->SaveNamespace=TEXT("../invalid");TestTrue(TEXT("Path traversal cannot select a save file"),Store->SlotFile(0).IsEmpty());TestFalse(TEXT("Invalid output cannot replace any queue record"),Store->WriteRecord(Record));Store->SaveNamespace=SafeNamespace;
 return true;
}
#endif
