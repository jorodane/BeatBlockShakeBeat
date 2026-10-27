#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "BBSBBladeTrail.generated.h"

/** A short world-space strip through the actual blade endpoints, without spline smoothing. */
UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBBladeTrailComponent : public UPrimitiveComponent
{
 GENERATED_BODY()
public:
 UBBSBBladeTrailComponent();
 UPROPERTY(Transient) TObjectPtr<UMaterialInterface> TrailMaterial;
 float Lifetime=.28f;
 FLinearColor Color=FLinearColor(1.f,.7f,.82f,.65f);
 void AddBladeSample(const FVector& Root,const FVector& Tip);
 void Finish();
 UFUNCTION(BlueprintPure,Category="Blade trail") int32 GetSampleCount() const { return Samples.Num(); }
 UFUNCTION(BlueprintPure,Category="Blade trail") FVector GetOldestRoot() const { return Samples.IsEmpty() ? FVector::ZeroVector:Samples[0].Root; }
 UFUNCTION(BlueprintPure,Category="Blade trail") FVector GetOldestTip() const { return Samples.IsEmpty() ? FVector::ZeroVector:Samples[0].Tip; }
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
 virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
 virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
 virtual int32 GetNumMaterials() const override { return 1; }
 virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override { return TrailMaterial; }
 virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials,bool bGetDebugMaterials=false) const override { if(TrailMaterial) OutMaterials.Add(TrailMaterial); }
private:
 struct FSample { FVector Root,Tip;double Born=0; };
 TArray<FSample> Samples;
 bool bFinished=false;
};
