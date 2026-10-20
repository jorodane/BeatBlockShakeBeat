#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BBSBWeaponVerification.generated.h"
class ACharacter;
class UBBSBFieldActionComponent;
class UBBSBCombatComponent;
class ABBSBBreakableProp;
class ABBSBInkEnemy;

UCLASS(NotBlueprintable)
class BBSB_API ABBSBWeaponVerification : public AActor
{
 GENERATED_BODY()
public:
 ABBSBWeaponVerification();
 virtual void BeginPlay() override;
 virtual void Tick(float DT) override;
 UPROPERTY(VisibleAnywhere) bool bComplete=false;
 UPROPERTY(VisibleAnywhere) int32 ChecksPassed=0;
 UPROPERTY(VisibleAnywhere) TArray<FString> Failures;
 UPROPERTY(VisibleAnywhere) int32 Step=0;
private:
 UPROPERTY() TObjectPtr<ACharacter> Character;
 UPROPERTY() TObjectPtr<UBBSBFieldActionComponent> Actions;
 UPROPERTY() TObjectPtr<UBBSBCombatComponent> Combat;
 UPROPERTY() TObjectPtr<ABBSBBreakableProp> Target;
 UPROPERTY() TObjectPtr<ABBSBInkEnemy> Enemy;
 UPROPERTY() TObjectPtr<AActor> Wall;
 FTransform OriginalTransform;
 FRotator OriginalView;
 uint8 OriginalWeapon=1;
 float OriginalHP=100,OriginalMaxHP=100,OriginalEcstatic=0;
 double StepAt=0;
 float HealthBefore=0,ChargeBefore=0;
 int32 Frame=0,Counter=0;
 bool bConductorStayedHeld=true;
 FVector EnemyBefore;
 FVector SavedAim;
 FRotator CameraBefore;
 float ChargeDistanceBefore=0;
 void Check(bool Condition,const TCHAR* Message);
 void Advance(int32 Next);
 void AimAt(FVector Position);
 void Finish();
 bool PresentationClearsGeometry() const;
};
