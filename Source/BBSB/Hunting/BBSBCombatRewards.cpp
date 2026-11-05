#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Hunting/BBSBLootPickup.h"
#include "BBSBCharacter.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Field/BBSBFieldActions.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

const UBBSBGrowthSettings* UBBSBHuntingComponent::RewardSettings() const
{
 if(Player && Player->EquipmentGrowth)
 {
  const auto* G=Player->EquipmentGrowth.Get();
  if(G->Settings)return G->Settings;
  if(auto* Loaded=G->SettingsAsset.LoadSynchronous())return Loaded;
 }
 return GetDefault<UBBSBGrowthSettings>();
}
UBBSBItemDefinition* UBBSBHuntingComponent::ResourceDefinition(EBBSBRewardResource Resource) const
{
 const auto* Settings=RewardSettings();UBBSBItemDefinition* Item=nullptr;
 if(Resource==EBBSBRewardResource::Ecstatic)Item=Settings->EcstaticPickup.LoadSynchronous();
 if(Resource==EBBSBRewardResource::Note)Item=Settings->NotePickup.LoadSynchronous();
 if(Resource==EBBSBRewardResource::Crystal)Item=Settings->EcstaticCrystal.LoadSynchronous();
 return Item && Item->RewardResource==Resource && Item->ResourceValue>0 ? Item:nullptr;
}
UBBSBItemDefinition* UBBSBHuntingComponent::EcstaticCrystalDefinition() const
{ return ResourceDefinition(EBBSBRewardResource::Crystal); }
FGuid UBBSBHuntingComponent::RewardWeaponInstance() const
{
 if(!Player || !Player->FieldActions)return FGuid();
 const auto* A=Player->FieldActions.Get();
 auto IsWeapon=[&](FGuid Id) { const auto* E=FindOwnedEntry(Id);return E && E->Item && E->Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon; };
 // Use applied equipment, never a wheel selection that is still queued behind an action.
 if(A->EquippedEquipmentId()!=0 && IsWeapon(A->EquippedWeaponInstance))return A->EquippedWeaponInstance;
 if(IsWeapon(A->LastCombatWeaponInstance))return A->LastCombatWeaponInstance;
 if(IsWeapon(A->EquippedWeaponInstance))return A->EquippedWeaponInstance;
 for(const auto& E:EquippedItems)if(E.GridY>=0 && E.GridY<6 && IsWeapon(E.InstanceId))return E.InstanceId;
 return FGuid();
}
bool UBBSBHuntingComponent::IsEcstasyWeapon(FGuid Id) const
{
 const auto* E=FindOwnedEntry(Id);
 return E && E->Item && E->Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon && E->WeaponLevel>=FMath::Clamp(E->Item->MaximumWeaponLevel,1,999);
}
int64 UBBSBHuntingComponent::WeaponExperienceToNextLevel(FGuid Id) const
{
 const auto* E=FindOwnedEntry(Id);
 return E && E->Item && E->Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon && !IsEcstasyWeapon(Id) ? E->Item->WeaponExperienceRequirement(E->WeaponLevel):0;
}
int64 UBBSBHuntingComponent::WeaponExperienceInLevel(FGuid Id) const
{
 const auto* E=FindOwnedEntry(Id);
 return E && E->Item && E->Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon && !IsEcstasyWeapon(Id) ? FMath::Max<int64>(0,E->WeaponEcstatic-E->Item->WeaponExperienceForLevel(E->WeaponLevel)):0;
}
int64 UBBSBHuntingComponent::AddWeaponEcstatic(FGuid Id,int64 Amount)
{
 auto* E=MutableOwnedEntry(Id);
 if(Amount<=0 || !E || !E->Item || E->Item->EquipmentKind!=EBBSBItemEquipmentKind::Weapon)return 0;
 E->NormalizeWeapon();const int32 Before=E->WeaponLevel;
 const int64 Accepted=FMath::Min(Amount,E->Item->WeaponExperienceForLevel(E->Item->MaximumWeaponLevel)-E->WeaponEcstatic);
 if(Accepted<=0)return 0;
 E->WeaponEcstatic+=Accepted;E->InvestedEcstatic+=Accepted;E->NormalizeWeapon();LastExperienceWeapon=Id;
 LastExperienceGain=int32(FMath::Min<int64>(Accepted,MAX_int32));LastExperienceAt=GetWorld()->GetTimeSeconds();
 if(E->WeaponLevel>Before)LastLevelUpAt=LastExperienceAt;
 ProgressChanged();return Accepted;
}
bool UBBSBHuntingComponent::CollectEcstatic(int64 Amount,bool ChargeConcerto)
{
 if(Amount<=0)return false;
 TotalEcstaticCollected+=FMath::Min(Amount,MAX_int64-TotalEcstaticCollected);
 AddWeaponEcstatic(RewardWeaponInstance(),Amount);
 if(ChargeConcerto && Player && Player->EquipmentGrowth)
  Player->EquipmentGrowth->AddConcertoCharge(float(FMath::Min<double>(100.,double(Amount)*FMath::Max(0.f,RewardSettings()->ConcertoChargePerEcstatic))));
 ProgressChanged();return true;
}
bool UBBSBHuntingComponent::AddNotes(int64 Amount)
{
 if(Amount<=0 || Notes>MAX_int64-Amount)return false;
 Notes+=Amount;ProgressChanged();return true;
}
bool UBBSBHuntingComponent::SpendNotes(int64 Amount)
{
 if(Amount<0 || Amount>Notes)return false;
 Notes-=Amount;if(Amount>0)ProgressChanged();return true;
}
namespace
{
 void EnsureRewardBudget(ABBSBInkEnemy* Enemy)
 {
  if(Enemy->EcstaticRewardBudget<0)Enemy->EcstaticRewardBudget=FMath::Max(0,Enemy->ExperienceReward);
  if(Enemy->NoteRewardBudget<0)Enemy->NoteRewardBudget=FMath::Max(0,Enemy->NoteReward);
 }
}
int32 UBBSBHuntingComponent::SpawnResourceAmount(ABBSBInkEnemy* Enemy,EBBSBRewardResource Kind,int32 Amount,bool FinalBurst,int32& Index)
{
 if(!Player || !Enemy || Amount<=0)return 0;
 auto* Definition=ResourceDefinition(Kind);if(!Definition)return 0;
 const bool Concerto=Player->EquipmentGrowth && Player->EquipmentGrowth->bConcertoActive;
 const double SingleShare=FMath::RoundToDouble(double(FMath::Clamp(RewardSettings()->ResourceSingleFraction,0.f,1.f))*1000000.)/1000000.;
 int32 Singles=FMath::Clamp(int32(FMath::CeilToDouble(double(Amount)*SingleShare-1.e-7)),0,Amount);
 const int32 Large=FMath::Clamp(RewardSettings()->LargeResourceUnits,2,20);int32 Remaining=Amount,Emitted=0;
 while(Remaining>0)
 {
  const int32 Quantity=Singles>0 || Remaining<Large ? 1:Large;
  if(Singles>0)--Singles;
  const float Angle=Index++*2.399963f+FMath::FRandRange(-.35f,.35f);
  const float Radius=FinalBurst ? FMath::FRandRange(160.f,420.f):FMath::FRandRange(80.f,180.f);
  const FVector Origin=Enemy->GetActorLocation();FVector Ground=Origin+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
  FHitResult Hit;
  if(GetWorld()->LineTraceSingleByObjectType(Hit,Ground+FVector::UpVector*120,Ground-FVector::UpVector*1200,BBSBCombatVisuals::GeometryTypes(),FCollisionQueryParams(SCENE_QUERY_STAT(BBSBResourceGround),true,Enemy)))Ground=Hit.ImpactPoint;
  else Ground.Z=Origin.Z-Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  const FTransform Transform(FRotator::ZeroRotator,Origin+FVector::UpVector*35);
  auto* Loot=GetWorld()->SpawnActorDeferred<ABBSBLootPickup>(ABBSBLootPickup::StaticClass(),Transform,Enemy,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(!Loot)break;
  Loot->Item=Definition;Loot->Quantity=Quantity;Loot->bConcertoReward=Concerto;
  Loot->DropSeconds=FinalBurst ? .64f:.50f;Loot->DropArcHeight=FinalBurst ? 230.f:75.f;
  Loot->FinishSpawning(Transform);Loot->BeginDrop(Ground+FVector::UpVector*16);
  Remaining-=Quantity;Emitted+=Quantity;
  if(Concerto)Collect(Loot);
 }
 return Emitted;
}
void UBBSBHuntingComponent::GrantEnemyDamageRewards(ABBSBInkEnemy* Enemy)
{
 if(!Player || !IsValid(Enemy) || Enemy->bDefeated || Enemy->Health<=0 || !Enemy->bGrantsHuntingRewards || Enemy->bHuntingRewardsGranted)return;
 EnsureRewardBudget(Enemy);
 const float Ratio=FMath::Clamp(Enemy->Health/FMath::Max(1.f,Enemy->MaximumHealth),0.f,1.f);
 Enemy->RewardLowestHealthFraction=FMath::Min(Enemy->RewardLowestHealthFraction,Ratio);
 const double Lost=FMath::RoundToDouble((1.-Enemy->RewardLowestHealthFraction)*1000000.)/1000000.;
 const double Fraction=FMath::RoundToDouble(double(FMath::Clamp(RewardSettings()->AliveResourceFraction,0.f,.9f))*1000000.)/1000000.;
 auto Desired=[&](int32 Budget)
 { return int32(FMath::Min(FMath::FloorToDouble(Budget*Fraction+1.e-7),FMath::CeilToDouble(Budget*Fraction*Lost-1.e-7))); };
 const int32 XP=FMath::Max(0,Desired(Enemy->EcstaticRewardBudget)-Enemy->EcstaticRewardsDropped);
 const int32 NotesToDrop=FMath::Max(0,Desired(Enemy->NoteRewardBudget)-Enemy->NoteRewardsDropped);
 Enemy->EcstaticRewardsDropped+=SpawnResourceAmount(Enemy,EBBSBRewardResource::Ecstatic,XP,false,Enemy->RewardDropSerial);
 Enemy->NoteRewardsDropped+=SpawnResourceAmount(Enemy,EBBSBRewardResource::Note,NotesToDrop,false,Enemy->RewardDropSerial);
}
void UBBSBHuntingComponent::SpawnBasicRewards(ABBSBInkEnemy* Enemy,int32& Index)
{
 if(!Player || !Enemy)return;EnsureRewardBudget(Enemy);Index=Enemy->RewardDropSerial;
 Enemy->EcstaticRewardsDropped+=SpawnResourceAmount(Enemy,EBBSBRewardResource::Ecstatic,FMath::Max(0,Enemy->EcstaticRewardBudget-Enemy->EcstaticRewardsDropped),true,Index);
 Enemy->NoteRewardsDropped+=SpawnResourceAmount(Enemy,EBBSBRewardResource::Note,FMath::Max(0,Enemy->NoteRewardBudget-Enemy->NoteRewardsDropped),true,Index);
 Enemy->RewardDropSerial=Index;
}
void UBBSBHuntingComponent::TickResourcePickups()
{
 if(!Player || !Player->Combat || Player->Combat->bDead)return;
 const double Now=GetWorld()->GetTimeSeconds();if(Now<NextResourceScanAt)return;NextResourceScanAt=Now+.1;
 const float Radius=FMath::Clamp(RewardSettings()->ResourcePickupRadius,0.f,500.f);if(Radius<=0)return;
 const FVector From=Player->GetActorLocation();
 for(const auto& Weak:LootActors)
 {
  auto* Loot=Weak.Get();
  if(IsValid(Loot) && Loot->IsAvailable() && Loot->Item->IsFieldResource() && FVector::DistSquared(From,Loot->GetActorLocation())<=FMath::Square(Radius) && LootVisible(Loot,From))Collect(Loot);
 }
}
FBBSBWeaponDismantlePreview UBBSBHuntingComponent::PreviewDismantleWeapon(FGuid Id) const
{
 FBBSBWeaponDismantlePreview Result;const auto* E=FindEntry(Id);
 if(!E || !E->Item || E->Item->EquipmentKind!=EBBSBItemEquipmentKind::Weapon) { Result.Reason=FText::FromString(TEXT("파기할 무기를 가방으로 옮겨 주세요"));return Result; }
 const auto* A=Player ? Player->FieldActions.Get():nullptr;
 if(!A || Player->Combat->bDead || A->IsEquipmentActionBusy() || A->IsGestureCaptured() || A->EquippedWeaponInstance==Id || A->PendingWeaponInstance==Id || (Player->EquipmentGrowth && Player->EquipmentGrowth->bConcertoActive))
 { Result.Reason=FText::FromString(TEXT("사용 중인 무기는 파기할 수 없어요"));return Result; }
 auto* Crystal=EcstaticCrystalDefinition();if(!Crystal) { Result.Reason=FText::FromString(TEXT("엑스태틱 결정 정보를 확인해 주세요"));return Result; }
 const double Value=FMath::FloorToDouble(double(E->InvestedEcstatic)*FMath::Clamp(RewardSettings()->CrystalRefundFraction,0.f,1.f)/Crystal->ResourceValue);
 if(Value>MAX_int32) { Result.Reason=FText::FromString(TEXT("환급량이 너무 커요"));return Result; }
 Result.CrystalCount=int32(Value);Result.RefundedEcstatic=int64(Result.CrystalCount)*Crystal->ResourceValue;
 for(const auto& O:E->Orbs)if(O.Item)++Result.ReturnedOrbs;
 Result.bAllowed=true;return Result;
}
bool UBBSBHuntingComponent::DismantleWeapon(FGuid Id)
{
 const auto Preview=PreviewDismantleWeapon(Id);auto* Growth=Player ? Player->EquipmentGrowth.Get():nullptr;
 if(!Preview.bAllowed) { if(Growth)Growth->Feedback=Preview.Reason.ToString();return false; }
 const auto Weapon=*FindEntry(Id);auto Bag=Items;Bag.RemoveAll([&](const auto& E){return E.InstanceId==Id;});
 // Build the entire result off to the side. No item or socket is lost on a full bag.
 auto Insert=[&](UBBSBItemDefinition* Item,int32 Quantity,FGuid Identity)
 {
  for(auto& E:Bag)if(E.Item==Item && E.Quantity<Item->StackLimit())
  { const int32 Added=FMath::Min(Quantity,Item->StackLimit()-E.Quantity);E.Quantity+=Added;Quantity-=Added;if(Quantity==0)return true; }
  while(Quantity>0)
  {
   FIntPoint Free(INDEX_NONE,INDEX_NONE);
   for(int32 Y=0;Y<Rows && Free.X==INDEX_NONE;++Y)for(int32 X=0;X<Columns;++X)
    if(!Bag.ContainsByPredicate([&](const auto& E){return E.GridX==X && E.GridY==Y;})) { Free=FIntPoint(X,Y);break; }
   if(Free.X==INDEX_NONE)return false;
   FBBSBInventoryEntry E;E.Item=Item;E.InstanceId=Identity.IsValid() ? Identity:FGuid::NewGuid();Identity.Invalidate();E.Quantity=FMath::Min(Quantity,Item->StackLimit());E.GridX=Free.X;E.GridY=Free.Y;E.NormalizeWeapon();Quantity-=E.Quantity;Bag.Add(E);
  }
  return true;
 };
 bool Fits=true;
 for(const auto& O:Weapon.Orbs)if(O.Item && !Insert(O.Item,1,O.InstanceId)) { Fits=false;break; }
 auto* Crystal=EcstaticCrystalDefinition();
 if(Fits && Preview.CrystalCount>0)Fits=Insert(Crystal,Preview.CrystalCount,FGuid());
 if(!Fits) { if(Growth)Growth->Feedback=TEXT("구슬과 결정을 돌려받을 가방 공간이 필요해요");return false; }
 Items=MoveTemp(Bag);
 if(Player->FieldActions->LastCombatWeaponInstance==Id)Player->FieldActions->LastCombatWeaponInstance.Invalidate();
 if(Preview.CrystalCount>0)ShowPickup(Crystal,Preview.CrystalCount);
 if(Growth)Growth->Feedback=FString::Printf(TEXT("무기 파기 · 결정 %d개 · 구슬 %d개 반환"),Preview.CrystalCount,Preview.ReturnedOrbs);
 ProgressChanged();return true;
}
bool UBBSBHuntingComponent::UseEcstaticCrystals(FGuid WeaponId,int32 Count)
{
 const auto* E=FindOwnedEntry(WeaponId);auto* Crystal=EcstaticCrystalDefinition();
 if(Count<=0 || !Crystal || !E || !E->Item || E->Item->EquipmentKind!=EBBSBItemEquipmentKind::Weapon || IsEcstasyWeapon(WeaponId) || ItemCount(Crystal)<Count)return false;
 const int64 Remaining=E->Item->WeaponExperienceForLevel(E->Item->MaximumWeaponLevel)-E->WeaponEcstatic;
 const int32 Used=int32(FMath::Min<int64>(Count,FMath::DivideAndRoundUp(Remaining,int64(Crystal->ResourceValue))));
 if(Used<=0 || !ConsumeItem(Crystal,Used))return false;
 const int64 Granted=AddWeaponEcstatic(WeaponId,int64(Used)*Crystal->ResourceValue);
 if(Player && Player->EquipmentGrowth)Player->EquipmentGrowth->Feedback=FString::Printf(TEXT("결정 %d개 사용 · 엑스태틱 +%lld"),Used,Granted);
 return Granted>0; // Crystals invest experience; they never charge concerto.
}
