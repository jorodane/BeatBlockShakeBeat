#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BBSBFormationVerification.generated.h"

class ACharacter;
class UBBSBCombatComponent;
class ABBSBCombatStage;
class ABBSBInkEnemy;
class ABBSBInkProjectile;

/** Explicit PIE-only obstacle fixture. Its actors are never saved to the field. */
UCLASS(NotBlueprintable)
class BBSB_API ABBSBFormationVerification : public AActor
{
	GENERATED_BODY()
public:
	ABBSBFormationVerification();
	virtual void BeginPlay() override;
	virtual void Tick(float DT) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(VisibleAnywhere) bool bComplete=false;
	UPROPERTY(VisibleAnywhere) int32 ChecksPassed=0;
	UPROPERTY(VisibleAnywhere) TArray<FString> Failures;
	UPROPERTY(VisibleAnywhere) int32 Step=0;
	UPROPERTY(VisibleAnywhere) FString PhaseLabel;
private:
	UPROPERTY() TObjectPtr<ACharacter> Character;
	UPROPERTY() TObjectPtr<UBBSBCombatComponent> Combat;
	UPROPERTY() TObjectPtr<ABBSBCombatStage> Stage;
	UPROPERTY() TObjectPtr<ABBSBInkEnemy> First;
	UPROPERTY() TObjectPtr<ABBSBInkEnemy> NearBacker;
	UPROPERTY() TObjectPtr<ABBSBInkEnemy> FarBacker;
	UPROPERTY() TObjectPtr<ABBSBInkProjectile> Witness;
	UPROPERTY() TObjectPtr<AActor> Wall;
	FTransform OriginalTransform;
	FRotator OriginalView;
	FVector Center, WitnessTarget;
	float SavedMaximumHealth=100, SavedHealth=100, SavedEcstatic=0;
	double StepAt=0;
	void Check(bool bCondition,const TCHAR* Message);
	void Advance(int32 Next,const TCHAR* Label);
	bool StartEncounter();
	bool Arrange(bool bOnlyBlockedCandidate,bool bLateWall=false);
	bool CorridorBlocked(ABBSBInkEnemy* Enemy) const;
	void Finish();
};
