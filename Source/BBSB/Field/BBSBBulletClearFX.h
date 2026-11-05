#pragma once
#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "BBSBBulletClearFX.generated.h"

/** Bounded, batched, camera-facing rings and fragments at actual cleared pellets. */
UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBBulletClearFXComponent : public UPrimitiveComponent
{
 GENERATED_BODY()
public:
 UBBSBBulletClearFXComponent();
 UPROPERTY(Transient) TObjectPtr<UMaterialInterface> EffectMaterial;
 UPROPERTY(EditAnywhere,Category="Bullet clear",meta=(ClampMin=".1",ClampMax="1")) float Lifetime=.34f;
 UPROPERTY(EditAnywhere,Category="Bullet clear",meta=(ClampMin="10",ClampMax="150",Units="cm")) float BurstRadius=60.f;
 UPROPERTY(EditAnywhere,Category="Bullet clear",meta=(ClampMin="4",ClampMax="128")) int32 MaximumBursts=64;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Bullet clear") int32 BurstsEmitted=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Bullet clear") FVector LastBurstPoint=FVector::ZeroVector;
 UFUNCTION(BlueprintPure,Category="Bullet clear") int32 ActiveBurstCount() const { return Bursts.Num(); }
 void AddBurst(const FVector& Point,const FLinearColor& Tint);
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function) override;
 virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
 virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
 virtual int32 GetNumMaterials() const override { return 1; }
 virtual UMaterialInterface* GetMaterial(int32 Index) const override { return EffectMaterial; }
 virtual void GetUsedMaterials(TArray<UMaterialInterface*>& Out,bool Debug=false) const override { if(EffectMaterial)Out.Add(EffectMaterial); }
private:
 struct FBurst { FVector Point;FLinearColor Tint;double Born=0; };
 TArray<FBurst> Bursts;
};
