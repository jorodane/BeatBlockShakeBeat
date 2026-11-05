#include "Hunting/BBSBGrowthData.h"
#include "Hunting/BBSBItem.h"
UBBSBGrowthSettings::UBBSBGrowthSettings()
{
 EcstaticPickup=TSoftObjectPtr<UBBSBItemDefinition>(FSoftObjectPath(TEXT("/Game/BBSB/Hunting/Rewards/DA_Reward_Ecstatic.DA_Reward_Ecstatic")));
 NotePickup=TSoftObjectPtr<UBBSBItemDefinition>(FSoftObjectPath(TEXT("/Game/BBSB/Hunting/Rewards/DA_Reward_Note.DA_Reward_Note")));
 EcstaticCrystal=TSoftObjectPtr<UBBSBItemDefinition>(FSoftObjectPath(TEXT("/Game/BBSB/Hunting/Rewards/DA_EcstaticCrystal.DA_EcstaticCrystal")));
}
int32 UBBSBModuleDefinition::UpgradeCost(int32 CurrentLevel) const
{
 if(CurrentLevel>=MaximumLevel)return 0;
 const double Cost=double(FMath::Max(0,UpgradeBaseCost))*FMath::Pow(double(FMath::Max(1.f,UpgradeCostGrowth)),FMath::Max(0,CurrentLevel-1));
 return int32(FMath::Min(double(MAX_int32),FMath::CeilToDouble(Cost)));
}
