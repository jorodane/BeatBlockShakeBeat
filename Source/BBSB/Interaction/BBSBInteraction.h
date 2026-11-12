#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/MovementComponent.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "BBSBInteraction.generated.h"

class ABBSBCharacter;
class ACharacter;
class ACameraActor;
class AHUD;
class UCanvas;
class USkeletalMeshComponent;
class UPrimitiveComponent;
class USphereComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class UAnimSequence;
class UTexture2D;

UENUM(BlueprintType)
enum class EBBSBDialogueSpeaker : uint8
{
 Partner,
 Reparado,
 Bunniar
};

UENUM(BlueprintType)
enum class EBBSBDialogueExpression : uint8
{
 Neutral UMETA(DisplayName="기본"),
 Warm UMETA(DisplayName="미소"),
 Annoyed UMETA(DisplayName="불만"),
 Surprised UMETA(DisplayName="놀람")
};

/** One editable turn. Portraits and names follow the speaker rather than parsing the displayed sentence. */
USTRUCT(BlueprintType)
struct BBSB_API FBBSBDialogueLine
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue") EBBSBDialogueSpeaker Speaker=EBBSBDialogueSpeaker::Partner;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue") EBBSBDialogueExpression Expression=EBBSBDialogueExpression::Neutral;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue",meta=(MultiLine=true)) FText Text;
};

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
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Dialogue|Presentation") bool bDialogueClosing=false;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Dialogue|Presentation") float DialogueCloseElapsed=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") TObjectPtr<class ABBSBInteractionNPC> ActiveNPC;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") int32 LineIndex=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") EBBSBDialogueSpeaker ActiveSpeaker=EBBSBDialogueSpeaker::Partner;
 /** X: Reparado, Y: Bunniar, Z: partner. Smoothly brighten the current speaker. */
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") FVector PortraitBrightness=FVector(.4f,.4f,1.f);
 /** Partner, then Bunniar, then Reparado rise in; the balloon follows the final arrival. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Presentation",meta=(ClampMin=".05",ClampMax="1",Units="s")) float PortraitRiseSeconds=.26f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Presentation",meta=(ClampMin=".03",ClampMax=".4",Units="s")) float PortraitSettleSeconds=.12f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Presentation",meta=(ClampMin="0",ClampMax="40")) float PortraitOvershootPixels=14.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Presentation",meta=(ClampMin="0",ClampMax=".3",Units="s")) float PortraitStaggerSeconds=.065f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Presentation",meta=(ClampMin=".05",ClampMax=".5",Units="s")) float BubbleGrowSeconds=.18f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Presentation",meta=(ClampMin=".03",ClampMax=".4",Units="s")) float BubbleCloseSeconds=.12f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Presentation",meta=(ClampMin=".08",ClampMax=".6",Units="s")) float PortraitExitSeconds=.23f;
 /** Logical pixels at 1280x720; a listener sits slightly below the speaking pose. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Presentation",meta=(ClampMin="0",ClampMax="50")) float InactivePortraitDrop=18.f;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Dialogue|Presentation") float PresentationElapsed=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Dialogue|Presentation") float BubbleRevealElapsed=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Dialogue|Presentation") FVector PortraitLowering=FVector(1,1,0);
 UFUNCTION(BlueprintPure,Category="Dialogue|Presentation") float PortraitEntranceProgress(EBBSBDialogueSpeaker Speaker) const;
 UFUNCTION(BlueprintPure,Category="Dialogue|Presentation") float PortraitOvershootOffset(EBBSBDialogueSpeaker Speaker) const;
 UFUNCTION(BlueprintPure,Category="Dialogue|Presentation") float PortraitExitProgress(EBBSBDialogueSpeaker Speaker) const;
 UFUNCTION(BlueprintPure,Category="Dialogue|Presentation") float BubbleRevealProgress() const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Presentation",meta=(ClampMin=".25",ClampMax="1")) float AttachedBunniarPortraitScale=.66f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Presentation",meta=(ClampMin=".03",ClampMax=".3",Units="s")) float BunniarPortraitFlipSeconds=.10f;
 /** -1: mirrored, +1: normal; passes edge-on through zero during a quick turn. */
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Dialogue|Presentation") float BunniarPortraitFacing=1.f;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Dialogue|Presentation") bool bBunniarPortraitAttached=false;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Dialogue|Presentation") float BunniarPortraitAttachmentAlpha=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Dialogue|Presentation") FVector BunniarPortraitLocalPoint=FVector::ZeroVector;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Dialogue|Presentation") FVector BunniarPortraitLocalNormal=FVector::UpVector;
 /** Attachment centre in the displayed (mirrored) partner portrait's UV space. */
 UFUNCTION(BlueprintPure,Category="Dialogue|Presentation") FVector2D BunniarAttachmentPortraitUV() const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Portraits") TObjectPtr<UTexture2D> ReparadoPortrait;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Portraits") TObjectPtr<UTexture2D> BunniarPortrait;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Portraits") TObjectPtr<UTexture2D> DefaultPartnerPortrait;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Portraits") TMap<EBBSBDialogueExpression,TObjectPtr<UTexture2D>> ReparadoExpressions;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Portraits") TMap<EBBSBDialogueExpression,TObjectPtr<UTexture2D>> BunniarExpressions;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Portraits") TMap<EBBSBDialogueExpression,TObjectPtr<UTexture2D>> DefaultPartnerExpressions;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") EBBSBDialogueExpression ReparadoExpression=EBBSBDialogueExpression::Neutral;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") EBBSBDialogueExpression BunniarExpression=EBBSBDialogueExpression::Neutral;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Dialogue") EBBSBDialogueExpression PartnerExpression=EBBSBDialogueExpression::Neutral;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Camera") float CameraDistance=230;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Camera") float CameraSideOffset=100;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Camera") float CameraFOV=45;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Camera") float CameraBlendSeconds=.3f;
 /** Conversation lenses remain near eye level, including counter-side scenes. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Camera",meta=(ClampMin="0",ClampMax="12",Units="deg")) float CameraMaximumPitch=12.f;
 UFUNCTION(BlueprintCallable,Category="Dialogue") bool BeginDialogue(ABBSBInteractionNPC* NPC);
 UFUNCTION(BlueprintCallable,Category="Dialogue") void AdvanceDialogue();
 /** Normal player close: play the reversed presentation, then release ownership. */
 UFUNCTION(BlueprintCallable,Category="Dialogue") void RequestDialogueClose();
 /** Immediate cleanup for loads, combat/cinematic handoffs, and owner destruction. */
 UFUNCTION(BlueprintCallable,Category="Dialogue") void EndDialogue();
 UFUNCTION(BlueprintPure,Category="Dialogue") FString DisplayedLine() const;
 UFUNCTION(BlueprintPure,Category="Dialogue") EBBSBDialogueSpeaker CurrentSpeaker() const;
 UFUNCTION(BlueprintPure,Category="Dialogue") EBBSBDialogueExpression CurrentExpression() const;
 UFUNCTION(BlueprintPure,Category="Dialogue") EBBSBDialogueExpression ExpressionForSpeaker(EBBSBDialogueSpeaker Speaker) const;
 UFUNCTION(BlueprintPure,Category="Dialogue") FText DisplayedSpeakerName() const;
 UFUNCTION(BlueprintPure,Category="Dialogue") UTexture2D* PortraitForSpeaker(EBBSBDialogueSpeaker Speaker) const;
 void DrawDialogue(AHUD* HUD,UCanvas* Canvas) const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Quick interaction") float QuickAimRadius=160;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Quick interaction") float QuickAimDegrees=8;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Quick interaction") float LockedFrontRange=600;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Quick interaction") float LockedFrontDegrees=65;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Quick interaction",meta=(ClampMin="100",ClampMax="600",Units="cm")) float NearbyFrontRange=320;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Quick interaction",meta=(ClampMin="20",ClampMax="85",Units="deg")) float NearbyFrontDegrees=75;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Quick interaction") TObjectPtr<AActor> QuickFocusActor;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Quick interaction") FText QuickFocusText;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Quick interaction") FText QuickFocusName;
 UFUNCTION(BlueprintPure,Category="Quick interaction") bool IsQuickInteractable(AActor* Actor) const;
 UFUNCTION(BlueprintPure,Category="Quick interaction") FText QuickInteractionText(AActor* Actor) const;
 UFUNCTION(BlueprintPure,Category="Quick interaction") FText QuickInteractionName(AActor* Actor) const;
 UFUNCTION(BlueprintCallable,Category="Quick interaction") bool FindQuickInteraction(FHitResult& Hit) const;
 void UpdateQuickInteraction();
 void RegisterInteractionCandidate(AActor* Actor);
 void DrawQuickInteraction(AHUD* HUD,UCanvas* Canvas) const;
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<ACameraActor> DialogueCamera;
 UPROPERTY(Transient) TObjectPtr<UInputComponent> Controls;
 TWeakObjectPtr<AActor> PreviousViewTarget;
 bool bOwnMoveIgnore=false,bOwnLookIgnore=false,bCameraReturning=false;
 TSet<TWeakObjectPtr<AActor>> InteractionCandidates;
 FDelegateHandle SpawnedHandle;
 double NextQuickScan=0;
 FVector CameraPlayerAnchor=FVector::ZeroVector,CameraPartnerAnchor=FVector::ZeroVector;
 float FramingAspect=0;
 bool bFramingReady=false;

 void BindControls();
 void UpdateCamera();
 void UpdateSpeaker(float DT,bool bImmediate=false);
 void UpdatePresentation(float DT,bool bImmediate,bool bNewSpeaker);
 void UpdateBunniarPortraitAttachment(float DT,bool bImmediate);
 TWeakObjectPtr<UPrimitiveComponent> BunniarPortraitSurface;
 float BunniarFlipElapsed=0,BunniarFlipFrom=1.f,BunniarFlipTo=1.f;
 float PresentationIntroSeconds() const;
 float DialogueCloseLeadSeconds() const;
 float DialogueCloseDuration() const;
 float ClosingBubbleFrom=1.f;
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
 /** Legacy dialogue is still supported when Conversation is empty. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue",meta=(MultiLine=true)) TArray<FText> Lines;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue") TArray<FBBSBDialogueLine> Conversation;
 /** Optional illustration for this NPC. Empty uses the default partner portrait. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue") TObjectPtr<UTexture2D> Portrait;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue") TMap<EBBSBDialogueExpression,TObjectPtr<UTexture2D>> PortraitExpressions;
 /** Anchors are authored on the original right-facing art, before display mirroring. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Portrait attachment") FVector2D PortraitHeadAnchor=FVector2D(.53,.23);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Portrait attachment",meta=(ClampMin=".01",ClampMax=".4")) float PortraitHeadRadius=.155f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue|Portrait attachment",meta=(ClampMin="-180",ClampMax="180",Units="deg")) float PortraitViewYaw=50.f;
 UFUNCTION(BlueprintPure,Category="Dialogue") int32 DialogueLineCount() const;
 UFUNCTION(BlueprintPure,Category="Dialogue") FBBSBDialogueLine DialogueLineAt(int32 Index) const;
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
 void ResolveAttachment(UPrimitiveComponent*& Component,FVector& Point,FVector& Normal,const FVector& LaunchPoint) const;
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
 UFUNCTION(BlueprintPure,Category="Carry") bool CanBeCarriedBy(const ACharacter* Player) const;
 /** A step crate becomes a solid platform only after its support and motion stay stable. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Step crate",meta=(ClampMin=".1",Units="s")) float StepSettleSeconds=.35f;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Step crate") float StepSupportedSeconds=0;
 UFUNCTION(BlueprintPure,Category="Step crate") bool HasStableStepSupport() const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Puzzle safety") bool bEnablePuzzleRecovery=true;
 /** Step crates opt in by tag; other physical props keep their normal behaviour. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Puzzle safety") bool bRecoverIfLost=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Puzzle safety",meta=(ClampMin=".4",Units="s")) float PuzzleRecoveryDelay=2.4f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Puzzle safety",meta=(ClampMin="100",Units="cm")) float PuzzleMaximumFall=500.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Puzzle safety",meta=(ClampMin="500",Units="cm")) float PuzzleMaximumTravel=3000.f;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Puzzle safety") int32 PuzzleRecoveryCount=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Puzzle safety") float PuzzleLostSeconds=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Puzzle safety") FTransform PuzzleRecoveryAnchor;
 UFUNCTION(BlueprintCallable,Category="Puzzle safety") bool TryRecoverPuzzleObject();
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
 double PawnIgnoreUntil=-1,NextRecoveryCheck=0,LastRecoveryCheck=0;
 double NextStepSupportCheck=0;
 void PrepareStepCrateSimulation();
 void RestoreReleasedPawnCollision();
 void TickStepCratePhysics(float DT,const ABBSBCharacter* Player);
 bool IsPuzzleRecoveryEnabled() const;
 void InitializeCarrySafety();
 void TickCarrySafety(float DT,const ABBSBCharacter* Player);
 bool PuzzlePlacementClear(const FTransform& Pose,bool IncludePlayer) const;
 bool PuzzleHasApproach(const FTransform& Pose) const;
 bool PuzzleVisibleToPlayer(const ABBSBCharacter* Player) const;
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
