#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Field/BBSBFieldGesture.h"
#include "BBSBRepairTool.generated.h"
class ABBSBCharacter;class UStaticMeshComponent;class UAnimSequence;
/** The board and rubber mallet are an item behaviour; the conductor remains in Reparado's hand. */
UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBRepairToolComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UBBSBRepairToolComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function) override;
 UFUNCTION(BlueprintPure) bool IsRepairToolEquipped() const;
 bool Gesture(EBBSBFieldGesture Gesture,int32 Channel,bool Initial);
 void ReleaseBoard();
 void GetBodyPose(UAnimSequence*& Animation,float& Time,float& Weight) const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Repair tool",meta=(ClampMin="1")) float Damage=22;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Repair tool") float DirectRecovery=.80f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Repair tool") float BoardBeatSeconds=.50f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Repair tool") float BoardTimingWindow=.16f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Repair tool") float Reach=1000;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Repair tool") float HitRadius=150;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Repair tool") TSoftObjectPtr<UAnimSequence> StrikeAnimation;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly) bool bBoardPlaced=false;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly) int32 BoardHits=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly) int32 HitsLanded=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly) FString Feedback;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly) FVector Target;
 UFUNCTION(BlueprintPure) float NextBeatProgress() const;
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Board;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Handle;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Head;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> Clip;
 double HitAt=-100,StartedAt=-100,ReadyAt=-100,BoardEpoch=-100,LastBoardBeat=-100;
 bool bStrikeBoard=false,bHitApplied=true;
 FVector From;
 void EnsureVisuals();
 FVector Aim() const;
 void ApplyHit();
};
