#include "Hunting/BBSBEquipmentGrowth.h"
#include "Hunting/BBSBHunting.h"
#include "BBSBCharacter.h"
#include "Character/BBSBTraversal.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatRules.h"
#include "Field/BBSBFieldActions.h"
#include "Field/BBSBWeaponAnimation.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

UBBSBEquipmentGrowthComponent::UBBSBEquipmentGrowthComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
 SettingsAsset=TSoftObjectPtr<UBBSBGrowthSettings>(FSoftObjectPath(TEXT("/Game/BBSB/Growth/DA_EquipmentGrowth.DA_EquipmentGrowth")));
}
void UBBSBEquipmentGrowthComponent::BeginPlay()
{
 Super::BeginPlay();Player=Cast<ABBSBCharacter>(GetOwner());
 if(Player && Player->Traversal)
 {
  const auto* T=Player->Traversal.Get();BaseGlideSpeed=T->GlideSpeed;BaseGlideFall=T->GlideFallSpeed;BaseWireSpeed=T->WireSpeed;BaseSkyWireDistance=T->SkyWireDistance;BaseWireRecharge=T->WireRechargeSeconds;
  AddTickPrerequisiteComponent(Player->Hunting);
 }
}
void UBBSBEquipmentGrowthComponent::EnsureInitialized()
{
 if(bInitialized)return;if(!Player)Player=Cast<ABBSBCharacter>(GetOwner());
 Settings=SettingsAsset.LoadSynchronous();if(!Settings)Settings=NewObject<UBBSBGrowthSettings>(this);bInitialized=true;
}
void UBBSBEquipmentGrowthComponent::Changed() { if(Player && Player->Hunting)Player->Hunting->NotifyGrowthChanged(); }
void UBBSBEquipmentGrowthComponent::InitializeProfile(bool NewOrLegacy)
{
 EnsureInitialized();
 if(NewOrLegacy && Settings->bGrantPrototypeTraversalModules)for(const auto& Module:Settings->StartingModules)GrantModule(Module.Get(),true);
 bProfileReady=true;bStatsPending=true;
}
void UBBSBEquipmentGrowthComponent::LoadFromSave(const UBBSBHuntingSave* Save)
{
 EnsureInitialized();OwnedModules.Reset();BunniarBagPositions.Reset();AerialModule=DirectionalModule=NAME_None;
 if(!Save)return;
 TSet<FName> Seen;
 for(auto M:Save->OwnedModules)if(M.Definition && !M.Definition->ModuleId.IsNone() && !Seen.Contains(M.Definition->ModuleId))
 { Seen.Add(M.Definition->ModuleId);M.Level=FMath::Clamp(M.Level,1,FMath::Max(1,M.Definition->MaximumLevel));OwnedModules.Add(M); }
 if(const auto* D=ModuleDefinition(Save->AerialModule);D && D->Slot==EBBSBModuleSlot::Aerial)AerialModule=D->ModuleId;
 if(const auto* D=ModuleDefinition(Save->DirectionalModule);D && D->Slot==EBBSBModuleSlot::Directional)DirectionalModule=D->ModuleId;
 BunniarBagPositions=Save->BunniarBagPositions;EnsureBunniarBag();
 ConcertoCharge=FMath::Clamp(Save->ConcertoCharge,0.f,100.f);bPendingAerial=bPendingDirectional=bModuleChangePending=false;bStatsPending=true;
}
void UBBSBEquipmentGrowthComponent::StoreInSave(UBBSBHuntingSave* Save) const
{ Save->OwnedModules=OwnedModules;Save->BunniarBagPositions=BunniarBagPositions;Save->AerialModule=AerialModule;Save->DirectionalModule=DirectionalModule;Save->ConcertoCharge=ConcertoCharge; }
UBBSBModuleDefinition* UBBSBEquipmentGrowthComponent::ModuleDefinition(FName Id) const
{ const auto* M=OwnedModules.FindByPredicate([&](const auto& V){return V.Definition && V.Definition->ModuleId==Id;});return M ? M->Definition.Get():nullptr; }
int32 UBBSBEquipmentGrowthComponent::ModuleLevel(FName Id) const
{ const auto* M=OwnedModules.FindByPredicate([&](const auto& V){return V.Definition && V.Definition->ModuleId==Id;});return M ? M->Level:0; }
bool UBBSBEquipmentGrowthComponent::GrantModule(UBBSBModuleDefinition* D,bool EquipIfEmpty)
{
 if(!D || D->ModuleId.IsNone() || (D->Action==EBBSBModuleAction::Glide ? D->Slot!=EBBSBModuleSlot::Aerial:D->Slot!=EBBSBModuleSlot::Directional))return false;
 const bool Acquired=!ModuleDefinition(D->ModuleId);
 if(Acquired) { FBBSBOwnedModule M;M.Definition=D;OwnedModules.Add(M);EnsureBunniarBag();Feedback=D->DisplayName.ToString()+TEXT(" 획득");Changed(); }
 if(Acquired && EquipIfEmpty && (D->Slot==EBBSBModuleSlot::Aerial ? AerialModule:DirectionalModule).IsNone())EquipModule(D->Slot,D->ModuleId);
 return true;
}
bool UBBSBEquipmentGrowthComponent::IsModuleSwapSafe() const
{ return !Player || (!bConcertoActive && !Player->FieldActions->IsGestureCaptured() && !Player->Traversal->IsBunniarTraversalActive() && !Player->Traversal->IsWireTraversalActive() && !Player->Traversal->IsParkourActive()); }
bool UBBSBEquipmentGrowthComponent::EquipModule(EBBSBModuleSlot Slot,FName Id)
{
 const auto* D=ModuleDefinition(Id);if(!Id.IsNone() && (!D || D->Slot!=Slot))return false;
 if(!IsModuleSwapSafe())
 {
  if(Slot==EBBSBModuleSlot::Aerial) { PendingAerialModule=Id;bPendingAerial=true; }
  else { PendingDirectionalModule=Id;bPendingDirectional=true; }
  bModuleChangePending=true;Feedback=TEXT("행동이 끝난 뒤 모듈을 교체해요");return true;
 }
 (Slot==EBBSBModuleSlot::Aerial ? AerialModule:DirectionalModule)=Id;bStatsPending=true;Changed();return true;
}
bool UBBSBEquipmentGrowthComponent::CanUpgradeModule(FName Id) const
{
 const auto* D=ModuleDefinition(Id);const int32 Level=ModuleLevel(Id);
 return D && Level<D->MaximumLevel && Player && Player->Hunting && (D->UpgradeCost(Level)==0 || (D->UpgradeMaterial && Player->Hunting->ItemCount(D->UpgradeMaterial)>=D->UpgradeCost(Level)));
}
bool UBBSBEquipmentGrowthComponent::UpgradeModule(FName Id)
{
 if(!CanUpgradeModule(Id)) { Feedback=TEXT("강화 재료나 모듈 단계를 확인해 주세요");return false; }
 auto* D=ModuleDefinition(Id);const int32 Cost=D->UpgradeCost(ModuleLevel(Id));
 if(Cost>0 && !Player->Hunting->ConsumeItem(D->UpgradeMaterial,Cost))return false;
 auto* M=OwnedModules.FindByPredicate([&](const auto& V){return V.Definition==D;});++M->Level;bStatsPending=true;Feedback=D->DisplayName.ToString()+TEXT(" 강화 완료");Changed();return true;
}
bool UBBSBEquipmentGrowthComponent::HasModuleAction(EBBSBModuleAction Action) const
{
 const auto* D=ModuleDefinition(Action==EBBSBModuleAction::Glide ? AerialModule:DirectionalModule);return D && D->Action==Action;
}
void UBBSBEquipmentGrowthComponent::ApplyModuleStats()
{
 if(!Player || !Player->Traversal)return;auto* T=Player->Traversal.Get();
 T->GlideSpeed=BaseGlideSpeed;T->GlideFallSpeed=BaseGlideFall;T->WireSpeed=BaseWireSpeed;T->SkyWireDistance=BaseSkyWireDistance;T->WireRechargeSeconds=BaseWireRecharge;
 if(const auto* D=ModuleDefinition(AerialModule)) { const float Steps=FMath::Max(0,ModuleLevel(AerialModule)-1);T->GlideSpeed*=1+Steps*D->SpeedPerLevel;T->GlideFallSpeed/=1+Steps*D->EfficiencyPerLevel; }
 if(const auto* D=ModuleDefinition(DirectionalModule)) { const float Steps=FMath::Max(0,ModuleLevel(DirectionalModule)-1);T->WireSpeed*=1+Steps*D->SpeedPerLevel;T->SkyWireDistance*=1+Steps*D->RangePerLevel;T->WireRechargeSeconds/=1+Steps*D->EfficiencyPerLevel; }
 bStatsPending=false;
}
FBBSBWeaponSnapshot UBBSBEquipmentGrowthComponent::WeaponSnapshot(FGuid Id) const
{
 FBBSBWeaponSnapshot S;const auto* E=Player && Player->Hunting ? Player->Hunting->FindOwnedEntry(Id):nullptr;
 if(!E || !E->Item || E->Item->EquipmentKind!=EBBSBItemEquipmentKind::Weapon)return S;
 S.InstanceId=Id;S.Definition=E->Item;S.TemplateId=E->Item->WeaponId;S.WeaponLevel=FMath::Max(1,E->WeaponLevel);S.Main=E->Item->ConcertoMain;S.Sub=E->Item->ConcertoSub;S.Mesh=E->Item->WorldMesh;
 S.DamageMultiplier=E->Item->WeaponDamageMultiplier*(1+(S.WeaponLevel-1)*E->Item->DamagePerLevel);
 for(const auto& Orb:E->Orbs)if(Orb.Item && Orb.Item->EquipmentKind==EBBSBItemEquipmentKind::Orb)
 {
  const auto& Effect=Orb.Item->OrbEffect;S.Effects.Add(Effect);
  if(Effect.Effect==EBBSBOrbEffect::Stats) { S.DamageMultiplier+=Effect.AttackBonus;S.FlatDamage+=Effect.FlatDamage;S.CooldownMultiplier*=1-FMath::Clamp(Effect.CooldownReduction,0.f,.8f); }
 }
 if(Player->Hunting->IsEcstasyWeapon(Id))
 { const float Ecstasy=1+FMath::Clamp(E->Item->EcstasyAttackBonus,0.f,2.f);S.DamageMultiplier*=Ecstasy;S.FlatDamage*=Ecstasy; }
 S.CooldownMultiplier=FMath::Clamp(S.CooldownMultiplier,.2f,3.f);
 if(!S.Mesh && !E->Item->WeaponAnimation.IsNull())if(const auto* Animation=E->Item->WeaponAnimation.LoadSynchronous())S.Mesh=Animation->WeaponMesh;
 if(!S.Mesh && Player->FieldActions)if(const auto* Asset=Player->FieldActions->WeaponAnimationSets.Find(EBBSBPrototypeWeapon(S.TemplateId)))if(const auto* Animation=Asset->LoadSynchronous())S.Mesh=Animation->WeaponMesh;
 return S;
}
float UBBSBEquipmentGrowthComponent::ConditionalDamageMultiplier(const FBBSBWeaponSnapshot& S) const
{
 float Multiplier=1;
 for(const auto& E:S.Effects)if(E.Effect==EBBSBOrbEffect::Auxiliary)
 {
  bool Applies=E.Condition==EBBSBOrbCondition::Always;
  if(Player)switch(E.Condition)
  {
   case EBBSBOrbCondition::OnBeat: { const double B=Player->Combat->IsStageLocked() ? Player->Combat->Stage->CurrentBeat:BBSBCombatRules::BeatAt(GetWorld()->GetTimeSeconds(),0,Player->Combat->FieldBPM);Applies=FMath::Abs(B-FMath::RoundToDouble(B))<.15;break; }
   case EBBSBOrbCondition::FullHealth:Applies=Player->Combat->Health>=Player->Combat->MaximumHealth;break;
   case EBBSBOrbCondition::Airborne:Applies=Player->GetCharacterMovement()->IsFalling();break;
   case EBBSBOrbCondition::TargetLocked:Applies=Player->Combat->HasTargetLock();break;
   default:break;
  }
  if(Applies)Multiplier+=FMath::Max(0.f,E.ConditionalBonus);
 }
 return Multiplier;
}
void UBBSBEquipmentGrowthComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(DT,Type,Tick);if(!Player || !Player->IsLocallyControlled() || !bProfileReady)return;
 TickStatuses(DT);
 if(Player->Combat->bDead) { CancelConcerto();return; }
 if(IsModuleSwapSafe())
 {
  if(bPendingAerial) { AerialModule=PendingAerialModule;bPendingAerial=false;bStatsPending=true;Changed(); }
  if(bPendingDirectional) { DirectionalModule=PendingDirectionalModule;bPendingDirectional=false;bStatsPending=true;Changed(); }
  bModuleChangePending=bPendingAerial || bPendingDirectional;if(bStatsPending)ApplyModuleStats();
 }
 TickPerformances(DT);
}
void UBBSBEquipmentGrowthComponent::EndPlay(const EEndPlayReason::Type Reason)
{ CancelConcerto();for(auto& Pair:Statuses)if(Pair.Key.IsValid()) { Pair.Key->GrowthSlowMultiplier=1;Pair.Key->GetCharacterMovement()->MaxWalkSpeed=Pair.Value.OriginalSpeed; }Statuses.Reset();Super::EndPlay(Reason); }
