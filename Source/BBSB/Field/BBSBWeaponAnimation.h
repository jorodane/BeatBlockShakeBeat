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
