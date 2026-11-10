#include "Save/BBSBSession.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

bool UBBSBSessionSave::IsValidRecord() const
{
 return Version==1 && Sequence>=0 && MapPackage.StartsWith(TEXT("/Game/")) && !MapPackage.Contains(TEXT("..")) &&
  !PlayerTransform.ContainsNaN() && PlayerTransform.GetLocation().GetAbsMax()<1.e8 && !ViewRotation.ContainsNaN() && !Velocity.ContainsNaN() &&
  FMath::IsFinite(Health) && Health>0 && Inventory.Num()>0 && Inventory.Num()<4*1024*1024 && Story.Num()<1024*1024 &&
  Actors.Num()<8192 && Loot.Num()<8192 && uint8(Movement)<=uint8(EBBSBSavedMovement::LedgeHang) && WireCharges>=0 && WireCharges<=2 &&
  !LadderBottom.ContainsNaN() && !LadderTop.ContainsNaN() && !SurfaceNormal.ContainsNaN();
}
FString UBBSBSessionSave::StateHash() const
{
 auto* Copy=DuplicateObject<UBBSBSessionSave>(this,GetTransientPackage());
 Copy->Sequence=0;Copy->SavedAt=FDateTime();Copy->Reason.Empty();
 TArray<uint8> Bytes;UGameplayStatics::SaveGameToMemory(Copy,Bytes);uint8 Digest[20];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);
 return FString::FromHexBlob(Digest,20);
}
FString UBBSBSessionSubsystem::SlotFile(int32 Index) const
{
 if(Index<0 || Index>=10 || SaveNamespace.IsEmpty() || SaveNamespace.Len()>80)return FString();
 for(TCHAR C:SaveNamespace)if(!FChar::IsAlnum(C) && C!=TEXT('_'))return FString();
 return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),FString::Printf(TEXT("%s_%02d.sav"),*SaveNamespace,Index+1));
}
UBBSBSessionSave* UBBSBSessionSubsystem::ReadRecord(int32 Index) const
{
 const FString File=SlotFile(Index);if(File.IsEmpty())return nullptr;
 for(const FString& Candidate:{File,File+TEXT(".bak")})
 {
  const int64 Size=IFileManager::Get().FileSize(*Candidate);if(Size<=0 || Size>32*1024*1024)continue;
  TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*Candidate))continue;
  uint32 Magic=0;if(Bytes.Num()<64)continue;FMemory::Memcpy(&Magic,Bytes.GetData(),sizeof(Magic));if(Magic!=0x53415647)continue;
  auto* Record=Cast<UBBSBSessionSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
  if(Record && Record->Version>1)return nullptr;
  if(Record && Record->IsValidRecord())return Record;
 }
 return nullptr;
}
void UBBSBSessionSubsystem::RefreshSlots()
{
 Slots.Reset();for(int32 I=0;I<10;++I)
 {
  FBBSBSessionSlot Info;Info.Index=I;const FString File=SlotFile(I);
  Info.bOccupied=!File.IsEmpty() && (IFileManager::Get().FileExists(*File) || IFileManager::Get().FileExists(*(File+TEXT(".bak"))));
  if(auto* Record=ReadRecord(I))
  {
   Info.bValid=true;Info.Sequence=Record->Sequence;Info.Title=Record->Reason;
   static const TCHAR* Modes[]={TEXT("지상"),TEXT("공중"),TEXT("활강"),TEXT("사다리"),TEXT("벽잡기"),TEXT("모서리 잡기")};
   const FString Region=Record->MapPackage.Contains(TEXT("Stage01_")) ? TEXT("버니아 반 발자국 상단"):TEXT("여행 중");
   Info.Details=FString::Printf(TEXT("%s  ·  %s\n%s"),*Record->SavedAt.ToString(TEXT("%m/%d %H:%M")),Modes[uint8(Record->Movement)],*Region);
  }
  else { Info.Title=Info.bOccupied ? TEXT("읽을 수 없는 기록"):TEXT("비어 있음");Info.Details=TEXT(""); }
  Slots.Add(Info);
 }
 Slots.Sort([](const auto& A,const auto& B){if(A.bValid!=B.bValid)return A.bValid;return A.Sequence==B.Sequence ? A.Index<B.Index:A.Sequence>B.Sequence;});
}
bool UBBSBSessionSubsystem::WriteRecord(UBBSBSessionSave* Record)
{
 if(!Record || !Record->IsValidRecord())return false;
 RefreshSlots();int32 Index=INDEX_NONE;int64 Sequence=0,Oldest=MAX_int64;
 for(const auto& Slot:Slots)
 {
  Sequence=FMath::Max(Sequence,Slot.Sequence);
  if(!Slot.bOccupied && Index==INDEX_NONE)Index=Slot.Index;
 }
 if(Index==INDEX_NONE)for(const auto& Slot:Slots)if(Slot.bValid && Slot.Sequence<Oldest) { Oldest=Slot.Sequence;Index=Slot.Index; }
 if(Index==INDEX_NONE || Sequence==MAX_int64)return false;
 const FString File=SlotFile(Index),Temp=File+TEXT(".tmp"),Backup=File+TEXT(".bak");if(File.IsEmpty())return false;
 Record->Sequence=Sequence+1;Record->SavedAt=FDateTime::Now();
 TArray<uint8> Bytes;if(!UGameplayStatics::SaveGameToMemory(Record,Bytes))return false;
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
 if(!FFileHelper::SaveArrayToFile(Bytes,*Temp))return false;
 // Verify the staged bytes before replacing a record. Keep the previous valid
 // file as recovery data; no queue entry is removed before a successful write.
 TArray<uint8> Verified;if(!FFileHelper::LoadFileToArray(Verified,*Temp))return false;
 auto* Check=Cast<UBBSBSessionSave>(UGameplayStatics::LoadGameFromMemory(Verified));if(!Check || !Check->IsValidRecord())return false;
 if(IFileManager::Get().FileExists(*File) && IFileManager::Get().Copy(*Backup,*File,true,true)!=COPY_OK)return false;
 if(!IFileManager::Get().Move(*File,*Temp,true,true,false,true))return false;
 RefreshSlots();return true;
}
