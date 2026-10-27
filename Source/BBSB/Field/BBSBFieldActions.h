#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "BBSBFieldGesture.h"
#include "BBSBWeaponAnimation.h"
#include "Combat/BBSBCombatRules.h"
#include "BBSBFieldActions.generated.h"

class ACharacter;
class UCameraComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UTextRenderComponent;
class UAnimSequence;
class UAnimMontage;
class UPrimitiveComponent;
class UAudioComponent;
class UCanvas;
class USoundWaveProcedural;
class ABBSBWeaponVerification;
class UDecalComponent;
class UMaterialInterface;
class UBBSBBladeTrailComponent;

UENUM(BlueprintType)
enum class EBBSBAimPresentation : uint8 { Hidden, Air, Surface, Interactive };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBBSBFieldGestureEvent, EBBSBFieldGesture, Gesture, int32, MouseChannel, float, Charge);
DECLARE_DELEGATE_RetVal_FourParams(bool, FBBSBEquipmentGesture, EBBSBFieldGesture, int32, float, bool);
DECLARE_DELEGATE_OneParam(FBBSBEquipmentPointerMotion, FVector2D);

UCLASS(ClassGroup=(BBSB), meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBFieldActionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBBSBFieldActionComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Movement", meta=(ClampMin="50")) float RunSpeed = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Movement", meta=(ClampMin="50")) float WalkSpeed = 175.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Dodge", meta=(ClampMin="0.1")) float DodgeCooldown = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Dodge", meta=(ClampMin="0.1")) float DodgeDuration = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Dodge", meta=(ClampMin="0")) float DodgeDistance = 330.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Dodge") float InvulnerableStart = 0.03f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Dodge") float InvulnerableEnd = 0.24f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Gestures", meta=(ClampMin="0.1")) float HoldThreshold = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Gestures", meta=(ClampMin="100")) float FlickSpeed = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Gestures", meta=(ClampMin="5")) float ShakeStrokePixels = 16.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon", meta=(ClampMin="100")) float WeaponReach = 650.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon") float WeaponHitRadius = 36.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon") float BaseDamage = 18.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon") FName WeaponName = "Floating baton";
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BBSB|Weapon") EBBSBPrototypeWeapon PrototypeWeapon=EBBSBPrototypeWeapon::Sword;
	/** Assign a different animation set per weapon; keys and timing are editable without C++. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Animations") TMap<EBBSBPrototypeWeapon,TSoftObjectPtr<UBBSBWeaponAnimationSet>> WeaponAnimationSets;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Animations") TObjectPtr<UBBSBWeaponAnimationSet> ActiveWeaponAnimationSet;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Weapon") float WeaponChargeSeconds=0;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Weapon") int32 SwordComboStep=0;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Weapon") float LastWeaponDamage=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Weapon|Trail") FVector LastBladeTrailRoot=FVector::ZeroVector;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Weapon|Trail") FVector LastBladeTrailTip=FVector::ZeroVector;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Weapon|Trail") bool bBladeTrailEmitting=false;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Weapon") bool bLastWeaponOnBeat=false;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Weapon") TArray<int32> HandAttacks={0,0};
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Weapon") FVector LastWeaponImpulse;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon") float ChargeMaximumSeconds=2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon") float RhythmWindowSeconds=0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon") float SwordComboWindow=1.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon") float WeaponReturnSeconds=0.28f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Interaction") float AimMarkerRadius=55.f;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Interaction") bool bWeaponAimLocked=false;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Interaction") FVector LockedAimPoint;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Interaction") TObjectPtr<AActor> LockedAimActor;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Interaction") bool bSpearEmbedded=false;
	UFUNCTION(BlueprintCallable, Category="BBSB|Weapon") bool SelectPrototypeWeapon(EBBSBPrototypeWeapon Type);
	void NextPrototypeWeapon();
	FString PrototypeWeaponLabel() const;
	/** A puzzle tool occupies the floating weapon slot and shares its gesture recognizers. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Equipment") TObjectPtr<AActor> FieldEquipment;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Equipment") FName FieldEquipmentName;
	FBBSBEquipmentGesture EquipmentGesture;
	UFUNCTION(BlueprintCallable, Category="BBSB|Equipment") bool EquipFieldEquipment(AActor* Equipment,FName Name);
	void ClearFieldEquipment();
	UFUNCTION(BlueprintPure, Category="BBSB|Equipment") FTransform FieldEquipmentRestTransform() const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Equipment",meta=(ClampMin="2",ClampMax="30",Units="cm")) float RestEquipmentGap=6.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Equipment",meta=(ClampMin="70",ClampMax="180",Units="cm")) float RestEquipmentDistance=100.f;
 /** Applies to waiting/returning equipment, after its visual interpolation. */
 bool ResolveRestEquipmentPose(FTransform& Pose,const FBox& LocalBounds,AActor* Equipment,bool bInterpolate=false,bool bWaiting=true) const;
	UFUNCTION(BlueprintPure, Category="BBSB|Interaction") FVector VisualAimPoint() const;
 /** Shared ray, reach and occlusion used by highlighting and Bunniar dispatch. */
 UFUNCTION(BlueprintPure,Category="BBSB|Interaction") bool TraceEquipmentAim(FHitResult& Hit) const;
 /** Always supplies the point on the rendered aim ray; true means a surface is in reach. */
 UFUNCTION(BlueprintPure,Category="BBSB|Interaction") bool ResolveEquipmentAim(FVector& Point,FHitResult& Hit) const;
 bool EquipmentAimRay(FVector& Origin,FVector& Direction) const;
 UFUNCTION(BlueprintPure,Category="BBSB|Interaction") bool IsEquipmentTarget(AActor* Actor) const;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Equipment") TObjectPtr<AActor> BunniarEquipment;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Equipment") TArray<EBBSBPrototypeWeapon> AvailableWeapons={EBBSBPrototypeWeapon::Sword};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Equipment") float EquipmentHoldSeconds=.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Equipment", meta=(ClampMin="0.001",ClampMax="1")) float EquipmentWheelTimeScale=.025f;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Equipment") bool bEquipmentWheelOpen=false;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Equipment") int32 EquipmentWheelIndex=INDEX_NONE;
	/** 0 is Bunniar; combat weapons retain their existing enum IDs. Latest request wins. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Equipment") int32 PendingEquipmentId=INDEX_NONE;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Equipment") bool bEquipmentInputBlocked=false;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Equipment") bool bEquipmentPointerCaptured=false;
	void SetEquipmentPointerCapture(bool bCapture);
	FBBSBEquipmentPointerMotion EquipmentPointerMotion;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Debug") bool bEnablePracticeControls=false;
	UFUNCTION(BlueprintCallable, Category="BBSB|Equipment") bool RequestEquipment(int32 EquipmentId);
	UFUNCTION(BlueprintCallable, Category="BBSB|Equipment") void CancelEquipmentSelection();
	UFUNCTION(BlueprintPure, Category="BBSB|Equipment") bool IsEquipmentActionBusy() const;
	UFUNCTION(BlueprintPure, Category="BBSB|Equipment") FString EquipmentLabel(int32 EquipmentId) const;
	UFUNCTION(BlueprintPure, Category="BBSB|Equipment") int32 EquippedEquipmentId() const;
	static constexpr int32 EquipmentWheelSlotCount=10;
	void DrawEquipmentWheel(AHUD* HUD,UCanvas* Canvas) const;
	double WeaponBeatPosition() const;
	/** Fallback viewport position when the player cannot be projected; normally mirror the player across the crosshair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon", meta=(ClampMin="0.05", ClampMax="0.95")) FVector2D WeaponRestScreenPosition = FVector2D(0.76f,0.68f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon") TArray<FBBSBWeaponActionRule> PrimaryRules;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Weapon") TArray<FBBSBWeaponActionRule> SecondaryRules;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Practice") EBBSBWeaponCommitment WeaponExperiment=EBBSBWeaponCommitment::Authored;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Camera") float FlickSettleSeconds = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Camera") float LookReturnSeconds = 0.22f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Camera",meta=(ClampMin="20",ClampMax="100",Units="deg")) float BunniarZoomFOV=55.f;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Camera") bool bBunniarZoomHeld=false;
 UFUNCTION(BlueprintPure,Category="BBSB|Weapon") bool IsGlideAttacking() const;
 void GetGlideAttackPose(UAnimSequence*& Animation,float& Time,float& Weight) const;
 bool GetWeaponArmPose(FVector& Hand,FVector& Elbow,float& Weight,float& BodyTurn) const;
 void GetWeaponStepPose(UAnimSequence*& Animation,float& Time,float& Weight) const;
 UFUNCTION(BlueprintPure,Category="BBSB|Weapon") float GetWeaponRecoveryRemaining() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Weapon") float GetWeaponActionProgress() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Weapon") bool IsWeaponReturning() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Weapon") bool CanContinueSwordCombo() const;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Action") EBBSBActionState ActiveState = EBBSBActionState::Idle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Action") int32 RejectedActions = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Action") int32 CanceledAttacks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Interaction") TObjectPtr<AActor> HoveredActor;
	UPROPERTY(VisibleAnywhere, Category="BBSB|Diagnostics") FVector LastAimPoint;
	UPROPERTY(VisibleAnywhere, Category="BBSB|Diagnostics") FString LastTraceSummary;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Feedback") bool bShowPracticeHUD = true;
	UPROPERTY(BlueprintAssignable, Category="BBSB|Gestures") FBBSBFieldGestureEvent OnGesture;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Gestures") EBBSBFieldGesture LastGesture = EBBSBFieldGesture::Beat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Gestures") TArray<int32> GestureCounts = {0,0,0,0,0};
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Feedback") int32 PropsBroken = 0;

 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Movement") bool bFaceCameraWhileMoving=true;
 UFUNCTION(BlueprintPure,Category="BBSB|Movement") FVector MovementIntent() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Movement") float GetDodgeAge() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Movement") float GetDodgeVisualAge() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Movement") bool IsDodgeRunExit() const { return bDodgeExitToRun; }
 UFUNCTION(BlueprintPure,Category="BBSB|Movement") float GetDodgeDirectionAngle() const;
 void UpdateMovementFacing();
	UFUNCTION(BlueprintPure, Category="BBSB|Movement") bool IsDodging() const { return bDodging; }
	UFUNCTION(BlueprintPure, Category="BBSB|Movement") bool IsDodgeInvulnerable() const;
	UFUNCTION(BlueprintPure, Category="BBSB|Gestures") bool IsGestureCaptured() const;
	UFUNCTION(BlueprintPure, Category="BBSB|Gestures") bool IsGestureChannelHeld(int32 Channel) const { return Channel>=0 && Channel<2 && Channels[Channel].bHeld; }
	UFUNCTION(BlueprintPure, Category="BBSB|Gestures") FVector2D GestureReleaseDirection(int32 Channel) const;
	UFUNCTION(BlueprintPure, Category="BBSB|Gestures") float GestureHeldSeconds(int32 Channel) const;
	UFUNCTION(BlueprintPure, Category="BBSB|Action") bool CanMove() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Movement") bool CanStartJump() const;
	UFUNCTION(BlueprintPure, Category="BBSB|Action") bool IsConductorAvailable() const;
	UFUNCTION(BlueprintPure, Category="BBSB|Camera") float GetLookInputScale() const;
	bool RequiresMouseGesture(int32 Channel) const;
	void DrawAimFeedback(AHUD* HUD) const;
	void InterruptAction(EBBSBActionState State, float Seconds);
	void ResetActionState();
	/** Refuses busy actions and held gestures; never queues a later preset change. */
	UFUNCTION(BlueprintCallable, Category="BBSB|Practice") bool SetWeaponExperiment(EBBSBWeaponCommitment Preset);
	UFUNCTION(BlueprintPure, Category="BBSB|Weapon") FBBSBWeaponActionRule GetWeaponRule(EBBSBFieldGesture Gesture, int32 Channel=0) const;
	UFUNCTION(BlueprintCallable, Category="BBSB|Movement") bool TryDodge();
	UFUNCTION(BlueprintCallable, Category="BBSB|Practice") void ResetPracticeProps();
	/** Debug hooks share the actual input pipeline, so automated checks exercise gameplay. */
	UFUNCTION(Exec) void FieldGesturePress(int32 Channel = 0);
	UFUNCTION(Exec) void FieldGestureRelease(int32 Channel = 0);
	UFUNCTION(Exec) void FieldGestureMotion(float X, float Y);
	void RecordMoveInput(float Right, float Forward);
 UFUNCTION(BlueprintPure,Category="BBSB|Movement") FVector MovementDashDirection() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Aim") bool IsSurfaceAimLocked() const;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Aim") EBBSBAimPresentation AimPresentation=EBBSBAimPresentation::Air;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Aim") FVector AimPresentationPoint;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Aim") FLinearColor AimPresentationColor;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Aim") TObjectPtr<AActor> AimPresentationActor;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Aim") TObjectPtr<UDecalComponent> SurfaceAimDecal;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Aim") TSoftObjectPtr<UMaterialInterface> SurfaceAimMaterial= TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/Interaction/Materials/M_BunniarAimRing.M_BunniarAimRing")));
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Aim") TSoftObjectPtr<UMaterialInterface> BuddyOutlineMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/Interaction/Materials/M_PP_BunniarOutline.M_PP_BunniarOutline")));
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Aim",meta=(ClampMin="2",Units="cm")) float SurfaceAimRadius=17.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Aim") FLinearColor AirAimColor=FLinearColor(.87f,.83f,.86f,.8f);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Aim") FLinearColor SurfaceAimColor=FLinearColor(.96f,.83f,.90f,1);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Aim") FLinearColor SurfaceDecalColor=FLinearColor(.58f,.32f,.45f,1);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Aim") FLinearColor NPCAimColor=FLinearColor(.98f,.68f,.81f,1);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Aim") FLinearColor CarryAimColor=FLinearColor(.57f,.79f,.98f,1);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Aim") FLinearColor DeviceAimColor=FLinearColor(.60f,.92f,.73f,1);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Aim") FLinearColor AttackAimColor=FLinearColor(.98f,.65f,.54f,1);
 void DrawContextAim(AHUD* HUD,UCanvas* Canvas) const;
	void ExerciseGesture(int32 Index);
	float GetCharge() const;
	float GetDodgeCooldownRemaining() const;
	FString GetMovementLabel() const;
	FString LastHitLabel;
	double GestureTimes[5] = {-100,-100,-100,-100,-100};
	double LastHitAt = -100.0;
	bool bHelpVisible = false;

protected:
	virtual void RegisterComponentTickFunctions(bool bRegister) override;

private:
	friend class ABBSBWeaponVerification;
 friend class ABBSBPlayerWeaponProjectile;
 struct FRestEquipmentPoseState { FVector OwnerOffset=FVector::ZeroVector; double UpdatedAt=-1; };
 mutable TMap<TWeakObjectPtr<AActor>,FRestEquipmentPoseState> RestEquipmentPoses;
	FActorComponentTickFunction WeaponViewTick;
	bool bEquipmentTabHeld=false,bEquipmentTabCancelled=false,bWheelOriginalCursor=false,bWheelOpenedInCombat=false;
	double EquipmentTabPressedAt=0,EquipmentLookResumeAt=0;
	float WheelSavedTimeDilation=1,WheelAppliedTimeDilation=1;
	FVector2D WheelSavedMousePosition=FVector2D::ZeroVector;
	bool bWheelSavedMousePosition=false;
	EBBSBPrototypeWeapon LastCombatWeapon=EBBSBPrototypeWeapon::Sword;
	void EquipmentTabDown(); void EquipmentTabUp();
	void TickEquipmentSelection(); void EnsureBunniarEquipment();
	void OpenEquipmentWheel(); void CloseEquipmentWheel();
	int32 WheelEquipmentId(int32 Index) const;
	struct FWeaponAction
	{
		EBBSBFieldGesture Gesture = EBBSBFieldGesture::Beat;
		int32 Channel = 0;
		float Charge = 0.f;
		float Duration = 0.3f;
		float Elapsed = 0.f;
		double StartedAt = 0;
		uint32 Revision = 0;
		float DamageScale = 1.f;
		FVector OwnerOrigin = FVector::ZeroVector;
		FVector Target = FVector::ZeroVector;
		TWeakObjectPtr<AActor> AimActor;
		FVector AimLocalPoint = FVector::ZeroVector;
		bool bRestrictToAimActor=false;
		FVector Direction = FVector::ForwardVector;
		bool bPrototype=false;
  bool bGlideAttack=false;
  bool bSwordThrow=false;
  bool bSwordTargetLocked=false;
  FQuat SwordThrowFrame=FQuat::Identity;
		float ExactDamage=0;
		FVector Impulse=FVector::ZeroVector;
		FVector PreviousPosition=FVector::ZeroVector;
		bool bHasPrevious=false;
		int32 ComboStep=1;
		FTransform StartPose;
		FBBSBWeaponAnimationClip Motion;
		bool bAuthoredMotion=false;
		TSet<TWeakObjectPtr<AActor>> HitActors;
  TWeakObjectPtr<UBBSBBladeTrailComponent> BladeTrail;
  bool bBladeTrailFinished=false;
  TWeakObjectPtr<UBBSBBladeTrailComponent> TravelTrail;
  TWeakObjectPtr<UStaticMeshComponent> TargetHighlight;
  bool bTravelTrailFinished=false,bImpactCuePlayed=false;
	};
	UPROPERTY(Transient) TObjectPtr<ACharacter> Character;
	UPROPERTY(Transient) TObjectPtr<AActor> FloatingWeapon;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Blade;
	UPROPERTY(Transient) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(EditDefaultsOnly, Category="BBSB|Animations") TArray<TObjectPtr<UAnimSequence>> GestureAnimations;
	FBBSBFieldGestureRecognizer Channels[2];
	TOptional<FWeaponAction> CurrentAction;
	TOptional<FWeaponAction> OtherWeaponAction;
	UPROPERTY(Transient) TObjectPtr<AActor> OtherFloatingWeapon;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> OtherBlade;
	UPROPERTY(Transient) TObjectPtr<AActor> ConductorWeapon;
	UPROPERTY(Transient) TObjectPtr<AActor> OtherConductorWeapon;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ConductorFull;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FormUpper;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FormLower;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> OtherFormLower;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> FieldMetronome;
	UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> FieldClick;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> HiddenOriginalStaff;
	double ChargePressAt=-100, LastSwordBeatAt=-100, FormUntil=-100;
	float ChargeAtPress=0;
	int32 ChargingChannel=-1, LastFieldClick=MIN_int32;
	bool bHammerSpending=false;
	double DaggerAwayUntil[2]={-100,-100};
	FVector LockedAimLocalPoint, LockedApproach=FVector::ForwardVector;
	int32 AimLockChannel=-1;
	bool bAimCameraNearAnchor=false;
	TWeakObjectPtr<AActor> SpearPiercedActor;
	double AimLockStartedAt=-100;
	FTransform AimLockWeaponFrom;
	FTransform FollowThroughPose[2];
	double FollowThroughUntil[2]={-100,-100};
	bool bHasFollowThrough[2]={false,false};
	FTransform CurrentUpperPose, CurrentLowerPose;
	float FormProgress=0;
	FBBSBActionArbiter Arbiter;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActionMontage;
	struct FHoverState { TWeakObjectPtr<UPrimitiveComponent> Primitive; bool bDepth=false; int32 Stencil=0; };
	TArray<FHoverState> HoverStates;
 TWeakObjectPtr<UPrimitiveComponent> HoverLockedSurface;
 TWeakObjectPtr<AActor> HoverBuddy;
 UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> BuddyOutlineMID;
 void UpdateAimPresentation();
	double CameraResumeAt = -100;
	TWeakObjectPtr<UInputComponent> BoundInput;
	FVector2D LastMove = FVector2D::ZeroVector;
	double LastMoveAt = -100.0;
	double DodgeStartedAt = -100.0;
	double LastDodgeAt = -100.0;
 double DodgeExitStartedAt=-100;
 bool bDodgeExitSettling=false;
 bool bDodgeExitToRun=false;
	FVector DodgeDirection = FVector(-1,0,0);
	FVector LastWeaponPosition = FVector::ZeroVector;
	FTransform WeaponReturnFromView;
	double WeaponReturnStartedAt = -100;
	bool bWeaponWasAttacking = false;
	FRotator OriginalMeshRotation;
	float OriginalFriction = 8.f;
	float OriginalFallingBraking=0;
	float OriginalBraking = 2000.f;
	float OriginalBrakingFactor = 2.f;
	float OriginalFOV = 90.f;
	bool bOriginalOrientToMovement = true;
	bool bDodging = false;
	bool bWeaponWasActive = false;
	bool bHUDInitialized = false;
	bool bSyntheticInput = false;
	FVector2D SyntheticMotion = FVector2D::ZeroVector;
	uint64 SampledMouseFrame = MAX_uint64;
	int32 ExerciseMode = -1;
	double ExerciseStartedAt = -100.0;
	int32 SavedRootMotionMode = 0;
	bool bSavedRootMotionMode = false;

	void BindControls();
	void PrimaryDown(); void PrimaryUp(); void SecondaryDown(); void SecondaryUp();
	void RotateHeldObject(float Steps);
	void RotateHeldObjectUp(); void RotateHeldObjectDown();
	void ToggleHelp();
	void BeginGesture(int32 Channel);
 bool CanUseBunniarZoom() const;
	void EndGesture(int32 Channel);
	void UpdateMouseMotion(double Now);
	bool EmitGesture(EBBSBFieldGesture Gesture, int32 Channel, float Charge, bool bGestureChain=true);
	FBBSBFieldGestureSettings GetGestureSettings() const;
	void CreateWeapon();
	FTransform GetWeaponRestTransform() const;
	void UpdateWeaponPresentation();
	void CreatePrototypeWeapon();
	bool PrototypeGesture(EBBSBFieldGesture Gesture,int32 Channel);
	bool StartPrototypeAttack(EBBSBFieldGesture Gesture,int32 Channel,float Damage,float Duration,FVector Impulse=FVector::ZeroVector);
	void TickPrototypeWeapon(float DT);
 void UpdateBladeTrail(FWeaponAction& Action,const FTransform& WeaponPose);
 void StopBladeTrail(FWeaponAction& Action);
 void UpdateTravelTrail(FWeaponAction& Action,float PreviousTime,float CurrentTime);
 void FinishTravelTrail(FWeaponAction& Action);
 void UpdateAttackHighlight(FWeaponAction& Action,float Time);
 void EmitAttackImpact(FWeaponAction& Action,const FVector& Point,const FVector& Normal);
 void EmitSurfaceImpact(FWeaponAction& Action);
	void TickPrototypeState();
	void PresentPrototypeWeapon();
	void ResetPrototypeState();
	FVector FindPrototypeCursorPoint(AActor** TargetActor=nullptr) const;
	void AcquireWeaponAim(int32 Channel);
	void ClearWeaponAim();
	void ReleaseSpearEmbed(bool bReturnToRest=false);
	void UpdateWeaponAim();
	FTransform PrototypeAttackPose(const FWeaponAction& Action,float Progress) const;
 void UpdateSwordTarget(FWeaponAction& Action);
	void WeaponOne(); void WeaponTwo(); void WeaponThree(); void WeaponFour(); void WeaponFive(); void WeaponSix();
	void TickWeapon(float DeltaTime);
	void TraceWeapon(const FVector& From, const FVector& To, FWeaponAction& Action,float Radius=-1.f);
 void TraceSwordBlade(const FTransform& From,const FTransform& To,FWeaponAction& Action);
	void FinishDodge(bool bContinueMovement=false);
	FVector FindAimPoint() const;
	void PlayGestureAnimation(EBBSBFieldGesture Gesture);
	void CancelCurrentAction(bool bCancelGesture,bool bCountCancel=true);
	void UpdateHover();
	void ClearHover();
};

/** Small field props use ordinary damage and produce short-lived physical fragments. */
UCLASS()
class BBSB_API ABBSBBreakableProp : public AActor
{
	GENERATED_BODY()
public:
	ABBSBBreakableProp();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Prop") FVector Size = FVector(55,55,70);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Prop") FLinearColor Tint = FLinearColor(0.65f,0.42f,0.5f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Prop", meta=(ClampMin="1")) float MaximumHealth = 40.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Prop") float Health = 40.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Prop") bool bBroken = false;
	UFUNCTION(BlueprintCallable, Category="BBSB|Prop") void ResetProp();
private:
	void Break(AActor* Causer);
	void UpdateAppearance();
};

/** Place one marker in a field to provide reusable targets without modifying terrain. */
UCLASS()
class BBSB_API ABBSBFieldPracticeArea : public AActor
{
	GENERATED_BODY()
public:
	ABBSBFieldPracticeArea();
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Practice") bool bSpawnPracticeProps = true;
};

UCLASS()
class BBSB_API ABBSBFieldPracticeHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};

UCLASS()
class BBSB_API ABBSBPlayerWeaponProjectile : public AActor
{
	GENERATED_BODY()
public:
	ABBSBPlayerWeaponProjectile();
	virtual void Tick(float DT) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) FVector FrozenTarget;
	UPROPERTY(VisibleAnywhere) FVector FlightVelocity;
	UPROPERTY(VisibleAnywhere) float Damage=18;
	void Launch(ACharacter* Shooter,FVector Target,float Amount,UStaticMesh* Mesh,UBBSBWeaponAnimationSet* AnimationSet=nullptr);
private:
	TWeakObjectPtr<ACharacter> SourceCharacter;
	UPROPERTY(Transient) TObjectPtr<ULevelSequence> FlightSequence;
 UPROPERTY(Transient) TObjectPtr<UBBSBWeaponAnimationSet> Effects;
 TWeakObjectPtr<UBBSBBladeTrailComponent> FlightTrail;
 TWeakObjectPtr<UStaticMeshComponent> TargetHighlight;
	FVector FlightOrigin;
	float FlightDistance=0, FlightSeconds=1;
	float Age=0;
};
