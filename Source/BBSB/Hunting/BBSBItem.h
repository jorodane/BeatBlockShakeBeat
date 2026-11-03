#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/SaveGame.h"
#include "BBSBItem.generated.h"

class UStaticMesh;
class UTexture2D;
class USkeletalMesh;

UENUM(BlueprintType)
enum class EBBSBItemRarity : uint8 { Common,Uncommon,Rare,Epic,Legendary };
UENUM(BlueprintType)
enum class EBBSBItemEquipmentKind : uint8 { None,Weapon,Companion,Armor,Accessory };
UENUM(BlueprintType)
enum class EBBSBItemCategory : uint8 { Equipment,Consumable,Material,Quest,Misc };

UCLASS(BlueprintType)
class BBSB_API UBBSBItemDefinition : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item") FName ItemId;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item") FText DisplayName;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Inventory|Sort") EBBSBItemCategory ItemCategory=EBBSBItemCategory::Material;
 /** Empty uses ItemId, so existing items already have a stable type grouping. */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Inventory|Sort") FName ItemType;
 EBBSBItemCategory SortCategory() const { return EquipmentKind==EBBSBItemEquipmentKind::None ? ItemCategory:EBBSBItemCategory::Equipment; }
 FName SortType() const { return !ItemType.IsNone() ? ItemType:!ItemId.IsNone() ? ItemId:GetFName(); }
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item") EBBSBItemRarity Rarity=EBBSBItemRarity::Common;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Equipment") EBBSBItemEquipmentKind EquipmentKind=EBBSBItemEquipmentKind::None;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Equipment",meta=(ClampMin="1",ClampMax="6",EditCondition="EquipmentKind==EBBSBItemEquipmentKind::Weapon")) int32 WeaponId=4;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Equipment",meta=(EditCondition="EquipmentKind==EBBSBItemEquipmentKind::Companion")) TSoftObjectPtr<USkeletalMesh> CompanionMesh;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item",meta=(MultiLine=true)) FText Description;
 // Retained so previously saved item assets load; all items now occupy one slot.
 UPROPERTY(meta=(DeprecatedProperty)) FIntPoint GridSize=FIntPoint(1,1);
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Inventory",meta=(ClampMin="1",ClampMax="999")) int32 MaximumStack=99;
 UPROPERTY(meta=(DeprecatedProperty)) bool bCanRotate=false;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Presentation") FLinearColor Color=FLinearColor(.52f,.82f,.73f);
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Presentation") FName IconGlyph="Droplet";
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Presentation") TObjectPtr<UTexture2D> Icon;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Presentation") TObjectPtr<UStaticMesh> WorldMesh;
 UFUNCTION(BlueprintPure,Category="Item") int32 StackLimit() const { return EquipmentKind==EBBSBItemEquipmentKind::None ? FMath::Clamp(MaximumStack,1,999):1; }
 UFUNCTION(BlueprintPure,Category="Equipment") bool IsBuiltInCompanion() const { return GetPathName()==TEXT("/Game/BBSB/Hunting/Equipment/DA_Equipment_Bunniar.DA_Equipment_Bunniar"); }
 FIntPoint Footprint(bool Rotated=false) const
 {
  return FIntPoint(1,1);
 }
 UFUNCTION(BlueprintPure,Category="Item") FLinearColor RarityColor() const
 {
  switch(Rarity)
  {
   case EBBSBItemRarity::Uncommon:return FLinearColor(.53f,.92f,.70f);
   case EBBSBItemRarity::Rare:return FLinearColor(.46f,.73f,1.f);
   case EBBSBItemRarity::Epic:return FLinearColor(.82f,.54f,1.f);
   case EBBSBItemRarity::Legendary:return FLinearColor(1.f,.77f,.36f);
   default:return FLinearColor(.92f,.91f,.86f);
  }
 }
 UFUNCTION(BlueprintPure,Category="Item") FText RarityLabel() const
 {
  static const TCHAR* Names[]={TEXT("일반"),TEXT("고급"),TEXT("희귀"),TEXT("영웅"),TEXT("전설")};
  return FText::FromString(Names[FMath::Clamp(int32(Rarity),0,4)]);
 }
};

USTRUCT(BlueprintType)
struct FBBSBLootRoll
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UBBSBItemDefinition> Item;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="1")) float Chance=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1",ClampMax="999")) int32 MinimumQuantity=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1",ClampMax="999")) int32 MaximumQuantity=1;
};

UCLASS(BlueprintType)
class BBSB_API UBBSBLootTable : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Loot") TArray<FBBSBLootRoll> Drops;
};

USTRUCT(BlueprintType)
struct FBBSBInventoryEntry
{
 GENERATED_BODY()
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) FGuid InstanceId;
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBBSBItemDefinition> Item;
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) int32 Quantity=1;
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) int32 GridX=0;
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) int32 GridY=0;
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) bool bRotated=false;
};

UCLASS()
class BBSB_API UBBSBHuntingSave : public USaveGame
{
 GENERATED_BODY()
public:
 // Keep the legacy default: older saves omitted this equal-to-default value.
 // New saves explicitly serialize their version instead of inheriting a new default.
 UPROPERTY(SaveGame) int32 Version=1;
 UPROPERTY(SaveGame) int32 Level=1;
 UPROPERTY(SaveGame) int32 Experience=0;
 UPROPERTY(SaveGame) TArray<FBBSBInventoryEntry> Items;
 UPROPERTY(SaveGame) TArray<FBBSBInventoryEntry> EquippedItems;
 UPROPERTY(SaveGame) int32 CompanionWheelSlot=0;
};
