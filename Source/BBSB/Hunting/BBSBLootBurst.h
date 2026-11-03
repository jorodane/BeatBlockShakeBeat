#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BBSBLootBurst.generated.h"
class UInstancedStaticMeshComponent;

/** A short ink-puff burst, sharing one instanced mesh and no gameplay collision. */
UCLASS()
class BBSB_API ABBSBLootBurst : public AActor
{
 GENERATED_BODY()
public:
 ABBSBLootBurst();
 virtual void Tick(float DT) override;
 void StartBurst(const FLinearColor& Color);
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> Puffs;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Burst",meta=(ClampMin="4",ClampMax="48")) int32 PuffCount=16;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Burst",meta=(ClampMin=".2",ClampMax="3")) float Lifetime=.85f;
private:
 struct FPuff { FVector Velocity;float Radius=10;FQuat Rotation; };
 TArray<FPuff> Particles;
 double StartedAt=0;
};
