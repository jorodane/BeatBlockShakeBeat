#include "Hunting/BBSBHunting.h"
#include "Save/BBSBSession.h"
#include "Hunting/BBSBLootPickup.h"
#include "Hunting/BBSBLootBurst.h"
#include "Hunting/BBSBInventoryWidget.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "BBSBCharacter.h"
#include "Character/BBSBRespawn.h"
#include "Caravan/BBSBCaravan.h"
#include "Character/BBSBTraversal.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Field/BBSBFieldActions.h"
#include "Interaction/BBSBInteraction.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UBBSBHuntingComponent::UBBSBHuntingComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
 DefaultLootTable=TSoftObjectPtr<UBBSBLootTable>(FSoftObjectPath(TEXT("/Game/BBSB/Hunting/DA_DefaultLoot.DA_DefaultLoot")));
 VacuumMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/Hunting/Materials/MI_LootVacuum.MI_LootVacuum")));
 InventoryWidgetClass=UBBSBInventoryWidget::StaticClass();
}
void UBBSBHuntingComponent::BeginPlay()
{
 Super::BeginPlay();Player=Cast<ABBSBCharacter>(GetOwner());
 Columns=5;Rows=8;
 // Possession can happen after component BeginPlay; initialize on the first
 // locally controlled tick so old character Blueprints also load their profile.
}
void UBBSBHuntingComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 StopCollection();CloseInventory();
 if(bPersistProgress)SaveProgress();
 for(auto Ribbon:VacuumRibbons)if(Ribbon)Ribbon->DestroyComponent();
 VacuumRibbons.Reset();if(VacuumRoot)VacuumRoot->DestroyComponent();
 Super::EndPlay(Reason);
}
void UBBSBHuntingComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DT,Type,Function);
 if(!Player || !Player->IsLocallyControlled()) { StopCollection();CloseInventory();return; }
 if(!bHuntingInitialized)
 {
  bHuntingInitialized=true;LoadedDefaultLoot=DefaultLootTable.LoadSynchronous();
  if(Player->EquipmentGrowth)Player->EquipmentGrowth->EnsureInitialized();
  const bool Loaded=bPersistProgress && !UBBSBSessionSubsystem::Owns(GetWorld()) && LoadProgress();
  if(!Loaded && Player->EquipmentGrowth)Player->EquipmentGrowth->InitializeProfile(true);
  if(!bEquipmentInitialized)InitializeEquipment();
  for(TActorIterator<ABBSBLootPickup> It(GetWorld());It;++It)RegisterLoot(*It);
 }
 const bool Dead=Player->Combat && Player->Combat->bDead;
 if(Dead)StopCollection();
 if(bCollectionHeld && (!Player->FieldActions || (!bQuickCollection && (Player->FieldActions->InteractionEquipment()!=Player->FieldActions->BunniarEquipment ||
    !Player->FieldActions->IsGestureChannelHeld(0))) ||
    (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive())))StopCollection();
 if(bQuickSuctionPending)
 { if(FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive())StopCollection();else TickQuickCollectionIntent(); }
 if(bCollectionHeld) { if(bQuickCollection)TickQuickCollection(DT);else UpdateVacuum(DT); }
 TickEquipmentLoadout();
 TickResourcePickups();
 if(bDirtyProgress && bPersistProgress && GetWorld()->GetRealTimeSeconds()>=SaveAt)SaveProgress();
}
int32 UBBSBHuntingComponent::ExperienceToNextLevel() const
{
 return int32(FMath::Clamp<int64>(int64(BaseExperienceToLevel)+int64(FMath::Max(0,Level-1))*ExperienceIncreasePerLevel,1,MAX_int32));
}
void UBBSBHuntingComponent::AddExperience(int32 Amount)
{
 // Compatibility entry point for existing Blueprints. Combat growth now belongs to weapons.
 CollectEcstatic(Amount);
}
void UBBSBHuntingComponent::GrantEnemyRewards(ABBSBInkEnemy* Enemy)
{
 if(!Player || !IsValid(Enemy) || !Enemy->bDefeated || !Enemy->bGrantsHuntingRewards || Enemy->bHuntingRewardsGranted)return;
 Enemy->bHuntingRewardsGranted=true;
 const FVector BurstOrigin=Enemy->GetActorLocation()+FVector::UpVector*12.f;
 FActorSpawnParameters BurstParams;BurstParams.Owner=Enemy;BurstParams.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 if(auto* Burst=GetWorld()->SpawnActor<ABBSBLootBurst>(BurstOrigin,FRotator::ZeroRotator,BurstParams))Burst->StartBurst(Enemy->Tint);
 Enemy->SetActorHiddenInGame(true);
 int32 Index=0;SpawnBasicRewards(Enemy,Index);
 auto* Table=Enemy->LootTable.IsNull() ? LoadedDefaultLoot.Get():Enemy->LootTable.LoadSynchronous();
 if(!Table && Enemy->LootTable.IsNull()) { LoadedDefaultLoot=DefaultLootTable.LoadSynchronous();Table=LoadedDefaultLoot.Get(); }
 if(!Table)return;
 if(Player->EquipmentGrowth)for(auto Module:Table->ModuleRewards)Player->EquipmentGrowth->GrantModule(Module,true);
 const FVector Origin=Enemy->GetActorLocation();
 TArray<int32> Order;for(int32 I=0;I<Table->Drops.Num();++I)Order.Add(I);
 for(int32 I=Order.Num()-1;I>0;--I)Order.Swap(I,FMath::RandRange(0,I));
 int32 OrdinaryDrops=0;
 for(int32 RollIndex:Order)
 {
  const auto& Roll=Table->Drops[RollIndex];if(!Roll.Item)continue;
  const bool Ordinary=!Roll.Item->IsFieldResource() && Roll.Item->ItemCategory!=EBBSBItemCategory::Quest;
  if(Ordinary && OrdinaryDrops>=FMath::Clamp(RewardSettings()->MaximumOrdinaryDrops,0,8))continue;
  const float Chance=Roll.Chance*(Ordinary ? FMath::Clamp(RewardSettings()->OrdinaryItemChanceScale,0.f,1.f):1.f);
  if(Chance<=0 || FMath::FRand()>=FMath::Clamp(Chance,0.f,1.f))continue;
  const int32 Low=FMath::Clamp(Roll.MinimumQuantity,1,999),High=FMath::Clamp(Roll.MaximumQuantity,Low,999);
  const float Angle=Index++*2.399963f+FMath::FRandRange(-.25f,.25f);
  const float Radius=FMath::FRandRange(120.f,270.f);
  const FVector Offset(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
  FVector Ground=Origin+Offset;FHitResult Hit;
  if(GetWorld()->LineTraceSingleByObjectType(Hit,Ground+FVector::UpVector*120,Ground-FVector::UpVector*1200,
    BBSBCombatVisuals::GeometryTypes(),FCollisionQueryParams(SCENE_QUERY_STAT(BBSBLootGround),true,Enemy)))Ground=Hit.ImpactPoint;
  else Ground.Z=Origin.Z-Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  FTransform Transform(FRotator::ZeroRotator,Origin+FVector::UpVector*35);
  auto* Loot=GetWorld()->SpawnActorDeferred<ABBSBLootPickup>(ABBSBLootPickup::StaticClass(),Transform,Enemy,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(!Loot)continue;
  Loot->Item=Roll.Item;Loot->Quantity=FMath::RandRange(Low,High);
  Loot->bConcertoReward=Roll.Item->IsFieldResource() && Player->EquipmentGrowth && Player->EquipmentGrowth->bConcertoActive;
  Loot->FinishSpawning(Transform);Loot->BeginDrop(Ground+FVector::UpVector*16);if(Loot->bConcertoReward)Collect(Loot);
  if(Ordinary)++OrdinaryDrops;
 }
}
bool UBBSBHuntingComponent::CanPlace(UBBSBItemDefinition* Item,int32 X,int32 Y,bool Rotated,FGuid Ignore) const
{
 if(!Item)return false;
 const FIntPoint Size=Item->Footprint(Rotated);
 if(X<0 || Y<0 || X+Size.X>FMath::Clamp(Columns,1,24) || Y+Size.Y>FMath::Clamp(Rows,1,24))return false;
 for(const auto& Entry:Items)
 {
  if(!Entry.Item || Entry.InstanceId==Ignore)continue;
  const FIntPoint Other=Entry.Item->Footprint(Entry.bRotated);
  if(X<Entry.GridX+Other.X && X+Size.X>Entry.GridX && Y<Entry.GridY+Other.Y && Y+Size.Y>Entry.GridY)return false;
 }
 return true;
}
bool UBBSBHuntingComponent::FindSpace(UBBSBItemDefinition* Item,int32& X,int32& Y,bool& Rotated) const
{
 Rotated=false;
 for(int32 Row=0;Row<FMath::Clamp(Rows,1,24);++Row)
   for(int32 Col=0;Col<FMath::Clamp(Columns,1,24);++Col)
    if(CanPlace(Item,Col,Row,false,FGuid())) { X=Col;Y=Row;return true; }
 return false;
}
int32 UBBSBHuntingComponent::AddItem(UBBSBItemDefinition* Item,int32 Quantity)
{
 if(!Item || Item->IsBuiltInCompanion() || Quantity<=0)return 0;
 if(Item->IsFieldResource())
 {
  const int64 Amount=int64(Quantity)*FMath::Max(1,Item->ResourceValue);
  const bool Accepted=Item->RewardResource==EBBSBRewardResource::Ecstatic ? CollectEcstatic(Amount):AddNotes(Amount);
  if(Accepted)ShowPickup(Item,Quantity);return Accepted ? Quantity:0;
 }
 const int32 Limit=Item->StackLimit();int32 Remaining=Quantity;
 for(auto& Entry:Items)if(Entry.Item==Item && Entry.Quantity<Limit)
 {
  const int32 Added=FMath::Min(Remaining,Limit-Entry.Quantity);Entry.Quantity+=Added;Remaining-=Added;
  if(Remaining==0)break;
 }
 while(Remaining>0)
 {
  int32 X=0,Y=0;bool Rotated=false;if(!FindSpace(Item,X,Y,Rotated))break;
  FBBSBInventoryEntry Entry;Entry.InstanceId=FGuid::NewGuid();Entry.Item=Item;Entry.Quantity=FMath::Min(Remaining,Limit);
  Entry.GridX=X;Entry.GridY=Y;Entry.bRotated=false;Entry.WeaponLevel=Item->DefaultWeaponLevel;Entry.NormalizeWeapon();Items.Add(Entry);Remaining-=Entry.Quantity;
 }
 const int32 Added=Quantity-Remaining;
 if(Added>0) { ShowPickup(Item,Added);ProgressChanged(); }
 return Added;
}
int32 UBBSBHuntingComponent::ItemCount(UBBSBItemDefinition* Item) const
{
 int64 Count=0;for(const auto& Entry:Items)if(Entry.Item==Item)Count+=Entry.Quantity;
 return int32(FMath::Min<int64>(Count,MAX_int32));
}
int32 UBBSBHuntingComponent::EntryAt(int32 X,int32 Y) const
{
 for(int32 I=0;I<Items.Num();++I)
 {
  const auto& Entry=Items[I];if(!Entry.Item)continue;
  const FIntPoint Size=Entry.Item->Footprint(Entry.bRotated);
  if(X>=Entry.GridX && X<Entry.GridX+Size.X && Y>=Entry.GridY && Y<Entry.GridY+Size.Y)return I;
 }
 return INDEX_NONE;
}
const FBBSBInventoryEntry* UBBSBHuntingComponent::FindEntry(FGuid Id) const
{ return Items.FindByPredicate([&](const auto& Entry){ return Entry.InstanceId==Id; }); }
bool UBBSBHuntingComponent::MoveItem(FGuid Id,int32 X,int32 Y,bool Rotated)
{
 if(!FindEntry(Id) || X<0 || X>=Columns || Y<0 || Y>=Rows)return false;
 return PlaceOwnedItem(Id,Y*Columns+X);
}
bool UBBSBHuntingComponent::SortInventory()
{
 const int32 Width=FMath::Clamp(Columns,1,24);
 if(Items.Num()>Width*FMath::Clamp(Rows,1,24))return false;
 const auto Before=Items;
 Items.StableSort([](const FBBSBInventoryEntry& A,const FBBSBInventoryEntry& B)
 {
  if(!A.Item || !B.Item)return bool(A.Item) && !B.Item;
  if(A.Item->SortCategory()!=B.Item->SortCategory())return A.Item->SortCategory()<B.Item->SortCategory();
  if(A.Item->SortType()!=B.Item->SortType())return A.Item->SortType().LexicalLess(B.Item->SortType());
  if(A.Quantity!=B.Quantity)return A.Quantity>B.Quantity;
  return A.Item->ItemId.LexicalLess(B.Item->ItemId);
 });
 bool Changed=false;
 for(int32 I=0;I<Items.Num();++I)
 {
  auto& E=Items[I];E.GridX=I%Width;E.GridY=I/Width;E.bRotated=false;
  Changed|=E.InstanceId!=Before[I].InstanceId || E.GridX!=Before[I].GridX || E.GridY!=Before[I].GridY || Before[I].bRotated;
 }
 if(Changed)ProgressChanged();return true;
}
void UBBSBHuntingComponent::ProgressChanged()
{ bDirtyProgress=true;SaveAt=GetWorld() ? GetWorld()->GetRealTimeSeconds()+.65:0; }
void UBBSBHuntingComponent::ShowPickup(UBBSBItemDefinition* Item,int32 Quantity)
{
 if(!Item || Item->IsFieldResource())return;
 if((Item->bImportantForAutoSave || Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon || Item->Rarity>=EBBSBItemRarity::Rare) && UBBSBSessionSubsystem::Owns(GetWorld()))
  UBBSBSessionSubsystem::Get(GetWorld())->RequestAutoSave(TEXT("중요한 물건 획득"));
 const double Now=GetWorld()->GetTimeSeconds();
 for(auto& Notice:Notices)if(Notice.Item==Item && Now-Notice.At<1.) { Notice.Quantity+=Quantity;Notice.At=Now;return; }
 FBBSBPickupNotice Notice;Notice.Item=Item;Notice.Quantity=Quantity;Notice.At=Now;Notices.Add(Notice);
 if(Notices.Num()>5)Notices.RemoveAt(0);
}
bool UBBSBHuntingComponent::SaveProgress()
{
 if(UBBSBSessionSubsystem::Owns(GetWorld())) { bDirtyProgress=false;UBBSBSessionSubsystem::Get(GetWorld())->MarkChanged();return true; }
 if(!bPersistProgress || bSaveLoadFailed || SaveSlotName.IsEmpty())return false;
 auto* Save=CaptureProfile();
 const bool Saved=UGameplayStatics::SaveGameToSlot(Save,SaveSlotName,0);
 if(Saved)bDirtyProgress=false;else SaveAt=GetWorld()->GetRealTimeSeconds()+3.;
 return Saved;
}
UBBSBHuntingSave* UBBSBHuntingComponent::CaptureProfile() const
{
 auto* Save=Cast<UBBSBHuntingSave>(UGameplayStatics::CreateSaveGameObject(UBBSBHuntingSave::StaticClass()));
 Save->Version=8;Save->InventoryRows=Rows;Save->Level=Level;Save->Experience=Experience;Save->Items=Items;Save->EquippedItems=EquippedItems;Save->CompanionWheelSlot=CompanionWheelSlot;
 Save->Notes=Notes;Save->TotalEcstaticCollected=TotalEcstaticCollected;
 if(Player)
 {
  Save->bBunniarSelected=Player->FieldActions->EquippedEquipmentId()==0;
  Save->SelectedWeaponInstance=Player->FieldActions->EquippedWeaponInstance;Save->LastCombatWeaponInstance=Player->FieldActions->LastCombatWeaponInstance;
  if(Player->EquipmentGrowth)Player->EquipmentGrowth->StoreInSave(Save);
  if(Player->Respawn)Player->Respawn->StoreTravel(Save);
 }
 return Save;
}
bool UBBSBHuntingComponent::LoadProgress()
{
 if(SaveSlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SaveSlotName,0))return false;
 bSaveLoadFailed=true;auto* Save=Cast<UBBSBHuntingSave>(UGameplayStatics::LoadGameFromSlot(SaveSlotName,0));
 return ApplyProfile(Save);
}
bool UBBSBHuntingComponent::ApplyProfile(UBBSBHuntingSave* Save)
{
 bSaveLoadFailed=true;
 if(!Save || Save->Version<1 || Save->Version>8 || Save->Notes<0 || Save->TotalEcstaticCollected<0)return false;
 const int32 PreviousRows=Rows,OldWheel=CompanionWheelSlot;
 if(Save->Version>=5)Rows=FMath::Clamp(Save->InventoryRows,8,24);
 TArray<FBBSBInventoryEntry> Previous=MoveTemp(Items);Items.Reset();TSet<FGuid> Seen;
 auto Fail=[&]() { Items=MoveTemp(Previous);Rows=PreviousRows;CompanionWheelSlot=OldWheel;return false; };
 for(auto Entry:Save->Items)
 {
  if(Entry.Item && Entry.Item->IsBuiltInCompanion())continue;
  if(!Entry.RegisterIdentity(Seen))return Fail();Entry.bRotated=false;Entry.NormalizeWeapon();
  if(Entry.Quantity<=0 || Entry.Quantity>Entry.Item->StackLimit())return Fail();
  if(!CanPlace(Entry.Item,Entry.GridX,Entry.GridY,false,FGuid()) && !FindSpace(Entry.Item,Entry.GridX,Entry.GridY,Entry.bRotated))return Fail();
  Items.Add(Entry);
 }
 const int32 SavedCompanion=FMath::Clamp(Save->CompanionWheelSlot,0,9);CompanionWheelSlot=INDEX_NONE;
 TArray<FBBSBInventoryEntry> NewEquipment;TSet<int32> Slots;
 if(Save->Version>=2)for(auto Entry:Save->EquippedItems)
 {
  if(Entry.Item && Entry.Item->IsBuiltInCompanion())continue;
  const bool LegacyWheel=Save->Version<8 && Entry.GridY>=0 && Entry.GridY<(Save->Version<5 ? 10:6) && Entry.GridY!=SavedCompanion && Entry.Item && Entry.Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon;
  if(!Entry.Item || Entry.Quantity!=1 || !Entry.RegisterIdentity(Seen) || Slots.Contains(Entry.GridY) || (!LegacyWheel && !FitsEquipment(Entry.Item,Entry.GridY)))return Fail();
  Entry.GridX=-1;Entry.bRotated=false;Entry.NormalizeWeapon();Slots.Add(Entry.GridY);NewEquipment.Add(Entry);
 }
 // Preserve the selected legacy weapon first; excess wheel items return to the
 // bag. A legacy full bag gains only the rows needed to retain every item.
 if(Save->Version<8)
 {
  const int32 Selected=NewEquipment.IndexOfByPredicate([&](const auto& E){return E.InstanceId==Save->SelectedWeaponInstance;});if(Selected>0)NewEquipment.Swap(0,Selected);
  TSet<int32> Used;
  for(auto& E:NewEquipment)if(E.Item && E.Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon && E.GridY<10)
  {
   int32 Slot=E.GridY-(E.GridY>SavedCompanion ? 1:0);if(Slot>=5 || Used.Contains(Slot)) { Slot=INDEX_NONE;for(int32 I=0;I<5;++I)if(!Used.Contains(I)) { Slot=I;break; } }
   if(Slot!=INDEX_NONE) { E.GridY=Slot;Used.Add(Slot);continue; }
   while(!FindSpace(E.Item,E.GridX,E.GridY,E.bRotated)) { if(Rows>=24)return Fail();++Rows; }
   Items.Add(E);E.Item=nullptr;
  }
  NewEquipment.RemoveAll([](const auto& E){return !E.Item;});
 }
 EquippedItems=MoveTemp(NewEquipment);bEquipmentInitialized=Save->Version>=2;if(!bEquipmentInitialized)InitializeEquipment();bEquipmentLayoutPending=true;
 Level=FMath::Clamp(Save->Level,1,FMath::Max(2,MaximumLevel));int64 XP=FMath::Max(0,Save->Experience);
 while(Level<MaximumLevel && XP>=ExperienceToNextLevel()) { XP-=ExperienceToNextLevel();++Level; }
 Experience=Level>=MaximumLevel ? 0:int32(XP);bDirtyProgress=false;bSaveLoadFailed=false;
 Notes=Save->Notes;TotalEcstaticCollected=Save->TotalEcstaticCollected;
 ExperienceGains.Reset();RecentNotesGain=0;LastNotesGainAt=-100;
 if(Player)
 {
  if(Save->Version>=4) { Player->FieldActions->EquippedWeaponInstance=Save->SelectedWeaponInstance;Player->FieldActions->LastCombatWeaponInstance=Save->LastCombatWeaponInstance;if(Save->bBunniarSelected)Player->FieldActions->RequestEquipment(0); }
  if(Player->Respawn)Player->Respawn->LoadTravel(Save);
  if(Player->EquipmentGrowth) { Player->EquipmentGrowth->LoadFromSave(Save);Player->EquipmentGrowth->InitializeProfile(Save->Version<4); }
 }
 return true;
}
