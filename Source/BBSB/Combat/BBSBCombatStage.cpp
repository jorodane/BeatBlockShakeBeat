#include "BBSBCombat.h"
#include "BBSBCombatVisuals.h"
#include "BBSB.h"
#include "Field/BBSBFieldActions.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"

using namespace BBSBCombatVisuals;

ABBSBCombatStage::ABBSBCombatStage()
{
	PrimaryActorTick.bCanEverTick=true;
	PrimaryActorTick.TickGroup=TG_PostPhysics;
	RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("StageOrigin"));
	Metronome=CreateDefaultSubobject<UAudioComponent>(TEXT("PracticeMetronome"));
	Metronome->SetupAttachment(RootComponent); Metronome->bAutoActivate=false; Metronome->bIsUISound=true;
}
void ABBSBCombatStage::BeginPlay() { Super::BeginPlay(); }

bool ABBSBCombatStage::Initialize(UBBSBCombatComponent* Combat,ABBSBInkEnemy* First)
{
	if (!Combat || !First || First->bDefeated) return false;
	Player=Combat; Front=First;EncounterID=First->EncounterID;
	ACharacter* Character=Cast<ACharacter>(Combat->GetOwner());
	if (!Character) return false;
	FVector Center;
	if (!Ground(GetWorld(),First->GetActorLocation(),Center)) return false;
	SetActorLocation(Center+FVector(0,0,2));
	FVector PlayerLocation;
	if (!FindFormation(Character,PlayerLocation)) return false;
	bFormationValid=true;
	PlayerLineFacing=(GetActorLocation()-PlayerLocation).GetSafeNormal2D();
	PreferredFrontOffset=FVector::ZeroVector;
	Character->SetActorLocation(PlayerLocation,false,nullptr,ETeleportType::TeleportPhysics);
	Character->SetActorRotation(PlayerLineFacing.Rotation());
	Character->GetCharacterMovement()->StopMovementImmediately();
	for (TActorIterator<ABBSBCombatPracticeArea> It(GetWorld()); It; ++It) if (First->PracticeArea==*It || It->EncounterID==First->EncounterID) { SetExperiment(It->ExperimentPreset); break; }
	EnteredAt=GetWorld()->GetTimeSeconds(); MusicEpoch=EnteredAt+0.18+3.0*60.0/BPM;
	if (UBBSBFieldActionComponent* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>()) Actions->InterruptAction(EBBSBActionState::Transition,float(MusicEpoch-EnteredAt));
	AddMember(First);
	// Experiment choice belongs to this encounter, not to global defaults.
	for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It)
	{
		if (*It!=First && !It->Stage && !It->bDefeated && It->EncounterID==First->EncounterID && FVector::DistSquared(It->GetActorLocation(),GetActorLocation())<FMath::Square(JoinRadius*0.65f)) AddMember(*It);
	}
	CreateInk(); UpdateFormation(0); UpdateInk();
	UE_LOG(LogBBSB,Display,TEXT("Stage formed center=%s player=%s members=%d BPM=%.0f"),*GetActorLocation().ToCompactString(),*PlayerLocation.ToCompactString(),Members.Num(),BPM);
	return true;
}

bool ABBSBCombatStage::FindFormation(ACharacter* Character,FVector& OutPlayer)
{
	const FVector Center=GetActorLocation();
	FVector Initial=(Character->GetActorLocation()-Center).GetSafeNormal2D();
	if (Initial.IsNearlyZero()) Initial=-Character->GetActorForwardVector();
	const float HalfHeight=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Radius=Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBStagePlacement),false,Character);
	for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It) Query.AddIgnoredActor(*It);
	FCollisionObjectQueryParams Solids; Solids.AddObjectTypesToQuery(ECC_WorldStatic); Solids.AddObjectTypesToQuery(ECC_WorldDynamic);
	double BestScore=DBL_MAX; bool bFound=false;
	for (int32 Index=0; Index<16; ++Index)
	{
		const float Angle=(Index%2 ? -1.f : 1.f)*FMath::CeilToFloat(Index/2.f)*22.5f;
		const FVector Candidate=Center+Initial.RotateAngleAxis(Angle,FVector::UpVector)*ParryDistance;
		FVector Floor; if (!Ground(GetWorld(),Candidate,Floor) || FMath::Abs(Floor.Z-Center.Z)>65.f) continue;
		const FVector Position=Floor+FVector(0,0,HalfHeight+2);
		if (GetWorld()->OverlapAnyTestByObjectType(Position,FQuat::Identity,Solids,FCollisionShape::MakeCapsule(Radius+3,HalfHeight),Query)) continue;
		FHitResult Obstacle;
		// The formation may slide the player into a corridor, but cannot teleport through a wall.
		if (GetWorld()->SweepSingleByObjectType(Obstacle,Character->GetActorLocation()+FVector(0,0,3),Position,FQuat::Identity,Solids,FCollisionShape::MakeCapsule(Radius,HalfHeight-4),Query)) continue;
		if (GetWorld()->LineTraceSingleByObjectType(Obstacle,Position+FVector(0,0,25),Center+FVector(0,0,100),Solids,Query)) continue;
		bool bRingClear=true;
		for (int32 Ring=0; Ring<8; ++Ring)
		{
			FVector Sample; const FVector RingPoint=Center+FVector(RingRadius,0,0).RotateAngleAxis(Ring*45.f,FVector::UpVector);
			if (!Ground(GetWorld(),RingPoint,Sample) || FMath::Abs(Sample.Z-Center.Z)>65.f) { bRingClear=false; break; }
		}
		if (!bRingClear) continue;
		const double Score=FVector::DistSquared(Position,Character->GetActorLocation());
		if (Score<BestScore) { BestScore=Score; OutPlayer=Position; bFound=true; }
	}
	return bFound;
}

void ABBSBCombatStage::AddMember(ABBSBInkEnemy* Enemy)
{
	if (!IsValid(Enemy) || Enemy->bDefeated || Enemy->EncounterID!=EncounterID || Members.Contains(Enemy) || (Enemy->Stage && Enemy->Stage!=this)) return;
	Members.Add(Enemy); Enemy->JoinStage(this,Enemy==Front);
	if (Members.Num()>1) ++Joins;
	RingRadius=220.f+FMath::Max(0,Members.Num()-2)*45.f;
	UE_LOG(LogBBSB,Display,TEXT("Stage join %s members=%d radius=%.0f"),*Enemy->GetName(),Members.Num(),RingRadius);
}

bool ABBSBCombatStage::IsActive() const { return Phase!=EBBSBStagePhase::Dissolved && Phase!=EBBSBStagePhase::Victory; }
bool ABBSBCombatStage::IsOnBeat(float WindowSeconds) const
{
	if (Phase!=EBBSBStagePhase::Fighting) return false;
	return FMath::Abs(CurrentBeat-FMath::RoundToDouble(CurrentBeat))*60.0/BPM<=WindowSeconds;
}
float ABBSBCombatStage::BeatPulse() const
{
	const double Beat=BBSBCombatRules::BeatAt(GetWorld()->GetTimeSeconds(),MusicEpoch,BPM);
	return 1.f-FMath::Clamp(float(FMath::Frac(Beat)*5),0.f,1.f);
}
FString ABBSBCombatStage::CountInText() const
{
	const double Age=GetWorld()->GetTimeSeconds()-EnteredAt;
	if (Age<0.18) return TEXT("Beat!");
	const int32 Count=FMath::FloorToInt((Age-0.18)*BPM/60.0);
	return Count==0 ? TEXT("Block,") : Count==1 ? TEXT("Shake~") : Count==2 ? TEXT("Beat!") : TEXT("");
}

void ABBSBCombatStage::Tick(float DT)
{
	Super::Tick(DT);
	if (!Player || !Player->GetOwner()) { if (IsActive()) Dissolve(false); return; }
	const double Now=GetWorld()->GetTimeSeconds();
	if (!IsActive())
	{
		if (Phase==EBBSBStagePhase::Dissolved)
		{
			bool bAllDead=!Members.IsEmpty();
			for (ABBSBInkEnemy* Member:Members) if (IsValid(Member) && !Member->bDefeated) bAllDead=false;
			if (bAllDead) { Phase=EBBSBStagePhase::Victory; ClearStageProjectiles(); }
		}
		return;
	}
	CurrentBeat=BBSBCombatRules::BeatAt(Now,MusicEpoch,BPM);
	if (Now<EnteredAt+0.18) Phase=EBBSBStagePhase::Forming;
	else if (Now<MusicEpoch) Phase=EBBSBStagePhase::CountIn;
	else if (Phase==EBBSBStagePhase::Forming || Phase==EBBSBStagePhase::CountIn)
	{
		Phase=EBBSBStagePhase::Fighting; Player->SetFeedback(TEXT("GO!"));
	}
	if (Phase==EBBSBStagePhase::Replacing && IsValid(Front))
	{
		const float Distance=FVector::Dist2D(Front->GetActorLocation(),GetActorLocation());
		if (Distance<45)
		{
			Phase=EBBSBStagePhase::Fighting;
			PendingNotes.Reset(); NextBar=FMath::Max(0,FMath::CeilToInt(float((CurrentBeat-2)/8.0)));
			Player->SetFeedback(FString::Printf(TEXT("NEW FRONT  %s"),*Front->EnemyName.ToString()));
		}
		else
		{
			if (Distance<ReplacementBestDistance-5.f) { ReplacementBestDistance=Distance; ReplacementProgressAt=Now; }
			if (Now-ReplacementProgressAt>=FMath::Max(0.5f,ReplacementStallSeconds))
			{
				UE_LOG(LogBBSB,Display,TEXT("Front replacement stalled %s distance=%.1f noProgress=%.2f"),*GetNameSafe(Front),Distance,Now-ReplacementProgressAt);
				if (!StartReplacement(true,Front)) { Dissolve(false); Player->SetFeedback(TEXT("FORMATION BLOCKED  |  stage released")); return; }
			}
		}
	}
	UpdateFormation(DT); UpdateInk();
	const int32 ClickBeat=FMath::FloorToInt(CurrentBeat);
	if (ClickBeat!=LastClickBeat) { LastClickBeat=ClickBeat; Click(ClickBeat); }
	if (Phase==EBBSBStagePhase::Fighting && !bPausePatternEmission)
	{
	 if(bUseCombatExchanges && ExperimentPreset==0) TickExchange();
	 else
	 {
		while (NextBar*8.0<=CurrentBeat+0.25) PlanBar(NextBar++);
		for (int32 Index=0; Index<PendingNotes.Num();)
		{
			if (PendingNotes[Index].ImpactBeat-2.0<=CurrentBeat)
			{
				const FNote Note=PendingNotes[Index]; PendingNotes.RemoveAt(Index); Launch(Note);
			}
			else ++Index;
		}
	 }
	}
	if (Now>=NextJoinCheck)
	{
		NextJoinCheck=Now+0.5;
		for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It)
			if (!It->Stage && !It->bDefeated && It->EncounterID==EncounterID && FVector::DistSquared(It->GetActorLocation(),GetActorLocation())<FMath::Square(JoinRadius))
   {
    FHitResult Cover;FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBReinforcementCover),false,*It);Q.AddIgnoredActor(Front);
    if(!GetWorld()->LineTraceSingleByObjectType(Cover,It->GetAimPoint(),GetActorLocation()+FVector(0,0,70),GeometryTypes(),Q)) AddMember(*It);
   }
	}
	const float Distance=FVector::Dist2D(Player->GetOwner()->GetActorLocation(),GetActorLocation());
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBAggro),false,this); Query.AddIgnoredActor(Player->GetOwner());
	for (ABBSBInkEnemy* Member:Members) if (Member) Query.AddIgnoredActor(Member);
	FHitResult Obstacle;
	const bool bBlocked=GetWorld()->LineTraceSingleByObjectType(Obstacle,GetActorLocation()+FVector(0,0,100),Player->GetOwner()->GetActorLocation(),GeometryTypes(),Query);
	if (Distance>LoseAggroDistance || bBlocked)
	{
		if (AggroLostAt<0) AggroLostAt=Now;
		if (Now-AggroLostAt>=LoseAggroSeconds) Dissolve(false);
	}
	else AggroLostAt=-1;
}

void ABBSBCombatStage::UpdateFormation(float DT)
{
	if (!Player || !Player->GetOwner()) return;
	const FVector PlayerPosition=Player->GetOwner()->GetActorLocation();
	FVector TowardPlayer=(PlayerPosition-GetActorLocation()).GetSafeNormal2D();
	if (TowardPlayer.IsNearlyZero()) TowardPlayer=-PlayerLineFacing;
	const float Distance=FVector::Dist2D(PlayerPosition,GetActorLocation());
	if (!IsCounterWindow() && !Player->bCombatStance && Distance>ParryDistance+140 && Phase==EBBSBStagePhase::Fighting)
	{
		const FVector Proposed=GetActorLocation()+TowardPlayer*FMath::Min(ChaseSpeed*DT,Distance-ParryDistance);
		FVector Floor; FHitResult Obstacle;
		if (Ground(GetWorld(),Proposed,Floor) && FMath::Abs(Floor.Z-GetActorLocation().Z)<35
			&& !GetWorld()->LineTraceSingleByObjectType(Obstacle,GetActorLocation()+FVector(0,0,70),Floor+FVector(0,0,70),GeometryTypes())) SetActorLocation(Floor+FVector(0,0,2));
	}
	int32 BackerIndex=0;
	int32 Living=0; for (ABBSBInkEnemy* Member:Members) if (IsValid(Member) && !Member->bDefeated) ++Living;
	for (ABBSBInkEnemy* Member:Members)
	{
		if (!IsValid(Member) || Member->bDefeated) continue;
		FVector Goal=GetActorLocation();
		if (Member!=Front)
		{
			const float Angle=Living<=2 ? 155.f : FMath::Lerp(115.f,245.f,float(BackerIndex)/FMath::Max(1,Living-2));
			Goal+=TowardPlayer.RotateAngleAxis(Angle,FVector::UpVector)*(RingRadius-25); ++BackerIndex;
		}
		FVector Floor; if (Ground(GetWorld(),Goal,Floor)) Goal.Z=Floor.Z+Member->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1;
		Member->FormationTarget=Goal;
	}
}

void ABBSBCombatStage::CreateInk()
{
	for (int32 Index=0; Index<40; ++Index) RingInk.Add(Ink(this,FName(*FString::Printf(TEXT("Ring_%d"),Index))));
}
void ABBSBCombatStage::UpdateInk()
{
	for (int32 Index=0; Index<RingInk.Num(); ++Index)
	{
		UStaticMeshComponent* Segment=RingInk[Index].Get(); if (!Segment) continue;
		const float Angle=360.f*Index/RingInk.Num();
		FVector Position=GetActorLocation()+FVector(RingRadius,0,0).RotateAngleAxis(Angle,FVector::UpVector);
		FVector Floor; if (Ground(GetWorld(),Position,Floor)) Position.Z=Floor.Z+2;
		Segment->SetWorldLocationAndRotation(Position,FRotator(0,Angle+90,0));
		Segment->SetWorldScale3D(FVector((2*PI*RingRadius/RingInk.Num()+2)/100.f,0.032f,0.015f));
	}
}

void ABBSBCombatStage::PlanBar(int32 Bar)
{
	if (!IsValid(Front) || Front->bDefeated) return;
	BBSBCombatRules::FSocketLedger Ledger;
	const float Start=2.f+Bar*8.f;
	const EBBSBInkPattern Main=ExperimentPreset==0 && bPreferStraightAttacks && Front->MainPattern!=EBBSBInkPattern::Hold ? EBBSBInkPattern::Tap:Front->MainPattern;
	for (int32 Index=0; Index<4; ++Index)
	{
		const float Offset=Index*2.f;
		const int32 Direction=(Bar+Index)%4;
		const uint8 Mask=uint8(1<<Direction);
		const bool bHold=(Main==EBBSBInkPattern::Hold && Index==2) || (ExperimentPreset==2 && Index%2==0);
		if (!Ledger.Reserve(Offset,Mask,bHold ? 1.5f : 0.f)) continue;
		const int32 Group=bHold ? NextHoldGroup++ : INDEX_NONE;
		const int32 Pulses=bHold ? 4 : 1;
		for (int32 Pulse=0; Pulse<Pulses; ++Pulse)
		{
			FNote Note; Note.ImpactBeat=Start+Offset+Pulse*0.5f; Note.Directions=Mask; Note.Source=Front;
			Note.Pattern=bHold ? EBBSBInkPattern::Hold : Main; Note.HoldGroup=Group;
			PendingNotes.Add(Note);
		}
		if (Bar>0 && Index==3 && ExperimentPreset!=4)
		{
			const uint8 Opposite=uint8(1<<((Direction+2)%4));
			if (Ledger.Reserve(Offset,Opposite)) { FNote Note; Note.ImpactBeat=Start+Offset; Note.Directions=Opposite; Note.Source=Front; PendingNotes.Add(Note); }
		}
	}
	TArray<ABBSBInkEnemy*> Backers;
	for (ABBSBInkEnemy* Member:Members) if (IsValid(Member) && Member!=Front && !Member->bDefeated) Backers.Add(Member);
	if (!Backers.IsEmpty() && ExperimentPreset!=4)
	{
		for (int32 Socket=1; Socket<8; Socket+=2)
		{
			ABBSBInkEnemy* Backer=Backers[(Bar+Socket/2)%Backers.Num()];
			const uint8 Mask=uint8(1<<((Bar+Socket+1)%4));
			if (!Ledger.Reserve(float(Socket),Mask)) continue;
			FNote Note; Note.ImpactBeat=Start+Socket; Note.Directions=Mask; Note.Source=Backer;
			Note.Pattern=ExperimentPreset==0 && bPreferStraightAttacks ? EBBSBInkPattern::Tap : (Backer->MainPattern==EBBSBInkPattern::Hold ? EBBSBInkPattern::Zigzag:Backer->MainPattern);
			PendingNotes.Add(Note);
		}
	}
}

void ABBSBCombatStage::Launch(const FNote& Note)
{
	ABBSBInkEnemy* Source=Note.Source.Get();
	if (!Source || Source->bDefeated || !Player || Player->bDead) return;
	FVector Origin,Normal,Right; Player->GetParryPlane(Origin,Normal,Right);
	const float Flight=FMath::Max(0.25f,float((Note.ImpactBeat-CurrentBeat)*60.0/BPM));
	for (int32 Direction=0; Direction<4; ++Direction)
	{
		if ((Note.Directions&(1<<Direction))==0) continue;
		FVector Target=Player->DefenseTarget(Direction);
		if (Note.Pattern==EBBSBInkPattern::Area)
		{
			FVector Floor; if (Ground(GetWorld(),Player->GetOwner()->GetActorLocation(),Floor)) Target=Floor+FVector(0,0,3);
		}
		ABBSBInkProjectile* Shot=GetWorld()->SpawnActor<ABBSBInkProjectile>(ABBSBInkProjectile::StaticClass(),Source->GetAimPoint(),FRotator::ZeroRotator);
		if (Shot)
  {
   Shot->Initialize(this,Source,Target,Note.Pattern,Flight,Note.HoldGroup);
   if(Note.Pattern!=EBBSBInkPattern::Area && Note.Pattern!=EBBSBInkPattern::Bounce)
   {
    const float Width=FMath::Max(0.f,float(Player->IncomingPathSpread.X)),Height=FMath::Max(0.f,float(Player->IncomingPathSpread.Y));
    // Raise the fan's center so the lower lane remains above the floor.
    Shot->ApproachOffset=FVector::UpVector*Height;
    if(Direction==0) Shot->ApproachOffset+=FVector::UpVector*Height;
    else if(Direction==2) Shot->ApproachOffset-=FVector::UpVector*Height;
    else Shot->ApproachOffset+=Right*(Direction==1 ? Width:-Width);
   }
   if(bUseCombatExchanges && ExperimentPreset==0) ExchangeProjectiles.Add(Shot);
   ++ShotsFired;
  }
	}
}

void ABBSBCombatStage::EnemyDefeated(ABBSBInkEnemy* Enemy)
{
	if (!IsActive()) return;
	bool bAnyLiving=false;
	for (ABBSBInkEnemy* Member:Members) if (IsValid(Member) && !Member->bDefeated) { bAnyLiving=true; break; }
	if (!bAnyLiving) { Dissolve(true); return; }
	if (Enemy==Front)
	{
		StartReplacement(false);
	}
}

bool ABBSBCombatStage::HasFrontCorridor(ABBSBInkEnemy* Enemy) const
{
	if (!IsValid(Enemy) || Enemy->bDefeated) return false;
	FVector Floor; if (!Ground(GetWorld(),GetActorLocation(),Floor)) return false;
	const UCapsuleComponent* Capsule=Enemy->GetCapsuleComponent();
	// Use the actual lateral radius: inflating it treats a resting wall contact as penetration.
	const float Radius=Capsule->GetScaledCapsuleRadius(), HalfHeight=Capsule->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBFrontCorridor),false,this);
	for (ABBSBInkEnemy* Member:Members) if (Member) Query.AddIgnoredActor(Member);
	if (Player) Query.AddIgnoredActor(Player->GetOwner());
	FHitResult Obstacle;
	return !GetWorld()->SweepSingleByObjectType(Obstacle,Enemy->GetActorLocation()+FVector(0,0,3),Floor+FVector(0,0,HalfHeight+4),FQuat::Identity,GeometryTypes(),FCollisionShape::MakeCapsule(Radius,FMath::Max(Radius,HalfHeight-3)),Query);
}
bool ABBSBCombatStage::StartReplacement(bool bRequireClear,ABBSBInkEnemy* Excluded)
{
	ABBSBInkEnemy* Clear=nullptr; ABBSBInkEnemy* Closest=nullptr;
	float ClearDistance=FLT_MAX, ClosestDistance=FLT_MAX;
	for (ABBSBInkEnemy* Member:Members)
	{
		if (!IsValid(Member) || Member->bDefeated || Member==Excluded) continue;
		const float Distance=FVector::DistSquared2D(Member->GetActorLocation(),GetActorLocation());
		if (Distance<ClosestDistance) { ClosestDistance=Distance; Closest=Member; }
		if (Distance<ClearDistance && HasFrontCorridor(Member)) { ClearDistance=Distance; Clear=Member; }
	}
	ABBSBInkEnemy* Next=Clear ? Clear : (bRequireClear ? nullptr : Closest);
	if (!Next) return false;
	if (IsValid(Front) && Front!=Next) { Front->bFront=false; Front->UpdateLabel(); }
	Front=Next; Front->JoinStage(this,true); ++FrontReplacements; Phase=EBBSBStagePhase::Replacing;
	PendingNotes.Reset(); UpdateFormation(0);
 bCounterAttackOpen=false;ExchangeNumber=0;CounterStartedBeat=-1;
 RefreshExchangeLabels();
	ReplacementBestDistance=FVector::Dist2D(Front->GetActorLocation(),GetActorLocation());
	ReplacementProgressAt=GetWorld()->GetTimeSeconds();
	if (Player) Player->SetFeedback(TEXT("FRONT CHANGE  |  incoming ink keeps flying"));
	return true;
}

void ABBSBCombatStage::ClearStageProjectiles()
{
	if (bCleanedProjectiles) return;
	bCleanedProjectiles=true;
	for (TActorIterator<ABBSBInkProjectile> It(GetWorld()); It; ++It) if (It->OwningStage.Get()==this) It->Destroy();
}
void ABBSBCombatStage::Dissolve(bool bAllDefeated)
{
	if (!IsActive()) return;
	Phase=bAllDefeated ? EBBSBStagePhase::Victory : EBBSBStagePhase::Dissolved;
	PendingNotes.Reset();
 bCounterAttackOpen=false;CounterStartedBeat=-1;
	if (bAllDefeated) ClearStageProjectiles();
	for (ABBSBInkEnemy* Member:Members) if (IsValid(Member)) { Member->Stage=nullptr; Member->bFront=false; Member->UpdateLabel(); }
	for (auto& Part:RingInk) if (Part.IsValid()) Part->SetVisibility(false);
	Metronome->Stop();
	if (Player && Player->Stage==this) Player->LeaveStage(bAllDefeated);
	SetLifeSpan(16.f);
	UE_LOG(LogBBSB,Display,TEXT("Stage end victory=%d shots=%d replacements=%d joins=%d"),bAllDefeated,ShotsFired,FrontReplacements,Joins);
}

void ABBSBCombatStage::Click(int32 Beat)
{
	if (!bAudibleMetronome || Beat<-3 || !IsActive()) return;
	if (!ClickSound)
	{
		ClickSound=NewObject<USoundWaveProcedural>(this); ClickSound->SetSampleRate(24000); ClickSound->NumChannels=1;
		ClickSound->Duration=INDEFINITELY_LOOPING_DURATION; ClickSound->SoundGroup=SOUNDGROUP_UI; Metronome->SetSound(ClickSound);
	}
	if (!ClickSound) return;
	// A quiet practice click; the encounter clock remains independent of frame accumulation.
	TArray<int16> Samples; Samples.SetNumUninitialized(960);
	const float Frequency=Beat%4==0 ? 1100.f : 750.f;
	for (int32 Index=0; Index<Samples.Num(); ++Index) Samples[Index]=int16(1800.f*FMath::Sin(2*PI*Frequency*Index/24000.f)*FMath::Exp(-Index/160.f));
	ClickSound->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Samples.Num()*sizeof(int16));
	if (!Metronome->IsPlaying()) Metronome->Play();
}
void ABBSBCombatStage::SetExperiment(int32 Preset)
{
	ExperimentPreset=FMath::Clamp(Preset,0,4);
	if (ExperimentPreset==1) BPM=95;
	if (ExperimentPreset==2) BPM=100;
	if (ExperimentPreset==3) BPM=90;
	if (ExperimentPreset==4) BPM=90;
}
