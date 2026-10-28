#include "BBSBCombatVerification.h"
#include "BBSBCombat.h"
#include "BBSB.h"
#include "Field/BBSBFieldActions.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"

namespace
{
	bool SameWeaponRules(const TArray<FBBSBWeaponActionRule>& A,const TArray<FBBSBWeaponActionRule>& B)
	{
		if (A.Num()!=B.Num()) return false;
		for (int32 I=0; I<A.Num(); ++I) if (!FBBSBWeaponActionRule::StaticStruct()->CompareScriptStruct(&A[I],&B[I],0)) return false;
		return true;
	}
}

ABBSBCombatVerification::ABBSBCombatVerification()
{
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostPhysics;
}
void ABBSBCombatVerification::BeginPlay()
{
	Super::BeginPlay();
	Character=Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this,0));
	Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>() : nullptr;
	Actions=Character ? Character->FindComponentByClass<UBBSBFieldActionComponent>() : nullptr;
	if (!Character || !Combat || !Actions) { Check(false,TEXT("Local combat character exists")); Finish(); return; }
	OriginalTransform=Character->GetActorTransform(); OriginalView=Character->GetController()->GetControlRotation();
	SavedMaxHP=Combat->MaximumHealth; SavedHP=Combat->Health; SavedEcstatic=Combat->Ecstatic;
	SavedDodgeDistance=Actions->DodgeDistance;
	OriginalPrototype=uint8(Actions->PrototypeWeapon);
	OriginalWeaponPreset=Actions->WeaponExperiment; SavedPrimaryRules=Actions->PrimaryRules; SavedSecondaryRules=Actions->SecondaryRules;
	OriginalCameraPreset=Combat->CameraExperiment; OriginalFeedbackPreset=Combat->FeedbackExperiment; OriginalCameraDistance=Combat->CombatCameraDistance;
	for (TActorIterator<ABBSBCombatPracticeArea> It(GetWorld()); It; ++It) { PracticeMarker=*It; OriginalPatternPreset=It->ExperimentPreset; break; }
	Combat->GoToPractice(); Combat->MaximumHealth=100000; Combat->Health=100000;
	Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Legacy);
	Actions->SetWeaponExperiment(EBBSBWeaponCommitment::Authored);
	for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It) if (!First || It->GetActorLocation().X<First->GetActorLocation().X) First=*It;
	if (!First) { Check(false,TEXT("Practice enemies spawn on this map's collision profiles")); Finish(); return; }
	Advance(0,TEXT("Aim first real weapon hit"));
}
void ABBSBCombatVerification::Check(bool bCondition,const TCHAR* Message)
{
	if (bCondition) { ++ChecksPassed; UE_LOG(LogBBSB,Display,TEXT("CombatVerify PASS: %s"),Message); }
	else { Failures.Add(Message); UE_LOG(LogBBSB,Error,TEXT("CombatVerify FAIL: %s"),Message); }
}
void ABBSBCombatVerification::Advance(int32 Next,const TCHAR* Label)
{
	Step=Next; PhaseLabel=Label; StepAt=GetWorld()->GetTimeSeconds(); bPressed=bReleased=bSecondPress=bRecovered=false;
}
void ABBSBCombatVerification::DirectionKey(int32 Direction,bool bDown)
{
	const FKey Keys[]={EKeys::W,EKeys::D,EKeys::S,EKeys::A};
	SimulateKey(Keys[FMath::Clamp(Direction,0,3)],bDown);
}
void ABBSBCombatVerification::SimulateKey(const FKey& Key,bool bDown)
{
	if (APlayerController* PC=Cast<APlayerController>(Character->GetController())) PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,bDown ? IE_Pressed : IE_Released,bDown ? 1.f : 0.f));
}
ABBSBInkProjectile* ABBSBCombatVerification::Shot(int32 Direction,float Flight,int32 HoldGroup)
{
	if (!Stage || !First) return nullptr;
	FVector Origin,Normal,Right; Combat->GetParryPlane(Origin,Normal,Right);
	FVector Target=Direction==4 ? Origin+Right*600:Combat->DefenseTarget(Direction);
	ABBSBInkEnemy* Source=IsValid(Stage->Front) && !Stage->Front->bDefeated ? Stage->Front.Get() : First.Get();
	ABBSBInkProjectile* Projectile=GetWorld()->SpawnActor<ABBSBInkProjectile>(ABBSBInkProjectile::StaticClass(),Source->GetAimPoint(),FRotator::ZeroRotator);
	if (Projectile) Projectile->Initialize(Stage,Source,Target,HoldGroup<0 ? EBBSBInkPattern::Tap : EBBSBInkPattern::Hold,Flight,HoldGroup);
	return Projectile;
}
void ABBSBCombatVerification::Tick(float DT)
{
	Super::Tick(DT);
	if (bComplete || !Combat || !Character || !Actions || !First) return;
	const double Age=GetWorld()->GetTimeSeconds()-StepAt;
	if (Age>12) { Check(false,TEXT("Scenario step completes before timeout")); Finish(); return; }
	switch (Step)
	{
	case 0:
	{
		APlayerController* PC=Cast<APlayerController>(Character->GetController()); FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
		PC->SetControlRotation(PC->GetControlRotation()+((First->GetAimPoint()-Eye).Rotation()-View).GetNormalized());
		if (Age>0.5)
		{
			EnemyHealthBefore=First->Health; PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton,IE_Pressed,1.f));
			Advance(1,TEXT("Release real gesture"));
		}
		break;
	}
	case 1:
		if (Age>0.02 && !bReleased)
		{
			Check(!Actions->CanMove(),TEXT("Mouse input starts committed attack and locks locomotion"));
			Cast<APlayerController>(Character->GetController())->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton,IE_Released,0.f)); bReleased=true;
		}
		if (Combat->Stage)
		{
			Stage=Combat->Stage; Stage->bPausePatternEmission=true;
			Check(FMath::IsNearlyEqual(EnemyHealthBefore-First->Health,Actions->BaseDamage,0.1f),TEXT("First weapon damage is applied before entry reward"));
			Check(FMath::IsNearlyEqual(Combat->Ecstatic,Combat->EntryBonus),TEXT("Stage grants one entry reward after initial hit"));
			Check(!Actions->IsGestureCaptured(),TEXT("Stage transition releases gesture capture"));
			Check(Stage->Members.Num()>=2,TEXT("Nearby enemies form a shared stage"));
			Advance(2,TEXT("Count-in and stance"));
		}
		else if (Age>1.3) { Check(false,TEXT("Real field hit forms a stage")); Finish(); }
		break;
	case 2:
		if (Stage->Phase==EBBSBStagePhase::Fighting)
		{
			Check(Combat->bCombatStance && !Actions->CanMove(),TEXT("Count-in resolves into movement-limited parry stance"));
			SimulateKey(EKeys::E,true); Advance(17,TEXT("Selected enemy and ranged aim"));
		}
		break;
	case 17:
		if (Age>0.04 && !bReleased) { SimulateKey(EKeys::E,false); bReleased=true; }
		if (Age>0.1 && !bPressed)
		{
			Check(Combat->SelectedEnemy && Combat->SelectedEnemy!=First,TEXT("E input selects another living stage enemy"));
			Actions->FieldGesturePress(0);
			Check((Actions->LastAimPoint-Character->GetActorLocation()).GetSafeNormal2D().Equals((Combat->AimPoint()-Character->GetActorLocation()).GetSafeNormal2D(),0.08),TEXT("Out-of-range attack retains selected target direction"));
			Actions->FieldGestureRelease(0); SimulateKey(EKeys::Q,true); bPressed=true;
		}
		if (Age>0.18 && !bSecondPress) { SimulateKey(EKeys::Q,false); bSecondPress=true; }
		if (Age>0.3)
		{
			Check(Combat->SelectedEnemy==First,TEXT("Q input cycles back to the previous enemy"));
			BlocksBefore=Combat->Blocks; TestDirection=0; Shot(0,0.55f); Advance(3,TEXT("Actual projectile parries"));
		}
		break;
	case 3:
		if (Age>=0.45 && !bPressed) { DirectionKey(TestDirection,true); bPressed=true; }
		if (Age>=0.75)
		{
			Check(Combat->Blocks==BlocksBefore+1,*FString::Printf(TEXT("World projectile blocks in actual contact region %d"),TestDirection));
			Check(Combat->CoachContact==EBBSBTimingKind::FreshPress && Combat->CoachLeadMs>=0 && Combat->CoachLeadMs<=Combat->BlockWindow*1000.f+0.01f && Combat->CoachSamples==TestDirection+1,TEXT("Physical contact records a fresh input lead without changing the block"));
			DirectionKey(TestDirection,false); ++TestDirection; BlocksBefore=Combat->Blocks;
			if (TestDirection<4) { Shot(TestDirection,0.55f); Advance(3,TEXT("Actual projectile parries")); }
			else { Shot(0,0.5f,777); Advance(4,TEXT("Held projectile sequence")); }
		}
		break;
	case 4:
		if (Age>=0.25 && !bReleased) { Shot(0,0.5f,777); bReleased=true; }
		if (Age>=0.42 && !bPressed) { DirectionKey(0,true); bPressed=true; }
		if (Age>=0.5 && !bSecondPress) { Shot(0,0.5f,777); bSecondPress=true; }
		if (Age>=1.2)
		{
			Check(Combat->Blocks==BlocksBefore+3,TEXT("Fresh hold start protects subsequent physical ticks")); DirectionKey(0,false);
			Check(Combat->CoachSamples==5,TEXT("Three physical hold hits add only their fresh starting press to timing history"));
			Check(Combat->CoachContact==EBBSBTimingKind::HoldTick && Combat->CoachLeadMs<0,TEXT("Hold continuation displays its own cue without a fabricated timing marker"));
			BlocksBefore=Combat->Blocks; MissesBefore=Combat->Misses; Shot(0,0.5f,778); Advance(5,TEXT("Hold miss and recovery"));
		}
		break;
	case 5:
		if (Age>=0.25 && !bReleased) { Shot(0,0.5f,778); bReleased=true; }
		if (Age>=0.42 && !bPressed) { DirectionKey(0,true); bPressed=true; }
		if (Age>=0.64 && !bSecondPress) { DirectionKey(0,false); Shot(0,0.5f,778); bSecondPress=true; }
		if (Age>=1.04 && !bRecovered) { DirectionKey(0,true); bRecovered=true; }
		if (Age>=1.4)
		{
			Check(Combat->Blocks>=BlocksBefore+2 && Combat->Misses>=MissesBefore+1,TEXT("Released hold takes damage and a later fresh press recovers")); DirectionKey(0,false);
			Check(Combat->CoachSamples==7 && Combat->CoachContact==EBBSBTimingKind::FreshPress,TEXT("Hold recovery records new presses and excludes the unpressed miss"));
			Actions->ResetActionState(); EnemyHealthBefore=First->Health; DodgeFrom=Character->GetActorLocation();
			Actions->FieldGesturePress(0); Witness=Shot(4,10); if (Witness) WitnessTarget=Witness->FrozenTarget;
			Actions->RecordMoveInput(0,-1); Check(Actions->TryDodge(),TEXT("Dodge can replace an active attack"));
			Actions->FieldGestureRelease(0); Advance(6,TEXT("Cancel and frozen flight"));
		}
		break;
	case 6:
		if (Age>0.5)
		{
			Check(FMath::IsNearlyEqual(First->Health,EnemyHealthBefore),TEXT("Canceled attack cannot deal delayed damage"));
			Check(Actions->ActiveState==EBBSBActionState::Idle,TEXT("No queued attack resumes after dodge"));
			Check(IsValid(Witness) && Witness->FrozenTarget.Equals(WitnessTarget),TEXT("Dodge preserves already-fired trajectory"));
			Check(!Combat->bCombatStance && Combat->IsStageLocked() && Actions->CanMove(),TEXT("Dodge leaves stance while retaining stage lock and movement"));
			Check(FVector::Dist2D(DodgeFrom,Character->GetActorLocation())>200,TEXT("Dodge moves the character through world collision"));
			StanceFrom=Character->GetActorLocation(); Actions->FieldGesturePress(0); Actions->FieldGestureRelease(0);
			Check(Combat->bCombatStance && Character->GetActorLocation().Equals(StanceFrom,1),TEXT("Attack re-enters stance at current location"));
			Actions->InterruptAction(EBBSBActionState::Hit,0.01f);
			CenterBefore=Stage->GetActorLocation();
			UGameplayStatics::ApplyDamage(Stage->Front,100000,Character->GetController(),Character,nullptr);
			Check(Stage->Front && Stage->Front!=First,TEXT("Defeating Front selects a living Backer"));
			BlocksBefore=Combat->Blocks; Shot(0,0.55f);
			Advance(7,TEXT("Physical Front replacement"));
		}
		break;
	case 7:
		if (Age>=0.45 && !bPressed) { DirectionKey(0,true); bPressed=true; }
		if (Age>=0.8 && !bReleased)
		{
			Check(Combat->Blocks==BlocksBefore+1,TEXT("Already-fired ink remains parryable while Front is being replaced")); DirectionKey(0,false); bReleased=true;
		}
		if (Age>3.0)
		{
			UE_LOG(LogBBSB,Display,TEXT("CombatVerify Front location=%s target=%s phase=%d"),*Stage->Front->GetActorLocation().ToCompactString(),*Stage->Front->FormationTarget.ToCompactString(),int32(Stage->Phase));
			Check(Stage->Phase==EBBSBStagePhase::Fighting,TEXT("New Front physically reaches formation and resumes pattern control"));
			Check(Stage->GetActorLocation().Equals(CenterBefore,1),TEXT("Front replacement keeps stage center"));
			Check(IsValid(Witness) && Witness->FrozenTarget.Equals(WitnessTarget),TEXT("Front replacement preserves fired trajectory"));
			RadiusBefore=Stage->RingRadius;
			const FVector Position=Stage->GetActorLocation()+FVector(-300,300,73);
			Joined=GetWorld()->SpawnActor<ABBSBInkEnemy>(ABBSBInkEnemy::StaticClass(),Position,FRotator::ZeroRotator);
			Advance(8,TEXT("Automatic join"));
		}
		break;
	case 8:
		if (Age>0.8)
		{
			Check(Joined && Joined->Stage==Stage && Stage->Members.Contains(Joined),TEXT("Nearby new enemy automatically joins stage"));
			Check(Stage->RingRadius>RadiusBefore,TEXT("Joining expands monster ring"));
			Check(IsValid(Witness) && Witness->FrozenTarget.Equals(WitnessTarget),TEXT("Joining does not retarget flying ink"));
			const auto Members=Stage->Members;
			for (ABBSBInkEnemy* Member:Members) if (IsValid(Member) && !Member->bDefeated) UGameplayStatics::ApplyDamage(Member,100000,Character->GetController(),Character,nullptr);
			Advance(9,TEXT("Victory cleanup"));
		}
		break;
	case 9:
		if (Age>0.2)
		{
			Check(!IsValid(Witness),TEXT("All enemies defeated clears remaining stage attacks"));
			Check(!Combat->IsStageLocked() && Combat->Victories>0,TEXT("Victory returns player to free field control"));
			if (Joined) Joined->Destroy(); Joined=nullptr;
			Combat->ResetEncounter();
			Check(Combat->CoachSamples==0 && Combat->CoachContact==EBBSBTimingKind::Waiting && Combat->CoachLeadMs<0,TEXT("Encounter reset clears the runtime coach history and marker"));
			UGameplayStatics::ApplyDamage(First,1,Character->GetController(),Character,nullptr); Stage=Combat->Stage;
			if (!Stage) { Check(false,TEXT("Second encounter can be formed")); Finish(); return; }
			Stage->bPausePatternEmission=true; Witness=Shot(4,10); if (Witness) WitnessTarget=Witness->FrozenTarget;
			Stage->Dissolve(false); Check(IsValid(Witness) && Witness->FrozenTarget.Equals(WitnessTarget),TEXT("Aggro dissolution preserves fired ink"));
			for (ABBSBInkEnemy* Member:Stage->Members) if (IsValid(Member) && !Member->bDefeated) UGameplayStatics::ApplyDamage(Member,100000,nullptr,this,nullptr);
			Advance(10,TEXT("Dissolved encounter cleanup"));
		}
		break;
	case 10:
		if (Age>0.3)
		{
			Check(!IsValid(Witness),TEXT("Later total defeat also clears a dissolved stage's attacks"));
			Combat->ResetEncounter(); Character->SetActorTransform(OriginalTransform,false,nullptr,ETeleportType::TeleportPhysics);
			Character->GetController()->SetControlRotation(FRotator(45,90,0)); Actions->ExerciseGesture(3);
			Advance(11,TEXT("Flick camera settling"));
		}
		break;
	case 11:
		if (Age>0.8 && !bPressed)
		{
			Check(Actions->LastGesture==EBBSBFieldGesture::Flick && !Actions->IsGestureCaptured(),TEXT("Fast release after Hold recognizes and executes Flick"));
			Check(Actions->GetLookInputScale()<0.01f,TEXT("Camera stays buffered after Flick animation ends")); bPressed=true;
		}
		if (Age>1.06 && !bReleased) { Check(Actions->GetLookInputScale()>0 && Actions->GetLookInputScale()<1,TEXT("Camera input returns through a gradual ramp")); bReleased=true; }
		if (Age>1.4)
		{
			Check(Actions->GetLookInputScale()>0.99f,TEXT("Camera fully returns after settling"));
			Actions->DodgeDistance=0; Actions->RecordMoveInput(0,0); Actions->TryDodge(); EnemyHealthBefore=Combat->Health;
			const FVector Forward=Character->GetActorForwardVector();
			Witness=GetWorld()->SpawnActor<ABBSBInkProjectile>(ABBSBInkProjectile::StaticClass(),Character->GetActorLocation()+Forward*150,FRotator::ZeroRotator);
			if (Witness) Witness->Initialize(nullptr,First,Character->GetActorLocation()-Forward*100,EBBSBInkPattern::Tap,0.15f,INDEX_NONE);
			Advance(12,TEXT("Invulnerability passes flying ink through"));
		}
		break;
	case 12:
		if (Age>0.4)
		{
			Check(FMath::IsNearlyEqual(Combat->Health,EnemyHealthBefore),TEXT("Dodge invulnerability prevents physical projectile damage"));
			Check(IsValid(Witness) && !Witness->bResolved,TEXT("Ink passes through dodge invulnerability without being erased"));
			Actions->DodgeDistance=SavedDodgeDistance; TestDirection=0; GestureCountBefore=Actions->GestureCounts[0];
			Actions->ExerciseGesture(0); Advance(13,TEXT("All five field gestures"));
		}
		break;
	case 13:
		if (Age>1.5)
		{
			Check(Actions->GestureCounts[TestDirection]>GestureCountBefore,*FString::Printf(TEXT("Gesture %d executes through recognizer and action arbitration"),TestDirection));
			++TestDirection;
			if (TestDirection>=5) { TestDirection=1; SimulateKey(EKeys::F3,true); Advance(14,TEXT("Reversible pattern experiments")); }
			else { GestureCountBefore=Actions->GestureCounts[TestDirection]; Actions->ExerciseGesture(TestDirection); Advance(13,TEXT("All five field gestures")); }
		}
		break;
	case 14:
		if (Age>0.04 && !bReleased) { SimulateKey(EKeys::F3,false); bReleased=true; }
		if (Age>0.15)
		{
			const ABBSBCombatPracticeArea* Area=Cast<ABBSBCombatPracticeArea>(PracticeMarker);
			Check(Area && Area->ExperimentPreset==(OriginalPatternPreset+TestDirection)%5,TEXT("F3 switches the isolated pattern experiment"));
			++TestDirection;
			if (TestDirection<=5) { SimulateKey(EKeys::F3,true); Advance(14,TEXT("Reversible pattern experiments")); }
			else
			{
				if (OriginalPatternPreset==0)
				{
					bool bRestored=true; for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It) bRestored&=It->MainPattern==It->AuthoredPattern;
					Check(bRestored,TEXT("Returning to baseline restores authored monster patterns"));
				}
				TestDirection=1; SimulateKey(EKeys::F4,true); Advance(15,TEXT("Camera framing experiments"));
			}
		}
		break;
	case 15:
		if (Age>0.04 && !bReleased) { SimulateKey(EKeys::F4,false); bReleased=true; }
		if (Age>0.15)
		{
			Check(Combat->CameraExperiment==(OriginalCameraPreset+TestDirection)%3,TEXT("F4 switches camera framing without changing combat state"));
			++TestDirection;
			if (TestDirection<=3) { SimulateKey(EKeys::F4,true); Advance(15,TEXT("Camera framing experiments")); }
			else { Check(FMath::IsNearlyEqual(Combat->CombatCameraDistance,OriginalCameraDistance),TEXT("Camera experiment cycle restores baseline distance")); TestDirection=1; SimulateKey(EKeys::F5,true); Advance(16,TEXT("Feedback experiments")); }
		}
		break;
	case 16:
		if (Age>0.04 && !bReleased) { SimulateKey(EKeys::F5,false); bReleased=true; }
		if (Age>0.15)
		{
			Check(Combat->FeedbackExperiment==(OriginalFeedbackPreset+TestDirection)%3,TEXT("F5 switches feedback and timing-coach experiment"));
			++TestDirection;
			if (TestDirection<=3) { SimulateKey(EKeys::F5,true); Advance(16,TEXT("Feedback experiments")); }
			else
			{
				Actions->ResetActionState(); Actions->DodgeDistance=0;
				Character->GetController()->SetControlRotation(FRotator(45,90,0));
				SimulateKey(EKeys::F6,true); Advance(18,TEXT("Agile weapon commitment"));
			}
		}
		break;
	case 18:
		if (Age>0.04 && !bReleased) { SimulateKey(EKeys::F6,false); bReleased=true; }
		if (Age>0.15)
		{
			Check(Actions->WeaponExperiment==EBBSBWeaponCommitment::Agile,TEXT("F6 input selects the Agile weapon preset"));
			const auto Rule=Actions->GetWeaponRule(EBBSBFieldGesture::Beat);
			Check(FMath::IsNearlyEqual(Rule.Duration,SavedPrimaryRules[0].Duration*0.8f) && FMath::IsNearlyEqual(Rule.DamageMultiplier,SavedPrimaryRules[0].DamageMultiplier*0.85f),TEXT("Agile runtime action uses shorter commitment and scaled damage"));
			Actions->FieldGesturePress(0);
			Check(!Actions->CanMove() && !Actions->IsConductorAvailable(),TEXT("Agile still respects the authored movement and conductor lock"));
			SimulateKey(EKeys::F6,true); Advance(19,TEXT("Busy preset rejection and Agile cancel"));
		}
		break;
	case 19:
		if (Age>0.04 && !bReleased) { SimulateKey(EKeys::F6,false); bReleased=true; }
		if (Age>0.07 && !bPressed)
		{
			Check(Actions->WeaponExperiment==EBBSBWeaponCommitment::Agile,TEXT("F6 cannot replace a weapon preset during an active attack"));
			Check(Actions->TryDodge(),TEXT("Agile attack can cancel immediately into a real dodge")); Actions->FieldGestureRelease(0);
			SimulateKey(EKeys::F6,true); bPressed=true;
		}
		if (Age>0.13 && !bSecondPress) { SimulateKey(EKeys::F6,false); bSecondPress=true; }
		if (Age>1.2)
		{
			Check(Actions->ActiveState==EBBSBActionState::Idle && Actions->WeaponExperiment==EBBSBWeaponCommitment::Agile,TEXT("Dodge rejects preset switching and queues neither a preset nor an old attack"));
			SimulateKey(EKeys::F6,true); Advance(20,TEXT("Heavy weapon opening"));
		}
		break;
	case 20:
		if (Age>0.04 && !bReleased) { SimulateKey(EKeys::F6,false); bReleased=true; }
		if (Age>0.15)
		{
			Check(Actions->WeaponExperiment==EBBSBWeaponCommitment::Heavy,TEXT("F6 input selects the Heavy weapon preset"));
			const auto Rule=Actions->GetWeaponRule(EBBSBFieldGesture::Beat);
			Check(FMath::IsNearlyEqual(Rule.Duration,SavedPrimaryRules[0].Duration*1.5f) && FMath::IsNearlyEqual(Rule.DamageMultiplier,SavedPrimaryRules[0].DamageMultiplier*1.35f),TEXT("Heavy runtime action uses longer commitment and scaled damage"));
			Actions->FieldGesturePress(0);
			Check(!Actions->TryDodge() && Actions->ActiveState==EBBSBActionState::Attack,TEXT("Heavy attack denies dodge during its committed opening"));
			SimulateKey(EKeys::F6,true); Advance(21,TEXT("Heavy late dodge cancel"));
		}
		break;
	case 21:
		if (Age>0.04 && !bReleased) { SimulateKey(EKeys::F6,false); bReleased=true; }
		if (Age>0.08 && !bPressed)
		{
			Check(Actions->WeaponExperiment==EBBSBWeaponCommitment::Heavy && !Actions->CanMove(),TEXT("Heavy commitment cannot be bypassed by switching presets")); bPressed=true;
		}
		if (Age>0.24 && !bSecondPress)
		{
			Check(Actions->TryDodge(),TEXT("Heavy opens real dodge cancellation after its authored fraction")); Actions->FieldGestureRelease(0); bSecondPress=true;
		}
		if (Age>0.65)
		{
			Check(Actions->ActiveState==EBBSBActionState::Idle,TEXT("Heavy canceled attack does not resume after dodge"));
			SimulateKey(EKeys::F6,true); Advance(22,TEXT("Exact baseline restoration"));
		}
		break;
	case 22:
		if (Age>0.04 && !bReleased) { SimulateKey(EKeys::F6,false); bReleased=true; }
		if (Age>0.15)
		{
			Check(Actions->WeaponExperiment==EBBSBWeaponCommitment::Authored,TEXT("Third F6 input returns to baseline"));
			bool bExact=SameWeaponRules(SavedPrimaryRules,Actions->PrimaryRules) && SameWeaponRules(SavedSecondaryRules,Actions->SecondaryRules);
			for (int32 Channel=0; Channel<2; ++Channel) for (int32 I=0; I<5; ++I)
			{
				const auto Effective=Actions->GetWeaponRule(static_cast<EBBSBFieldGesture>(I),Channel);
				const auto& Expected=Channel==0 ? SavedPrimaryRules[I] : SavedSecondaryRules[I];
				bExact&=FBBSBWeaponActionRule::StaticStruct()->CompareScriptStruct(&Effective,&Expected,0);
			}
			Check(bExact,TEXT("Every primary and secondary rule and permission is restored exactly"));
			for (auto State:{EBBSBActionState::Hit,EBBSBActionState::Transition,EBBSBActionState::Dead})
			{
				Actions->InterruptAction(State,1.f);
				Check(!Actions->SetWeaponExperiment(EBBSBWeaponCommitment::Agile),TEXT("Hit, transition and death cannot be bypassed by a preset change")); Actions->ResetActionState();
			}
			Actions->FieldGesturePress(0); Advance(23,TEXT("Held input blocks preset change after action expiry"));
		}
		break;
	case 23:
		if (Age>1.0 && !bPressed)
		{
			Check(Actions->IsGestureCaptured() && Actions->ActiveState==EBBSBActionState::Idle,TEXT("Held-input fixture remains captured after its timed action ends"));
			SimulateKey(EKeys::F6,true); bPressed=true;
		}
		if (Age>1.08)
		{
			SimulateKey(EKeys::F6,false);
			Check(Actions->WeaponExperiment==EBBSBWeaponCommitment::Authored,TEXT("Held input blocks weapon switching even after action expiry"));
			Actions->FieldGestureRelease(0); Finish();
		}
		break;
	}
}
void ABBSBCombatVerification::Finish()
{
	bComplete=true; PhaseLabel=Failures.IsEmpty() ? TEXT("PASS") : TEXT("FAIL");
	if (Combat)
	{
		for (int32 Direction=0; Direction<4; ++Direction) DirectionKey(Direction,false);
		SimulateKey(EKeys::LeftMouseButton,false); SimulateKey(EKeys::F3,false); SimulateKey(EKeys::F4,false); SimulateKey(EKeys::F5,false); SimulateKey(EKeys::F6,false);
		SimulateKey(EKeys::Q,false); SimulateKey(EKeys::E,false);
		Combat->ResetEncounter(); Combat->MaximumHealth=SavedMaxHP; Combat->Health=SavedHP; Combat->Ecstatic=SavedEcstatic;
		Combat->SetFeedback(FString::Printf(TEXT("Verification: %d passed, %d failed"),ChecksPassed,Failures.Num()));
	}
	if (Actions) { Actions->DodgeDistance=SavedDodgeDistance; Actions->SetWeaponExperiment(OriginalWeaponPreset); Actions->SelectPrototypeWeapon(static_cast<EBBSBPrototypeWeapon>(OriginalPrototype)); }
	if (Character) { Character->SetActorTransform(OriginalTransform,false,nullptr,ETeleportType::TeleportPhysics); Character->GetController()->SetControlRotation(OriginalView); }
	if (Joined) Joined->Destroy();
	UE_LOG(LogBBSB,Display,TEXT("CombatVerify COMPLETE passed=%d failed=%d"),ChecksPassed,Failures.Num());
	SetActorTickEnabled(false);
}
