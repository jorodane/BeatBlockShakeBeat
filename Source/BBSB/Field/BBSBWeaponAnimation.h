#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "BBSBFieldGesture.h"
#include "BBSBWeaponAnimation.generated.h"

class ULevelSequence;
class UStaticMesh;
class UStaticMeshComponent;
class UAnimSequence;
class UMaterialInterface;
class UNiagaraSystem;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EBBSBPrototypeWeapon : uint8 { Legacy, Greatsword, Spear, Hammer, Sword, Dagger, Crescent };

/** Sequence time is normalized to the action duration. WeaponMotion is local to the hit anchor. */
USTRUCT(BlueprintType)
struct BBSB_API FBBSBWeaponAnimationClip
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) EBBSBFieldGesture Gesture=EBBSBFieldGesture::Beat;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="3")) int32 ComboStep=0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="-1",ClampMax="1")) int32 MouseChannel=-1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<ULevelSequence> Sequence;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.05")) float Duration=0.9f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1")) float ActiveStart=0.2f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1")) float ActiveEnd=0.64f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1")) float BlendIn=0.2f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bMirrorLeftHand=false;
 /** Spear Flick uses the same clip in an impulse-facing frame. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseImpulseFrame=false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> CharacterAnimation;
 /** Optional airborne override. The traversal library provides the shared placeholder combo. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> AirCharacterAnimation;
 /** Advance during the active swing; swept capsule movement respects walls. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",Units="cm")) float ForwardTravel=0;
 /** Sample WeaponMotion around the character instead of the remote hit anchor. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bUseOwnerFrame=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(Units="deg")) float BodyTurnDegrees=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",Units="s")) float RecoverySeconds=0;
 /** Optional legs-only step; torso and authored arm path stay independent. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> StepAnimation;
};

UCLASS(BlueprintType)
class BBSB_API UBBSBWeaponAnimationSet : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") EBBSBPrototypeWeapon Weapon=EBBSBPrototypeWeapon::Hammer;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon") TObjectPtr<UStaticMesh> WeaponMesh;
 /** Dagger curves use a 250 cm reference flight; X scales to the real frozen target distance. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile", meta=(ClampMin="100")) float ProjectileSpeed=1900.f;
 /** Upper and Lower component transform tracks; start=staff, end=equipped form. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reparaton") TObjectPtr<ULevelSequence> FormSequence;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reparaton") bool bSplitReparaton=true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reparaton") bool bKeepFormEquipped=true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reparaton", meta=(ClampMin="0.01")) float FormTransitionSeconds=0.18f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Actions", meta=(TitleProperty="Sequence")) TArray<FBBSBWeaponAnimationClip> Clips;
 /** Blade-local endpoints exclude the guard and hilt. Ribbon samples stay in world space. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Blade trail") bool bEnableBladeTrail=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Blade trail") TObjectPtr<UMaterialInterface> BladeTrailMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Blade trail",meta=(Units="cm")) FVector BladeTrailRoot=FVector(0,0,34);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Blade trail",meta=(Units="cm")) FVector BladeTrailTip=FVector(0,0,89.8f);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Blade trail",meta=(ClampMin="0.03",ClampMax="2",Units="s")) float BladeTrailLifetime=.28f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Blade trail") FLinearColor BladeTrailColor=FLinearColor(1.f,.7f,.82f,.65f);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Travel trail") bool bEnableTravelTrail=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Travel trail") TObjectPtr<UMaterialInterface> TravelTrailMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Travel trail",meta=(ClampMin=".03",ClampMax="1",Units="s")) float TravelTrailLifetime=.16f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Travel trail",meta=(ClampMin=".05",ClampMax="1")) float TravelTrailWidth=.3f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Travel trail") FLinearColor TravelTrailColor=FLinearColor(1.f,.62f,.84f,.38f);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Attack highlight") TObjectPtr<UStaticMesh> AttackHighlightMesh;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Attack highlight") TObjectPtr<UMaterialInterface> AttackHighlightMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Attack highlight",meta=(ClampMin="2",Units="cm")) float AttackHighlightRadius=14.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Attack highlight") FLinearColor AttackHighlightColor=FLinearColor(1.f,.24f,.52f,1.f);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Attack impact") TObjectPtr<UNiagaraSystem> AttackImpactBurst;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Attack impact") TObjectPtr<UNiagaraSystem> AttackImpactRing;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Attack impact",meta=(ClampMin="0",ClampMax="3")) float AttackImpactScale=1.f;
 /** Sword throws return after the embedded hold; flight endpoints stay frozen. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sword throw",meta=(ClampMin="0",ClampMax="1")) float SwordReturnStart=.77f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sword throw",meta=(ClampMin="0",Units="cm")) float SwordEmbedDepth=3.f;
 /** Second Beat: draw backward along the embedded blade before curling toward 10 o'clock. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sword throw",meta=(ClampMin="1",Units="cm")) float SwordRecoilDistance=65.f;
 /** Center travel of the actual diagonal cut; the blade adds its own physical reach. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sword cut",meta=(ClampMin="20",Units="cm")) float SwordSwingSpan=140.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Blade hit",meta=(ClampMin="1",ClampMax="20",Units="cm")) float BladeHitRadius=5.f;
 /** Independent weapon curves and upper-body animation while supported by Bunniar. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Glide attacks",meta=(TitleProperty="Sequence")) TArray<FBBSBWeaponAnimationClip> GlideClips;
 const FBBSBWeaponAnimationClip* FindClip(EBBSBFieldGesture Gesture,int32 Channel,int32 Combo,bool bGliding=false) const;
 /** Read ordinary absolute transform curves directly so hit sweeps and visible motion share one source. */
 static bool SampleTransform(const ULevelSequence* Sequence,FName Binding,float Alpha,FTransform& Out);
 UFUNCTION(BlueprintPure, Category="Animation") FTransform SampleClip(int32 ClipIndex,float NormalizedTime) const;
 UFUNCTION(BlueprintPure,Category="Animation") FTransform SampleGlideClip(int32 ClipIndex,float NormalizedTime) const;
};

/** Place in the animation studio. Sequencer controls the named components directly. */
UCLASS()
class BBSB_API ABBSBWeaponAnimationPreview : public AActor
{
 GENERATED_BODY()
public:
 ABBSBWeaponAnimationPreview();
 virtual void OnConstruction(const FTransform& Transform) override;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TObjectPtr<UBBSBWeaponAnimationSet> AnimationSet;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") int32 ClipIndex=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Animation") bool bPreviewGlideAttack=false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation", meta=(ClampMin="0",ClampMax="1")) float PreviewTime=1;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parts") TObjectPtr<USceneComponent> Grip;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parts") TObjectPtr<USkeletalMeshComponent> CharacterBody;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parts") TObjectPtr<USceneComponent> OtherGrip;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parts") TObjectPtr<USceneComponent> HitAnchor;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parts") TObjectPtr<USceneComponent> WeaponMotion;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parts") TObjectPtr<USceneComponent> ArmAnchor;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parts") TObjectPtr<USceneComponent> ArmMotion;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parts") TObjectPtr<UStaticMeshComponent> WeaponModel;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parts") TObjectPtr<UStaticMeshComponent> Full;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parts") TObjectPtr<UStaticMeshComponent> Upper;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parts") TObjectPtr<UStaticMeshComponent> Lower;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Parts") TObjectPtr<UStaticMeshComponent> OtherLower;
 UFUNCTION(CallInEditor, BlueprintCallable, Category="Animation") void PreviewForm();
 UFUNCTION(CallInEditor, BlueprintCallable, Category="Animation") void PreviewAction();
};
