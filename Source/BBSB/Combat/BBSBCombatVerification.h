#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BBSBCombatRules.h"
#include "BBSBCombatVerification.generated.h"

class ACharacter;
class UBBSBCombatComponent;
class UBBSBFieldActionComponent;
class ABBSBCombatStage;
class ABBSBInkEnemy;
class ABBSBInkProjectile;

/** Development-only scenario, spawned explicitly by CombatVerify. Never placed in the shipped field. */
UCLASS(NotBlueprintable)
class BBSB_API ABBSBCombatVerification : public AActor
{
	GENERATED_BODY()
public:
	ABBSBCombatVerification();
	virtual void BeginPlay() override;
	virtual void Tick(float DT) override;
	UPROPERTY(VisibleAnywhere) bool bComplete=false;
	UPROPERTY(VisibleAnywhere) int32 ChecksPassed=0;
	UPROPERTY(VisibleAnywhere) TArray<FString> Failures;
	UPROPERTY(VisibleAnywhere) FString PhaseLabel;
	UPROPERTY(VisibleAnywhere) int32 Step=0;
private:
	UPROPERTY() TObjectPtr<ACharacter> Character;
	UPROPERTY() TObjectPtr<UBBSBCombatComponent> Combat;
	UPROPERTY() TObjectPtr<UBBSBFieldActionComponent> Actions;
	UPROPERTY() TObjectPtr<ABBSBCombatStage> Stage;
	UPROPERTY() TObjectPtr<ABBSBInkEnemy> First;
	UPROPERTY() TObjectPtr<ABBSBInkEnemy> Joined;
	UPROPERTY() TObjectPtr<ABBSBInkProjectile> Witness;
	FTransform OriginalTransform;
	FRotator OriginalView;
	float SavedMaxHP=100, SavedHP=100, SavedEcstatic=0;
	float SavedDodgeDistance=330;
	uint8 OriginalPrototype=0;
	FVector WitnessTarget, CenterBefore, DodgeFrom, StanceFrom;
	float EnemyHealthBefore=0, RadiusBefore=0;
	double StepAt=0;
	int32 TestDirection=0, BlocksBefore=0, MissesBefore=0;
	int32 GestureCountBefore=0;
	int32 OriginalPatternPreset=0, OriginalCameraPreset=0, OriginalFeedbackPreset=0;
	float OriginalCameraDistance=640;
	EBBSBWeaponCommitment OriginalWeaponPreset=EBBSBWeaponCommitment::Authored;
	TArray<FBBSBWeaponActionRule> SavedPrimaryRules, SavedSecondaryRules;
	UPROPERTY() TObjectPtr<AActor> PracticeMarker;
	void SimulateKey(const FKey& Key,bool bDown);
	bool bPressed=false, bReleased=false, bSecondPress=false, bRecovered=false;
	void Check(bool bCondition,const TCHAR* Message);
	void Advance(int32 Next,const TCHAR* Label);
	void Finish();
	ABBSBInkProjectile* Shot(int32 Direction,float Flight,int32 HoldGroup=-1);
	void DirectionKey(int32 Direction,bool bDown);
};
