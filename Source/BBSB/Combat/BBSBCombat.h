#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerInput.h"
#include "BBSBCombatRules.h"
#include "BBSBCombat.generated.h"

class ABBSBInkEnemy;
class ABBSBCombatStage;
class ABBSBInkProjectile;
class ABBSBCombatPracticeArea;
class UStaticMeshComponent;
class UTextRenderComponent;
class USpringArmComponent;
class UAudioComponent;
class USoundWaveProcedural;
class UCameraComponent;
class APlayerController;
class UAnimSequence;
class UBBSBDefensePresentation;
class AHUD;
class UCanvas;

UCLASS(ClassGroup=(BBSB), meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBCombatComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBBSBCombatComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DT, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Vitals") float MaximumHealth=100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Vitals") float Health=100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ecstatic") float Ecstatic=0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Ecstatic") float MaximumEcstatic=500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Ecstatic") float EntryBonus=75.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Ecstatic") float BlockGain=24.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Ecstatic") float MissLoss=45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Ecstatic") float DodgeCost=100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Parry") float BlockWindow=0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Parry") float EarlyWindow=0.28f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Parry") FVector2D ParryHalfSize=FVector2D(145,125);
 // Mid-flight separation (horizontal / vertical) returns to the frozen physical body target at impact.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Parry",meta=(ClampMin="0",Units="cm")) FVector2D IncomingPathSpread=FVector2D(110,75);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Camera") float CombatCameraDistance=250.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Camera",meta=(Units="cm")) FVector CombatCameraOffset=FVector(0,60,65);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Camera",meta=(ClampMin="-30",ClampMax="10",Units="deg")) float CombatCameraPitch=-7.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Camera") float CameraFollowSpeed=3.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Combat") bool bCombatStance=false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Combat") bool bDead=false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Combat") TObjectPtr<ABBSBCombatStage> Stage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Combat") TObjectPtr<ABBSBInkEnemy> SelectedEnemy;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Feedback") int32 Blocks=0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Feedback") int32 Misses=0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Feedback") int32 Victories=0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Feedback") FString Feedback;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Feedback") double FeedbackAt=-100;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Feedback") double LastParryAt=-100;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Feedback") EBBSBParryResult LastParry=EBBSBParryResult::Miss;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Feedback") EBBSBParryDirection LastDirection=EBBSBParryDirection::Up;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Practice") int32 CameraExperiment=0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Practice") int32 FeedbackExperiment=0;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|TimingCoach") EBBSBTimingKind CoachContact=EBBSBTimingKind::Waiting;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|TimingCoach") float CoachLeadMs=-1.f;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|TimingCoach") int32 CoachSamples=0;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|TimingCoach") float CoachMedianMs=0.f;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|TimingCoach") float CoachMinMs=0.f;
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="BBSB|TimingCoach") float CoachMaxMs=0.f;

 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Defense") TSoftObjectPtr<UBBSBDefensePresentation> DefenseLibrary;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") TObjectPtr<UBBSBDefensePresentation> DefensePresentation;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") TObjectPtr<UAnimSequence> DefenseAnimation;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") int32 ActiveDefenseMask=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") float DefensePoseTime=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") float DefensePoseWeight=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") bool bDefenseHitReaction=false;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") int32 DefenseMotionSerial=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") int32 DefenseCombo=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") int32 BestDefenseCombo=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") int32 BrokenDefenseCombo=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") int32 DefenseEffectsSpawned=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") double LastDefenseImpactAt=-100;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") FVector LastDefenseImpact=FVector::ZeroVector;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Defense") bool bLastDefenseBlocked=false;
 UFUNCTION(BlueprintPure,Category="BBSB|Defense") uint8 HeldParryMask() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Defense") bool IsDefenseMotionActive() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Defense") FVector DefenseTarget(int32 Direction) const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Defense|Cues") bool bShowDefenseDirectionCues=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Defense|Cues",meta=(ClampMin=".5",ClampMax="2")) float DefenseCueScale=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Defense|Cues") FLinearColor DefenseCueColor=FLinearColor(1.f,.83f,.32f);
 /** Up/right/down/left: seconds to physical contact inside BlockWindow; -1 means no available parry. */
 UFUNCTION(BlueprintPure,Category="BBSB|Defense|Cues") TArray<float> DefenseCueTimes() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Defense|Cues") int32 DefenseCueMask() const;
 void GetDefensePose(UAnimSequence*& Clip,float& Time,float& Weight) const;
 void DrawDefenseFeedback(AHUD* HUD,UCanvas* Canvas) const;
 void DrawDefenseDirectionCues(AHUD* HUD,UCanvas* Canvas) const;
	bool BlocksMovement() const;
	bool IsStageLocked() const;
	bool CanAttack() const;
 bool CanDodge() const;
	float DamageMultiplier() const;
	FVector AimPoint() const;
	void OnAttackStarted();
	void OnDodge();
	void EnemyStruck(ABBSBInkEnemy* Enemy);
	void AttachStage(ABBSBCombatStage* NewStage);
	void LeaveStage(bool bVictory);
	void TakeCombatDamage(float Damage, AActor* Causer, bool bInterrupt=true);
	void ParryDown(int32 Direction);
	void ParryUp(int32 Direction);
	EBBSBParryResult ResolveContact(const FVector& Contact, int32 HoldGroup, float Damage, AActor* Causer);
	void GetParryPlane(FVector& Origin, FVector& Normal, FVector& Right) const;
	void CycleTarget(int32 Step);
	UFUNCTION(BlueprintCallable, Category="BBSB|Practice") void ResetEncounter();
	UFUNCTION(BlueprintCallable, Category="BBSB|Practice") void GoToPractice();
	void SetFeedback(const FString& Text);
	FString StatusString() const;
	bool IsDirectionHeld(int32 Direction) const;

private:
	UPROPERTY(Transient) TObjectPtr<ACharacter> Character;
	UPROPERTY(Transient) TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(Transient) TObjectPtr<UCameraComponent> Camera;
	TWeakObjectPtr<UInputComponent> BoundInput;
	TWeakObjectPtr<UPlayerInput> DebugInput;
	TArray<TPair<int32,FKeyBind>> SavedViewBindings;
	void RestoreViewBindings();
	FBBSBParryInput Inputs[4];
	FBBSBParryTimingHistory TimingHistory;
	void SyncTimingCoach();
	bool bDownS=false, bDownX=false;
	TSet<int32> RewardedEncounters;
	float OriginalArmLength=250.f;
	float OriginalCombatDistance=640.f;
	FVector OriginalSocketOffset=FVector::ZeroVector;
	FRotator CameraRelativeRotation=FRotator::ZeroRotator;
	FRotator CombatViewRotation=FRotator::ZeroRotator;
	TWeakObjectPtr<APlayerController> StageLookController;
	bool bOriginalCameraLag=false, bOriginalRotationLag=false;
	bool bStageOriginalCursor=false;
	void SetStageCameraLock(bool bLocked);
	TWeakObjectPtr<ABBSBInkEnemy> PracticeFocus;
	double PracticeFocusUntil=-100;
	void BindControls();
	void UpDown(); void UpUp(); void RightDown(); void RightUp();
	void DownSDown(); void DownSUp(); void DownXDown(); void DownXUp();
	void LeftDown(); void LeftUp(); void PreviousTarget(); void NextTarget();
	void NextExperiment();
	void NextCameraExperiment();
	void NextFeedbackExperiment();
	void NextWeaponExperiment();
 double DefenseStartedAt=-100;
 float DefenseDuration=0;
 void StartDefenseMotion(uint8 Mask,bool bHit=false);
 void CancelDefenseMotion();
 void UpdateDefensePresentation();
 void DefenseImpact(const FVector& Point,bool bBlock);
	void ClearParry();
};

/** Placeholder ink creature: replace its visual component without changing stage/pattern logic. */
UCLASS()
class BBSB_API ABBSBInkEnemy : public ACharacter
{
	GENERATED_BODY()
public:
	ABBSBInkEnemy();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DT) override;
	virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Enemy") FName EnemyName="Inkling";
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Enemy") float MaximumHealth=110.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Enemy") float Health=110.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Enemy") float ShotDamage=8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Enemy") float MoveSpeed=210.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Enemy") FLinearColor Tint=FLinearColor(0.42f,0.36f,0.55f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Enemy") EBBSBInkPattern MainPattern=EBBSBInkPattern::Tap;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Attack phrase") TArray<EBBSBParryDirection> AttackDirections={EBBSBParryDirection::Up,EBBSBParryDirection::Right,EBBSBParryDirection::Left};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Enemy") int32 EncounterID=1;
 // Possession assigns Actor.Owner to the AI controller; keep the spawning area's identity separately.
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Practice") TObjectPtr<ABBSBCombatPracticeArea> PracticeArea;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Enemy") bool bDefeated=false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Enemy") bool bFront=false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Enemy") TObjectPtr<ABBSBCombatStage> Stage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Enemy") FVector FormationTarget=FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Enemy") float LastDamageTaken=0;
	void JoinStage(ABBSBCombatStage* NewStage, bool bIsFront);
	void ApplyWeaponImpulse(FVector Impulse);
	void ResetEnemy();
	FVector GetAimPoint() const;
	void SetSelected(bool bSelected);
	void UpdateLabel();
	FVector Home=FVector::ZeroVector;
	EBBSBInkPattern AuthoredPattern=EBBSBInkPattern::Tap;
private:
	double HitFlashUntil=-100;
	double WeaponImpulseUntil=-100;
};

/** All enemies share this impact schedule; individual AI never starts independent rhythm timers. */
UCLASS()
class BBSB_API ABBSBCombatStage : public AActor
{
	GENERATED_BODY()
public:
	ABBSBCombatStage();
	virtual void BeginPlay() override;
	virtual void Tick(float DT) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Rhythm") float BPM=110.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Pattern") bool bPreferStraightAttacks=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Exchange") bool bUseCombatExchanges=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Exchange",meta=(ClampMin="2",ClampMax="4")) float EnemyWindupBeats=2.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Exchange",meta=(ClampMin="2",ClampMax="8")) float CounterWindowBeats=3.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="BBSB|Exchange",meta=(ClampMin="1",ClampMax="3")) float CounterDamageMultiplier=1.35f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Exchange") bool bCounterAttackOpen=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Exchange") int32 ExchangeNumber=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Exchange") FString EnemyPatternHint;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Stage") int32 EncounterID=INDEX_NONE;
 UFUNCTION(BlueprintPure,Category="BBSB|Exchange") bool IsCounterWindow() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Exchange") float CounterSecondsRemaining() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Exchange") int32 UnresolvedAttackCount() const;
 UFUNCTION(BlueprintPure,Category="BBSB|Exchange") FString ExchangeHint() const;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Stage") float RingRadius=220.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Stage") float ParryDistance=570.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Stage") float JoinRadius=1000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Stage") float LoseAggroDistance=2200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Stage") float LoseAggroSeconds=5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Stage") float ChaseSpeed=130.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Stage", meta=(ClampMin="0.5")) float ReplacementStallSeconds=3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Rhythm") bool bAudibleMetronome=true;
	/** Used by deterministic PIE verification; normal encounters always emit their schedule. */
	UPROPERTY(Transient,EditAnywhere,BlueprintReadWrite,Category="BBSB|Practice") bool bPausePatternEmission=false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Stage") EBBSBStagePhase Phase=EBBSBStagePhase::Forming;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Stage") TObjectPtr<ABBSBInkEnemy> Front;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Stage") TArray<TObjectPtr<ABBSBInkEnemy>> Members;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Stage") TObjectPtr<UBBSBCombatComponent> Player;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Rhythm") double CurrentBeat=0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Rhythm") int32 ShotsFired=0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Rhythm") int32 FrontReplacements=0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Rhythm") int32 Joins=0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Stage") bool bFormationValid=false;
	bool Initialize(UBBSBCombatComponent* Combat, ABBSBInkEnemy* First);
	void AddMember(ABBSBInkEnemy* Enemy);
	void EnemyDefeated(ABBSBInkEnemy* Enemy);
	void Dissolve(bool bAllDefeated=false);
	FString CountInText() const;
	float BeatPulse() const;
	bool IsActive() const;
	bool IsOnBeat(float WindowSeconds=0.09f) const;
	void SetExperiment(int32 Preset);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Practice") int32 ExperimentPreset=0;

private:
	struct FNote
	{
		double ImpactBeat=0;
		uint8 Directions=1;
		EBBSBInkPattern Pattern=EBBSBInkPattern::Tap;
		int32 HoldGroup=INDEX_NONE;
		TWeakObjectPtr<ABBSBInkEnemy> Source;
	};
	TArray<FNote> PendingNotes;
 TArray<TWeakObjectPtr<ABBSBInkProjectile>> ExchangeProjectiles;
 double ExchangeStartedBeat=0,EarliestCounterBeat=0,CounterStartedBeat=-1;
 void BeginExchange();
 void TickExchange();
 void RefreshExchangeLabels();
	TArray<TWeakObjectPtr<UStaticMeshComponent>> RingInk;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Metronome;
	UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> ClickSound;
	double EnteredAt=0, MusicEpoch=0, AggroLostAt=-1, NextJoinCheck=0;
	int32 NextBar=0, LastClickBeat=MIN_int32, NextHoldGroup=1;
	FVector PlayerLineFacing=FVector::ForwardVector;
	FVector PreferredFrontOffset=FVector::ZeroVector;
	bool bCleanedProjectiles=false;
	double ReplacementProgressAt=0;
	float ReplacementBestDistance=0;
	bool HasFrontCorridor(ABBSBInkEnemy* Enemy) const;
	bool StartReplacement(bool bRequireClear,ABBSBInkEnemy* Excluded=nullptr);
	bool FindFormation(ACharacter* Character, FVector& OutPlayer);
	void CreateInk();
	void UpdateInk();
	void UpdateFormation(float DT);
	void PlanBar(int32 Bar);
	void Launch(const FNote& Note);
	void Click(int32 Beat);
	void ClearStageProjectiles();
};

/** Free world-space flight. No target actor or homing update is retained after launch. */
UCLASS()
class BBSB_API ABBSBInkProjectile : public AActor
{
	GENERATED_BODY()
public:
	ABBSBInkProjectile();
	virtual void Tick(float DT) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ink") FVector LaunchOrigin;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ink") FVector FrozenTarget;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ink") FVector FlightVelocity;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="BBSB|Ink") FVector ApproachOffset=FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ink") EBBSBInkPattern Pattern=EBBSBInkPattern::Tap;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ink") float Age=0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ink") int32 HoldGroup=INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ink") float Damage=8;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ink") bool bResolved=false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ink") TWeakObjectPtr<ABBSBCombatStage> OwningStage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Ink") TWeakObjectPtr<ABBSBInkEnemy> Source;
	void Initialize(ABBSBCombatStage* InStage, ABBSBInkEnemy* InSource, FVector Target, EBBSBInkPattern InPattern, float FlightSeconds, int32 InHoldGroup);
	float TimeToTarget() const { return FMath::Max(0.f,TravelSeconds-Age); }
 /** Read-only forecast using the same trajectory, body sweep and world obstruction rules as flight. */
 UFUNCTION(BlueprintPure,Category="BBSB|Ink") bool PredictBodyContact(ACharacter* Target,float MaxSeconds,FVector& Contact,float& Seconds) const;
private:
	float TravelSeconds=1.f;
	int32 Bounces=0;
	FVector Side=FVector::RightVector;
	FVector PreviousPosition=FVector::ZeroVector;
 FVector FlightPosition(float AtAge) const;
	void ResolveAndFade();
};

/** A reversible encounter marker. F2 goes here; F3 selects an isolated practice experiment. */
UCLASS()
class BBSB_API ABBSBCombatPracticeArea : public AActor
{
	GENERATED_BODY()
public:
	ABBSBCombatPracticeArea();
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Practice") int32 EncounterID=1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Practice") int32 EnemyCount=3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Practice") bool bSpawnEnemies=true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BBSB|Practice") float EnemyHealth=110.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB|Practice") int32 ExperimentPreset=0;
	void CycleExperiment();
	virtual FVector EntryLocation() const;
 virtual void SpawnPracticeEnemies();
};
