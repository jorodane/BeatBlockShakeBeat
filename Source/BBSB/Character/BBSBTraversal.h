#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "BBSBTraversal.generated.h"

class ABBSBCharacter;
class ABBSBBunniarCompanion;
class UAnimSequence;
class UBlendSpace;
class UPoseSearchDatabase;
class ULevelSequence;
class UNiagaraSystem;
class UNiagaraComponent;
class USplineMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;
struct FMinimalViewInfo;

UENUM(BlueprintType)
enum class EBBSBWireArrival : uint8 { Air, Floor, Wall };

UENUM(BlueprintType)
enum class EBBSBParkourMode : uint8 { None, LedgeHang, Mantle, Vault, WallSlide };

/** Linked Sequencer clips and the four traversal effects, editable without changing code. */
UCLASS(BlueprintType)
class BBSB_API UBBSBTraversalPresentation : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement motion") TObjectPtr<UBlendSpace> GroundDashBlendSpace;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement motion") TObjectPtr<UPoseSearchDatabase> GroundLocomotionDatabase;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement motion") TObjectPtr<UPoseSearchDatabase> GroundDashDatabase;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement motion") bool bUseDashMotionMatching=true;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="Authoring") bool RebuildGroundDashBlendSpace();
 UFUNCTION(BlueprintPure,Category="Authoring") TArray<float> GroundDashSampleWeights(float Angle) const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Motion") TObjectPtr<UAnimSequence> DoubleJump;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Motion") TObjectPtr<UAnimSequence> AirDash;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Motion") TObjectPtr<UAnimSequence> WirePull;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Motion") TObjectPtr<UAnimSequence> WireLanding;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Motion") TObjectPtr<UAnimSequence> WireWallLatch;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour motion") TObjectPtr<UAnimSequence> LedgeHang;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour motion") TObjectPtr<UAnimSequence> LedgeClimb;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour motion") TObjectPtr<UAnimSequence> LowVault;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour motion") TObjectPtr<UAnimSequence> WallSlide;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour motion") TObjectPtr<UAnimSequence> WallJump;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Weapon motion") TArray<TObjectPtr<UAnimSequence>> GroundWeaponCombo;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Weapon motion") TArray<TObjectPtr<UAnimSequence>> AirWeaponCombo;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects") TObjectPtr<UNiagaraSystem> WireLandingSmoke;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects") TObjectPtr<UNiagaraSystem> WireWallSmoke;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects") TObjectPtr<UMaterialInterface> WireMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Authoring") TArray<TObjectPtr<ULevelSequence>> EditableSources;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects") TObjectPtr<UNiagaraSystem> JumpSmoke;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects") TObjectPtr<UNiagaraSystem> LandingSmoke;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects") TObjectPtr<UNiagaraSystem> DoubleJumpVortex;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects") TObjectPtr<UNiagaraSystem> AirDashDrill;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Running") TObjectPtr<UNiagaraSystem> RunFootstepSmoke;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Running") FName LeftFootSocket=TEXT("foot_l");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Running") FName RightFootSocket=TEXT("foot_r");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Running",meta=(ClampMin="50",Units="cm/s")) float RunSmokeMinimumSpeed=180.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Running",meta=(ClampMin="25",Units="cm")) float FootstepSpacing=90.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Dash") TObjectPtr<UNiagaraSystem> DashBurst;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Dash") TObjectPtr<UNiagaraSystem> DashWake;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Dash",meta=(ClampMin=".025",Units="s")) float DashWakeInterval=.055f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Wall") TObjectPtr<UNiagaraSystem> WallGrabBurst;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Wall") TObjectPtr<UNiagaraSystem> WallSlideSmoke;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects|Wall",meta=(ClampMin=".05",Units="s")) float WallSlideSmokeInterval=.12f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Effects",meta=(ClampMin="0",ClampMax="3")) float EffectScale=1.f;
};

/** Ground jump, one airborne lift, and a falling-mode glide using the existing companion. */
UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBTraversalComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UBBSBTraversalComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function) override;
 UFUNCTION(BlueprintCallable,Category="BBSB|Traversal") bool TryJump();
 UFUNCTION(BlueprintCallable,Category="BBSB|Traversal") bool TryGlideDash();
 bool PrepareSurfaceAttachment();
 UFUNCTION(BlueprintPure,Category="BBSB|Parkour") bool IsParkourActive() const { return ParkourMode!=EBBSBParkourMode::None; }
 UFUNCTION(BlueprintCallable,Category="BBSB|Parkour") bool TryLowVault();
 UFUNCTION(BlueprintCallable,Category="BBSB|Parkour") bool TryLedgeClimb();
 UFUNCTION(BlueprintCallable,Category="BBSB|Parkour") bool TryWallJump();
 bool IsWallJumpSteeringLocked() const;
 UFUNCTION(BlueprintCallable,Category="BBSB|Parkour") bool DropParkour();
 void CancelParkour();
 float ParkourMotionPhase() const { return FMath::Clamp(ParkourAge/FMath::Max(.1f,ParkourDuration),0.f,1.f); }
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="6",ClampMax="100",Units="cm")) float ParkourJumpClearance=18.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="0",ClampMax="0.3",Units="s")) float LedgePopPreparationSeconds=.04f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="0.5",ClampMax="2")) float ParkourPaceScale=1.f;
 FVector LedgeHand(bool Left) const;
 void ApplyClimbCamera(float DeltaTime,FMinimalViewInfo& View);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="0",ClampMax="1")) float ClimbCameraFollow=.8f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="3",ClampMax="40")) float ClimbCameraBlendSpeed=18.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="0",ClampMax="30",Units="cm")) float ClimbFootHeight=12.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parkour") float ClimbFootAlpha=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parkour") FVector ClimbFootPoint=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parkour") EBBSBParkourMode ParkourMode=EBBSBParkourMode::None;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parkour") EBBSBParkourMode ParkourVisualMode=EBBSBParkourMode::None;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parkour") FVector LedgePoint=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parkour") FVector LedgeNormal=FVector::ForwardVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parkour") float ParkourAge=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parkour") float ParkourPoseAlpha=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parkour") float ParkourHandAlpha=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Parkour") float WallJumpAge=-1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour") bool bAutoCatchLedges=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour") bool bAutoWallSlide=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour") bool bPredictRisingMantle=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="40",ClampMax="150",Units="cm")) float LowVaultMaximumHeight=110.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="40",ClampMax="200",Units="cm")) float LowVaultMaximumDepth=140.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="0.3",Units="s")) float LowVaultSeconds=.70f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="0.3",Units="s")) float LedgeClimbSeconds=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="30",ClampMax="100",Units="cm")) float LedgeReachAbove=80.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="35",ClampMax="80",Units="cm")) float LedgeHangBelow=58.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="100",Units="cm/s")) float WallJumpOutSpeed=430.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="100",Units="cm/s")) float WallJumpUpSpeed=520.f;
 /** Let the wall kick clear the surface before directional air control resumes. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="0",ClampMax="0.5",Units="s")) float WallJumpSteeringLockSeconds=.18f;
 /** Preserve the kick even when no movement key is held. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="0",ClampMax="1",Units="s")) float WallJumpMomentumSeconds=.45f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parkour",meta=(ClampMin="0.1",Units="s")) float ParkourRegrabDelay=.45f;
 void BeginAirWeaponHang(float Duration);
 void EndAirWeaponHang();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Air attack") bool bAirWeaponHanging=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Air attack",meta=(ClampMin="0",ClampMax="60",Units="cm")) float AirWeaponLift=18.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Air attack",meta=(ClampMin="0.05",ClampMax="0.4",Units="s")) float AirWeaponLiftSeconds=.16f;
 UFUNCTION(BlueprintCallable,Category="BBSB|Wire") bool TryWireRecall(ABBSBBunniarCompanion* Buddy);
 UFUNCTION(BlueprintCallable,Category="BBSB|Wire") void CancelWireRecall();
 UFUNCTION(BlueprintPure,Category="BBSB|Wire") bool IsWireTraversalActive() const { return bWirePulling || bWireArrival; }
 void UpdateWirePresentation();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Wire") bool bWirePulling=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Wire") bool bWireArrival=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Wire") EBBSBWireArrival WireArrival=EBBSBWireArrival::Air;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Wire") FVector WireDirection=FVector::ForwardVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Wire") FVector WireAnchor=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Wire") float WireMotionAge=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Wire") float WirePoseAlpha=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wire",meta=(ClampMin="100",Units="cm/s")) float WireSpeed=1800.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wire",meta=(ClampMin="100",Units="cm/s^2")) float WireAcceleration=5500.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wire",meta=(ClampMin="0",Units="cm/s")) float WireAirExitSpeed=240.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wire",meta=(ClampMin="0.1",ClampMax="1",Units="s")) float WireArrivalSeconds=.25f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wire",meta=(ClampMin="0.1",ClampMax="2",Units="s")) float WireWallContactSeconds=.5f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wire",meta=(ClampMin="0",ClampMax="1",Units="s")) float WireWallSlideStartsAt=.3f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wire",meta=(ClampMin="0",ClampMax="200",Units="cm/s")) float WireWallSlideSpeed=70.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wire",meta=(ClampMin="0",Units="cm/s")) float DispatchLiftSpeed=220.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="Wire") TArray<TObjectPtr<USplineMeshComponent>> WireSegments;
 UFUNCTION(BlueprintPure,Category="BBSB|Traversal") bool IsBunniarTraversalActive() const { return bGliding || bGlideDashing; }
 UFUNCTION(BlueprintCallable,Category="BBSB|Traversal") void EndGlide();
 void Landed(const FHitResult& Hit);
 UFUNCTION(BlueprintPure,Category="BBSB|Traversal") FVector GlideGrip(bool bLeft) const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump",meta=(ClampMin="100",Units="cm/s")) float AirJumpVelocity=460.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Air motion",meta=(ClampMin="0.04",ClampMax="0.5",Units="s")) float AirAnticipationSeconds=.25f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Air motion",meta=(ClampMin="0",ClampMax="100",Units="cm")) float AirAnticipationDrop=26.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Air motion",meta=(ClampMin="0",ClampMax="150",Units="cm")) float AirDashRise=18.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Air motion",meta=(ClampMin="0",ClampMax="150",Units="cm")) float AirDashDrop=120.f;
 /** Negative yaw is counterclockwise when seen from above. Visual pose only; never turns the camera. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Air motion",meta=(ClampMin="0",ClampMax="720",Units="deg")) float DoubleJumpSpinDegrees=360.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Air motion") float DoubleJumpAge=-1.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Air motion") float DoubleJumpYaw=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Air motion") float GlideHandAlpha=1;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Air motion") float AirDashPoseAlpha=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Air motion") bool bAirLiftPending=false;
 /** Applied only after the apex, preserving normal jump ascent. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump",meta=(ClampMin="1",ClampMax="5")) float FallGravityMultiplier=2.4f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Jump") float BaseGravityScale=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Glide",meta=(ClampMin="50",Units="cm/s")) float GlideSpeed=550.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Glide",meta=(ClampMin="20",Units="cm/s")) float GlideFallSpeed=140.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Glide",meta=(ClampMin="0.01",ClampMax="1")) float GlideGravityScale=.22f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Glide",meta=(ClampMin="0",ClampMax="1")) float GlideAirControl=.85f;
 /** Follows the companion library's normal equipped size. */
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Presentation") float BunniarScale=.175f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation") FVector BunniarOffset=FVector(-5,-17,51);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation") FVector LeftGripOffset=FVector(-5,-17,46);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation") FVector RightGripOffset=FVector(0,8,44);
 /** Optional authored body pose; only the left arm is adjusted to hold Bunniar. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation") TObjectPtr<UAnimSequence> GlideBodyPose;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation") TObjectPtr<UBBSBTraversalPresentation> Presentation;
 UPROPERTY(EditDefaultsOnly,Category="Presentation") TSoftObjectPtr<UBBSBTraversalPresentation> PresentationLibrary;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="Effects") TObjectPtr<UNiagaraComponent> JumpVortexComponent;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="Effects") TObjectPtr<UNiagaraComponent> DashDrillComponent;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation") bool bUseGlideHandIK=true;
 /** Authored GlideBodyPose overrides the default relaxed limb pose. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation") bool bUseRelaxedGlidePose=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation",meta=(ClampMin="0.9",ClampMax="0.999")) float RelaxedLimbExtension=.99f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation",meta=(ClampMin="0",ClampMax="90",Units="deg")) float PullLeanDegrees=82.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation",meta=(ClampMin="0",ClampMax="25",Units="deg")) float TravelLeanDegrees=10.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Presentation",meta=(ClampMin="0",ClampMax="8",Units="deg")) float GlideSwayDegrees=2.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Presentation") FVector2D GlideBodyLean=FVector2D::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Presentation") FVector GlideLeadOffset=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traversal") bool bGliding=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traversal") bool bAirJumpUsed=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traversal") bool bGlideDashing=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traversal") float GlideDashElapsed=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traversal") FVector GlideDashDirection=FVector::ForwardVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traversal") FVector GlideDashTangent=FVector::ForwardVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Air motion",meta=(ClampMin="0",ClampMax="80",Units="cm")) float AirDashRecovery=24.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Glide dash",meta=(ClampMin="50",Units="cm")) float GlideDashDistance=650.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Glide dash",meta=(ClampMin="0.1",Units="s")) float GlideDashSeconds=.60f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Glide dash",meta=(ClampMin="0.01",Units="s")) float GlideDashLeadSeconds=.16f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Glide dash",meta=(ClampMin="10",Units="cm")) float GlideDashLeadDistance=68.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Glide dash",meta=(ClampMin="0")) float GlideDashSpinHz=4.f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Traversal") float GlidePoseAlpha=0;
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Character;
 UPROPERTY(Transient) TObjectPtr<ABBSBBunniarCompanion> Glider;
 float SavedGravity=1,SavedAirControl=.35f;
 float SavedFallingBraking=0,SavedFallingFriction=0;
 bool bDashPhysicsSaved=false;
 double GlideDashStartedAt=0;
 double AirLiftStartedAt=0;
 float AirLiftStartZ=0,GlideDashStartZ=0;
 FVector DashInitialVelocity=FVector::ZeroVector,PreviousDashPosition=FVector::ZeroVector;
 float PreviousDashStep=0,DashDistanceTravelled=0;
 FVector2D GlideLeanVelocity=FVector2D::ZeroVector;
 float GlidePoseTime=0;
 bool bWasFalling=false;
 float LastFallSpeed=0;
 UPROPERTY(Transient) TObjectPtr<ABBSBBunniarCompanion> WireBuddy;
 TWeakObjectPtr<UPrimitiveComponent> WireSurface;
 FVector WireLocalAnchor=FVector::ZeroVector,WireLocalNormal=FVector::UpVector,WireNormal=FVector::UpVector;
 FVector WireDestination=FVector::ZeroVector,WirePreviousPosition=FVector::ZeroVector;
 float WireSavedAirControl=0,WireSavedFlyingBraking=0,WireSavedMaxFlySpeed=0;
 float WirePreviousStep=0,WireCurrentSpeed=0;
 bool bWireSavedOrient=true;
 double WireStartedAt=0,WireArrivalAt=0;
 double AirWeaponStartedAt=0,AirWeaponHangUntil=0;
 float AirWeaponStartZ=0;
 struct FLedgeCandidate { TWeakObjectPtr<UPrimitiveComponent> Surface;FVector Point,Normal,Hang,Stand; };
 bool FindLedge(const FVector& WallPoint,const FVector& Normal,UPrimitiveComponent* Surface,FLedgeCandidate& Out) const;
 bool FindLowLedge(const FVector& WallPoint,const FVector& Normal,UPrimitiveComponent* Surface,FLedgeCandidate& Out) const;
 void BeginLowMantle(const FLedgeCandidate& Ledge);
 bool TryWireLedge();
 bool TryAutoLedge();
 bool BeginWallSlide(const FVector& Point,const FVector& Normal,UPrimitiveComponent* Surface);
 void TickWallSlide(float DeltaTime);
 bool ValidParkourSurface(UPrimitiveComponent* Surface) const;
 bool CapsuleClear(const FVector& Position) const;
 bool CapsulePathClear(const FVector& From,const FVector& To) const;
 bool PathClear(const FVector& From,const FVector& To,float Apex) const;
 bool PlanParkourJump(const FVector& From,const FVector& To,const FVector& Normal,float MinimumApex,float& Apex,float& Seconds) const;
 FVector ParkourJumpPoint(const FVector& From,const FVector& To,const FVector& Normal,float Apex,float Gravity,float Seconds,float Age) const;
 void StartParkourJump();
 void TickParkourJump(float DeltaTime);
 void FinishParkourJump();
 void RestoreParkourRunExit();
 bool bParkourRunExitPending=false;
 float ParkourExitFriction=0,ParkourExitBraking=0,ParkourExitBrakingFactor=0;
 void BeginParkour(EBBSBParkourMode Mode);
 void BeginLedgeHang(const FLedgeCandidate& Ledge);
 void TickParkour(float DeltaTime);
 void RestoreParkourMovement(bool Grounded);
 UPROPERTY(Transient) TWeakObjectPtr<UPrimitiveComponent> ParkourSurface;
 FVector LedgeLocalPoint,LedgeLocalNormal,LedgeLocalStand;
 FVector ParkourFrom,ParkourTo;
 float ParkourApex=0,ParkourDuration=0;
 float ParkourSavedAirControl=0,ParkourSavedFlyingBraking=0,ParkourSavedMaxFlySpeed=0;
 float ParkourSavedFallingBraking=0,ParkourSavedFallingFriction=0;
 bool bParkourJumpPlanned=false,bParkourJumping=false;
 FVector ParkourJumpStart,ParkourEntryVelocity;
 float ParkourJumpGravityScale=1.f;
 float ParkourJumpSeconds=0,ParkourJumpPreparation=0;
 double ParkourJumpStartedAt=0;
 bool bParkourSavedOrient=true;
 double ParkourStartedAt=0,ParkourIgnoreUntil=0,WallJumpStartedAt=-100;
 bool bWallJumpPhysicsSaved=false;
 float WallJumpSavedAirControl=0,WallJumpSavedFallingBraking=0,WallJumpSavedFallingFriction=0;
 void RestoreWallJumpMovement();
 void TickWallJumpMovement();
 float ClimbStartPelvisZ=0,ClimbCameraZ=0;
 uint64 ClimbCameraFrame=MAX_uint64;
 bool bClimbQueued=false;
 void TickWireMovement(float DeltaTime);
 void ArriveFromWire();
 void RestoreWireMovement(bool bGrounded);
 float DashHeightAt(float Age) const;
 void SpawnSmoke(UNiagaraSystem* System,const FVector& Position,const FVector& Normal,float Strength);
 void TickMovementEffects(float DeltaTime);
 bool bFootContact[2]={false,false},bEffectWasDashing=false,bEffectWasOnWall=false;
 float FootstepTravel=0;
 double LastFootstepEffectAt=-100,LastDashWakeAt=-100,LastWallSlideEffectAt=-100;
 int32 NextFootstep=0;
 void StartMotionEffect(UNiagaraComponent* Component,UNiagaraSystem* System);
 bool BeginGlide(bool bApplyLift);
 void ResumeGlideAfterDash();
 void UpdateHangingPresentation(float DeltaTime);
 void ReleaseForFall();
};
