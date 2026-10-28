#include "BBSBCombatRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBCounterSafetyTest,"BBSB.Combat.Exchange.CounterWaitsForPacket",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBCounterSafetyTest::RunTest(const FString& Parameters)
{
 TestFalse(TEXT("No early opening before the final contact and settling beat"),BBSBCombatRules::CanOpenCounter(4.99,5.,false,0));
 TestFalse(TEXT("An unlaunched attack cannot be hidden by a recovery cue"),BBSBCombatRules::CanOpenCounter(5.,5.,true,0));
 TestFalse(TEXT("A missed or bouncing projectile delays recovery until it is harmless"),BBSBCombatRules::CanOpenCounter(6.,5.,false,1));
 TestTrue(TEXT("The opening becomes available once the complete packet is harmless"),BBSBCombatRules::CanOpenCounter(6.5,5.,false,0));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBTimingHistoryTest,"BBSB.Combat.Parry.TimingCoachObservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBTimingHistoryTest::RunTest(const FString& Parameters)
{
	FBBSBParryInput Input; FBBSBParryTimingHistory History;
	Input.Down(10); History.Observe(Input,10.1,5,true,0.28f);
	TestTrue(TEXT("Contact records the real fresh press lead in milliseconds"),History.Kind==EBBSBTimingKind::FreshPress && FMath::IsNearlyEqual(History.LastLeadMs,100.f,0.001f));
	TestTrue(TEXT("Observation cannot consume input or change block outcome"),Input.ConsumedSerial==0 && Input.Resolve(10.1,5,0.15f,0.28f)==EBBSBParryResult::Block);
	History.Observe(Input,10.6,5,true,0.28f);
	TestTrue(TEXT("Automatic hold ticks never inflate the timing distribution"),History.Kind==EBBSBTimingKind::HoldTick && History.Samples.Num()==1 && History.LastLeadMs<0);
	Input.Up(); History.Observe(Input,10.7,5,true,0.28f);
	TestTrue(TEXT("Released or consumed input is not counted as a fresh sample"),History.Kind==EBBSBTimingKind::NoRecentPress && History.Samples.Num()==1);
	Input.Down(11); History.Observe(Input,11.2,INDEX_NONE,true,0.28f);
	TestTrue(TEXT("Early presses are measured without widening their judgement"),History.Samples.Num()==2 && Input.Resolve(11.2,INDEX_NONE,0.15f,0.28f)==EBBSBParryResult::Early);
	Input.Up(); Input.Down(12); History.Observe(Input,12.05,INDEX_NONE,false,0.28f);
	TestTrue(TEXT("Conductor or stance unavailability does not produce timing samples"),History.Kind==EBBSBTimingKind::Unavailable && History.Samples.Num()==2 && Input.PressSerial!=Input.ConsumedSerial);
	History.Observe(Input,13,INDEX_NONE,true,0.28f);
	TestTrue(TEXT("Stale input is excluded rather than plotted at a false zero"),History.Kind==EBBSBTimingKind::NoRecentPress && History.LastLeadMs<0 && History.Samples.Num()==2);
	History.Observe(Input,11.9,INDEX_NONE,true,0.28f);
	TestTrue(TEXT("Future timestamps cannot create a timing sample"),History.Samples.Num()==2);
	History.Reset();
	for (int32 I=0; I<30; ++I) { Input.Up(); Input.Down(20+I); History.Observe(Input,20+I+(I+1)*0.001,INDEX_NONE,true,0.28f); }
	TestTrue(TEXT("History retains only the most recent twenty-four fresh contacts"),History.Samples.Num()==24 && FMath::IsNearlyEqual(History.Samples[0],7.f,0.001f));
	TestTrue(TEXT("Rolling median and range use the retained sample set"),FMath::IsNearlyEqual(History.MedianMs(),18.5f,0.001f) && FMath::IsNearlyEqual(History.MinMs(),7.f,0.001f) && FMath::IsNearlyEqual(History.MaxMs(),30.f,0.001f));
	History.Reset();
	TestTrue(TEXT("Reset clears both statistics and the last marker"),History.Samples.IsEmpty() && History.Kind==EBBSBTimingKind::Waiting && History.LastLeadMs<0 && History.MedianMs()==0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBWeaponPresetTest,"BBSB.Combat.Actions.WeaponCommitmentPresets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBWeaponPresetTest::RunTest(const FString& Parameters)
{
	FBBSBWeaponActionRule Authored; Authored.Duration=0.6f; Authored.DodgeCancelOpen=0.2f; Authored.ChainCancelOpen=0.4f;
	Authored.DamageMultiplier=1.7f; Authored.Priority=32;
	const auto Agile=BBSBCombatRules::WeaponRuleForPreset(Authored,EBBSBWeaponCommitment::Agile);
	const auto Heavy=BBSBCombatRules::WeaponRuleForPreset(Authored,EBBSBWeaponCommitment::Heavy);
	TestTrue(TEXT("Agile reduces commitment and damage from the authored rule"),FMath::IsNearlyEqual(Agile.Duration,0.48f,0.00001f) && FMath::IsNearlyEqual(Agile.DamageMultiplier,1.445f,0.00001f));
	TestTrue(TEXT("Heavy increases commitment and damage from the authored rule"),FMath::IsNearlyEqual(Heavy.Duration,0.9f,0.00001f) && FMath::IsNearlyEqual(Heavy.DamageMultiplier,2.295f,0.00001f));
	FBBSBActionArbiter Gate; Gate.Start(EBBSBActionState::Attack,Heavy,0,1);
	TestFalse(TEXT("Heavy denies dodge before its actual opening"),Gate.CanDodge(1.2));
	TestTrue(TEXT("Heavy permits dodge after its actual opening"),Gate.CanDodge(1.5));
	Gate.Start(EBBSBActionState::Attack,Agile,0,2);
	TestTrue(TEXT("Agile opens cancellation earlier"),Gate.CanDodge(2.04));
	TestFalse(TEXT("Agile still denies a new attack during commitment"),Gate.CanAttack(Agile,0,2.04,false));
	Authored.bLockMovement=false; Authored.bOccupyConductor=false; Authored.bDodgeCancelable=false; Authored.bAllowGestureChain=false;
	for (auto Preset:{EBBSBWeaponCommitment::Agile,EBBSBWeaponCommitment::Heavy})
	{
		const auto Rule=BBSBCombatRules::WeaponRuleForPreset(Authored,Preset);
		TestTrue(TEXT("Preset preserves authored permissions and action priority"),!Rule.bLockMovement && !Rule.bOccupyConductor && !Rule.bDodgeCancelable && !Rule.bAllowGestureChain && Rule.Priority==32);
		Gate.Start(EBBSBActionState::Attack,Rule,0,3);
		TestFalse(TEXT("Preset cannot enable a forbidden dodge cancel"),Gate.CanDodge(3+Rule.Duration*0.9));
	}
	const auto Baseline=BBSBCombatRules::WeaponRuleForPreset(Authored,EBBSBWeaponCommitment::Authored);
	TestTrue(TEXT("Baseline is exactly the authored rule, including every permission"),FBBSBWeaponActionRule::StaticStruct()->CompareScriptStruct(&Authored,&Baseline,0));
	TestTrue(TEXT("Comparison leaves source values unchanged"),FMath::IsNearlyEqual(Authored.Duration,0.6f) && FMath::IsNearlyEqual(Authored.DamageMultiplier,1.7f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBActionCancelTest,"BBSB.Combat.Actions.PriorityAndCancel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBActionCancelTest::RunTest(const FString& Parameters)
{
	FBBSBActionArbiter Gate;
	FBBSBWeaponActionRule Attack; Attack.Duration=0.5f; Attack.ChainCancelOpen=0.4f;
	Gate.Start(EBBSBActionState::Attack,Attack,0,1.0);
	const uint32 AttackRevision=Gate.Revision;
	TestFalse(TEXT("A new attack cannot silently queue during windup"),Gate.CanAttack(Attack,0,1.1));
	TestFalse(TEXT("Other channel cannot overlap a committed action"),Gate.CanAttack(Attack,1,1.3));
	TestTrue(TEXT("Same gesture chain can replace after its authored window"),Gate.CanAttack(Attack,0,1.3));
	TestFalse(TEXT("A new mouse press cannot bypass animation commitment"),Gate.CanAttack(Attack,0,1.3,false));
	TestTrue(TEXT("Dodge can cancel the default attack immediately"),Gate.CanDodge(1.01));
	FBBSBWeaponActionRule Dodge; Dodge.Duration=0.3f; Dodge.Priority=60;
	Gate.Start(EBBSBActionState::Dodge,Dodge,-1,1.1);
	TestTrue(TEXT("Old attack revision is invalid as soon as dodge starts"),AttackRevision!=Gate.Revision);
	TestFalse(TEXT("No attack can execute during dodge"),Gate.CanAttack(Attack,0,1.2));
	TestTrue(TEXT("No old attack remains after dodge ends"),!Gate.IsBusy(1.41));
	TestEqual(TEXT("No queued transition occurs on expiry"),int32(Gate.State),int32(EBBSBActionState::Dodge));
	Attack.DodgeCancelOpen=0.5f; Gate.Start(EBBSBActionState::Attack,Attack,0,2);
	TestFalse(TEXT("Heavy weapon can author a non-cancelable opening"),Gate.CanDodge(2.1));
	TestTrue(TEXT("Heavy weapon opens dodge cancellation later"),Gate.CanDodge(2.3));
	Gate.Start(EBBSBActionState::Dead,Attack,-1,3);
	TestFalse(TEXT("Death cannot expire into new attacks"),Gate.CanAttack(Attack,0,999));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBParryRecoveryTest,"BBSB.Combat.Parry.FreshPressAndHoldRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBParryRecoveryTest::RunTest(const FString& Parameters)
{
	FBBSBParryInput Input;
	Input.Down(0);
	TestTrue(TEXT("Holding a key long before contact does not tap-parry"),Input.Resolve(1,INDEX_NONE,0.15f,0.28f)==EBBSBParryResult::Miss);
	Input.Up(); Input.Down(1.1);
	TestTrue(TEXT("Fresh key-down blocks first hold hit"),Input.Resolve(1.2,7,0.15f,0.28f)==EBBSBParryResult::Block);
	TestTrue(TEXT("Keeping that key held blocks later ticks of the same hold"),Input.Resolve(1.7,7,0.15f,0.28f)==EBBSBParryResult::Block);
	TestTrue(TEXT("A different hold cannot reuse a consumed tap"),Input.Resolve(1.8,8,0.15f,0.28f)==EBBSBParryResult::Miss);
	Input.Up();
	TestTrue(TEXT("Releasing a hold loses its next tick"),Input.Resolve(2,7,0.15f,0.28f)==EBBSBParryResult::Miss);
	Input.Down(2.4);
	TestTrue(TEXT("New key-down can recover later ticks"),Input.Resolve(2.5,7,0.15f,0.28f)==EBBSBParryResult::Block);
	Input.Up(); Input.Down(3); Input.Up();
	TestTrue(TEXT("A short released tap still has its timing window"),Input.Resolve(3.08,INDEX_NONE,0.15f,0.28f)==EBBSBParryResult::Block);
	Input.Down(4);
	TestTrue(TEXT("Early press has its own outcome at contact"),Input.Resolve(4.2,INDEX_NONE,0.15f,0.28f)==EBBSBParryResult::Early);
	FBBSBParryInput Up,Down;
	Up.Down(5); Down.Down(5);
	TestTrue(TEXT("WX supports independent simultaneous Up and Down"),Up.Resolve(5.1,INDEX_NONE,0.15f,0.28f)==EBBSBParryResult::Block && Down.Resolve(5.1,INDEX_NONE,0.15f,0.28f)==EBBSBParryResult::Block);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBContactGeometryTest,"BBSB.Combat.Parry.WorldPlaneContact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBContactGeometryTest::RunTest(const FString& Parameters)
{
	FVector Contact;
	TestTrue(TEXT("Incoming segment crosses actual parry plane"),BBSBCombatRules::PlaneIntersection(FVector(100,-20,30),FVector(-100,100,130),FVector::ZeroVector,FVector::ForwardVector,Contact));
	TestTrue(TEXT("Contact is interpolated rather than classified from spawn side"),Contact.Equals(FVector(0,40,80),0.01));
	TestTrue(TEXT("The final contact is Up despite starting on the left"),BBSBCombatRules::DirectionAtPlane(FVector2D(Contact.Y,Contact.Z),FVector2D(145,125))==EBBSBParryDirection::Up);
	TestFalse(TEXT("A segment departing the player does not hit again"),BBSBCombatRules::PlaneIntersection(FVector(-20,0,0),FVector(20,0,0),FVector::ZeroVector,FVector::ForwardVector,Contact));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBRhythmSocketTest,"BBSB.Combat.Rhythm.SocketAndClock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBRhythmSocketTest::RunTest(const FString& Parameters)
{
	BBSBCombatRules::FSocketLedger Slots;
	TestTrue(TEXT("Hold reserves its direction across duration"),Slots.Reserve(0,1,1.5));
	TestFalse(TEXT("A tap cannot collide with a reserved hold tick"),Slots.Reserve(1,1));
	TestTrue(TEXT("Opposite direction remains a playable combination"),Slots.Reserve(1,4));
	TestFalse(TEXT("Three simultaneous directions are rejected"),Slots.Reserve(1,2));
	TestTrue(TEXT("A freed later socket can be used"),Slots.Reserve(2,1));
	for (int32 FPS:{30,60,120})
	{
		const double Epoch=500.125;
		const int32 Frames=FPS*600;
		const double Last=Epoch+double(Frames)/FPS;
		TestTrue(*FString::Printf(TEXT("Ten-minute clock has no frame accumulation drift at %d FPS"),FPS),FMath::IsNearlyEqual(BBSBCombatRules::BeatAt(Last,Epoch,110),1100.0,0.00001));
	}
	TestEqual(TEXT("Entry bonus must be applied after calculating the initial hit"),BBSBCombatRules::EcstaticDamageMultiplier(0),1.f);
	TestTrue(TEXT("Each complete Ecstatic level increases later damage"),BBSBCombatRules::EcstaticDamageMultiplier(175)>1.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBBodyContactTest,"BBSB.Combat.Projectile.PhysicalBodyContact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBBodyContactTest::RunTest(const FString& Parameters)
{
 FVector Hit;float T=0;
 TestTrue(TEXT("A high-speed shot finds the first physical capsule surface"),BBSBCombatRules::SweptCapsuleContact(FVector(1000,0,0),FVector(-1000,0,0),FVector::ZeroVector,42,92,10,Hit,T) && FMath::Abs(Hit.X-52)<.04f && T>0 && T<1);
 TestFalse(TEXT("A path through the former wide input plane can miss the player"),BBSBCombatRules::SweptCapsuleContact(FVector(1000,100,0),FVector(-1000,100,0),FVector::ZeroVector,42,92,10,Hit,T));
 TestTrue(TEXT("The rounded head of the capsule is hittable"),BBSBCombatRules::SweptCapsuleContact(FVector(1000,0,80),FVector(-1000,0,80),FVector::ZeroVector,42,92,10,Hit,T) && Hit.X>40 && Hit.X<44);
 TestFalse(TEXT("An overhead near miss causes no body contact"),BBSBCombatRules::SweptCapsuleContact(FVector(1000,0,140),FVector(-1000,0,140),FVector::ZeroVector,42,92,10,Hit,T));
 TestFalse(TEXT("Moving off the frozen trajectory avoids collision"),BBSBCombatRules::SweptCapsuleContact(FVector(1000,0,0),FVector(-1000,0,0),FVector(0,150,0),42,92,10,Hit,T));
 return true;
}

#endif
