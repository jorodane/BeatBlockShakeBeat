#pragma once

#include "Combat/BBSBCombat.h"
#include "BBSBFieldAttack.generated.h"

class UInstancedStaticMeshComponent;

/** Free-field shapes share bullet damage grace, reset and aim-ignore rules with ink pellets. */
UCLASS()
class BBSB_API ABBSBFieldAttack : public ABBSBInkProjectile
{
 GENERATED_BODY()
public:
 ABBSBFieldAttack();
 virtual void Tick(float DT) override;
 UFUNCTION(BlueprintCallable,Category="Field attack") void InitializeField(ABBSBInkEnemy* Enemy,EBBSBFieldAttackKind Kind,FVector Target,float Delay=0.f);
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Field attack") EBBSBFieldAttackKind Attack=EBBSBFieldAttackKind::GroundSpikes;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field attack",meta=(ClampMin=".1",Units="s")) float WarningSeconds=.7f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field attack",meta=(ClampMin=".05",Units="s")) float ActiveSeconds=.4f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field attack",meta=(ClampMin="1",Units="cm")) float AttackRadius=220.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field attack",meta=(ClampMin="1",Units="cm")) float AttackThickness=26.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field attack",meta=(ClampMin="1",Units="cm")) float AttackHeight=180.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field attack",meta=(ClampMin="100",Units="cm/s")) float AttackSpeed=720.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field attack",meta=(ClampMin=".2",Units="s")) float FlightSeconds=1.3f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field attack",meta=(ClampMin="0",Units="cm")) float ArcHeight=540.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field attack",meta=(ClampMin=".05",Units="s")) float SpikeStepSeconds=.14f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Field attack") TArray<FVector> SpikePoints;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Field attack") bool bDamageActive=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Field attack") int32 Contacts=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Field attack") FVector ImpactPoint=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> Telegraph;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> Energy;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> Stones;
private:
 bool bInitialized=false;
 float LaunchDelay=0.f,TotalSeconds=1.f;
 FVector Forward=FVector::ForwardVector,Right=FVector::RightVector,PreviousHurt=FVector::ZeroVector;
 TSet<int32> ContactedShapes;
 int32 MarkerCount=0,EnergyCount=0,StoneCount=0;
 FCollisionQueryParams WorldQuery() const;
 bool FloorAt(const FVector& Point,FVector& Floor) const;
 FVector ShellAt(float Time) const;
 FVector WavePoint(float Time,int32 Segment) const;
 bool ContactAt(float Before,float Now,const FVector& Hurt,float HurtRadius,int32& Shape,FVector& Contact) const;
 void RenderShape();
 void Instance(UInstancedStaticMeshComponent* Mesh,int32& Count,const FVector& Point,const FVector& Size,FRotator Rotation=FRotator::ZeroRotator);
 void Ring(UInstancedStaticMeshComponent* Mesh,int32& Count,const FVector& Point,float Radius,float Width,float Height,int32 Segments=48);
 void Trim(UInstancedStaticMeshComponent* Mesh,int32 Count);
};
