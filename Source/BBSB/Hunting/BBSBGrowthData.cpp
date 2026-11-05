#include "Hunting/BBSBGrowthData.h"
int32 UBBSBModuleDefinition::UpgradeCost(int32 CurrentLevel) const
{
 if(CurrentLevel>=MaximumLevel)return 0;
 const double Cost=double(FMath::Max(0,UpgradeBaseCost))*FMath::Pow(double(FMath::Max(1.f,UpgradeCostGrowth)),FMath::Max(0,CurrentLevel-1));
 return int32(FMath::Min(double(MAX_int32),FMath::CeilToDouble(Cost)));
}
