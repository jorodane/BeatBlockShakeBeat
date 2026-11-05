#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BBSBGrowthData.generated.h"

class UBBSBItemDefinition;
class UAnimSequence;
class UTexture2D;
class UStaticMesh;

UENUM(BlueprintType)
enum class EBBSBModuleSlot : uint8 { Aerial,Directional };
UENUM(BlueprintType)
enum class EBBSBModuleAction : uint8 { Glide,Wire };
UENUM(BlueprintType)
enum class EBBSBOrbEffect : uint8 { Stats,Element,Auxiliary,Ensemble };
UENUM(BlueprintType)
enum class EBBSBOrbCondition : uint8 { Always,OnBeat,FullHealth,Airborne,TargetLocked };
UENUM(BlueprintType)
enum class EBBSBPerformancePattern : uint8 { Slash,Thrust,Orbit,Volley,Pulse,Spiral };

USTRUCT(BlueprintType)
struct FBBSBOrbEffectSpec
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) EBBSBOrbEffect Effect=EBBSBOrbEffect::Stats;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float AttackBonus=.15f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float FlatDamage=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax=".8")) float CooldownReduction=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) EBBSBOrbCondition Condition=EBBSBOrbCondition::Always;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float ConditionalBonus=.2f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float BurnDamagePerSecond=4;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float StatusSeconds=3;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin=".1",ClampMax="1")) float SlowMultiplier=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin=".1")) float EnsembleCooldown=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bPartnerNeedsEnsembleOrb=false;
};

USTRUCT(BlueprintType)
struct FBBSBConcertoCameraKey
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float Time=0;
 // Unwrapped azimuth decreases for a counterclockwise orbit viewed from above.
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float Azimuth=90;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float Distance=150;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float Height=150;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float FocusHeight=140;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float FOV=55;
};
UCLASS(BlueprintType)
class BBSB_API UBBSBConcertoCameraPreset : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float BlendInFraction=.07f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="1")) float FaceSlowStart=.20f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax=".49",Units="s")) float FaceSlowSeconds=.32f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin=".7",ClampMax="1")) float FaceSlowRate=.85f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float ReturnStart=.88f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float InterruptedReturnSeconds=.35f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(TitleProperty="Time")) TArray<FBBSBConcertoCameraKey> Keys;
};
USTRUCT(BlueprintType)
struct FBBSBPerformanceRole
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) EBBSBPerformancePattern Pattern=EBBSBPerformancePattern::Slash;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float DamageMultiplier=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin=".05")) float Interval=.5f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin=".1")) float Duration=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="10")) float Radius=180;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="100")) float Range=1800;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> ConductorAnimation;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UBBSBConcertoCameraPreset> CameraPreset;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1",ClampMax="8")) float SpiralBladeScale=4.5f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="20",ClampMax="500")) float SpiralWindWidth=240.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin=".06",ClampMax="1",Units="s")) float SpiralHitInterval=.16f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="2")) float SpiralHitDamageScale=.25f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float SpiralTurns=5;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float SweepRadius=160;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float WarmupFraction=.26f;
};

USTRUCT(BlueprintType)
struct FBBSBOrbSocket
{
 GENERATED_BODY()
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) FGuid InstanceId;
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBBSBItemDefinition> Item;
};

UCLASS(BlueprintType)
class BBSB_API UBBSBModuleDefinition : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FName ModuleId;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FText DisplayName;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(MultiLine=true)) FText Description;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) EBBSBModuleSlot Slot=EBBSBModuleSlot::Aerial;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) EBBSBModuleAction Action=EBBSBModuleAction::Glide;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<UTexture2D> Icon;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) int32 MaximumLevel=5;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<UBBSBItemDefinition> UpgradeMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0")) int32 UpgradeBaseCost=3;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float UpgradeCostGrowth=1.5f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0")) float SpeedPerLevel=.1f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0")) float EfficiencyPerLevel=.06f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0")) float RangePerLevel=.08f;
 UFUNCTION(BlueprintPure) int32 UpgradeCost(int32 CurrentLevel) const;
};

USTRUCT(BlueprintType)
struct FBBSBOwnedModule
{
 GENERATED_BODY()
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBBSBModuleDefinition> Definition;
 UPROPERTY(SaveGame,VisibleAnywhere,BlueprintReadOnly) int32 Level=1;
};

USTRUCT(BlueprintType)
struct FBBSBWeaponSnapshot
{
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FGuid InstanceId;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBBSBItemDefinition> Definition;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 WeaponLevel=1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 TemplateId=4;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float DamageMultiplier=1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float FlatDamage=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float CooldownMultiplier=1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<FBBSBOrbEffectSpec> Effects;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FBBSBPerformanceRole Main;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FBBSBPerformanceRole Sub;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMesh> Mesh;
};

/** Prototype balance lives in this asset; no value here declares a final cap or economy. */
UCLASS(BlueprintType)
class BBSB_API UBBSBGrowthSettings : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Prototype compatibility") bool bGrantPrototypeTraversalModules=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Modules") TArray<TObjectPtr<UBBSBModuleDefinition>> StartingModules;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Concerto",meta=(ClampMin=".1")) float Duration=3.5f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Concerto",meta=(ClampMin="0")) float PassiveChargePerSecond=0.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Concerto",meta=(ClampMin="0")) float ChargePerDamage=.12f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Concerto",meta=(ClampMin="0")) float SupportChargeScale=.3f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Concerto",meta=(ClampMin="100")) float BulletClearRadius=1800;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Concerto",meta=(ClampMin="0")) float MainDamageMultiplier=2.5f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Concerto",meta=(ClampMin="0")) float SubDamageMultiplier=.7f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Ensemble",meta=(ClampMin="0")) float EnsembleDamageMultiplier=.5f;
};
