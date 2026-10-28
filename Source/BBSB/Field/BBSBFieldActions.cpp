#include "BBSBFieldActions.h"
#include "BBSBCharacter.h"
#include "Framework/Application/SlateApplication.h"
#include "Character/BBSBTraversal.h"
#include "Character/BBSBMovementAuthoring.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "RenderUtils.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "BBSB.h"
#include "Caravan/BBSBCaravan.h"
#include "Interaction/BBSBInteraction.h"
#include "Combat/BBSBCombat.h"
#include "Components/MeshComponent.h"
#include "Components/DecalComponent.h"

namespace
{
	const TCHAR* GestureName(EBBSBFieldGesture Gesture)
	{
		switch (Gesture)
		{
		case EBBSBFieldGesture::Beat: return TEXT("Beat");
		case EBBSBFieldGesture::Hold: return TEXT("Hold");
		case EBBSBFieldGesture::Shake: return TEXT("Shake");
		case EBBSBFieldGesture::Flick: return TEXT("Flick");
		default: return TEXT("Dive");
		}
	}
	FLinearColor GestureColor(EBBSBFieldGesture Gesture)
	{
		static const FLinearColor Colors[] = {
			FLinearColor(0.9f,0.42f,0.56f), FLinearColor(0.76f,0.63f,0.28f),
			FLinearColor(0.52f,0.45f,0.8f), FLinearColor(0.28f,0.7f,0.74f), FLinearColor(0.7f,0.48f,0.3f)};
		return Colors[static_cast<int32>(Gesture)];
	}
	void TintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
		if (!MID)
		{
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))) Mesh->SetMaterial(0,Base);
			MID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
		}
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetScalarParameterValue(TEXT("Roughness"),0.9f);
		}
	}
	UStaticMeshComponent* AddWeaponPart(AActor* Actor, UStaticMesh* Mesh, const TCHAR* Name, FVector Position, FVector Scale, FLinearColor Color)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Actor, Name);
		Actor->AddInstanceComponent(Part);
		Part->SetupAttachment(Actor->GetRootComponent());
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetRelativeLocation(Position);
		Part->SetRelativeScale3D(Scale);
		Part->RegisterComponent();
		TintMesh(Part, Color);
		return Part;
	}
}

UBBSBFieldActionComponent::UBBSBFieldActionComponent()
{
	const TCHAR* AnimationNames[]={TEXT("Legacy"),TEXT("Greatsword"),TEXT("Spear"),TEXT("Hammer"),TEXT("Sword"),TEXT("Dagger"),TEXT("Crescent")};
	for (int32 I=1;I<7;++I)
		WeaponAnimationSets.Add(EBBSBPrototypeWeapon(I),TSoftObjectPtr<UBBSBWeaponAnimationSet>(FSoftObjectPath(FString::Printf(TEXT("/Game/BBSB/AnimationAuthoring/%s/DA_%s.DA_%s"),AnimationNames[I],AnimationNames[I],AnimationNames[I]))));
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	WeaponViewTick.bCanEverTick = true;
	WeaponViewTick.TickGroup = TG_PostUpdateWork;
	PrimaryRules.SetNum(5); SecondaryRules.SetNum(5);
	const float Durations[]={0.28f,0.45f,0.4f,0.32f,0.42f};
	const int32 Priorities[]={20,24,26,32,30};
	const float Multipliers[]={1.f,0.75f,0.8f,1.45f,1.3f};
	for (int32 Index=0; Index<5; ++Index)
	{
		PrimaryRules[Index].Duration=Durations[Index]; PrimaryRules[Index].Priority=Priorities[Index];
		PrimaryRules[Index].DamageMultiplier=Multipliers[Index];
		PrimaryRules[Index].ChainCancelOpen=Index==0 ? 0.22f : 0.35f;
		SecondaryRules[Index]=PrimaryRules[Index];
	}
	PrimaryRules[1].bLockMovement=false; PrimaryRules[1].bOccupyConductor=false;
	SecondaryRules[1].bLockMovement=false; SecondaryRules[1].bOccupyConductor=false;
	PrimaryRules[1].ChainCancelOpen=SecondaryRules[1].ChainCancelOpen=0.f;
	PrimaryRules[2].ChainCancelOpen=SecondaryRules[2].ChainCancelOpen=0.1f;
	SecondaryRules[4].Duration=0.55f; SecondaryRules[4].DodgeCancelOpen=0.15f;
	SecondaryRules[4].DamageMultiplier=1.6f;
	const TCHAR* AnimPaths[] = {
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"),
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack"),
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"),
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_03.MM_Attack_03")};
	for (const TCHAR* Path : AnimPaths)
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> Motion(Path);
		GestureAnimations.Add(Motion.Object);
	}
}

void UBBSBFieldActionComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);
	if (bRegister)
	{
		if (SetupActorComponentTickFunction(&WeaponViewTick)) WeaponViewTick.Target=this;
	}
	else WeaponViewTick.UnRegisterTickFunction();
}

void UBBSBFieldActionComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<ACharacter>(GetOwner());
	if (!Character) { SetComponentTickEnabled(false); return; }
	Camera = Character->FindComponentByClass<UCameraComponent>();
	if (Camera) OriginalFOV = Camera->FieldOfView;
	OriginalMeshRotation = Character->GetMesh()->GetRelativeRotation();
	// Fresh play starts with the real one-handed sword, including old Blueprint defaults.
	PrototypeWeapon=EBBSBPrototypeWeapon::Sword;
	AvailableWeapons={EBBSBPrototypeWeapon::Sword};
	Character->GetCharacterMovement()->AddTickPrerequisiteComponent(this);
	if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
	{
		SavedRootMotionMode = static_cast<int32>(Anim->RootMotionMode.GetValue());
		bSavedRootMotionMode = true;
		Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}
}

void UBBSBFieldActionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if(CurrentAction.IsSet()) StopBladeTrail(CurrentAction.GetValue());
 if(OtherWeaponAction.IsSet()) StopBladeTrail(OtherWeaponAction.GetValue());

 bBunniarZoomHeld=false;
 if(Camera) Camera->SetFieldOfView(OriginalFOV);
	CancelEquipmentSelection();
	FinishDodge();
	ClearHover();
 if(SurfaceAimDecal) { SurfaceAimDecal->DestroyComponent();SurfaceAimDecal=nullptr; }
 if(Camera && BuddyOutlineMID) Camera->RemoveBlendable(BuddyOutlineMID);
	if (bSavedRootMotionMode && Character)
		if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance()) Anim->SetRootMotionMode(static_cast<ERootMotionMode::Type>(SavedRootMotionMode));
	if (IsValid(FloatingWeapon)) FloatingWeapon->Destroy();
	if (IsValid(OtherFloatingWeapon)) OtherFloatingWeapon->Destroy();
	if (IsValid(ConductorWeapon)) ConductorWeapon->Destroy();
	if (IsValid(OtherConductorWeapon)) OtherConductorWeapon->Destroy();
	for (auto& Original:HiddenOriginalStaff) if (Original.IsValid()) Original->SetVisibility(true);
	if (FieldMetronome) FieldMetronome->DestroyComponent();
	Super::EndPlay(Reason);
}

FBBSBFieldGestureSettings UBBSBFieldActionComponent::GetGestureSettings() const
{
	FBBSBFieldGestureSettings Settings;
	Settings.HoldSeconds = HoldThreshold;
	Settings.FlickPixelsPerSecond = FlickSpeed;
	Settings.ShakeStrokePixels = ShakeStrokePixels;
	return Settings;
}

void UBBSBFieldActionComponent::BindControls()
{
	if (!Character || !Character->IsLocallyControlled()) return;
	UInputComponent* Input = Character->FindComponentByClass<UInputComponent>();
	if (!Input || BoundInput == Input) return;
	Input->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &UBBSBFieldActionComponent::PrimaryDown);
	Input->BindKey(EKeys::LeftMouseButton, IE_Released, this, &UBBSBFieldActionComponent::PrimaryUp);
	Input->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &UBBSBFieldActionComponent::SecondaryDown);
	Input->BindKey(EKeys::RightMouseButton, IE_Released, this, &UBBSBFieldActionComponent::SecondaryUp);
	Input->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &UBBSBFieldActionComponent::RotateHeldObjectUp).bConsumeInput=false;
	Input->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &UBBSBFieldActionComponent::RotateHeldObjectDown).bConsumeInput=false;
	Input->BindKey(EKeys::Tab, IE_Pressed, this, &UBBSBFieldActionComponent::EquipmentTabDown);
	Input->BindKey(EKeys::Tab, IE_Released, this, &UBBSBFieldActionComponent::EquipmentTabUp);
	if (bEnablePracticeControls)
	{
		Input->BindKey(EKeys::R, IE_Pressed, this, &UBBSBFieldActionComponent::ResetPracticeProps);
		Input->BindKey(EKeys::F1, IE_Pressed, this, &UBBSBFieldActionComponent::ToggleHelp);
		Input->BindKey(EKeys::F7, IE_Pressed, this, &UBBSBFieldActionComponent::NextPrototypeWeapon);
		Input->BindKey(EKeys::One, IE_Pressed, this, &UBBSBFieldActionComponent::WeaponOne);
		Input->BindKey(EKeys::Two, IE_Pressed, this, &UBBSBFieldActionComponent::WeaponTwo);
		Input->BindKey(EKeys::Three, IE_Pressed, this, &UBBSBFieldActionComponent::WeaponThree);
		Input->BindKey(EKeys::Four, IE_Pressed, this, &UBBSBFieldActionComponent::WeaponFour);
		Input->BindKey(EKeys::Five, IE_Pressed, this, &UBBSBFieldActionComponent::WeaponFive);
		Input->BindKey(EKeys::Six, IE_Pressed, this, &UBBSBFieldActionComponent::WeaponSix);
	}
	BoundInput = Input;
	UE_LOG(LogBBSB, Display, TEXT("Field controls ready: %s"), *Character->GetName());
}

void UBBSBFieldActionComponent::PrimaryDown() { BeginGesture(0); }
void UBBSBFieldActionComponent::PrimaryUp() { EndGesture(0); }
void UBBSBFieldActionComponent::SecondaryDown() { BeginGesture(1); }
void UBBSBFieldActionComponent::SecondaryUp() { EndGesture(1); }
void UBBSBFieldActionComponent::RotateHeldObjectUp() { RotateHeldObject(1); }
void UBBSBFieldActionComponent::RotateHeldObjectDown() { RotateHeldObject(-1); }
void UBBSBFieldActionComponent::RotateHeldObject(float Steps)
{
	if(bEquipmentWheelOpen || bEquipmentInputBlocked || !IsGestureChannelHeld(0)) return;
	if(auto* Buddy=Cast<ABBSBBunniarCompanion>(FieldEquipment);Buddy && Buddy->bSurfaceAttached && IsValid(Buddy->SurfaceComponent))
		if(auto* Object=Cast<ABBSBCarryObject>(Buddy->SurfaceComponent->GetOwner())) Object->RotateFromWheel(Steps);
}
void UBBSBFieldActionComponent::ToggleHelp() { bHelpVisible = !bHelpVisible; }

void UBBSBFieldActionComponent::FieldGesturePress(int32 Channel)
{
	bSyntheticInput = true;
	BeginGesture(Channel);
}
void UBBSBFieldActionComponent::FieldGestureRelease(int32 Channel)
{
	EndGesture(Channel);
	if (!IsGestureCaptured()) bSyntheticInput = false;
}
void UBBSBFieldActionComponent::FieldGestureMotion(float X, float Y) { SyntheticMotion += FVector2D(X,Y); }

void UBBSBFieldActionComponent::ExerciseGesture(int32 Index)
{
	if (Index < 0 || Index > 4 || bDodging) return;
	for (auto& Channel : Channels) Channel.Cancel();
	CancelCurrentAction(true); Arbiter.Clear();
	ExerciseMode = Index;
	ExerciseStartedAt = GetWorld()->GetTimeSeconds();
	FieldGesturePress(0);
}

void UBBSBFieldActionComponent::BeginGesture(int32 Channel)
{
 if (bEquipmentWheelOpen) return;
 if(Channel==1 && CanUseBunniarZoom()) { bBunniarZoomHeld=true;return; }
 if (auto* Player=Cast<ABBSBCharacter>(Character);Player && Player->Traversal)
 {
  if((Player->Traversal->IsWireTraversalActive() || Player->Traversal->IsParkourActive())) return;
  if (Player->Traversal->bGliding && FieldEquipment && FieldEquipment==BunniarEquipment)
  {
   if (Channel!=0 || !Player->Traversal->PrepareSurfaceAttachment()) return;
  }
  else if (Player->Traversal->bGlideDashing) return;
 }
 if (PendingEquipmentId!=INDEX_NONE) return;
	if (!Character || bDodging || Channel < 0 || Channel > 1 || Channels[Channel].bHeld) return;
	if (EmitGesture(EBBSBFieldGesture::Beat, Channel, 0.f,false)) Channels[Channel].Begin(GetWorld()->GetTimeSeconds());
}

void UBBSBFieldActionComponent::EndGesture(int32 Channel)
{
 if(Channel==1 && bBunniarZoomHeld) { bBunniarZoomHeld=false;return; }
	if (!Character || Channel < 0 || Channel > 1 || !Channels[Channel].bHeld) return;
	const double Now = GetWorld()->GetTimeSeconds();
	UpdateMouseMotion(Now);
	const bool MouseGesture=RequiresMouseGesture(Channel);
	const float Charge = FMath::Clamp((Now - Channels[Channel].PressedAt - HoldThreshold) / 1.1, 0.0, 1.0);
	const EBBSBFieldGesture Gesture=Channels[Channel].End(Now, GetGestureSettings());
	const bool bAccepted=(FieldEquipment || PrototypeWeapon==EBBSBPrototypeWeapon::Legacy || MouseGesture || PrototypeWeapon==EBBSBPrototypeWeapon::Hammer) ? EmitGesture(Gesture,Channel,Charge) : false;
	const double Finish=bAccepted && CurrentAction.IsSet() ? CurrentAction->StartedAt+CurrentAction->Duration : Now;
	if (MouseGesture) CameraResumeAt=FMath::Max(CameraResumeAt,Finish+(Gesture==EBBSBFieldGesture::Flick ? FlickSettleSeconds : 0.07f));
}

bool UBBSBFieldActionComponent::IsGestureCaptured() const { return Channels[0].bHeld || Channels[1].bHeld; }

bool UBBSBFieldActionComponent::CanUseBunniarZoom() const
{
 const auto* Buddy=Cast<ABBSBBunniarCompanion>(FieldEquipment);
 const auto* Player=Cast<ABBSBCharacter>(Character);
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 const auto* Dialogue=Character ? Character->FindComponentByClass<UBBSBInteractionComponent>():nullptr;
 return Buddy && !Buddy->bSurfaceAttached && !Buddy->bAirHovering && PC && PC->IsLocalController() && !PC->IsMoveInputIgnored() &&
  !bEquipmentWheelOpen && !bEquipmentInputBlocked && !bDodging && PendingEquipmentId==INDEX_NONE &&
  !(Dialogue && Dialogue->bDialogueActive) && !(Combat && (Combat->bDead || Combat->IsStageLocked() || Combat->BlocksMovement())) &&
  !(Player && Player->Traversal && (Player->Traversal->bGlideDashing || Player->Traversal->IsWireTraversalActive() || Player->Traversal->IsParkourActive()));
}

float UBBSBFieldActionComponent::GetCharge() const
{
	if (!FieldEquipment && PrototypeWeapon!=EBBSBPrototypeWeapon::Legacy) return WeaponChargeSeconds/FMath::Max(ChargeMaximumSeconds,0.01f);
	float Charge = 0.f;
	for (const auto& Channel : Channels)
		if (Channel.bHeld) Charge = FMath::Max(Charge, static_cast<float>(FMath::Clamp((GetWorld()->GetTimeSeconds() - Channel.PressedAt - HoldThreshold) / 1.1, 0.0, 1.0)));
	return Charge;
}

void UBBSBFieldActionComponent::TickComponent(float DT, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	if (TickFunction==&WeaponViewTick) { UpdateWeaponPresentation(); UpdateAimPresentation(); return; }
	Super::TickComponent(DT, TickType, TickFunction);
	// Unpossession destroys the pawn input component; invalidate our binding cache before it is recreated.
	if (!Character || !Character->IsLocallyControlled()) { BoundInput.Reset(); ClearHover(); return; }
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) { BoundInput.Reset(); return; }
	BindControls();
 if(bBunniarZoomHeld && (!CanUseBunniarZoom() || (!bSyntheticInput && !PC->IsInputKeyDown(EKeys::RightMouseButton)) ||
    (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive()))) bBunniarZoomHeld=false;
	if (!FloatingWeapon) CreateWeapon();
	TickEquipmentSelection();
	if (!bHUDInitialized && bShowPracticeHUD)
	{
		PC->ClientSetHUD(ABBSBFieldPracticeHUD::StaticClass());
		bHUDInitialized = true;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Arbiter.IsBusy(Now) && Arbiter.State!=EBBSBActionState::Idle && !bDodging)
	{
		if (PrototypeWeapon==EBBSBPrototypeWeapon::Legacy || (!CurrentAction.IsSet() && !OtherWeaponAction.IsSet())) { CurrentAction.Reset(); Arbiter.Clear(); ActiveState=EBBSBActionState::Idle; }
	}
	if (ExerciseMode >= 0)
	{
		const float Age = Now - ExerciseStartedAt;
		const float PreviousAge = FMath::Max(0.f,Age-DT);
		if (ExerciseMode == 2) SyntheticMotion.X += 45.f*(FMath::Sin(Age*16*PI)-FMath::Sin(PreviousAge*16*PI));
		if (ExerciseMode == 3)
			SyntheticMotion.X += 1800.f*FMath::Max(0.f,FMath::Min(Age,0.42f)-FMath::Max(PreviousAge,0.30f));
		const float EndAt = ExerciseMode == 0 ? 0.06f : ExerciseMode == 4 ? 0.12f : ExerciseMode == 3 ? 0.42f : 0.65f;
		if (Age >= EndAt) { FieldGestureRelease(0); ExerciseMode=-1; }
	}
	// Losing window focus must never leave a held gesture or camera capture latched.
	if (!bSyntheticInput)
	{
		if (Channels[0].bHeld && !PC->IsInputKeyDown(EKeys::LeftMouseButton)) Channels[0].Cancel();
		if (Channels[1].bHeld && !PC->IsInputKeyDown(EKeys::RightMouseButton)) Channels[1].Cancel();
	}
	UpdateMouseMotion(Now);

	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (bDodging)
	{
		const float Age = Now - DodgeStartedAt;
		if (Age >= DodgeDuration || !(Move->IsMovingOnGround() || Move->IsFalling())) FinishDodge(Age>=DodgeDuration && (Move->IsMovingOnGround() || Move->IsFalling()));
		else
		{
			// Average the easing curve over the frame so distance is stable at different FPS.
			const float T0 = FMath::Clamp(Age / DodgeDuration, 0.f, 1.f);
			const float T1 = FMath::Clamp((Age + DT) / DodgeDuration, 0.f, 1.f);
			const float ExitSpeed=RunSpeed*MovementIntent().Size();
            // The completion frame can extend past the dash interval; its run portion still covers the whole tick.
            const float DistanceThisFrame=ExitSpeed*DT+(DodgeDistance-ExitSpeed*DodgeDuration)*(FMath::Cos(PI*T0)-FMath::Cos(PI*T1))*.5f;
			const FVector PlanarVelocity=DodgeDirection*(DistanceThisFrame/FMath::Max(DT,0.0001f));
			Move->Velocity.X=PlanarVelocity.X;Move->Velocity.Y=PlanarVelocity.Y;
            // The directional blend space supplies the body lean and the leg motion.
		}
	}
	else
	{
		const auto* Player=Cast<ABBSBCharacter>(Character);
		Move->MaxWalkSpeed=Player && Player->Traversal && Player->Traversal->bGliding ? Player->Traversal->GlideSpeed:RunSpeed;
		if (!CanMove()) { Character->ConsumeMovementInputVector(); Move->Velocity.X=Move->Velocity.Y=0; }
	}
	if (Camera)
	{
		const UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
		const bool LockedFrame=Combat && Combat->IsStageLocked();
		const float ExtraFOV = bDodging ? 5.f : FMath::Clamp(Move->Velocity.Size2D() / RunSpeed,0.f,1.f) * 2.f;
  const float TargetFOV=bBunniarZoomHeld ? FMath::Min(OriginalFOV,BunniarZoomFOV):OriginalFOV+ExtraFOV;
		Camera->SetFieldOfView(LockedFrame ? OriginalFOV:FMath::FInterpTo(Camera->FieldOfView,TargetFOV,DT,7.f));
	}
	TickWeapon(DT);
	UpdateHover();
}

void UBBSBFieldActionComponent::UpdateMouseMotion(double Now)
{
	if (SampledMouseFrame == GFrameCounter) return;
	SampledMouseFrame = GFrameCounter;
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PC) return;
	float MX=0.f, MY=0.f;
	if (PC->PlayerInput)
	{
		MX=PC->PlayerInput->GetRawKeyValue(EKeys::MouseX);
		MY=PC->PlayerInput->GetRawKeyValue(EKeys::MouseY);
	}
	const FVector2D Motion=bEquipmentWheelOpen ? FVector2D::ZeroVector:bSyntheticInput ? SyntheticMotion : FVector2D(MX,MY);
	SyntheticMotion=FVector2D::ZeroVector;
	if (FieldEquipment && bEquipmentPointerCaptured && !bEquipmentWheelOpen) EquipmentPointerMotion.ExecuteIfBound(FVector2D(Motion.X,-Motion.Y));
	const auto Settings=GetGestureSettings();
	for (int32 Index=0; Index<2; ++Index)
		for (auto Event : Channels[Index].Update(Now,Motion,Settings)) EmitGesture(Event,Index,GetCharge());
	TickPrototypeState();
}

void UBBSBFieldActionComponent::RecordMoveInput(float Right, float Forward)
{
	LastMove = FVector2D(Right, Forward);
	LastMoveAt = GetWorld()->GetTimeSeconds();
}

bool UBBSBFieldActionComponent::TryDodge()
{
 if(const auto* Dialogue=Character ? Character->FindComponentByClass<UBBSBInteractionComponent>():nullptr;Dialogue && Dialogue->bDialogueActive) return false;
	if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && (P->Traversal->IsWireTraversalActive() || P->Traversal->IsParkourActive())) return false;
 if (!Character || bDodging || GetDodgeCooldownRemaining() > 0.f) return false;
	if (!Arbiter.CanDodge(GetWorld()->GetTimeSeconds())) return false;
	UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
	if (Combat && (!Combat->CanDodge() || Combat->bDead)) return false;
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!Move->IsMovingOnGround() && !Move->IsFalling()) return false;
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return false;
 DodgeDirection=MovementDashDirection();
 if(bFaceCameraWhileMoving && MovementIntent().IsNearlyZero())
 {
  FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);DodgeDirection=FRotator(0,View.Yaw,0).Vector();
 }
	OriginalFriction = Move->GroundFriction;
	OriginalBraking = Move->BrakingDecelerationWalking;
	OriginalFallingBraking=Move->BrakingDecelerationFalling;
	OriginalBrakingFactor = Move->BrakingFrictionFactor;
	bOriginalOrientToMovement = Move->bOrientRotationToMovement;
	Move->GroundFriction = 0.f;
	Move->BrakingDecelerationWalking = 0.f;
	Move->BrakingDecelerationFalling=0.f;
	Move->BrakingFrictionFactor = 0.f;
	Move->bOrientRotationToMovement = false;
	Character->ConsumeMovementInputVector();
 // A leg-only dash must not replace the active weapon's arbiter revision or recall its model.
 const bool PreserveWeapon=!FieldEquipment && (CurrentAction.IsSet() || OtherWeaponAction.IsSet() || bSpearEmbedded || IsWeaponReturning());
 if(!PreserveWeapon)
 {
  CancelCurrentAction(true);bSyntheticInput=false;ExerciseMode=-1;
 }
 bDodging = true;bDodgeExitSettling=false;
	DodgeStartedAt = LastDodgeAt = GetWorld()->GetTimeSeconds();
	FBBSBWeaponActionRule DodgeRule; DodgeRule.Duration=DodgeDuration; DodgeRule.Priority=60;
 if(!PreserveWeapon) { Arbiter.Start(EBBSBActionState::Dodge,DodgeRule,-1,DodgeStartedAt);ActiveState=EBBSBActionState::Dodge; }
	CameraResumeAt=DodgeStartedAt+0.08;
	if (Combat) Combat->OnDodge();
	UE_LOG(LogBBSB, Display, TEXT("Field Dodge dir=%s"), *DodgeDirection.ToCompactString());
	return true;
}

FVector UBBSBFieldActionComponent::MovementIntent() const
{
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 if(!PC || !PC->IsLocalController() || PC->IsMoveInputIgnored() || !GetWorld() ||
    (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive())) return FVector::ZeroVector;
 FVector2D Input=GetWorld()->GetTimeSeconds()-LastMoveAt<.12 ? LastMove:FVector2D::ZeroVector;
 const float Right=float(PC->IsInputKeyDown(EKeys::D))-float(PC->IsInputKeyDown(EKeys::A));
 const float Forward=float(PC->IsInputKeyDown(EKeys::W))-float(PC->IsInputKeyDown(EKeys::S)||PC->IsInputKeyDown(EKeys::X));
 if(Right!=0 || Forward!=0) Input=FVector2D(Right,Forward);
 FRotator View=PC->GetControlRotation();if(IsSurfaceAimLocked()) { FVector Eye;PC->GetPlayerViewPoint(Eye,View); }
 return FRotator(0,View.Yaw,0).RotateVector(FVector(Input.Y,Input.X,0).GetClampedToMaxSize(1.f));
}
float UBBSBFieldActionComponent::GetDodgeAge() const
{
 return bDodging && GetWorld() ? FMath::Max(0.f,float(GetWorld()->GetTimeSeconds()-DodgeStartedAt)):0.f;
}
float UBBSBFieldActionComponent::GetDodgeVisualAge() const
{
 if(bDodging) return GetDodgeAge();
 if(bDodgeExitSettling && Character && CanMove() && Character->GetCharacterMovement()->IsMovingOnGround())
 {
  const float Age=GetWorld()->GetTimeSeconds()-DodgeExitStartedAt;if(Age<(bDodgeExitToRun ? .06f:.1f)) return DodgeDuration+Age;
 }
 return -1.f;
}
float UBBSBFieldActionComponent::GetDodgeDirectionAngle() const
{
 const FVector Local=Character ? Character->GetActorQuat().UnrotateVector(DodgeDirection):FVector::ForwardVector;
 return FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X));
}
void UBBSBFieldActionComponent::UpdateMovementFacing()
{
 if(!Character || !Character->IsLocallyControlled()) return;
 auto* Move=Character->GetCharacterMovement();const auto* Player=Cast<ABBSBCharacter>(Character);
 const auto* PC=Cast<APlayerController>(Character->GetController());
 if(!PC || PC->IsMoveInputIgnored()) return;
 if(Player && Player->Traversal)
 {
  if(Player->Traversal->IsParkourActive() || Player->Traversal->IsWireTraversalActive()) return;
  if(Player->Traversal->IsBunniarTraversalActive()) { Move->bOrientRotationToMovement=true;return; }
 }
 if(!bFaceCameraWhileMoving) { if(!bDodging && CanMove()) Move->bOrientRotationToMovement=true;return; }
 if(!bDodging && !CanMove()) return;
 if(!bDodging && !Move->IsMovingOnGround()) { Move->bOrientRotationToMovement=true;return; }
 Move->bOrientRotationToMovement=false;
 if(bDodging || !MovementIntent().IsNearlyZero() || Move->Velocity.SizeSquared2D()>FMath::Square(20.f))
 {
  FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);Character->SetActorRotation(FRotator(0,View.Yaw,0));
 }
}
void UBBSBFieldActionComponent::FinishDodge(bool bContinueMovement)
{
 if(!bContinueMovement) bDodgeExitSettling=false;
 if(!bDodging || !Character) return;
 auto* Move=Character->GetCharacterMovement();
 Move->GroundFriction=OriginalFriction;Move->BrakingDecelerationWalking=OriginalBraking;
 Move->BrakingDecelerationFalling=OriginalFallingBraking;Move->BrakingFrictionFactor=OriginalBrakingFactor;
 Move->bOrientRotationToMovement=bOriginalOrientToMovement;
 Character->GetMesh()->SetRelativeRotation(OriginalMeshRotation);bDodging=false;
 if(Arbiter.State==EBBSBActionState::Dodge) { Arbiter.Clear();ActiveState=EBBSBActionState::Idle; }
 const FVector Intent=bContinueMovement && CanMove() ? MovementIntent():FVector::ZeroVector;
 bDodgeExitSettling=bContinueMovement && CanMove() && Move->IsMovingOnGround();bDodgeExitToRun=!Intent.IsNearlyZero();DodgeExitStartedAt=GetWorld()->GetTimeSeconds();
 if(bContinueMovement) UBBSBMovementAuthoring::MatchAfterDash(Character->GetMesh()->GetAnimInstance());
 Move->MaxWalkSpeed=RunSpeed;Move->Velocity.X=Intent.X*RunSpeed;Move->Velocity.Y=Intent.Y*RunSpeed;
 if(!Intent.IsNearlyZero() && Character->GetPendingMovementInputVector().IsNearlyZero()) Character->AddMovementInput(Intent.GetSafeNormal(),Intent.Size());
 if(bContinueMovement) UpdateMovementFacing();
}

bool UBBSBFieldActionComponent::IsDodgeInvulnerable() const
{
	if (!bDodging || !GetWorld()) return false;
	const float Age = GetWorld()->GetTimeSeconds() - DodgeStartedAt;
	return Age >= InvulnerableStart && Age <= FMath::Min(InvulnerableEnd,DodgeDuration);
}

float UBBSBFieldActionComponent::GetDodgeCooldownRemaining() const
{
	return GetWorld() ? FMath::Max(0.f, static_cast<float>(LastDodgeAt+DodgeCooldown-GetWorld()->GetTimeSeconds())) : 0.f;
}

FString UBBSBFieldActionComponent::GetMovementLabel() const
{
	if (bDodging) return TEXT("DASH");
 if (const auto* Player=Cast<ABBSBCharacter>(Character);Player && Player->Traversal)
 {
  if (Player->Traversal->bGlideDashing) return TEXT("BUNNIAR DASH");
  if(Player->Traversal->IsWireTraversalActive()) return TEXT("Wire");
  if (Player->Traversal->bGliding) return TEXT("GLIDE");
 }
 if (Character && Character->GetCharacterMovement()->IsFalling()) return TEXT("AIR");
	if (Arbiter.IsBusy(GetWorld()->GetTimeSeconds())) return ActiveState==EBBSBActionState::Attack ? TEXT("ATTACK") : TEXT("BUSY");
	if (!Character || Character->GetVelocity().Size2D() < 5.f) return TEXT("IDLE");
	return Character->GetCharacterMovement()->MaxWalkSpeed <= WalkSpeed+1.f ? TEXT("WALK") : TEXT("RUN");
}

FVector UBBSBFieldActionComponent::FindAimPoint() const
{
	const APlayerController* PC = IsValid(Character) ? Cast<APlayerController>(Character->GetController()):nullptr;
	if (!IsValid(PC) || !PC->IsLocalController() || !GetWorld()) return FindPrototypeCursorPoint();
	FVector Eye; FRotator Rotation;
	PC->GetPlayerViewPoint(Eye,Rotation);
	const UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
	const bool bSelectedAim=Combat && Combat->IsStageLocked() && IsValid(Combat->SelectedEnemy);
	const FVector End = Eye + Rotation.Vector() * 2500.f;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBFieldAim),false,Character);
	if (FloatingWeapon) Query.AddIgnoredActor(FloatingWeapon);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit,Eye,End,ECC_Visibility,Query);
	const FVector Shoulder = Character->GetActorLocation() + FVector(0,0,35);
	FVector Target = bSelectedAim ? Combat->AimPoint() : bHit ? Hit.ImpactPoint : Eye + Rotation.Vector() * WeaponReach;
	FVector Delta = Target - Shoulder;
	if (Delta.Size() > WeaponReach) Target = Shoulder + Delta.GetSafeNormal()*WeaponReach;
	if (Delta.Size() < 100.f) Target = Shoulder + Rotation.Vector()*100.f;
	// A shoulder-to-target check prevents a camera peeking around a wall from attacking through it.
	if (GetWorld()->LineTraceSingleByChannel(Hit,Shoulder,Target,ECC_Visibility,Query)) Target = Hit.ImpactPoint;
	return Target;
}

void UBBSBFieldActionComponent::CreateWeapon()
{
	if (PrototypeWeapon!=EBBSBPrototypeWeapon::Legacy) { CreatePrototypeWeapon(); return; }
	FActorSpawnParameters Spawn;
	Spawn.Owner = Character;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FloatingWeapon = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),Character->GetActorTransform(),Spawn);
	if (!FloatingWeapon) return;
	USceneComponent* Root = NewObject<USceneComponent>(FloatingWeapon,TEXT("WeaponRoot"));
	FloatingWeapon->AddInstanceComponent(Root);
	FloatingWeapon->SetRootComponent(Root);
	Root->RegisterComponent();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
	Blade = AddWeaponPart(FloatingWeapon,Cube,TEXT("FloatingBlade"),FVector(0,0,28),FVector(0.14f,0.055f,0.95f),FLinearColor(0.76f,0.67f,0.82f));
	AddWeaponPart(FloatingWeapon,Cube,TEXT("Guard"),FVector(0,0,-16),FVector(0.4f,0.08f,0.055f),FLinearColor(0.68f,0.53f,0.3f));
	AddWeaponPart(FloatingWeapon,Cube,TEXT("Grip"),FVector(0,0,-29),FVector(0.07f,0.07f,0.2f),FLinearColor(0.6f,0.25f,0.4f));
	FloatingWeapon->SetActorTransform(GetWeaponRestTransform());
	LastWeaponPosition = FloatingWeapon->GetActorLocation();
}

FTransform UBBSBFieldActionComponent::GetWeaponRestTransform() const
{
	FVector Eye = Character->GetActorLocation();
	FRotator View = Character->GetActorRotation();
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (PC) PC->GetPlayerViewPoint(Eye,View);
	const FVector Forward = View.Vector();
	const FVector Right = FRotationMatrix(View).GetUnitAxis(EAxis::Y);
	FVector Position = Character->GetActorLocation()+Forward*55.f+Right*190.f+FVector(0,0,35);
	if (PC)
	{
		int32 Width=0, Height=0; PC->GetViewportSize(Width,Height);
		FVector2D Anchor(Width*WeaponRestScreenPosition.X,Height*WeaponRestScreenPosition.Y);
		FVector2D PlayerScreen;
		if (PC->ProjectWorldLocationToScreen(Character->GetActorLocation(),PlayerScreen))
		{
			Anchor.X=FMath::Clamp(Width-PlayerScreen.X,Width*0.05,Width*0.95);
			Anchor.Y=FMath::Clamp(PlayerScreen.Y,Height*0.05,Height*0.95);
		}
		FVector RayOrigin, RayDirection;
		if (Width>0 && Height>0 && PC->DeprojectScreenPositionToWorld(
			Anchor.X,Anchor.Y,RayOrigin,RayDirection))
		{
			// Equal screen-space distance on opposite sides of the crosshair, at the player's depth.
			const double Depth = FMath::Max(100.0,FVector::DotProduct(Character->GetActorLocation()-Eye,Forward));
			const double AlongView = FVector::DotProduct(RayDirection,Forward);
			if (AlongView>0.01f)
				Position = RayOrigin+RayDirection*((Depth-FVector::DotProduct(RayOrigin-Eye,Forward))/AlongView);
		}
	}
	Position=Character->GetActorLocation()+(Position-Character->GetActorLocation()).GetClampedToMaxSize(FMath::Max(70.f,RestEquipmentDistance));
	const FQuat Rotation = View.Quaternion()*FRotator(10,0,-15).Quaternion();
	return FTransform(Rotation,Position);
}

void UBBSBFieldActionComponent::UpdateWeaponPresentation()
{
 // Traversal borrows the companion without changing the selected weapon or its form.
 for (AActor* Visual:{FloatingWeapon.Get(),OtherFloatingWeapon.Get(),ConductorWeapon.Get(),OtherConductorWeapon.Get()}) if (Visual) Visual->SetActorHiddenInGame(false);
	if (IsValid(FieldEquipment))
	{
		if (FloatingWeapon) FloatingWeapon->SetActorHiddenInGame(true);
		if (OtherFloatingWeapon) OtherFloatingWeapon->SetActorHiddenInGame(true);
		// Reparaton remains in the hand; its ordinary complete form conducts the tool.
		if (ConductorWeapon) ConductorWeapon->SetActorHiddenInGame(false);
		if (OtherConductorWeapon) OtherConductorWeapon->SetActorHiddenInGame(true);
		if (ConductorFull) ConductorFull->SetVisibility(true);
		for (auto* Part:{FormUpper.Get(),FormLower.Get(),OtherFormLower.Get()}) if (Part) Part->SetVisibility(false);
		return;
	}
	if (PrototypeWeapon!=EBBSBPrototypeWeapon::Legacy) { PresentPrototypeWeapon(); return; }
	if (!Character || !Character->IsLocallyControlled() || !IsValid(FloatingWeapon) || CurrentAction.IsSet()) return;
	APlayerController* PC=Cast<APlayerController>(Character->GetController());
	if (!PC) return;
	// PostUpdateWork runs after the camera manager, so this pose uses the view rendered this frame.
	FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
	const FTransform ViewTransform(View,Eye);
	const double Now=GetWorld()->GetTimeSeconds();
	if (bWeaponWasAttacking)
	{
		WeaponReturnFromView=FloatingWeapon->GetActorTransform().GetRelativeTransform(ViewTransform);
		WeaponReturnStartedAt=Now; bWeaponWasAttacking=false;
	}
	FTransform Pose=GetWeaponRestTransform();
	const float ReturnAlpha=FMath::Clamp((Now-WeaponReturnStartedAt)/0.08,0.0,1.0);
	if (ReturnAlpha<1.f)
	{
		FTransform RelativePose;
		RelativePose.Blend(WeaponReturnFromView,Pose.GetRelativeTransform(ViewTransform),ReturnAlpha);
		Pose=RelativePose*ViewTransform;
	}
	FloatingWeapon->SetActorHiddenInGame(!ResolveRestEquipmentPose(Pose,FloatingWeapon->CalculateComponentsBoundingBoxInLocalSpace(true),FloatingWeapon,false,ReturnAlpha>=1.f));
	FloatingWeapon->SetActorTransform(Pose);
	LastWeaponPosition=Pose.GetLocation();
	bWeaponWasActive=false;
}

bool UBBSBFieldActionComponent::EmitGesture(EBBSBFieldGesture Gesture, int32 Channel, float Charge,bool bGestureChain)
{
 if(const auto* Dialogue=Character ? Character->FindComponentByClass<UBBSBInteractionComponent>():nullptr;Dialogue && Dialogue->bDialogueActive && !bGestureChain) return false;
	if (IsValid(FieldEquipment))
	{
		const UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
		const bool Continuation=bGestureChain && Arbiter.State==EBBSBActionState::Attack;
  const auto* Buddy=Cast<ABBSBBunniarCompanion>(FieldEquipment);
  const bool WireRequest=!bGestureChain && Gesture==EBBSBFieldGesture::Beat && Channel==1 && Buddy && (Buddy->bAirHovering || Buddy->bSurfaceViewLocked);
  if (bDodging || (Arbiter.IsBusy(GetWorld()->GetTimeSeconds()) && !Continuation && !WireRequest) || (Combat && !Combat->CanAttack())) return false;
		if (EquipmentGesture.IsBound() && !EquipmentGesture.Execute(Gesture,Channel,Charge,!bGestureChain)) return false;
		LastGesture=Gesture; ++GestureCounts[int32(Gesture)]; GestureTimes[int32(Gesture)]=GetWorld()->GetTimeSeconds();
		OnGesture.Broadcast(Gesture,Channel,Charge);
  if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && P->Traversal->IsWireTraversalActive()) return true;
		if (Gesture==EBBSBFieldGesture::Beat && !bGestureChain)
		{
			FBBSBWeaponActionRule Rule; Rule.Duration=.18f; Rule.bLockMovement=false; Rule.bOccupyConductor=true;
			Arbiter.Start(EBBSBActionState::Attack,Rule,Channel,GetWorld()->GetTimeSeconds()); ActiveState=EBBSBActionState::Attack;
			if (GestureAnimations.Num()>0 && GestureAnimations[0]) if (UAnimInstance* Anim=Character->GetMesh()->GetAnimInstance())
				ActionMontage=Anim->PlaySlotAnimationAsDynamicMontage(GestureAnimations[0],TEXT("DefaultSlot"),.04f,.08f,GestureAnimations[0]->GetPlayLength()/.3f);
		}
		return true;
	}
	if (PrototypeWeapon!=EBBSBPrototypeWeapon::Legacy) return PrototypeGesture(Gesture,Channel);
	const double Now=GetWorld()->GetTimeSeconds();
	const FBBSBWeaponActionRule Rule=GetWeaponRule(Gesture,Channel);
	UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
	if (!Arbiter.CanAttack(Rule,Channel,Now,bGestureChain) || (Combat && !Combat->CanAttack()))
	{
		++RejectedActions; return false;
	}
	CancelCurrentAction(false);
	if (Gesture==EBBSBFieldGesture::Beat) AcquireWeaponAim(Channel);
	Arbiter.Start(EBBSBActionState::Attack,Rule,Channel,Now); ActiveState=EBBSBActionState::Attack;
	if (Rule.bLockMovement) { Character->ConsumeMovementInputVector(); Character->GetCharacterMovement()->StopMovementImmediately(); }
	if (Combat) Combat->OnAttackStarted();
	LastGesture = Gesture;
	const int32 Index = static_cast<int32>(Gesture);
	++GestureCounts[Index];
	GestureTimes[Index] = GetWorld()->GetTimeSeconds();
	OnGesture.Broadcast(Gesture,Channel,Charge);
	UE_LOG(LogBBSB, Display, TEXT("Field Gesture %s channel=%d charge=%.2f"), GestureName(Gesture),Channel,Charge);
	FWeaponAction Action;
	Action.Gesture = Gesture;
	Action.Channel = Channel;
	Action.Charge = Charge;
	Action.StartedAt=Now; Action.Revision=Arbiter.Revision; Action.OwnerOrigin=Character->GetActorLocation();
	Action.DamageScale=Rule.DamageMultiplier*(Combat ? Combat->DamageMultiplier() : 1.f);
	Action.Target = bWeaponAimLocked ? LockedAimPoint : FindAimPoint();
	LastAimPoint=Action.Target; LastTraceSummary=TEXT("No active sweep hit");
	Action.Direction = (Action.Target - Character->GetActorLocation()).GetSafeNormal2D();
	if (Action.Direction.IsNearlyZero()) Action.Direction = Character->GetActorForwardVector();
	Action.Duration=FMath::Max(Rule.Duration,0.08f);
	CurrentAction.Emplace(MoveTemp(Action));
	PlayGestureAnimation(Gesture);
	if (Blade) TintMesh(Blade,GestureColor(Gesture));
	bWeaponWasActive=false;
	return true;
}

void UBBSBFieldActionComponent::PlayGestureAnimation(EBBSBFieldGesture Gesture)
{
	if (GestureAnimations.Num() < 4) return;
	int32 Index = 0;
	if (Gesture == EBBSBFieldGesture::Hold) return; // Charging leaves locomotion intact.
	if (Gesture == EBBSBFieldGesture::Shake) Index = 2;
	if (Gesture == EBBSBFieldGesture::Flick) Index = 3;
	if (Gesture == EBBSBFieldGesture::Dive) Index = 1;
	const auto& AnimationAction=CurrentAction.IsSet() ? CurrentAction : OtherWeaponAction;
	if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
		if (GestureAnimations[Index] && AnimationAction.IsSet())
			ActionMontage=Anim->PlaySlotAnimationAsDynamicMontage(GestureAnimations[Index],TEXT("DefaultSlot"),0.045f,0.06f,GestureAnimations[Index]->GetPlayLength()/AnimationAction->Duration,1,0.06f);
}

void UBBSBFieldActionComponent::TickWeapon(float DT)
{
	if (FieldEquipment) return;
	if (PrototypeWeapon!=EBBSBPrototypeWeapon::Legacy) { TickPrototypeWeapon(DT); return; }
	if (!FloatingWeapon) return;
	if (!CurrentAction.IsSet())
	{
		bWeaponWasActive = false;
		return;
	}
	bWeaponWasAttacking=true;
	FWeaponAction& Action = CurrentAction.GetValue();
	Action.Elapsed = GetWorld()->GetTimeSeconds()-Action.StartedAt;
	const uint32 Revision=Action.Revision;
	const float T = FMath::Clamp(Action.Elapsed / Action.Duration,0.f,1.f);
	const FVector Forward = Action.Direction;
	const FVector Right = FVector::CrossProduct(FVector::UpVector,Forward);
	const float Side = Action.Channel == 0 ? 1.f : -1.f;
	FVector Position = Action.Target;
	if (!Arbiter.Rule.bLockMovement) Position+=Character->GetActorLocation()-Action.OwnerOrigin;
	FRotator Rotation = Forward.Rotation();
	switch (Action.Gesture)
	{
	case EBBSBFieldGesture::Beat:
		Position += Right * FMath::Lerp(-105.f,105.f,T)*Side + FVector(0,0,FMath::Sin(PI*T)*30.f);
		Rotation += FRotator(0,0,FMath::Lerp(-70.f,70.f,T)*Side);
		break;
	case EBBSBFieldGesture::Hold:
		Position += FVector(0,0,8.f+FMath::Sin(PI*T)*8.f);
		Rotation += FRotator(0,0,180.f*T);
		break;
	case EBBSBFieldGesture::Shake:
		Position += Right * FMath::Cos(4.f*PI*T)*70.f + FVector::UpVector * FMath::Sin(4.f*PI*T)*70.f;
		Rotation += FRotator(0,0,720.f*T*Side);
		break;
	case EBBSBFieldGesture::Flick:
		Position += Forward * FMath::Lerp(-140.f,90.f,T);
		Rotation += FRotator(90,0,0);
		break;
	case EBBSBFieldGesture::Dive:
		Position += FVector(0,0,FMath::Lerp(160.f,-20.f,T*T));
		Rotation += FRotator(0,0,25.f);
		break;
	}
	// Windup travel has no damage; active sweeps start from the authored path.
	if (T < 0.18f) Position = FMath::Lerp(LastWeaponPosition,Position,FMath::Clamp(T/0.18f,0.f,1.f));
	FloatingWeapon->SetActorLocationAndRotation(Position,Rotation);
	const bool bActive = T >= 0.18f && T <= 0.94f;
	if (bActive) TraceWeapon(bWeaponWasActive ? LastWeaponPosition : Position,Position,Action);
	if (Arbiter.Revision!=Revision) return; // Damage may synchronously form a stage or interrupt this action.
	LastWeaponPosition = Position;
	bWeaponWasActive = bActive;
	if (T >= 1.f) { CurrentAction.Reset(); Arbiter.Clear(); ActiveState=EBBSBActionState::Idle; }
}

void UBBSBFieldActionComponent::TraceWeapon(const FVector& From, const FVector& To, FWeaponAction& Action,float Radius)
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBFieldWeapon),false,Character);
	Query.AddIgnoredActor(FloatingWeapon);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(Hits,From,To,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(Radius>=0 ? Radius:WeaponHitRadius),Query);
	if (!Hits.IsEmpty()) LastTraceSummary=FString::Printf(TEXT("%d hits: %s point=%s"),Hits.Num(),*GetNameSafe(Hits[0].GetActor()),*Hits[0].ImpactPoint.ToCompactString());
	for (const FHitResult& Hit : Hits)
	{
		AActor* Actor = Hit.GetActor();
		if (!Actor || Action.HitActors.Contains(Actor)) continue;
		if (Action.bRestrictToAimActor && Actor!=Action.AimActor.Get()) continue;
		ABBSBBreakableProp* Prop = Cast<ABBSBBreakableProp>(Actor);
		ABBSBInkEnemy* Enemy=Cast<ABBSBInkEnemy>(Actor);
		ABBSBCarryObject* Object=Cast<ABBSBCarryObject>(Actor);
  if ((!Prop || Prop->bBroken) && (!Enemy || Enemy->bDefeated) && (!Object || Object->bBroken)) continue;
		// Check each target from the player as well; sweeping around a corner cannot bypass a wall.
		FHitResult Obstruction;
		const FVector Origin = Action.bSwordThrow ? Action.StartPose.GetLocation():Character->GetActorLocation()+FVector(0,0,30);
		if (GetWorld()->LineTraceSingleByChannel(Obstruction,Origin,Hit.ImpactPoint,ECC_Visibility,Query) && Obstruction.GetActor()!=Actor) continue;
		Action.HitActors.Add(Actor);
		const float Damage = Action.bPrototype ? Action.ExactDamage : BaseDamage*Action.DamageScale*(1.f+Action.Charge);
		const FVector Impulse=Action.Impulse;
		const uint32 Revision=Action.Revision;
		const EBBSBFieldGesture Gesture=Action.Gesture;
		const FVector Direction=Action.Direction;
		const bool bPreviouslyBroken = (Prop && Prop->bBroken) || (Object && Object->bBroken);
		EmitAttackImpact(Action,Hit.ImpactPoint,Hit.ImpactNormal);
		UGameplayStatics::ApplyPointDamage(Actor,Damage,Direction,Hit,Character->GetController(),Character,nullptr);
		if (Enemy && !Enemy->bDefeated && !Impulse.IsNearlyZero()) Enemy->ApplyWeaponImpulse(Impulse);
		const bool BrokenNow=(Prop && Prop->bBroken) || (Object && Object->bBroken);
  if (!bPreviouslyBroken && BrokenNow) ++PropsBroken;
		LastHitAt = GetWorld()->GetTimeSeconds();
		LastHitLabel = FString::Printf(TEXT("%s  %.0f%s"),GestureName(Gesture),Damage,BrokenNow ? TEXT("  BREAK!") : Enemy && Enemy->bDefeated ? TEXT("  DOWN!") : TEXT(""));
		UE_LOG(LogBBSB, Display, TEXT("Field Hit %s target=%s hp=%.1f"),GestureName(Gesture),*Actor->GetName(),Prop ? Prop->Health : Enemy ? Enemy->Health : Object->Health);
		if (Arbiter.Revision!=Revision) return;
		if (PrototypeWeapon==EBBSBPrototypeWeapon::Spear && Gesture==EBBSBFieldGesture::Beat
			&& Channels[Action.Channel].bHeld && (!Enemy || !Enemy->bDefeated) && (!Prop || !Prop->bBroken))
		{
			FloatingWeapon->SetActorTransform(PrototypeAttackPose(Action,1.f));
			if (FloatingWeapon->AttachToActor(Actor,FAttachmentTransformRules::KeepWorldTransform))
			{ bSpearEmbedded=true; SpearPiercedActor=Actor; }
		}
	}
}

void UBBSBFieldActionComponent::ResetPracticeProps()
{
	for (TActorIterator<ABBSBBreakableProp> It(GetWorld()); It; ++It) It->ResetProp();
	PropsBroken = 0;
	if (Character) if (UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>()) Combat->ResetEncounter();
	LastHitLabel = TEXT("Targets restored");
	LastHitAt = GetWorld()->GetTimeSeconds();
}

FBBSBWeaponActionRule UBBSBFieldActionComponent::GetWeaponRule(EBBSBFieldGesture Gesture,int32 Channel) const
{
	const auto& Rules=Channel==0 ? PrimaryRules : SecondaryRules;
	const int32 Index=static_cast<int32>(Gesture);
	return BBSBCombatRules::WeaponRuleForPreset(Rules.IsValidIndex(Index) ? Rules[Index] : FBBSBWeaponActionRule(),WeaponExperiment);
}
bool UBBSBFieldActionComponent::SetWeaponExperiment(EBBSBWeaponCommitment Preset)
{
	if (!GetWorld() || !Character || uint8(Preset)>uint8(EBBSBWeaponCommitment::Heavy)) return false;
	if (bDodging || CurrentAction.IsSet() || IsGestureCaptured() || Arbiter.IsBusy(GetWorld()->GetTimeSeconds())) return false;
	const UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
	if (Combat && !Combat->CanAttack()) return false;
	WeaponExperiment=Preset;
	return true;
}
bool UBBSBFieldActionComponent::CanStartJump() const
{
 return Character && GetWorld() && CanMove() && (!Arbiter.IsBusy(GetWorld()->GetTimeSeconds()) || (CurrentAction.IsSet() && (CurrentAction.GetValue().bSwordThrow || CurrentAction.GetValue().bGlideAttack)) || (OtherWeaponAction.IsSet() && OtherWeaponAction.GetValue().bGlideAttack));
}
bool UBBSBFieldActionComponent::CanMove() const
{
 if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && (P->Traversal->IsWireTraversalActive() || P->Traversal->IsParkourActive())) return false;
 if(const auto* Dialogue=Character ? Character->FindComponentByClass<UBBSBInteractionComponent>():nullptr;Dialogue && Dialogue->bDialogueActive) return false;
	if (bEquipmentWheelOpen) return false;
	if (bDodging) return false;
	if (Arbiter.IsBusy(GetWorld()->GetTimeSeconds()) && Arbiter.Rule.bLockMovement) return false;
	const UBBSBCombatComponent* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>() : nullptr;
	return !Combat || !Combat->BlocksMovement();
}
bool UBBSBFieldActionComponent::IsConductorAvailable() const
{
 if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && (P->Traversal->IsWireTraversalActive() || P->Traversal->IsParkourActive())) return false;
	if (bEquipmentWheelOpen) return false;
	return !bDodging && (!Arbiter.IsBusy(GetWorld()->GetTimeSeconds()) || !Arbiter.Rule.bOccupyConductor);
}
float UBBSBFieldActionComponent::GetLookInputScale() const
{
	const UBBSBCombatComponent* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>() : nullptr;
	if (Combat && Combat->IsStageLocked()) return 0.f;
 // Wire commits actions, never the view; skip the old attachment/gesture look-return delay.
 if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && P->Traversal->IsWireTraversalActive()) return 1.f;
	if (bEquipmentWheelOpen || bEquipmentPointerCaptured || FPlatformTime::Seconds()<EquipmentLookResumeAt) return 0.f;
	if (bWeaponAimLocked) return 0.f;
	for (int32 Channel=0;Channel<2;++Channel) if (Channels[Channel].bHeld && RequiresMouseGesture(Channel)) return 0.f;
	if (!GetWorld()) return 1.f;
	const float T=FMath::Clamp(float((GetWorld()->GetTimeSeconds()-CameraResumeAt)/FMath::Max(LookReturnSeconds,0.001f)),0.f,1.f);
	return T*T*(3-2*T);
}
void UBBSBFieldActionComponent::CancelCurrentAction(bool bCancelGesture,bool bCountCancel)
{
 // Capture the visible world pose before clearing the damaging action or detaching an embedded weapon.
 FTransform ReturnFrom[2];bool Return[2]={false,false};
 if(bCancelGesture && !FieldEquipment && PrototypeWeapon!=EBBSBPrototypeWeapon::Legacy && GetWorld())
 {
  const double Now=GetWorld()->GetTimeSeconds();
  for(int32 Index=0;Index<2;++Index)
  {
   AActor* Visual=Index==0 ? FloatingWeapon.Get():OtherFloatingWeapon.Get();
   const bool Action=Index==0 ? CurrentAction.IsSet():OtherWeaponAction.IsSet();
   const bool Returning=bHasFollowThrough[Index] && Now<FollowThroughUntil[Index];
   Return[Index]=IsValid(Visual) && (Action || Returning || (Index==0 && (bSpearEmbedded || bWeaponAimLocked)));
   if(Return[Index]) ReturnFrom[Index]=Visual->GetActorTransform();
  }
 }
 if(CurrentAction.IsSet()) StopBladeTrail(CurrentAction.GetValue());
 if(OtherWeaponAction.IsSet()) StopBladeTrail(OtherWeaponAction.GetValue());
 if(bCountCancel && (CurrentAction.IsSet() || OtherWeaponAction.IsSet())) ++CanceledAttacks;
 CurrentAction.Reset();OtherWeaponAction.Reset();bWeaponWasActive=false;
 if(Character && ActionMontage)
  if(UAnimInstance* Anim=Character->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.045f,ActionMontage);
 ActionMontage=nullptr;
 if(bCancelGesture)
 {
  ResetPrototypeState();ClearWeaponAim();bHasFollowThrough[0]=bHasFollowThrough[1]=false;
  for(auto& Channel:Channels) Channel.Cancel();
  bSyntheticInput=false;ExerciseMode=-1;SyntheticMotion=FVector2D::ZeroVector;
  for(int32 Index=0;Index<2;++Index) if(Return[Index])
  {
   FollowThroughPose[Index]=ReturnFrom[Index];FollowThroughUntil[Index]=GetWorld()->GetTimeSeconds()+FMath::Max(.05f,WeaponReturnSeconds);bHasFollowThrough[Index]=true;
  }
 }
}
void UBBSBFieldActionComponent::InterruptAction(EBBSBActionState State,float Seconds)
{
	const int32 Priority=State==EBBSBActionState::Dead ? 100 : State==EBBSBActionState::Transition ? 90 : 80;
	if (Arbiter.IsBusy(GetWorld()->GetTimeSeconds()) && Arbiter.Rule.Priority>Priority) return;
	FinishDodge(); CancelCurrentAction(true);
	FBBSBWeaponActionRule Rule; Rule.Duration=Seconds; Rule.Priority=Priority; Rule.bDodgeCancelable=false;
	Rule.bOccupyConductor=State!=EBBSBActionState::Hit;
	Arbiter.Start(State,Rule,-1,GetWorld()->GetTimeSeconds()); ActiveState=State;
	CameraResumeAt=GetWorld()->GetTimeSeconds()+0.1;
}
void UBBSBFieldActionComponent::ResetActionState()
{
 bBunniarZoomHeld=false;
	CancelEquipmentSelection();
	FinishDodge(); CancelCurrentAction(true); Arbiter.Clear(); ActiveState=EBBSBActionState::Idle; CameraResumeAt=-100;
}
void UBBSBFieldActionComponent::ClearHover()
{
	for (const FHoverState& State:HoverStates)
		if (UPrimitiveComponent* Primitive=State.Primitive.Get()) { Primitive->SetRenderCustomDepth(State.bDepth); Primitive->SetCustomDepthStencilValue(State.Stencil); }
	if (ABBSBInkEnemy* Enemy=Cast<ABBSBInkEnemy>(HoveredActor))
	{
		const UBBSBCombatComponent* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>() : nullptr;
		Enemy->SetSelected(Combat && Combat->SelectedEnemy==Enemy && !Enemy->bDefeated);
	}
	HoverStates.Reset(); HoveredActor=nullptr;HoverLockedSurface.Reset();HoverBuddy.Reset();
}
void UBBSBFieldActionComponent::UpdateHover()
{
	if (bEquipmentWheelOpen || (Character && Character->FindComponentByClass<UBBSBInteractionComponent>() && Character->FindComponentByClass<UBBSBInteractionComponent>()->bDialogueActive)) { ClearHover(); return; }
	APlayerController* PC=Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController() || (Character->FindComponentByClass<UBBSBCombatComponent>() && Character->FindComponentByClass<UBBSBCombatComponent>()->bDead)) { ClearHover(); return; }
 AActor* Target=nullptr;
 const auto* Buddy=Cast<ABBSBBunniarCompanion>(FieldEquipment);
 UPrimitiveComponent* LockedSurface=nullptr;
 AActor* HighlightBuddy=nullptr;
 if(Buddy && Buddy->bSurfaceAttached && IsValid(Buddy->SurfaceComponent))
 {
  AActor* Attached=Buddy->SurfaceComponent->GetOwner();
  if(Buddy->bSurfaceViewLocked) { Target=Attached;LockedSurface=Buddy->SurfaceComponent;HighlightBuddy=const_cast<ABBSBBunniarCompanion*>(Buddy); }
  else if(IsEquipmentTarget(Attached)) Target=Attached;
 }
 else
 {
  FHitResult Hit;if(TraceEquipmentAim(Hit) && IsEquipmentTarget(Hit.GetActor())) Target=Hit.GetActor();
 }
 if(!FieldEquipment && bWeaponAimLocked && IsEquipmentTarget(LockedAimActor)) Target=LockedAimActor;
	const int32 HighlightStencil=bWeaponAimLocked || LockedSurface ? 3 : 2;
	if (Target==HoveredActor && LockedSurface==HoverLockedSurface.Get() && HighlightBuddy==HoverBuddy.Get())
	{
		for (const FHoverState& State:HoverStates) if (UPrimitiveComponent* Mesh=State.Primitive.Get()) { Mesh->SetRenderCustomDepth(true); Mesh->SetCustomDepthStencilValue(Mesh->GetOwner()==HighlightBuddy ? 4:HighlightStencil); }
		return;
	}
	ClearHover(); HoveredActor=Target;HoverLockedSurface=LockedSurface;HoverBuddy=HighlightBuddy;
	if (!Target) return;
	TArray<UPrimitiveComponent*> Meshes;
 // A plain wall/floor is only a camera anchor. Highlight the attached companion, not the surface.
 if(!LockedSurface) { TArray<UMeshComponent*> TargetMeshes;Target->GetComponents<UMeshComponent>(TargetMeshes);for(UMeshComponent* Mesh:TargetMeshes) Meshes.Add(Mesh); }
 if(HighlightBuddy) { TArray<UMeshComponent*> BuddyMeshes;HighlightBuddy->GetComponents<UMeshComponent>(BuddyMeshes);for(UMeshComponent* Mesh:BuddyMeshes) Meshes.AddUnique(Mesh); }
	for (UPrimitiveComponent* Mesh:Meshes)
	{
		FHoverState State; State.Primitive=Mesh; State.bDepth=Mesh->bRenderCustomDepth; State.Stencil=Mesh->CustomDepthStencilValue; HoverStates.Add(State);
		Mesh->SetRenderCustomDepth(true); Mesh->SetCustomDepthStencilValue(Mesh->GetOwner()==HighlightBuddy ? 4:HighlightStencil);
	}
}

ABBSBBreakableProp::ABBSBBreakableProp()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Body->SetStaticMesh(Cube.Object);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HealthLabel"));
	Label->SetupAttachment(RootComponent);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(14.f);
	Label->SetTextRenderColor(FColor(86,65,85));
	Tags.Add(TEXT("BBSB_FieldProp"));
}

void ABBSBBreakableProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Body->SetRelativeLocation(FVector(0,0,Size.Z*0.5f));
	Body->SetRelativeScale3D(Size/100.f);
	Label->SetRelativeLocation(FVector(0,0,Size.Z+18));
	TintMesh(Body,Tint);
	UpdateAppearance();
}

void ABBSBBreakableProp::BeginPlay() { Super::BeginPlay(); ResetProp(); }

void ABBSBBreakableProp::UpdateAppearance()
{
	Label->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),Health,MaximumHealth)));
	Label->SetVisibility(!bBroken);
	Body->SetVisibility(!bBroken);
	Body->SetCollisionEnabled(bBroken ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

float ABBSBBreakableProp::TakeDamage(float Amount, const FDamageEvent& Event, AController* Controller, AActor* Causer)
{
	if (bBroken || Amount <= 0.f) return 0.f;
	const float Applied = FMath::Min(Amount,Health);
	Health -= Applied;
	if (Health <= 0.f) Break(Causer);
	UpdateAppearance();
	return Applied;
}

void ABBSBBreakableProp::ResetProp() { Health=MaximumHealth; bBroken=false; UpdateAppearance(); }

void ABBSBBreakableProp::Break(AActor* Causer)
{
	bBroken = true;
	FVector Away = Causer ? (GetActorLocation()-Causer->GetActorLocation()).GetSafeNormal2D() : FVector::ForwardVector;
	for (int32 Index=0; Index<8; ++Index)
	{
		const FVector Offset((Index&1 ? 1.f:-1.f)*Size.X*0.22f,(Index&2 ? 1.f:-1.f)*Size.Y*0.22f,Size.Z*0.25f+(Index&4 ? Size.Z*0.45f:0));
		FActorSpawnParameters Spawn;
		Spawn.Owner=this;
		Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Piece=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),GetActorLocation()+Offset,GetActorRotation(),Spawn);
		if (!Piece) continue;
		UStaticMeshComponent* Mesh=NewObject<UStaticMeshComponent>(Piece);
		Piece->AddInstanceComponent(Mesh);
		Piece->SetRootComponent(Mesh);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Body->GetStaticMesh());
		Mesh->SetWorldScale3D(Size/100.f*0.43f);
		Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Mesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
		Mesh->RegisterComponent();
		TintMesh(Mesh,Tint*FMath::Lerp(0.8f,1.1f,Index/7.f));
		Mesh->SetSimulatePhysics(true);
		Mesh->AddImpulse(Away*220.f+FVector(Offset.X*3,Offset.Y*3,240.f+Index*16),NAME_None,true);
		Mesh->AddAngularImpulseInDegrees(FVector(Index*37.f,120.f,90.f),NAME_None,true);
		Piece->SetLifeSpan(3.f);
	}
}

ABBSBFieldPracticeArea::ABBSBFieldPracticeArea()
{
	RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Tags.Add(TEXT("BBSB_FieldPractice"));
}

void ABBSBFieldPracticeArea::BeginPlay()
{
	Super::BeginPlay();
	if (!bSpawnPracticeProps) return;
	const FVector Layout[] = {FVector(300,-220,0),FVector(500,-80,0),FVector(500,150,0),FVector(750,-270,0),FVector(800,170,0),FVector(1050,-100,0)};
	for (int32 Index=0; Index<UE_ARRAY_COUNT(Layout); ++Index)
	{
		FVector Location=GetActorTransform().TransformPosition(Layout[Index]);
		FHitResult Ground;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBPracticeGround),false,this);
		if (!GetWorld()->LineTraceSingleByChannel(Ground,Location+FVector(0,0,1200),Location-FVector(0,0,2000),ECC_WorldStatic,Query)) continue;
		Location.Z=Ground.ImpactPoint.Z+1;
		const FTransform Transform(GetActorRotation()+FRotator(0,180,0),Location);
		ABBSBBreakableProp* Prop=GetWorld()->SpawnActorDeferred<ABBSBBreakableProp>(ABBSBBreakableProp::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Prop) continue;
		Prop->Size=FVector(45+Index*5,45+Index*4,55+Index*8);
		Prop->MaximumHealth=30.f+Index*8.f;
		Prop->Tint=GestureColor(static_cast<EBBSBFieldGesture>(Index%5))*0.85f+FLinearColor(0.12f,0.12f,0.12f,0.f);
		UGameplayStatics::FinishSpawningActor(Prop,Transform);
	}
}

void ABBSBFieldPracticeHUD::DrawHUD()
{
	Super::DrawHUD();
	APawn* Pawn=GetOwningPawn();
	UBBSBFieldActionComponent* Actions=Pawn ? Pawn->FindComponentByClass<UBBSBFieldActionComponent>() : nullptr;
	if (!Canvas || !Actions) return;
 if(const auto* Dialogue=Pawn->FindComponentByClass<UBBSBInteractionComponent>();Dialogue && Dialogue->bDialogueActive) { Dialogue->DrawDialogue(this,Canvas);return; }
	if (Actions->bEquipmentWheelOpen) { Actions->DrawEquipmentWheel(this,Canvas); return; }
	const float Scale=FMath::Clamp(Canvas->SizeX/1280.f,0.7f,1.35f);
	const float W=Canvas->SizeX, H=Canvas->SizeY;
	const float CX=W*0.5f, CY=H*0.5f;
	Actions->DrawContextAim(this,Canvas);
	Actions->DrawAimFeedback(this);
 if(const auto* Player=Cast<ABBSBCharacter>(Pawn);Player && Player->Traversal &&
  (Actions->FieldEquipment==Actions->BunniarEquipment || Player->Traversal->IsWireTraversalActive()))
 {
  const auto* T=Player->Traversal.Get();
  float AimX=CX,AimY=CY;
  if(const auto* PC=GetOwningPlayerController();PC && PC->bShowMouseCursor) PC->GetMousePosition(AimX,AimY);
  const float AimScale=FMath::Clamp(W/1280.f,.7f,1.4f);
  const float OuterRadius=30.f*AimScale,InnerRadius=23.f*AimScale;
  const FVector2D Center(AimX,AimY);
  auto Point=[&](float Angle,float Radius) { return Center+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius; };
  auto Arc=[&](float Start,float End,float Fill,const FLinearColor& Color)
  {
   if(Fill<=0.f) return;
   const int32 Segments=FMath::Max(1,FMath::CeilToInt(48.f*Fill));
   TArray<FCanvasUVTri> Tris;Tris.Reserve(Segments*2);
   auto Triangle=[&](const FVector2D& A,const FVector2D& B,const FVector2D& C)
   {
    FCanvasUVTri Tri;Tri.V0_Pos=A;Tri.V1_Pos=B;Tri.V2_Pos=C;
    Tri.V0_UV=Tri.V1_UV=Tri.V2_UV=FVector2D::ZeroVector;
    Tri.V0_Color=Tri.V1_Color=Tri.V2_Color=Color;Tris.Add(Tri);
   };
   for(int32 Edge=0;Edge<Segments;++Edge)
   {
    const float A=FMath::Lerp(Start,End,Fill*float(Edge)/Segments);
    const float B=FMath::Lerp(Start,End,Fill*float(Edge+1)/Segments);
    const FVector2D IA=Point(A,InnerRadius),OA=Point(A,OuterRadius),IB=Point(B,InnerRadius),OB=Point(B,OuterRadius);
    Triangle(IA,OA,OB);Triangle(IA,OB,IB);
   }
   FCanvasTriangleItem Item(Tris,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
  };
  auto Border=[&](const FVector2D& From,const FVector2D& To,float Alpha)
  {
   DrawLine(From.X,From.Y,To.X,To.Y,FLinearColor(.12f,.1f,.16f,.85f),2.5f*AimScale);
   DrawLine(From.X,From.Y,To.X,To.Y,FLinearColor(.9f,.9f,.98f,Alpha),.85f*AimScale);
  };
  // Two separate annular bars cup the reticle, with a clear gap at six o'clock.
  for(int32 I=0;I<2;++I)
  {
   const float Start=FMath::DegreesToRadians(I==0 ? 154.f:84.f),End=FMath::DegreesToRadians(I==0 ? 96.f:26.f);
   const float Fill=I<T->WireCharges ? 1.f:I==T->WireCharges ? FMath::Clamp(T->WireRechargeProgress,0.f,1.f):0.f;
   Arc(Start,End,1.f,FLinearColor(.14f,.12f,.19f,.8f));
   Arc(Start,End,Fill,FLinearColor(.82f,.89f,.98f));
   const float Alpha=Fill>=1.f ? .95f:.6f;
   for(int32 Edge=0;Edge<48;++Edge)
   {
    const float A=FMath::Lerp(Start,End,float(Edge)/48.f),B=FMath::Lerp(Start,End,float(Edge+1)/48.f);
    Border(Point(A,OuterRadius),Point(B,OuterRadius),Alpha);
    Border(Point(A,InnerRadius),Point(B,InnerRadius),Alpha);
   }
   Border(Point(Start,InnerRadius),Point(Start,OuterRadius),Alpha);
   Border(Point(End,InnerRadius),Point(End,OuterRadius),Alpha);
  }
 }
	const double Now=GetWorld()->GetTimeSeconds();
	if (Now-Actions->LastHitAt<1.2)
		DrawText(Actions->LastHitLabel,FLinearColor(0.98f,0.7f,0.8f),CX-80*Scale,CY+28*Scale,GEngine->GetMediumFont(),Scale);
	const float Charge=Actions->GetCharge();
	if (Actions->FieldEquipment || Actions->PrototypeWeapon!=EBBSBPrototypeWeapon::Legacy)
	{
		const float X=Canvas->ClipX-260*Scale;
		DrawText(Actions->EquipmentLabel(Actions->EquippedEquipmentId())+TEXT("  |  Tab 전환 · 길게 선택"),FLinearColor(0.94f,0.82f,0.9f),X,18*Scale,GEngine->GetSmallFont(),Scale);
		if (Actions->PendingEquipmentId!=INDEX_NONE) DrawText(TEXT("행동 종료 후 → ")+Actions->EquipmentLabel(Actions->PendingEquipmentId),FLinearColor(.98f,.80f,.58f),X,58*Scale,GEngine->GetSmallFont(),1.15f*Scale);

		if (!Actions->FieldEquipment)
		{
			if (Charge>0) DrawText(FString::Printf(TEXT("Charge %.2f / 2 s"),Actions->WeaponChargeSeconds),FLinearColor(0.94f,0.8f,0.87f),X,38*Scale,GEngine->GetSmallFont(),Scale);
			else if (Actions->bLastWeaponOnBeat && GetWorld()->GetTimeSeconds()-Actions->GestureTimes[int32(Actions->LastGesture)]<0.7) DrawText(TEXT("ON BEAT  x1.5"),FLinearColor(0.98f,0.78f,0.86f),X,38*Scale,GEngine->GetSmallFont(),Scale);
		}
		const double Beat=Actions->WeaponBeatPosition();
		const float Pulse=1.f-FMath::Clamp(float(FMath::Frac(Beat)*5),0.f,1.f);
		DrawRect(FLinearColor(0.6f+0.3f*Pulse,0.5f+0.3f*Pulse,0.65f+0.3f*Pulse),Canvas->ClipX-22*Scale,20*Scale,6*Scale,6*Scale);
	}
	if (Charge>0.f && !Actions->FieldEquipment)
	{
		DrawRect(FLinearColor(0.2f,0.16f,0.23f,0.8f),CX-70*Scale,CY+55*Scale,140*Scale,5*Scale);
		DrawRect(FLinearColor(0.9f,0.68f,0.8f),CX-70*Scale,CY+55*Scale,140*Scale*Charge,5*Scale);
	}
	UBBSBCombatComponent* Combat=Pawn->FindComponentByClass<UBBSBCombatComponent>();
	if (Combat)
	{
		Combat->DrawDefenseFeedback(this,Canvas);
		const float BarW=260*Scale;
		DrawRect(FLinearColor(0.13f,0.1f,0.16f,0.8f),14*Scale,14*Scale,BarW+16*Scale,70*Scale);
		DrawRect(FLinearColor(0.28f,0.2f,0.27f),22*Scale,23*Scale,BarW,10*Scale);
		DrawRect(FLinearColor(0.88f,0.58f,0.65f),22*Scale,23*Scale,BarW*FMath::Clamp(Combat->Health/Combat->MaximumHealth,0.f,1.f),10*Scale);
		DrawRect(FLinearColor(0.64f,0.57f,0.84f),22*Scale,40*Scale,BarW*FMath::Clamp(Combat->Ecstatic/Combat->MaximumEcstatic,0.f,1.f),6*Scale);
		DrawText(FString::Printf(TEXT("HP %.0f  |  Ecstatic %d  |  %s"),Combat->Health,FMath::FloorToInt(Combat->Ecstatic/100.f),Combat->IsStageLocked() ? (Combat->bCombatStance ? TEXT("STANCE") : TEXT("FREE")) : TEXT("FIELD")),FLinearColor(0.95f,0.87f,0.94f),22*Scale,56*Scale,GEngine->GetSmallFont(),Scale);
		if (Combat->FeedbackExperiment==2)
		{
			const float X=22*Scale,Y=100*Scale,MeterW=306*Scale;
			DrawRect(FLinearColor(0.13f,0.1f,0.16f,0.82f),X-8*Scale,Y-8*Scale,MeterW+16*Scale,100*Scale);
			DrawText(TEXT("TIMING COACH  |  R clears history"),FLinearColor(0.87f,0.82f,0.96f),X,Y,GEngine->GetSmallFont(),Scale);
			FString Cue=TEXT("Waiting for a fresh parry");
			if (Combat->CoachContact==EBBSBTimingKind::FreshPress) Cue=FString::Printf(TEXT("%.0f ms before contact"),Combat->CoachLeadMs);
			else if (Combat->CoachContact==EBBSBTimingKind::HoldTick) Cue=TEXT("Hold continuation (not sampled)");
			else if (Combat->CoachContact==EBBSBTimingKind::NoRecentPress) Cue=TEXT("No recent press in this region");
			else if (Combat->CoachContact==EBBSBTimingKind::Unavailable) Cue=TEXT("Parry unavailable for this contact");
			DrawText(Cue,FLinearColor(0.92f,0.88f,0.95f),X,Y+18*Scale,GEngine->GetSmallFont(),Scale);
			const float Horizon=FMath::Max(0.001f,FMath::Max(Combat->EarlyWindow,Combat->BlockWindow));
			DrawRect(FLinearColor(0.65f,0.46f,0.47f),X,Y+40*Scale,MeterW,6*Scale);
			DrawRect(FLinearColor(0.55f,0.78f,0.7f),X,Y+40*Scale,MeterW*FMath::Clamp(Combat->BlockWindow/Horizon,0.f,1.f),6*Scale);
			if (Combat->CoachContact==EBBSBTimingKind::FreshPress)
			{
				const float Marker=X+MeterW*FMath::Clamp(Combat->CoachLeadMs/(1000.f*Horizon),0.f,1.f);
				DrawLine(Marker,Y+36*Scale,Marker,Y+50*Scale,FLinearColor(0.99f,0.94f,0.83f),2*Scale);
			}
			DrawText(TEXT("0 / contact"),FLinearColor(0.82f,0.86f,0.89f),X,Y+51*Scale,GEngine->GetSmallFont(),Scale*0.9f);
			DrawText(FString::Printf(TEXT("%.0f ms earlier"),Horizon*1000.f),FLinearColor(0.9f,0.78f,0.8f),X+207*Scale,Y+51*Scale,GEngine->GetSmallFont(),Scale*0.9f);
			const FString Stats=Combat->CoachSamples ? FString::Printf(TEXT("n %d/24  median %.0f ms  range %.0f-%.0f"),Combat->CoachSamples,Combat->CoachMedianMs,Combat->CoachMinMs,Combat->CoachMaxMs) : TEXT("Fresh presses only  |  block zone is green");
			DrawText(Stats,FLinearColor(0.84f,0.86f,0.94f),X,Y+73*Scale,GEngine->GetSmallFont(),Scale*0.9f);
		}
		if (Now-Combat->FeedbackAt<1.6 || Combat->bDead)
			DrawText(Combat->Feedback,Now-Combat->LastParryAt<0.3 && Combat->LastParry==EBBSBParryResult::Block ? FLinearColor(0.7f,0.95f,0.86f) : FLinearColor(0.97f,0.78f,0.86f),CX-175*Scale,H*0.23f,GEngine->GetMediumFont(),Scale);
		if (Combat->IsStageLocked())
		{
			ABBSBCombatStage* Stage=Combat->Stage;
			const FString Count=Stage->CountInText();
			if (!Count.IsEmpty()) DrawText(Count,FLinearColor(0.96f,0.8f,0.89f),CX-80*Scale,H*0.34f,GEngine->GetLargeFont(),Scale*1.6f);
			const float Pulse=Stage->BeatPulse();
			DrawRect(FLinearColor(0.84f,0.7f,0.9f,0.4f+0.5f*Pulse),CX-60*Scale,H-180*Scale,120*Scale,4*Scale);
			DrawText(FString::Printf(TEXT("%.0f BPM  |  %s"),Stage->BPM,Stage->Front ? *Stage->Front->EnemyName.ToString() : TEXT("")),FLinearColor(0.9f,0.83f,0.94f),CX-73*Scale,H-174*Scale,GEngine->GetSmallFont(),Scale);


			if (Combat->FeedbackExperiment==2)
			{
				FVector Eye; FRotator View; GetOwningPlayerController()->GetPlayerViewPoint(Eye,View);
				for (TActorIterator<ABBSBInkProjectile> It(GetWorld()); It; ++It)
				{
					if (It->bResolved || It->OwningStage.Get()!=Stage || It->TimeToTarget()<=0) continue;
					FVector2D Screen; if (!GetOwningPlayerController()->ProjectWorldLocationToScreen(It->GetActorLocation(),Screen,true)) continue;
					FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBCoachVisibility),false,Pawn); Query.AddIgnoredActor(*It);
					for (ABBSBInkEnemy* Enemy:Stage->Members) if (Enemy) Query.AddIgnoredActor(Enemy);
					FHitResult Cover; if (GetWorld()->LineTraceSingleByChannel(Cover,Eye,It->GetActorLocation(),ECC_Visibility,Query)) continue;
					DrawText(FString::Printf(TEXT("%.1f"),It->TimeToTarget()),FLinearColor(0.95f,0.85f,0.98f),Screen.X+8*Scale,Screen.Y-16*Scale,GEngine->GetSmallFont(),Scale);
				}
			}
			if (IsValid(Combat->SelectedEnemy))
				DrawText(FString::Printf(TEXT("TARGET  %s  %.0f / %.0f%s"),*Combat->SelectedEnemy->EnemyName.ToString(),Combat->SelectedEnemy->Health,Combat->SelectedEnemy->MaximumHealth,FVector::Dist(Pawn->GetActorLocation(),Combat->AimPoint())>Actions->WeaponReach ? TEXT("  OUT OF RANGE") : TEXT("")),FLinearColor(0.96f,0.84f,0.92f),CX-135*Scale,20*Scale,GEngine->GetSmallFont(),Scale);
		}
	}
	if (!Actions->bHelpVisible) return;
	const float X=18*Scale, Y=H-171*Scale;
	DrawRect(FLinearColor(0.12f,0.1f,0.15f,0.76f),X-8*Scale,Y-10*Scale,550*Scale,164*Scale);
	DrawText(TEXT("FIELD PRACTICE"),FLinearColor(0.96f,0.85f,0.9f),X,Y,GEngine->GetMediumFont(),Scale);
	DrawText(TEXT("WASD Run / Parry   Shift Dash   Space Jump / Glide toggle   R Reset   F1 Help"),FLinearColor(0.88f,0.85f,0.9f),X,Y+25*Scale,GEngine->GetSmallFont(),Scale);
	DrawText(TEXT("LMB / RMB: press > hold > shake > release (fast = Flick)"),FLinearColor(0.88f,0.85f,0.9f),X,Y+44*Scale,GEngine->GetSmallFont(),Scale);
	for (int32 Index=0; Index<5; ++Index)
	{
		const bool Recent=Now-Actions->GestureTimes[Index]<0.55;
		const FLinearColor Color=GestureColor(static_cast<EBBSBFieldGesture>(Index));
		DrawRect(Recent ? Color : FLinearColor(0.25f,0.21f,0.28f,0.9f),X+Index*96*Scale,Y+67*Scale,88*Scale,24*Scale);
		DrawText(GestureName(static_cast<EBBSBFieldGesture>(Index)),FLinearColor::White,X+(Index*96+7)*Scale,Y+70*Scale,GEngine->GetSmallFont(),Scale);
	}
	const float Cooldown=Actions->GetDodgeCooldownRemaining();
	const FString Status=FString::Printf(TEXT("%s   Dodge %s   F2 Combat   F3 Pattern   F4 View   F5 Feedback"),*Actions->GetMovementLabel(),Cooldown<=0 ? TEXT("Ready") : *FString::Printf(TEXT("%.1fs"),Cooldown));
	DrawText(Status,FLinearColor(0.94f,0.81f,0.88f),X,Y+105*Scale,GEngine->GetSmallFont(),Scale);
	const FBBSBWeaponActionRule BeatRule=Actions->GetWeaponRule(EBBSBFieldGesture::Beat);
	const FString Cancel=BeatRule.bDodgeCancelable ? FString::Printf(TEXT("%.2fs"),BeatRule.Duration*BeatRule.DodgeCancelOpen) : TEXT("locked");
	DrawText(FString::Printf(TEXT("F6 Weapon: %s  |  Beat %.2fs / dodge %s  |  DMG x%.2f"),BBSBCombatRules::WeaponCommitmentName(Actions->WeaponExperiment),BeatRule.Duration,*Cancel,BeatRule.DamageMultiplier),FLinearColor(0.79f,0.87f,0.94f),X,Y+130*Scale,GEngine->GetSmallFont(),Scale);
}
