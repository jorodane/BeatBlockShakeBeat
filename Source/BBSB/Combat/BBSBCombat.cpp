#include "BBSBCombat.h"
#include "BBSBCombatVisuals.h"
#include "BBSB.h"
#include "Field/BBSBFieldActions.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

using namespace BBSBCombatVisuals;

UBBSBCombatComponent::UBBSBCombatComponent()
{
	PrimaryComponentTick.bCanEverTick=true;
	PrimaryComponentTick.TickGroup=TG_PrePhysics;
}

void UBBSBCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	Character=Cast<ACharacter>(GetOwner());
	if (!Character) { SetComponentTickEnabled(false); return; }
	Health=MaximumHealth;
	Boom=Character->FindComponentByClass<USpringArmComponent>();
	if (Boom) { OriginalArmLength=Boom->TargetArmLength; OriginalSocketOffset=Boom->SocketOffset; bOriginalCameraLag=Boom->bEnableCameraLag; bOriginalRotationLag=Boom->bEnableCameraRotationLag; }
	if (FMath::IsNearlyEqual(CombatCameraDistance,640.f)) CombatCameraDistance=OriginalArmLength;
	OriginalCombatDistance=CombatCameraDistance;
	Camera=Character->FindComponentByClass<UCameraComponent>();
	if (Camera) CameraRelativeRotation=Camera->GetRelativeRotation();
	Character->GetCharacterMovement()->AddTickPrerequisiteComponent(this);
 Character->GetMesh()->AddTickPrerequisiteComponent(this);
}

void UBBSBCombatComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	SetStageCameraLock(false);
	if (Boom) { Boom->TargetArmLength=OriginalArmLength; Boom->SocketOffset=OriginalSocketOffset; }
	if (Camera) Camera->SetRelativeRotation(CameraRelativeRotation);
	if (IsValid(SelectedEnemy)) SelectedEnemy->SetSelected(false);
	RestoreViewBindings();
	Super::EndPlay(Reason);
}

void UBBSBCombatComponent::BindControls()
{
	UInputComponent* Input=Character ? Character->FindComponentByClass<UInputComponent>() : nullptr;
	if (!Input || BoundInput==Input) return;
#if !UE_BUILD_SHIPPING
	RestoreViewBindings();
	if (APlayerController* PC=Cast<APlayerController>(Character->GetController()))
	{
		DebugInput=PC->PlayerInput;
		if (DebugInput.IsValid())
		{
			for (int32 Index=DebugInput->DebugExecBindings.Num()-1; Index>=0; --Index)
			{
				const FKeyBind& Bind=DebugInput->DebugExecBindings[Index];
				const bool bLabKey=Bind.Key==EKeys::F1 || Bind.Key==EKeys::F2 || Bind.Key==EKeys::F3 || Bind.Key==EKeys::F4 || Bind.Key==EKeys::F5 || Bind.Key==EKeys::F7;
				if (bLabKey && Bind.Command.StartsWith(TEXT("viewmode "),ESearchCase::IgnoreCase))
				{
					SavedViewBindings.Emplace(Index,Bind); DebugInput->DebugExecBindings.RemoveAt(Index);
				}
			}
		}
	}
#endif
	Input->BindKey(EKeys::W,IE_Pressed,this,&UBBSBCombatComponent::UpDown).bConsumeInput=false;
	Input->BindKey(EKeys::W,IE_Released,this,&UBBSBCombatComponent::UpUp).bConsumeInput=false;
	Input->BindKey(EKeys::D,IE_Pressed,this,&UBBSBCombatComponent::RightDown).bConsumeInput=false;
	Input->BindKey(EKeys::D,IE_Released,this,&UBBSBCombatComponent::RightUp).bConsumeInput=false;
	Input->BindKey(EKeys::S,IE_Pressed,this,&UBBSBCombatComponent::DownSDown).bConsumeInput=false;
	Input->BindKey(EKeys::S,IE_Released,this,&UBBSBCombatComponent::DownSUp).bConsumeInput=false;
	Input->BindKey(EKeys::X,IE_Pressed,this,&UBBSBCombatComponent::DownXDown).bConsumeInput=false;
	Input->BindKey(EKeys::X,IE_Released,this,&UBBSBCombatComponent::DownXUp).bConsumeInput=false;
	Input->BindKey(EKeys::A,IE_Pressed,this,&UBBSBCombatComponent::LeftDown).bConsumeInput=false;
	Input->BindKey(EKeys::A,IE_Released,this,&UBBSBCombatComponent::LeftUp).bConsumeInput=false;
	Input->BindKey(EKeys::Q,IE_Pressed,this,&UBBSBCombatComponent::PreviousTarget);
	Input->BindKey(EKeys::E,IE_Pressed,this,&UBBSBCombatComponent::NextTarget);
	if (auto* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>(); Actions && Actions->bEnablePracticeControls)
	{
		Input->BindKey(EKeys::F2,IE_Pressed,this,&UBBSBCombatComponent::GoToPractice);
		Input->BindKey(EKeys::F3,IE_Pressed,this,&UBBSBCombatComponent::NextExperiment);
		Input->BindKey(EKeys::F4,IE_Pressed,this,&UBBSBCombatComponent::NextCameraExperiment);
		Input->BindKey(EKeys::F5,IE_Pressed,this,&UBBSBCombatComponent::NextFeedbackExperiment);
		Input->BindKey(EKeys::F6,IE_Pressed,this,&UBBSBCombatComponent::NextWeaponExperiment);
	}
	BoundInput=Input;
}
void UBBSBCombatComponent::RestoreViewBindings()
{
#if !UE_BUILD_SHIPPING
	if (DebugInput.IsValid())
		for (int32 Index=SavedViewBindings.Num()-1; Index>=0; --Index)
			DebugInput->DebugExecBindings.Insert(SavedViewBindings[Index].Value,FMath::Clamp(SavedViewBindings[Index].Key,0,DebugInput->DebugExecBindings.Num()));
#endif
	SavedViewBindings.Reset(); DebugInput.Reset();
}

void UBBSBCombatComponent::TickComponent(float DT,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DT,TickType,TickFunction);
	if (!Character || !Character->IsLocallyControlled() || !Cast<APlayerController>(Character->GetController())) { BoundInput.Reset(); return; }
	BindControls();
	if (Stage && !IsValid(Stage)) LeaveStage(false);
	if (IsStageLocked() && (!IsValid(SelectedEnemy) || SelectedEnemy->bDefeated || SelectedEnemy->Stage!=Stage)) CycleTarget(1);
	if (BlocksMovement())
	{
		Character->ConsumeMovementInputVector();
		UCharacterMovementComponent* Move=Character->GetCharacterMovement();
		Move->Velocity.X=Move->Velocity.Y=0;
	}
	SetStageCameraLock(IsStageLocked());
	if (Boom) { Boom->TargetArmLength=IsStageLocked() ? CombatCameraDistance:OriginalArmLength; Boom->SocketOffset=OriginalSocketOffset; }
	if (Camera) Camera->SetRelativeRotation(CameraRelativeRotation);
	if (!IsStageLocked() && PracticeFocus.IsValid() && GetWorld()->GetTimeSeconds()<PracticeFocusUntil)
	{
		const UBBSBFieldActionComponent* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();
		if (Actions && Actions->IsGestureCaptured()) PracticeFocusUntil=-100;
		else if (APlayerController* PC=Cast<APlayerController>(Character->GetController()))
		{
			FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
			PC->SetControlRotation(PC->GetControlRotation()+((PracticeFocus->GetAimPoint()-Eye).Rotation()-View).GetNormalized()*0.65f);
		}
	}
	if (IsStageLocked() && !bDead)
	{
		const FVector Facing=(Stage->GetActorLocation()-Character->GetActorLocation()).GetSafeNormal2D();
		UBBSBFieldActionComponent* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();
		if (bCombatStance && !Facing.IsNearlyZero())
		{
			Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(),Facing.Rotation(),DT,8.f));
			Character->GetCharacterMovement()->bOrientRotationToMovement=false;
		}
		else if (!Actions || !Actions->IsDodging()) Character->GetCharacterMovement()->bOrientRotationToMovement=true;
	}
 if(auto* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>()) Actions->UpdateMovementFacing();
}

void UBBSBCombatComponent::SetStageCameraLock(bool bLocked)
{
	APlayerController* PC=Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (bLocked && PC)
	{
		if (!StageLookController.IsValid())
		{
			PC->SetIgnoreLookInput(true); StageLookController=PC; bStageOriginalCursor=PC->bShowMouseCursor; PC->bShowMouseCursor=true;
			PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen));
		}
		PC->RotationInput=FRotator::ZeroRotator;
		PC->SetControlRotation(CombatViewRotation);
		if (Boom) { Boom->bEnableCameraLag=false; Boom->bEnableCameraRotationLag=false; }
	}
	else if (StageLookController.IsValid())
	{
		StageLookController->SetIgnoreLookInput(false); StageLookController->bShowMouseCursor=bStageOriginalCursor;
		if (!bStageOriginalCursor) StageLookController->SetInputMode(FInputModeGameOnly());
		StageLookController.Reset();
		if (Boom) { Boom->bEnableCameraLag=bOriginalCameraLag; Boom->bEnableCameraRotationLag=bOriginalRotationLag; }
	}
}

bool UBBSBCombatComponent::IsStageLocked() const { return IsValid(Stage) && Stage->IsActive(); }
bool UBBSBCombatComponent::BlocksMovement() const
{
	return bDead || (IsStageLocked() && (bCombatStance || Stage->Phase==EBBSBStagePhase::Forming || Stage->Phase==EBBSBStagePhase::CountIn));
}
bool UBBSBCombatComponent::CanAttack() const
{
	return !bDead && (!IsStageLocked() || (Stage->Phase!=EBBSBStagePhase::Forming && Stage->Phase!=EBBSBStagePhase::CountIn));
}
float UBBSBCombatComponent::DamageMultiplier() const
{
	const float BeatBonus=IsStageLocked() && Stage->IsOnBeat() ? 1.1f : 1.f;
	return BBSBCombatRules::EcstaticDamageMultiplier(Ecstatic)*BeatBonus;
}
FVector UBBSBCombatComponent::AimPoint() const { return IsValid(SelectedEnemy) ? SelectedEnemy->GetAimPoint() : FVector::ZeroVector; }

void UBBSBCombatComponent::OnAttackStarted()
{
	if (IsStageLocked() && !bCombatStance)
	{
		bCombatStance=true; ClearParry();
		SetFeedback(TEXT("STANCE  |  WASD / X parry"));
	}
}

void UBBSBCombatComponent::OnDodge()
{
	if (IsStageLocked()) { bCombatStance=false; Ecstatic=FMath::Max(0.f,Ecstatic-DodgeCost); }
	ClearParry();
	SetFeedback(TEXT("DODGE  |  free movement, attack to take stance"));
}

void UBBSBCombatComponent::EnemyStruck(ABBSBInkEnemy* Enemy)
{
	if (bDead || !IsValid(Enemy)) return;
	if (IsStageLocked())
	{
		if (!Enemy->bDefeated && !Enemy->Stage) Stage->AddMember(Enemy);
		return;
	}
	ABBSBInkEnemy* Living=Enemy;
	if (Enemy->bDefeated)
	{
		Living=nullptr;
		for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It)
			if (!It->bDefeated && It->EncounterID==Enemy->EncounterID && FVector::DistSquared(It->GetActorLocation(),Enemy->GetActorLocation())<FMath::Square(1000.f)) { Living=*It; break; }
	}
	if (!Living) return;
	ABBSBCombatStage* NewStage=GetWorld()->SpawnActor<ABBSBCombatStage>(ABBSBCombatStage::StaticClass(),Living->GetActorLocation(),FRotator::ZeroRotator);
	if (!NewStage) return;
	if (!NewStage->Initialize(this,Living)) { NewStage->Destroy(); SetFeedback(TEXT("Find a clearer corridor to form the stage")); return; }
	AttachStage(NewStage);
	// The hit was applied by TakeDamage before reaching this function. Entry rewards cannot affect it.
	if (!RewardedEncounters.Contains(Enemy->EncounterID))
	{
		RewardedEncounters.Add(Enemy->EncounterID);
		Ecstatic=FMath::Clamp(Ecstatic+EntryBonus,0.f,MaximumEcstatic);
	}
	UE_LOG(LogBBSB,Display,TEXT("Combat entry after damage: firstHit=%.1f ecstatic=%.1f"),Enemy->LastDamageTaken,Ecstatic);
}

void UBBSBCombatComponent::AttachStage(ABBSBCombatStage* NewStage)
{
	Stage=NewStage; bCombatStance=true; ClearParry();
	CombatViewRotation=FRotator(0,(Stage->GetActorLocation()-Character->GetActorLocation()).Rotation().Yaw,0);
	SetStageCameraLock(true);
	if (IsValid(SelectedEnemy)) SelectedEnemy->SetSelected(false);
	SelectedEnemy=Stage->Front;
	if (SelectedEnemy) SelectedEnemy->SetSelected(true);
	if (APlayerController* PC=Cast<APlayerController>(Character->GetController()))
	{
		FVector2D Screen;
		if (SelectedEnemy && PC->ProjectWorldLocationToScreen(SelectedEnemy->GetAimPoint(),Screen)) PC->SetMouseLocation(Screen.X,Screen.Y);
	}
	SetFeedback(TEXT("Beat!"));
}

void UBBSBCombatComponent::LeaveStage(bool bVictory)
{
	if (IsValid(SelectedEnemy)) SelectedEnemy->SetSelected(false);
	SelectedEnemy=nullptr; Stage=nullptr; bCombatStance=false; ClearParry();
	SetStageCameraLock(false);
	if (Character) Character->GetCharacterMovement()->bOrientRotationToMovement=true;
	if (bVictory) { ++Victories; SetFeedback(TEXT("CLEAR!  |  All ink attacks dispersed")); }
	else SetFeedback(TEXT("Stage released  |  fired ink remains in the field"));
}

void UBBSBCombatComponent::TakeCombatDamage(float Damage,AActor* Causer,bool bInterrupt)
{
	if (bDead || Damage<=0 || !Character) return;
	UBBSBFieldActionComponent* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();
	if (Actions && Actions->IsDodgeInvulnerable()) return;
	Health=FMath::Max(0.f,Health-Damage);
	Ecstatic=FMath::Max(0.f,Ecstatic-MissLoss);
	if (bInterrupt && Actions) Actions->InterruptAction(EBBSBActionState::Hit,0.2f);
	if (Health<=0)
	{
		bDead=true; ClearParry();
		if (Actions) Actions->InterruptAction(EBBSBActionState::Dead,100000.f);
		if (IsStageLocked()) Stage->Dissolve(false);
		SetFeedback(TEXT("DOWN  |  R to reset encounter"));
	}
	UE_LOG(LogBBSB,Display,TEXT("Player damage %.1f HP=%.1f source=%s"),Damage,Health,*GetNameSafe(Causer));
}

void UBBSBCombatComponent::GetParryPlane(FVector& Origin,FVector& Normal,FVector& Right) const
{
	Normal=Character ? Character->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	if (IsStageLocked()) Normal=(Stage->GetActorLocation()-Character->GetActorLocation()).GetSafeNormal2D();
	if (Normal.IsNearlyZero()) Normal=FVector::ForwardVector;
	Right=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();
	Origin=(Character ? Character->GetActorLocation() : FVector::ZeroVector)+Normal*65.f;
}

void UBBSBCombatComponent::ParryDown(int32 Direction)
{
	if (Direction<0 || Direction>3 || !IsStageLocked() || !bCombatStance || bDead || (Stage->Phase!=EBBSBStagePhase::Fighting && Stage->Phase!=EBBSBStagePhase::Replacing)) return;
	const UBBSBFieldActionComponent* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();
	if (Actions && !Actions->IsConductorAvailable()) return;
	Inputs[Direction].Down(GetWorld()->GetTimeSeconds());
}
void UBBSBCombatComponent::ParryUp(int32 Direction) { if (Direction>=0 && Direction<4) Inputs[Direction].Up(); }
bool UBBSBCombatComponent::IsDirectionHeld(int32 Direction) const { return Direction>=0 && Direction<4 && Inputs[Direction].bHeld; }
void UBBSBCombatComponent::ClearParry() { for (auto& Input:Inputs) Input=FBBSBParryInput(); bDownS=bDownX=false; }
void UBBSBCombatComponent::UpDown() { ParryDown(0); } void UBBSBCombatComponent::UpUp() { ParryUp(0); }
void UBBSBCombatComponent::RightDown() { ParryDown(1); } void UBBSBCombatComponent::RightUp() { ParryUp(1); }
void UBBSBCombatComponent::LeftDown() { ParryDown(3); } void UBBSBCombatComponent::LeftUp() { ParryUp(3); }
void UBBSBCombatComponent::DownSDown() { bDownS=true; ParryDown(2); }
void UBBSBCombatComponent::DownSUp() { bDownS=false; if (!bDownX) ParryUp(2); }
void UBBSBCombatComponent::DownXDown() { bDownX=true; ParryDown(2); }
void UBBSBCombatComponent::DownXUp() { bDownX=false; if (!bDownS) ParryUp(2); }
void UBBSBCombatComponent::PreviousTarget() { CycleTarget(-1); } void UBBSBCombatComponent::NextTarget() { CycleTarget(1); }

EBBSBParryResult UBBSBCombatComponent::ResolveContact(const FVector& Contact,int32 HoldGroup,float Damage,AActor* Causer)
{
	UBBSBFieldActionComponent* Actions=Character ? Character->FindComponentByClass<UBBSBFieldActionComponent>() : nullptr;
	if (bDead || (Actions && Actions->IsDodgeInvulnerable())) return EBBSBParryResult::Evade;
	FVector Origin,Normal,Right; GetParryPlane(Origin,Normal,Right);
	const FVector Local=Contact-Origin;
	LastDirection=BBSBCombatRules::DirectionAtPlane(FVector2D(FVector::DotProduct(Local,Right),Local.Z),ParryHalfSize);
	const int32 Index=static_cast<int32>(LastDirection);
	LastParry=EBBSBParryResult::Miss;
	LastParryAt=GetWorld()->GetTimeSeconds();
	const bool bCanParry=bCombatStance && IsStageLocked() && (Stage->Phase==EBBSBStagePhase::Fighting || Stage->Phase==EBBSBStagePhase::Replacing) && (!Actions || Actions->IsConductorAvailable());
	TimingHistory.Observe(Inputs[Index],LastParryAt,HoldGroup,bCanParry,FMath::Max(BlockWindow,EarlyWindow));
	SyncTimingCoach();
	if (bCanParry)
		LastParry=Inputs[Index].Resolve(GetWorld()->GetTimeSeconds(),HoldGroup,BlockWindow,EarlyWindow);
	if (LastParry==EBBSBParryResult::Block)
	{
		++Blocks; Ecstatic=FMath::Clamp(Ecstatic+BlockGain,0.f,MaximumEcstatic);
		SetFeedback(FString::Printf(TEXT("BLOCK!  %s%s"),DirectionName(Index),HoldGroup==INDEX_NONE ? TEXT("") : TEXT("  HOLD")));
	}
	else
	{
		Inputs[Index].HoldingGroup=INDEX_NONE; Inputs[Index].ConsumedSerial=Inputs[Index].PressSerial;
		++Misses;
		TakeCombatDamage(Damage,Causer);
		if (!bDead) SetFeedback(FString::Printf(TEXT("%s  %s  -%.0f HP"),LastParry==EBBSBParryResult::Early ? TEXT("EARLY") : TEXT("MISS"),DirectionName(Index),Damage));
	}
	UE_LOG(LogBBSB,Display,TEXT("Parry result=%d dir=%d hold=%d blocks=%d misses=%d"),int32(LastParry),Index,HoldGroup,Blocks,Misses);
	return LastParry;
}

void UBBSBCombatComponent::SyncTimingCoach()
{
	CoachContact=TimingHistory.Kind; CoachLeadMs=TimingHistory.LastLeadMs; CoachSamples=TimingHistory.Samples.Num();
	CoachMedianMs=TimingHistory.MedianMs(); CoachMinMs=TimingHistory.MinMs(); CoachMaxMs=TimingHistory.MaxMs();
}

void UBBSBCombatComponent::CycleTarget(int32 Step)
{
	if (!IsStageLocked()) return;
	TArray<ABBSBInkEnemy*> Living;
	for (ABBSBInkEnemy* Member:Stage->Members) if (IsValid(Member) && !Member->bDefeated) Living.Add(Member);
	Living.Sort([this](const ABBSBInkEnemy& A,const ABBSBInkEnemy& B) { return A.GetActorLocation().Y<B.GetActorLocation().Y; });
	if (IsValid(SelectedEnemy)) SelectedEnemy->SetSelected(false);
	if (Living.IsEmpty()) { SelectedEnemy=nullptr; return; }
	const int32 Old=Living.IndexOfByKey(SelectedEnemy);
	const int32 Next=Old==INDEX_NONE ? 0 : (Old+Step+Living.Num())%Living.Num();
	SelectedEnemy=Living[Next]; SelectedEnemy->SetSelected(true);
}

void UBBSBCombatComponent::SetFeedback(const FString& Text) { Feedback=Text; FeedbackAt=GetWorld()->GetTimeSeconds(); }
FString UBBSBCombatComponent::StatusString() const
{
	return FString::Printf(TEXT("HP %.0f/%.0f | Ecstatic %d +%.0f | %s | Block %d Miss %d Clear %d"),Health,MaximumHealth,FMath::FloorToInt(Ecstatic/100.f),FMath::Fmod(Ecstatic,100.f),bDead ? TEXT("DOWN") : IsStageLocked() ? (bCombatStance ? TEXT("STANCE") : TEXT("FREE / LOCKED")) : TEXT("FIELD"),Blocks,Misses,Victories);
}

void UBBSBCombatComponent::ResetEncounter()
{
	if (IsStageLocked()) Stage->Dissolve(false);
	for (TActorIterator<ABBSBInkProjectile> It(GetWorld()); It; ++It) It->Destroy();
	for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It) It->ResetEnemy();
	for (TActorIterator<ABBSBCombatStage> It(GetWorld()); It; ++It) It->Destroy();
	LeaveStage(false); bDead=false; Health=MaximumHealth; Ecstatic=0; Blocks=Misses=0; RewardedEncounters.Reset();
	TimingHistory.Reset(); SyncTimingCoach();
	if (Character)
	{
		if (UBBSBFieldActionComponent* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>()) Actions->ResetActionState();
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
	SetFeedback(TEXT("Encounter restored  |  F2 practice, F3 experiments"));
}

void UBBSBCombatComponent::GoToPractice()
{
	if (!Character) return;
	ResetEncounter();
	for (TActorIterator<ABBSBCombatPracticeArea> It(GetWorld()); It; ++It)
	{
		Character->SetActorLocation(It->EntryLocation(),false,nullptr,ETeleportType::TeleportPhysics);
		if (AController* PC=Character->GetController())
		{
			FRotator View=(It->GetActorLocation()-Character->GetActorLocation()).GetSafeNormal2D().Rotation();
			View.Pitch=-8; PC->SetControlRotation(View-(Camera ? Camera->GetRelativeRotation() : CameraRelativeRotation));
		}
		PracticeFocus.Reset(); float Closest=FLT_MAX;
		for (TActorIterator<ABBSBInkEnemy> Enemy(GetWorld()); Enemy; ++Enemy)
		{
			const float Distance=FVector::DistSquared(Enemy->GetActorLocation(),Character->GetActorLocation());
			if (!Enemy->bDefeated && Enemy->EncounterID==It->EncounterID && Distance<Closest) { Closest=Distance; PracticeFocus=*Enemy; }
		}
		PracticeFocusUntil=GetWorld()->GetTimeSeconds()+0.45;
		return;
	}
	SetFeedback(TEXT("Place a CombatPracticeArea marker to enable F2"));
}
void UBBSBCombatComponent::NextExperiment()
{
	ResetEncounter();
	for (TActorIterator<ABBSBCombatPracticeArea> It(GetWorld()); It; ++It) { It->CycleExperiment(); break; }
}
void UBBSBCombatComponent::NextCameraExperiment()
{
	CameraExperiment=(CameraExperiment+1)%3;
	CombatCameraDistance=OriginalCombatDistance*(CameraExperiment==1 ? 0.8f : CameraExperiment==2 ? 1.25f : 1.f);
	SetFeedback(FString::Printf(TEXT("VIEW  %s"),CameraExperiment==1 ? TEXT("Close") : CameraExperiment==2 ? TEXT("Wide") : TEXT("Standard")));
}
void UBBSBCombatComponent::NextFeedbackExperiment()
{
	FeedbackExperiment=(FeedbackExperiment+1)%3;
	SetFeedback(FString::Printf(TEXT("FEEDBACK  %s"),FeedbackExperiment==1 ? TEXT("Pastel pulse") : FeedbackExperiment==2 ? TEXT("Timing coach") : TEXT("Soft")));
}
void UBBSBCombatComponent::NextWeaponExperiment()
{
	UBBSBFieldActionComponent* Actions=Character ? Character->FindComponentByClass<UBBSBFieldActionComponent>() : nullptr;
	if (!Actions) return;
	const auto Next=static_cast<EBBSBWeaponCommitment>((uint8(Actions->WeaponExperiment)+1)%3);
	if (!Actions->SetWeaponExperiment(Next)) { SetFeedback(TEXT("WEAPON  Finish the action and release first")); return; }
	SetFeedback(FString::Printf(TEXT("WEAPON  %s"),BBSBCombatRules::WeaponCommitmentName(Next)));
}

ABBSBInkEnemy::ABBSBInkEnemy()
{
	PrimaryActorTick.bCanEverTick=true;
	GetCapsuleComponent()->InitCapsuleSize(38,72);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InkBody")); Body->SetupAttachment(GetRootComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Body->SetStaticMesh(Sphere.Object); Body->SetRelativeScale3D(FVector(0.75f,0.65f,1.25f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); Body->SetRelativeLocation(FVector(0,0,-6));
	for (int32 Side=-1; Side<=1; Side+=2)
	{
		UStaticMeshComponent* Eye=CreateDefaultSubobject<UStaticMeshComponent>(Side<0 ? TEXT("InkEyeLeft") : TEXT("InkEyeRight"));
		Eye->SetupAttachment(Body); Eye->SetStaticMesh(Sphere.Object); Eye->SetRelativeLocation(FVector(47,Side*17,14));
		Eye->SetRelativeScale3D(FVector(0.09f,0.07f,0.12f)); Eye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Eye->SetCastShadow(false); Eye->ComponentTags.Add(TEXT("InkEye"));
	}
	Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("EnemyLabel")); Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocation(FVector(0,0,92)); Label->SetWorldSize(17); Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetTextRenderColor(FColor(55,35,65));
	AIControllerClass=AAIController::StaticClass(); AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->MaxWalkSpeed=210; GetCharacterMovement()->bOrientRotationToMovement=true;
	GetCharacterMovement()->RotationRate=FRotator(0,360,0); GetCharacterMovement()->bUseRVOAvoidance=true;
	Tags.Add(TEXT("BBSBInteractable"));
}
void ABBSBInkEnemy::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); BBSBCombatVisuals::Tint(Body,Tint); UpdateLabel(); }
void ABBSBInkEnemy::BeginPlay()
{
	Super::BeginPlay(); Home=GetActorLocation(); FormationTarget=Home; Health=MaximumHealth; AuthoredPattern=MainPattern; BBSBCombatVisuals::Tint(Body,this->Tint); UpdateLabel();
	TArray<UStaticMeshComponent*> Parts; GetComponents(Parts);
	for (UStaticMeshComponent* Part:Parts) if (Part->ComponentHasTag(TEXT("InkEye"))) BBSBCombatVisuals::Tint(Part,FLinearColor(0.02f,0.015f,0.035f));
}
void ABBSBInkEnemy::Tick(float DT)
{
	Super::Tick(DT);
	if (bDefeated) return;
	GetCharacterMovement()->MaxWalkSpeed=MoveSpeed;
	FVector Goal=Stage && Stage->IsActive() ? FormationTarget : Home;
	const FVector Delta=Goal-GetActorLocation();
	const float Speed=GetVelocity().Size2D();
	Body->SetRelativeLocation(FVector(0,0,-6+FMath::Sin(GetWorld()->GetTimeSeconds()*(Speed>20 ? 12.f : 2.f))*(Speed>20 ? 4.f : 1.5f)));
	if (GetWorld()->GetTimeSeconds()<WeaponImpulseUntil) { ConsumeMovementInputVector(); }
	else if (Delta.Size2D()>24 && (!Stage || (Stage->Phase!=EBBSBStagePhase::Forming && Stage->Phase!=EBBSBStagePhase::CountIn))) AddMovementInput(Delta.GetSafeNormal2D());
	else if (Stage && Stage->Player && Stage->Player->GetOwner())
	{
		const FRotator Facing=(Stage->Player->GetOwner()->GetActorLocation()-GetActorLocation()).Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,Facing.Yaw,0),DT,5));
	}
	BBSBCombatVisuals::Tint(Body,GetWorld()->GetTimeSeconds()<HitFlashUntil ? FLinearColor(0.9f,0.65f,0.7f) : this->Tint);
	if (APlayerCameraManager* Camera=UGameplayStatics::GetPlayerCameraManager(this,0)) Label->SetWorldRotation((Camera->GetCameraLocation()-Label->GetComponentLocation()).Rotation());
}
float ABBSBInkEnemy::TakeDamage(float Amount,const FDamageEvent& Event,AController* InstigatorController,AActor* Causer)
{
	if (bDefeated || Amount<=0) return 0;
	LastDamageTaken=FMath::Min(Amount,Health); Health=FMath::Max(0.f,Health-Amount);
	HitFlashUntil=GetWorld()->GetTimeSeconds()+0.13; UpdateLabel();
	if (Health<=0)
	{
		bDefeated=true; SetSelected(false); GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->SetAvoidanceEnabled(false); GetCharacterMovement()->DisableMovement();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->SetRelativeScale3D(FVector(0.85f,0.8f,0.08f)); Body->SetRelativeLocation(FVector(0,0,-65));
		if (IsValid(Stage)) Stage->EnemyDefeated(this);
	}
	// Deal damage first. Entering a stage and granting Ecstatic happens strictly afterwards.
	AActor* Attacker=InstigatorController ? InstigatorController->GetPawn() : Causer;
	if (UBBSBCombatComponent* Combat=Attacker ? Attacker->FindComponentByClass<UBBSBCombatComponent>() : nullptr) Combat->EnemyStruck(this);
	UE_LOG(LogBBSB,Display,TEXT("Ink enemy hit %s damage=%.1f hp=%.1f defeated=%d"),*GetName(),LastDamageTaken,Health,bDefeated);
	return LastDamageTaken;
}
void ABBSBInkEnemy::ApplyWeaponImpulse(FVector Impulse)
{
	if (bDefeated) return;
	WeaponImpulseUntil=GetWorld()->GetTimeSeconds()+0.45;
	ConsumeMovementInputVector(); GetCharacterMovement()->StopMovementImmediately(); LaunchCharacter(Impulse,true,true);
}
void ABBSBInkEnemy::JoinStage(ABBSBCombatStage* NewStage,bool bIsFront) { Stage=NewStage; bFront=bIsFront; UpdateLabel(); }
void ABBSBInkEnemy::ResetEnemy()
{
	WeaponImpulseUntil=-100;
	Stage=nullptr; bDefeated=bFront=false; Health=MaximumHealth; SetSelected(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking); GetCharacterMovement()->SetAvoidanceEnabled(true);
	Body->SetRelativeScale3D(FVector(0.75f,0.65f,1.25f)); Body->SetRelativeLocation(FVector(0,0,-6));
	SetActorLocation(Home,false,nullptr,ETeleportType::TeleportPhysics); GetCharacterMovement()->StopMovementImmediately(); UpdateLabel();
}
FVector ABBSBInkEnemy::GetAimPoint() const { return GetActorLocation()+FVector(0,0,10); }
void ABBSBInkEnemy::SetSelected(bool bSelected) { Body->SetRenderCustomDepth(bSelected); Body->SetCustomDepthStencilValue(bSelected ? 3 : 0); }
void ABBSBInkEnemy::UpdateLabel() { Label->SetText(FText::FromString(FString::Printf(TEXT("%s%s  %.0f"),bFront ? TEXT("FRONT  ") : TEXT(""),*EnemyName.ToString(),Health))); }

ABBSBCombatPracticeArea::ABBSBCombatPracticeArea()
{
	RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("PracticeOrigin"));
}
void ABBSBCombatPracticeArea::BeginPlay()
{
	Super::BeginPlay();
	if (!bSpawnEnemies) return;
	const FVector Offsets[]={FVector(0,0,0),FVector(210,-170,0),FVector(240,180,0),FVector(700,150,0)};
	const FName Names[]={TEXT("Quill"),TEXT("Waver"),TEXT("Ribbon"),TEXT("Latecomer")};
	for (int32 Index=0; Index<FMath::Clamp(EnemyCount,1,4); ++Index)
	{
		FVector Location; if (!Ground(GetWorld(),GetActorTransform().TransformPosition(Offsets[Index]),Location)) continue;
		Location.Z+=73;
		const FTransform Transform(GetActorRotation()+FRotator(0,180,0),Location);
		ABBSBInkEnemy* Enemy=GetWorld()->SpawnActorDeferred<ABBSBInkEnemy>(ABBSBInkEnemy::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Enemy) continue;
		Enemy->EnemyName=Names[Index]; Enemy->EncounterID=EncounterID; Enemy->MaximumHealth=EnemyHealth;
		Enemy->MainPattern=Index==0 ? EBBSBInkPattern::Tap : Index==1 ? EBBSBInkPattern::Curve : EBBSBInkPattern::Hold;
		Enemy->Tint=Index==0 ? FLinearColor(0.43f,0.35f,0.51f) : Index==1 ? FLinearColor(0.36f,0.5f,0.53f) : FLinearColor(0.58f,0.38f,0.44f);
		UGameplayStatics::FinishSpawningActor(Enemy,Transform);
	}
}
FVector ABBSBCombatPracticeArea::EntryLocation() const
{
	FVector GroundPoint; const FVector Point=GetActorLocation()-GetActorForwardVector()*510;
	return Ground(GetWorld(),Point,GroundPoint) ? GroundPoint+FVector(0,0,96) : Point+FVector(0,0,96);
}
void ABBSBCombatPracticeArea::CycleExperiment()
{
	ExperimentPreset=(ExperimentPreset+1)%5;
	for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It)
	{
		if (It->EncounterID!=EncounterID) continue;
		It->MainPattern=ExperimentPreset==1 ? EBBSBInkPattern::Curve : ExperimentPreset==2 ? EBBSBInkPattern::Hold : ExperimentPreset==3 ? EBBSBInkPattern::Bounce : ExperimentPreset==4 ? EBBSBInkPattern::Area : It->AuthoredPattern;
	}
	if (APlayerController* PC=UGameplayStatics::GetPlayerController(this,0))
		if (UBBSBCombatComponent* Combat=PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UBBSBCombatComponent>() : nullptr)
			Combat->SetFeedback(FString::Printf(TEXT("EXPERIMENT %d  |  %s"),ExperimentPreset,ExperimentPreset==1 ? TEXT("Curved approach") : ExperimentPreset==2 ? TEXT("Hold / release recovery") : ExperimentPreset==3 ? TEXT("World bounce") : ExperimentPreset==4 ? TEXT("Ground area / dodge") : TEXT("Baseline rhythm")));
}
