#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "BBSBDefensePresentation.generated.h"

class UAnimSequence;
class ULevelSequence;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FBBSBDefenseClip
{
 GENERATED_BODY()
 // Bits: up=1, right=2, down=4, left=8. Every nonempty combination is editable.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1",ClampMax="15")) int32 Directions=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Animation;
};

UCLASS(BlueprintType)
class BBSB_API UBBSBDefensePresentation : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Motion") TArray<FBBSBDefenseClip> Parries;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Motion") TObjectPtr<UAnimSequence> HitReaction;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Authoring") TArray<TObjectPtr<ULevelSequence>> EditableSources;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Feedback") FLinearColor BlockColor=FLinearColor(.98f,.79f,.9f);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Feedback") FLinearColor HitColor=FLinearColor(.56f,.24f,.36f);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Feedback",meta=(ClampMin=".1",ClampMax="2")) float EffectScale=1.f;
 UAnimSequence* FindParry(uint8 Mask) const;
};

/** Short-lived world-space sparks/ink. No debug lines, collision or persistent level actors. */
UCLASS(NotBlueprintable)
class BBSB_API ABBSBDefenseImpactEffect : public AActor
{
 GENERATED_BODY()
public:
 ABBSBDefenseImpactEffect();
 virtual void Tick(float DT) override;
 void Initialize(const FVector& Point,const FVector& Normal,bool bBlock,const FLinearColor& Color,float Scale);
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bBlockEffect=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float Age=0;
private:
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Pieces;
 FVector Across=FVector::RightVector,Up=FVector::UpVector,Out=FVector::ForwardVector;
 float EffectScale=1.f;
};
