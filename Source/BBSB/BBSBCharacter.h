// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "BBSBCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UBBSBFieldActionComponent;
class UBBSBCombatComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class ABBSBCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	/** Constructor */
	ABBSBCharacter();	

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB")
	TObjectPtr<UBBSBFieldActionComponent> FieldActions;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BBSB") TObjectPtr<UBBSBCombatComponent> Combat;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(Exec) void FieldPress(int32 Channel = 0);
	UFUNCTION(Exec) void FieldRelease(int32 Channel = 0);
	UFUNCTION(Exec) void FieldMotion(float X, float Y);
	UFUNCTION(Exec) void FieldDodge();
	UFUNCTION(Exec) void FieldReset();
	UFUNCTION(Exec) void FieldStatus();
	UFUNCTION(Exec) void FieldExercise(int32 GestureIndex);
	UFUNCTION(Exec) void FieldView(float Yaw, float Pitch);
	UFUNCTION(Exec) void CombatStatus();
	UFUNCTION(Exec) void CombatParry(int32 Direction, bool bPressed=true);
	UFUNCTION(Exec) void CombatPractice();
	UFUNCTION(Exec) void CombatTarget(int32 Step=1);
	UFUNCTION(Exec) void CombatVerify();
	UFUNCTION(Exec) void CombatFormationVerify();
	UFUNCTION(Exec) void WeaponSelect(int32 Index=1);
	UFUNCTION(Exec) void WeaponVerify();
	/** Development hook for repeatable MCP playtests of the lab controls. */
	UFUNCTION(Exec) void CombatLabKey(FString KeyName, bool bPressed=true);

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

