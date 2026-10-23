#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/MovementComponent.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "BBSBInteraction.generated.h"

class ABBSBCharacter;
class ACameraActor;
class AHUD;
class UCanvas;
class USkeletalMeshComponent;
class USphereComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class UAnimSequence;

/** Prototype dialogue: input, camera ownership and text are independent of the held attachment. */
UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBInteractionComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UBBSBInteractionComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") bool bDialogueActive=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") TObjectPtr<class ABBSBInteractionNPC> ActiveNPC;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") int32 LineIndex=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Camera") float CameraDistance=230;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Camera") float CameraSideOffset=100;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Camera") float CameraFOV=45;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Camera") float CameraBlendSeconds=.3f;
 UFUNCTION(BlueprintCallable,Category="Dialogue") bool BeginDialogue(ABBSBInteractionNPC* NPC);
 UFUNCTION(BlueprintCallable,Category="Dialogue") void AdvanceDialogue();
 UFUNCTION(BlueprintCallable,Category="Dialogue") void EndDialogue();
 UFUNCTION(BlueprintPure,Category="Dialogue") FString DisplayedLine() const;
 void DrawDialogue(AHUD* HUD,UCanvas* Canvas) const;
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<ACameraActor> DialogueCamera;
 UPROPERTY(Transient) TObjectPtr<UInputComponent> Controls;
 TWeakObjectPtr<AActor> PreviousViewTarget;
 bool bOwnMoveIgnore=false,bOwnLookIgnore=false;
 void BindControls();
 void UpdateCamera();
};

/** Replace the mesh/idle clip and reaction parameters in Details without changing interaction logic. */
UCLASS(Blueprintable)
class BBSB_API ABBSBInteractionNPC : public AActor,public IBBSBBunniarSurface
{
 GENERATED_BODY()
public:
 ABBSBInteractionNPC();
 virtual void BeginPlay() override;
 virtual void Tick(float DT) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual float TakeDamage(float Amount,const FDamageEvent& Event,AController* Controller,AActor* Causer) override { return 0; }
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UCapsuleComponent> BodyTarget;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Body;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USphereComponent> HeadTarget;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USphereComponent> FaceTarget;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue") FText DisplayName;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue") TArray<FText> Lines;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue") bool bCaravanMember=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Animation") TObjectPtr<UAnimSequence> IdleAnimation;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Animation") float FamiliarRecoilDegrees=8;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Animation") float StrangerRecoilDegrees=18;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Bunniar|Contact") float HeadAttachmentHeight=16;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Bunniar|Contact") float FaceAttachmentForward=13.5f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") bool bBunniarAttached=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") bool bFaceCovered=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") int32 AttachReactions=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") int32 ReleaseReactions=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Animation") FRotator HeadReaction;
 UFUNCTION(BlueprintPure,Category="Dialogue") FVector HeadPoint() const;
 void ResolveAttachment(UPrimitiveComponent*& Component,FVector& Point,FVector& Normal) const;
 virtual void BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector Point,FVector Normal) override;
 virtual void BunniarArrived_Implementation(ABBSBBunniarCompanion* Buddy) override;
 virtual void BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy) override;
private:
 TWeakObjectPtr<ABBSBBunniarCompanion> AttachedBuddy;
 double ReactionAt=-100,RecallAt=-100;
 FQuat NeutralHeadRelative=FQuat::Identity;
 bool bHeadFrameReady=false;
};

UCLASS(Transient)
class BBSB_API UBBSBNPCAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};

/** Engine swept movement supplies penetration recovery when a resting physics body is grabbed. */
UCLASS(Transient)
class BBSB_API UBBSBCarryMovementComponent : public UMovementComponent
{
 GENERATED_BODY()
};

/** Kinematic while held, physical after release. Throwing conserves measured recent motion. */
UCLASS(Blueprintable)
class BBSB_API ABBSBCarryObject : public AActor,public IBBSBBunniarSurface
{
 GENERATED_BODY()
public:
 ABBSBCarryObject();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float DT) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual float TakeDamage(float Amount,const FDamageEvent& Event,AController* Controller,AActor* Causer) override;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Object") FVector Size=FVector(32);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Object") FLinearColor Color=FLinearColor(.65f,.4f,.45f);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Object",meta=(ClampMin=".1")) float MassKg=2;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Object",meta=(ClampMin="1")) float MaximumHealth=60;
 /** Health lost per metre/second of normal closing speed. UE centimetres are converted once. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Impact",meta=(ClampMin="0")) float ImpactDamagePerMeterPerSecond=12;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Impact",meta=(ClampMin="0")) float MinimumImpactSpeed=60;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Carry",meta=(ClampMin=".05")) float DragCentimetersPerPixel=1.2f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Carry",meta=(ClampMin="1",Units="cm/s")) float FollowSpeed=1000.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Carry",meta=(ClampMin="1",ClampMax="90")) float WheelRotationDegrees=15;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Carry",meta=(ClampMin=".02",ClampMax=".25")) float VelocitySampleSeconds=.10f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Carry",meta=(ClampMin=".1",ClampMax="1")) float BlockedReleaseFraction=.5f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Object") float Health=60;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Object") bool bBroken=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Carry") bool bCarried=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Carry") bool bManipulating=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Carry") bool bCarryBlocked=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Carry") float BlockedTravel=0;
 /** Camera-to-object distance captured on the initial click or after a gentle right release. */
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Carry") float HoldDistance=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Carry") FVector CarryVelocity=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Carry") FVector LastThrowVelocity=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Impact") float LastImpactSpeed=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Impact") float LastImpactDamage=0;
 UFUNCTION(BlueprintCallable,Category="Object") void ResetObject();
 UFUNCTION(BlueprintPure,Category="Impact") float DamageForSpeed(float CentimetersPerSecond) const;
 bool BeginManipulation();
 void EndManipulation(bool bFlick);
 void MoveFromPointer(FVector2D Delta);
 void RotateFromWheel(float Steps);
 virtual void BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector Point,FVector Normal) override;
 virtual void BunniarArrived_Implementation(ABBSBBunniarCompanion* Buddy) override;
 virtual void BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy) override;
private:
 UPROPERTY(Transient) TObjectPtr<UBBSBCarryMovementComponent> HeldMovement;
 TWeakObjectPtr<ABBSBBunniarCompanion> AttachedBuddy;
 TWeakObjectPtr<ABBSBCharacter> CarryOwner;
 FVector CameraForward,CameraRight,PreviousPlayerPosition,PrePhysicsVelocity;
 FVector ManipulationGoal,BlockedPlayerAnchor,BlockedObjectAnchor;
 FVector CameraLocalHoldOffset=FVector::ZeroVector;
 FVector OrbitAxis=FVector::UpVector;
 float CarryReachLimit=0;
 ECollisionResponse NormalPawnResponse=ECR_Block,NormalCameraResponse=ECR_Block;
 bool bOwnBodyMoveIgnore=false,bOwnPlayerMoveIgnore=false;
 FRotator FixedView;
 FTransform Home;
 struct FMotionSample { double Time; FVector Position; };
 TArray<FMotionSample> Motion;
 TMap<TWeakObjectPtr<UPrimitiveComponent>,double> Contacts;
 double PawnIgnoreUntil=-1;
 bool GetCarryView(FVector& Location,FRotator& Rotation) const;
 bool GetCarryAim(FVector& Location,FRotator& Rotation,FVector& Direction) const;
 void CaptureHoldAnchor(bool bKeepPosition);
 void MoveHeld(FVector Desired,float DT);
 FVector OrbitStep(FVector Target,FVector Center,float MaximumDistance);
 void UpdateCarryObstruction(bool bBlocked,ABBSBCharacter* Player);
 void ReleaseCarryIgnores();
 void SampleMotion();
 FVector ReleaseVelocity();
 void Drop(bool bThrow);
 void BreakObject();
 UFUNCTION() void OnBodyHit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
};
