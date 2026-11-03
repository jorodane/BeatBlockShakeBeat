#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBLootPickup.h"
#include "Hunting/BBSBLootBurst.h"
#include "Hunting/BBSBInventoryWidget.h"
#include "BBSBCharacter.h"
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
 if(bDirtyProgress && bPersistProgress)SaveProgress();
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
  if(bPersistProgress)LoadProgress();
  if(!bEquipmentInitialized)InitializeEquipment();
  for(TActorIterator<ABBSBLootPickup> It(GetWorld());It;++It)RegisterLoot(*It);
 }
 const bool Dead=Player->Combat && Player->Combat->bDead;
 if(Dead)StopCollection();
 if(bCollectionHeld && (!Player->FieldActions || (!bQuickCollection && (Player->FieldActions->FieldEquipment!=Player->FieldActions->BunniarEquipment ||
    !Player->FieldActions->IsGestureChannelHeld(0))) ||
    (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive())))StopCollection();
 if(bCollectionHeld) { if(bQuickCollection)TickQuickCollection(DT);else UpdateVacuum(DT); }
 TickEquipmentLoadout();
 if(bDirtyProgress && bPersistProgress && GetWorld()->GetRealTimeSeconds()>=SaveAt)SaveProgress();
}
int32 UBBSBHuntingComponent::ExperienceToNextLevel() const
{
 return int32(FMath::Clamp<int64>(int64(BaseExperienceToLevel)+int64(FMath::Max(0,Level-1))*ExperienceIncreasePerLevel,1,MAX_int32));
}
void UBBSBHuntingComponent::AddExperience(int32 Amount)
{
 if(Amount<=0 || Level>=MaximumLevel)return;
 int64 Total=int64(Experience)+Amount;const int32 Before=Level;
 while(Level<MaximumLevel && Total>=ExperienceToNextLevel()) { Total-=ExperienceToNextLevel();++Level; }
 Experience=Level>=MaximumLevel ? 0:int32(FMath::Min<int64>(Total,MAX_int32));
 LastExperienceGain=Amount;LastExperienceAt=GetWorld()->GetTimeSeconds();
 if(Level>Before)LastLevelUpAt=LastExperienceAt;
 ProgressChanged();
}
void UBBSBHuntingComponent::GrantEnemyRewards(ABBSBInkEnemy* Enemy)
{
 if(!Player || !IsValid(Enemy) || !Enemy->bDefeated || !Enemy->bGrantsHuntingRewards || Enemy->bHuntingRewardsGranted)return;
 Enemy->bHuntingRewardsGranted=true;
 const FVector BurstOrigin=Enemy->GetActorLocation()+FVector::UpVector*12.f;
 FActorSpawnParameters BurstParams;BurstParams.Owner=Enemy;BurstParams.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 if(auto* Burst=GetWorld()->SpawnActor<ABBSBLootBurst>(BurstOrigin,FRotator::ZeroRotator,BurstParams))Burst->StartBurst(Enemy->Tint);
 Enemy->SetActorHiddenInGame(true);
 AddExperience(FMath::Max(0,Enemy->ExperienceReward));
 auto* Table=Enemy->LootTable.IsNull() ? LoadedDefaultLoot.Get():Enemy->LootTable.LoadSynchronous();
 if(!Table && Enemy->LootTable.IsNull()) { LoadedDefaultLoot=DefaultLootTable.LoadSynchronous();Table=LoadedDefaultLoot.Get(); }
 if(!Table)return;
 const FVector Origin=Enemy->GetActorLocation();int32 Index=0;
 for(const auto& Roll:Table->Drops)
 {
  if(!Roll.Item || Roll.Chance<=0 || FMath::FRand()>=FMath::Clamp(Roll.Chance,0.f,1.f))continue;
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
  Loot->Item=Roll.Item;Loot->Quantity=FMath::RandRange(Low,High);Loot->FinishSpawning(Transform);Loot->BeginDrop(Ground+FVector::UpVector*16);
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
  Entry.GridX=X;Entry.GridY=Y;Entry.bRotated=Rotated;Items.Add(Entry);Remaining-=Entry.Quantity;
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
 const double Now=GetWorld()->GetTimeSeconds();
 for(auto& Notice:Notices)if(Notice.Item==Item && Now-Notice.At<1.) { Notice.Quantity+=Quantity;Notice.At=Now;return; }
 FBBSBPickupNotice Notice;Notice.Item=Item;Notice.Quantity=Quantity;Notice.At=Now;Notices.Add(Notice);
 if(Notices.Num()>5)Notices.RemoveAt(0);
}
bool UBBSBHuntingComponent::SaveProgress()
{
 if(!bPersistProgress || SaveSlotName.IsEmpty())return false;
 auto* Save=Cast<UBBSBHuntingSave>(UGameplayStatics::CreateSaveGameObject(UBBSBHuntingSave::StaticClass()));
 Save->Version=3;Save->Level=Level;Save->Experience=Experience;Save->Items=Items;Save->EquippedItems=EquippedItems;Save->CompanionWheelSlot=CompanionWheelSlot;
 const bool Saved=UGameplayStatics::SaveGameToSlot(Save,SaveSlotName,0);
 if(Saved)bDirtyProgress=false;else SaveAt=GetWorld()->GetRealTimeSeconds()+3.;
 return Saved;
}
bool UBBSBHuntingComponent::LoadProgress()
{
 if(SaveSlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(SaveSlotName,0))return false;
 auto* Save=Cast<UBBSBHuntingSave>(UGameplayStatics::LoadGameFromSlot(SaveSlotName,0));
 if(!Save || Save->Version<1 || Save->Version>3)return false;
 // Validate into a temporary grid; invalid/overfull profiles leave the current
 // inventory and the on-disk save intact rather than silently dropping items.
 TArray<FBBSBInventoryEntry> Previous=MoveTemp(Items);Items.Reset();TSet<FGuid> Seen;
 for(auto Entry:Save->Items)
 {
  // The former starter item is now the implicit, permanent companion.
  if(Entry.Item && Entry.Item->IsBuiltInCompanion())continue;
  Entry.bRotated=false;
  if(!Entry.Item || Entry.Quantity<=0 || Entry.Quantity>Entry.Item->StackLimit() || !Entry.InstanceId.IsValid() || Seen.Contains(Entry.InstanceId))
  { Items=MoveTemp(Previous);return false; }
  if(!CanPlace(Entry.Item,Entry.GridX,Entry.GridY,Entry.bRotated,FGuid()))
   if(!FindSpace(Entry.Item,Entry.GridX,Entry.GridY,Entry.bRotated)) { Items=MoveTemp(Previous);return false; }
  Seen.Add(Entry.InstanceId);Items.Add(Entry);
 }
 const int32 OldWheel=CompanionWheelSlot;CompanionWheelSlot=FMath::Clamp(Save->CompanionWheelSlot,0,9);
 TArray<FBBSBInventoryEntry> NewEquipment;TSet<int32> Slots;
 if(Save->Version>=2)for(auto Entry:Save->EquippedItems)
 {
  if(Entry.Item && Entry.Item->IsBuiltInCompanion())continue;
  if(!Entry.Item || Entry.Quantity!=1 || !Entry.InstanceId.IsValid() || Seen.Contains(Entry.InstanceId) || Slots.Contains(Entry.GridY) || !FitsEquipment(Entry.Item,Entry.GridY))
  { Items=MoveTemp(Previous);CompanionWheelSlot=OldWheel;return false; }
  Entry.GridX=-1;Entry.bRotated=false;Seen.Add(Entry.InstanceId);Slots.Add(Entry.GridY);NewEquipment.Add(Entry);
 }
 EquippedItems=MoveTemp(NewEquipment);bEquipmentInitialized=Save->Version>=2;
 if(!bEquipmentInitialized)InitializeEquipment();
 bEquipmentLayoutPending=true;
 Level=FMath::Clamp(Save->Level,1,FMath::Max(2,MaximumLevel));int64 XP=FMath::Max(0,Save->Experience);
 while(Level<MaximumLevel && XP>=ExperienceToNextLevel()) { XP-=ExperienceToNextLevel();++Level; }
 Experience=Level>=MaximumLevel ? 0:int32(XP);bDirtyProgress=false;
 return true;
}
