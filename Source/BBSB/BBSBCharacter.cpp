// Copyright Epic Games, Inc. All Rights Reserved.

#include "BBSBCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "BBSB.h"
#include "Field/BBSBFieldActions.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVerification.h"
#include "Combat/BBSBFormationVerification.h"
#include "Field/BBSBWeaponVerification.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"

ABBSBCharacter::ABBSBCharacter()
{
	FieldActions = CreateDefaultSubobject<UBBSBFieldActionComponent>(TEXT("FieldActions"));
	Combat = CreateDefaultSubobject<UBBSBCombatComponent>(TEXT("Combat"));
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ABBSBCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ABBSBCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABBSBCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABBSBCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ABBSBCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABBSBCharacter::Look);
	}
	else
	{
		UE_LOG(LogBBSB, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ABBSBCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ABBSBCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ABBSBCharacter::DoMove(float Right, float Forward)
{
	if (FieldActions)
	{
		FieldActions->RecordMoveInput(Right, Forward);
		if (!FieldActions->CanMove()) return;
	}
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ABBSBCharacter::DoLook(float Yaw, float Pitch)
{
	if (FieldActions && FieldActions->GetLookInputScale()<=0.f) return;
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		const float InputScale=FieldActions ? FieldActions->GetLookInputScale() : 1.f;
		AddControllerYawInput(Yaw*InputScale);
		AddControllerPitchInput(Pitch*InputScale);
	}
}

void ABBSBCharacter::DoJumpStart()
{
	if (FieldActions) FieldActions->TryDodge();
}

void ABBSBCharacter::DoJumpEnd()
{
	// Space is a directional dodge in BBSB; releasing it does not interrupt the dash.
}

float ABBSBCharacter::TakeDamage(float Amount, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer)
{
	if (FieldActions && FieldActions->IsDodgeInvulnerable()) return 0.f;
	if (Combat) { const float Before=Combat->Health; Combat->TakeCombatDamage(Amount,Causer); return Before-Combat->Health; }
	return Super::TakeDamage(Amount,Event,InstigatorController,Causer);
}

void ABBSBCharacter::FieldPress(int32 Channel) { if (FieldActions) FieldActions->FieldGesturePress(Channel); }
void ABBSBCharacter::FieldRelease(int32 Channel) { if (FieldActions) FieldActions->FieldGestureRelease(Channel); }
void ABBSBCharacter::FieldMotion(float X, float Y) { if (FieldActions) FieldActions->FieldGestureMotion(X,Y); }
void ABBSBCharacter::FieldDodge() { if (FieldActions) FieldActions->TryDodge(); }
void ABBSBCharacter::FieldReset() { if (FieldActions) FieldActions->ResetPracticeProps(); }
void ABBSBCharacter::FieldExercise(int32 GestureIndex) { if (FieldActions) FieldActions->ExerciseGesture(GestureIndex); }
void ABBSBCharacter::FieldView(float Yaw, float Pitch) { if (GetController()) GetController()->SetControlRotation(FRotator(Pitch,Yaw,0)); }
void ABBSBCharacter::CombatStatus() { if (Combat) UE_LOG(LogBBSB,Display,TEXT("Combat %s"),*Combat->StatusString()); }
void ABBSBCharacter::CombatParry(int32 Direction,bool bPressed) { if (Combat) { if (bPressed) Combat->ParryDown(Direction); else Combat->ParryUp(Direction); } }
void ABBSBCharacter::CombatPractice() { if (Combat) Combat->GoToPractice(); }
void ABBSBCharacter::CombatTarget(int32 Step) { if (Combat) Combat->CycleTarget(Step); }
void ABBSBCharacter::CombatVerify()
{
#if !UE_BUILD_SHIPPING
	for (TActorIterator<ABBSBWeaponVerification> It(GetWorld()); It; ++It) if (!It->bComplete) return;
	for (TActorIterator<ABBSBCombatVerification> It(GetWorld()); It; ++It) if (!It->bComplete) return;
	for (TActorIterator<ABBSBFormationVerification> It(GetWorld()); It; ++It) if (!It->bComplete) return;
	GetWorld()->SpawnActor<ABBSBCombatVerification>();
#endif
}
void ABBSBCharacter::CombatLabKey(FString KeyName,bool bPressed)
{
#if !UE_BUILD_SHIPPING
	const FKey Key{FName(*KeyName)};
	const TArray<FKey> Allowed={EKeys::F1,EKeys::F2,EKeys::F3,EKeys::F4,EKeys::F5,EKeys::F6,EKeys::F7,EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Q,EKeys::E};
	if (Allowed.Contains(Key))
		if (APlayerController* PC=Cast<APlayerController>(GetController())) PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,bPressed ? IE_Pressed : IE_Released,bPressed ? 1.f : 0.f));
#endif
}
void ABBSBCharacter::CombatFormationVerify()
{
#if !UE_BUILD_SHIPPING
	for (TActorIterator<ABBSBWeaponVerification> It(GetWorld()); It; ++It) if (!It->bComplete) return;
	for (TActorIterator<ABBSBCombatVerification> It(GetWorld()); It; ++It) if (!It->bComplete) return;
	for (TActorIterator<ABBSBFormationVerification> It(GetWorld()); It; ++It) if (!It->bComplete) return;
	GetWorld()->SpawnActor<ABBSBFormationVerification>();
#endif
}
void ABBSBCharacter::WeaponSelect(int32 Index)
{
	if (FieldActions && Index>=0 && Index<=6) FieldActions->SelectPrototypeWeapon(static_cast<EBBSBPrototypeWeapon>(Index));
}
void ABBSBCharacter::WeaponVerify()
{
#if !UE_BUILD_SHIPPING
	for (TActorIterator<ABBSBWeaponVerification> It(GetWorld()); It; ++It) if (!It->bComplete) return;
	for (TActorIterator<ABBSBCombatVerification> It(GetWorld()); It; ++It) if (!It->bComplete) return;
	for (TActorIterator<ABBSBFormationVerification> It(GetWorld()); It; ++It) if (!It->bComplete) return;
	GetWorld()->SpawnActor<ABBSBWeaponVerification>();
#endif
}
void ABBSBCharacter::FieldStatus()
{
	if (!FieldActions) return;
	UE_LOG(LogBBSB, Display, TEXT("Field Status %s pos=%s capture=%d dodge=%d cooldown=%.2f broken=%d gestures=%d,%d,%d,%d,%d"),
		*FieldActions->GetMovementLabel(),*GetActorLocation().ToCompactString(),FieldActions->IsGestureCaptured(),FieldActions->IsDodging(),
		FieldActions->GetDodgeCooldownRemaining(),FieldActions->PropsBroken,
		FieldActions->GestureCounts[0],FieldActions->GestureCounts[1],FieldActions->GestureCounts[2],FieldActions->GestureCounts[3],FieldActions->GestureCounts[4]);
}
