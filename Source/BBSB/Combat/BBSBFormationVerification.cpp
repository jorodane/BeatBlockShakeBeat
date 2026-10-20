#include "BBSBFormationVerification.h"
#include "BBSBCombat.h"
#include "BBSBCombatVisuals.h"
#include "BBSB.h"
#include "Field/BBSBFieldActions.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABBSBFormationVerification::ABBSBFormationVerification()
{
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostPhysics;
}
void ABBSBFormationVerification::BeginPlay()
{
	Super::BeginPlay();
	Character=Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this,0));
	Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>() : nullptr;
	if (!Character || !Combat || !Character->GetController()) { Check(false,TEXT("Local combat character exists")); Finish(); return; }
	OriginalTransform=Character->GetActorTransform(); OriginalView=Character->GetController()->GetControlRotation();
	SavedMaximumHealth=Combat->MaximumHealth; SavedHealth=Combat->Health; SavedEcstatic=Combat->Ecstatic;
	Combat->MaximumHealth=Combat->Health=100000;
	if (!StartEncounter()) { Finish(); return; }
	Advance(0,TEXT("Reachable Front replacement fixture"));
}
void ABBSBFormationVerification::Check(bool bCondition,const TCHAR* Message)
{
	if (bCondition) { ++ChecksPassed; UE_LOG(LogBBSB,Display,TEXT("FormationVerify PASS: %s"),Message); }
	else { Failures.Add(Message); UE_LOG(LogBBSB,Error,TEXT("FormationVerify FAIL: %s"),Message); }
}
void ABBSBFormationVerification::Advance(int32 Next,const TCHAR* Label)
{
	Step=Next; PhaseLabel=Label; StepAt=GetWorld()->GetTimeSeconds();
}
bool ABBSBFormationVerification::StartEncounter()
{
	if (Wall) Wall->Destroy(); Wall=nullptr;
	Combat->GoToPractice();
	First=nullptr;
	for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It) if (!First || It->GetActorLocation().X<First->GetActorLocation().X) First=*It;
	if (!First) { Check(false,TEXT("Practice enemies exist")); return false; }
	UGameplayStatics::ApplyDamage(First,1,Character->GetController(),Character,nullptr); Stage=Combat->Stage;
	Check(Stage && Stage->Members.Num()>=3,TEXT("Three-enemy fixture forms a real stage"));
	if (!Stage || Stage->Members.Num()<3) return false;
	Stage->bPausePatternEmission=true; Stage->bAudibleMetronome=false; Stage->JoinRadius=1;
	return true;
}
bool ABBSBFormationVerification::CorridorBlocked(ABBSBInkEnemy* Enemy) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBFormationFixture),false,this);
	Query.AddIgnoredActor(Character);
	for (ABBSBInkEnemy* Member:Stage->Members) if (Member) Query.AddIgnoredActor(Member);
	FHitResult Hit;
	return GetWorld()->SweepSingleByObjectType(Hit,Enemy->GetActorLocation()+FVector(0,0,3),Center+FVector(0,0,74),FQuat::Identity,BBSBCombatVisuals::GeometryTypes(),FCollisionShape::MakeCapsule(38,69),Query);
}
bool ABBSBFormationVerification::Arrange(bool bOnlyBlockedCandidate,bool bLateWall)
{
	Center=Stage->GetActorLocation(); NearBacker=FarBacker=nullptr;
	for (ABBSBInkEnemy* Member:Stage->Members) if (Member!=Stage->Front && !Member->bDefeated) { if (!NearBacker) NearBacker=Member; else if (!FarBacker) FarBacker=Member; }
	if (!NearBacker || !FarBacker) { Check(false,TEXT("Two replacement candidates exist")); return false; }
	NearBacker->SetActorLocation(Center+FVector(0,-200,71),false,nullptr,ETeleportType::TeleportPhysics);
	FarBacker->SetActorLocation(Center+FVector(0,300,71),false,nullptr,ETeleportType::TeleportPhysics);
	NearBacker->GetCharacterMovement()->StopMovementImmediately(); FarBacker->GetCharacterMovement()->StopMovementImmediately();
	Wall=GetWorld()->SpawnActor<AActor>();
	if (!Wall) { Check(false,TEXT("Temporary obstacle spawns")); return false; }
	UBoxComponent* Shape=NewObject<UBoxComponent>(Wall,TEXT("TemporaryFormationWall")); Wall->AddInstanceComponent(Shape); Wall->SetRootComponent(Shape);
	Shape->SetMobility(EComponentMobility::Movable); Shape->SetBoxExtent(FVector(300,25,150));
	Shape->SetCollisionProfileName(TEXT("BlockAll")); Shape->SetCollisionObjectType(ECC_WorldStatic); Shape->RegisterComponent();
	Wall->SetActorLocation(Center+FVector(0,-100,148));
	DrawDebugBox(GetWorld(),Wall->GetActorLocation(),Shape->GetScaledBoxExtent(),FColor(180,135,190),false,7,0,2);
	Check(CorridorBlocked(NearBacker) && !CorridorBlocked(FarBacker),TEXT("Real collision blocks the nearer candidate while leaving the farther corridor open"));
	if (bLateWall) Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bOnlyBlockedCandidate) UGameplayStatics::ApplyDamage(FarBacker,100000,Character->GetController(),Character,nullptr);
	const FVector Launch=Center+FVector(-100,0,200);
	Witness=GetWorld()->SpawnActor<ABBSBInkProjectile>(ABBSBInkProjectile::StaticClass(),Launch,FRotator::ZeroRotator);
	WitnessTarget=Launch+FVector(500,0,0);
	if (Witness) Witness->Initialize(Stage,NearBacker,WitnessTarget,EBBSBInkPattern::Tap,12,INDEX_NONE);
	UGameplayStatics::ApplyDamage(Stage->Front,100000,Character->GetController(),Character,nullptr);
	if (bLateWall) Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	UE_LOG(LogBBSB,Display,TEXT("FormationVerify selected=%s near=%s far=%s center=%s"),*GetNameSafe(Stage->Front),*GetNameSafe(NearBacker),*GetNameSafe(FarBacker),*Center.ToCompactString());
	return true;
}
void ABBSBFormationVerification::Tick(float DT)
{
	Super::Tick(DT);
	if (bComplete || !Combat || !Stage) return;
	const double Age=GetWorld()->GetTimeSeconds()-StepAt;
	if (Age>12) { Check(false,TEXT("Obstacle fixture completes within its timeout")); Finish(); return; }
	if ((Step==0 || Step==2 || Step==4) && Stage->Phase==EBBSBStagePhase::Fighting)
	{
		if (!Arrange(Step==2,Step==4)) { Finish(); return; }
		if (Step==0) Check(Stage->Front==FarBacker,TEXT("Replacement prefers the reachable Backer over a nearer blocked Backer"));
		if (Step==4) Check(Stage->Front==NearBacker,TEXT("A newly appearing wall blocks the candidate only after it was selected"));
		Advance(Step==0 ? 1 : Step==2 ? 3 : 5,Step==0 ? TEXT("Physical replacement around an obstruction") : Step==2 ? TEXT("Blocked replacement releases stage safely") : TEXT("Dynamic obstacle switches to an alternate Backer"));
	}
	else if (Step==1 && Age>3.0)
	{
		Check(Stage->Phase==EBBSBStagePhase::Fighting && Stage->Front==FarBacker && FVector::Dist2D(FarBacker->GetActorLocation(),Center)<45,TEXT("Reachable replacement physically arrives and resumes combat"));
		Check(Stage->GetActorLocation().Equals(Center,1),TEXT("Obstacle replacement preserves stage center"));
		Check(IsValid(Witness) && !Witness->bResolved && Witness->FrozenTarget.Equals(WitnessTarget),TEXT("Replacement preserves the already-fired projectile and its target"));
		if (!StartEncounter()) { Finish(); return; } Advance(2,TEXT("Only blocked candidate fixture"));
	}
	else if (Step==3 && Age>6.3)
	{
		Check(Stage->Phase==EBBSBStagePhase::Dissolved && !Combat->IsStageLocked(),TEXT("A replacement that cannot make progress releases its stage instead of locking combat indefinitely"));
		const auto Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();
		Check(Actions && Actions->CanMove(),TEXT("Blocked replacement restores player field movement"));
		Check(IsValid(Witness) && !Witness->bResolved && Witness->FrozenTarget.Equals(WitnessTarget),TEXT("Blocked-stage release preserves already-fired projectiles"));
		Check(IsValid(NearBacker) && !NearBacker->bDefeated && NearBacker->GetActorLocation().Y<Center.Y-125,TEXT("Blocked candidate remains alive and is not teleported through the wall"));
		if (!StartEncounter()) { Finish(); return; } Advance(4,TEXT("Obstacle appears after candidate selection"));
	}
	else if (Step==5 && Age>8.5)
	{
		Check(Stage->Phase==EBBSBStagePhase::Fighting && Stage->Front==FarBacker && Stage->FrontReplacements==2,TEXT("Stalled replacement retries a different reachable Backer and resumes combat"));
		Check(IsValid(NearBacker) && !NearBacker->bFront && FarBacker->bFront,TEXT("Retry transfers the Front role without leaving stale Front flags"));
		Check(IsValid(Witness) && !Witness->bResolved && Witness->FrozenTarget.Equals(WitnessTarget),TEXT("Dynamic replacement retry preserves already-fired projectiles"));
		Finish();
	}
}
void ABBSBFormationVerification::Finish()
{
	if (bComplete) return;
	bComplete=true; PhaseLabel=Failures.IsEmpty() ? TEXT("PASS") : TEXT("FAIL");
	if (Wall) Wall->Destroy(); Wall=nullptr;
	if (Combat)
	{
		Combat->ResetEncounter(); Combat->MaximumHealth=SavedMaximumHealth; Combat->Health=SavedHealth; Combat->Ecstatic=SavedEcstatic;
		Combat->SetFeedback(FString::Printf(TEXT("Formation: %d passed, %d failed"),ChecksPassed,Failures.Num()));
	}
	if (Character && Character->GetController()) { Character->SetActorTransform(OriginalTransform,false,nullptr,ETeleportType::TeleportPhysics); Character->GetController()->SetControlRotation(OriginalView); }
	UE_LOG(LogBBSB,Display,TEXT("FormationVerify COMPLETE passed=%d failed=%d"),ChecksPassed,Failures.Num());
	SetActorTickEnabled(false);
}
void ABBSBFormationVerification::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Wall) Wall->Destroy();
	Super::EndPlay(Reason);
}
