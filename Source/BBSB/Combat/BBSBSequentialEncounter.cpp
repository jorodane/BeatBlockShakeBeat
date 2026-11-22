#include "Combat/BBSBSequentialEncounter.h"
#include "World/BBSBPlacement.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "BBSBCharacter.h"
#include "Stage01/BBSBStage01.h"
#include "Character/BBSBRespawn.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Save/BBSBSession.h"
#include "UI/BBSBGuides.h"
#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Serialization/CustomVersion.h"

namespace
{
const FGuid WaveAuthoringVersion(0x4A9A5D37,0x308E4C44,0xA490AF71,0xC4B87D57);
constexpr int32 StructuredWaves=1;
const FCustomVersionRegistration RegisterWaveAuthoring(WaveAuthoringVersion,StructuredWaves,TEXT("BBSBEncounterWaves"));

float NonNegative(float Value)
{
 return FMath::IsFinite(Value) ? FMath::Max(0.f,Value):0.f;
}

bool IsSpawnableEnemyClass(const TSubclassOf<ABBSBInkEnemy>& Class)
{
 return Class && !Class->HasAnyClassFlags(CLASS_Abstract);
}

bool IsSpawnEntry(const FBBSBEncounterSpawnEntry& Entry)
{
 return Entry.Count>0 && Entry.Repeat>0 && IsSpawnableEnemyClass(Entry.MonsterClass);
}
}

ABBSBEncounterSamplePoint::ABBSBEncounterSamplePoint()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Sample")));
 SetActorEnableCollision(false);
#if WITH_EDITORONLY_DATA
 bIsEditorOnlyActor=true;
#endif
}

ABBSBSequentialEncounter::ABBSBSequentialEncounter()
{
 PrimaryActorTick.bCanEverTick=true;
 PrimaryActorTick.TickInterval=.1f;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("EncounterOrigin")));
 Path=CreateDefaultSubobject<USplineComponent>(TEXT("SpawnPath"));
 Path->SetupAttachment(RootComponent);
 Path->ClearSplinePoints(false);
 for(const FVector Point:{FVector(600,0,0),FVector(0,600,0),FVector(-600,0,0),FVector(0,-600,0)})
  Path->AddSplinePoint(Point,ESplineCoordinateSpace::Local,false);
 Path->SetClosedLoop(true,true);
 EnemyClasses={ABBSBInkEnemy::StaticClass()};
 Waves.AddDefaulted_GetRef().Entries.AddDefaulted_GetRef().MonsterClass=ABBSBInkEnemy::StaticClass();
 Tags.Add(TEXT("BBSBSequentialEncounter"));
}

void ABBSBSequentialEncounter::Serialize(FArchive& Ar)
{
 if(Ar.IsPersistent())
 {
  Ar.UsingCustomVersion(WaveAuthoringVersion);
  if(Ar.IsLoading())bReadLegacyWaves=Ar.CustomVer(WaveAuthoringVersion)<StructuredWaves;
 }
 Super::Serialize(Ar);
}

void ABBSBSequentialEncounter::PostLoad()
{
 Super::PostLoad();
 if(!bReadLegacyWaves)return;
 bReadLegacyWaves=false;
 FBBSBEncounterWave LegacyWave;
 LegacyWave.Delay=SpawnDelay;
 LegacyWave.AdvanceDistance=AdvanceDistance;
 for(const auto& Class:EnemyClasses)
  if(IsSpawnableEnemyClass(Class))LegacyWave.Entries.AddDefaulted_GetRef().MonsterClass=Class;
 Waves.Reset();
 if(!LegacyWave.Entries.IsEmpty())Waves.Add(MoveTemp(LegacyWave));
}

void ABBSBSequentialEncounter::BeginPlay()
{
 Super::BeginPlay();
 SetInitialDirection();
 WaveRandom.GenerateNewSeed();
}

void ABBSBSequentialEncounter::SetInitialDirection()
{
 const double Length=Path->GetSplineLength();
 CursorDistance=FMath::Clamp(double(StartDistance),0.,Length);
 TravelDirection=bReverseOpenPath ? -1:1;
 if(!Path->IsClosedLoop() || Length<1.)return;

 // Determine winding from the actual world-space curve, including its tangents.
 constexpr int32 Segments=32;
 const FVector Origin=Path->GetLocationAtDistanceAlongSpline(0,ESplineCoordinateSpace::World);
 FVector Previous=FVector::ZeroVector;
 double Area=0.;
 for(int32 Index=1;Index<Segments;++Index)
 {
  const FVector Current=Path->GetLocationAtDistanceAlongSpline(Length*Index/Segments,ESplineCoordinateSpace::World)-Origin;
  Area+=Previous.X*Current.Y-Previous.Y*Current.X;
  Previous=Current;
 }
 TravelDirection=(Area>=0. ? -1:1)*(bClockwise ? 1:-1);
}

bool ABBSBSequentialEncounter::CanSpawnNow() const
{
 const auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if(!bEnabled || bCompleted || Waves.IsEmpty() || Path->GetNumberOfSplinePoints()<2 || Path->GetSplineLength()<1 || !Player || Player->Combat->bDead ||
  Player->Respawn->IsSequenceActive() || Player->Interaction->bDialogueActive || Player->EquipmentGrowth->bConcertoActive)return false;
 const auto* Session=UBBSBSessionSubsystem::Get(this);
 if(Session && (!Session->bGameStarted || Session->bLoading || Session->bMenuOpen))return false;
 if(const auto* Guides=UBBSBGuideSubsystem::Get(this);Guides && (Guides->bOpeningActive || Guides->bGuideOpen))return false;
 if(Story && ((bRequireFirstWeapon && !Story->HasTool()) || Story->Phase==EBBSBStage01Phase::ShopBattle ||
  Story->Phase==EBBSBStage01Phase::Trio || Story->Phase==EBBSBStage01Phase::Captain))return false;
 const FVector Near=Path->FindLocationClosestToWorldLocation(Player->GetActorLocation(),ESplineCoordinateSpace::World);
 return FVector::Dist2D(Near,Player->GetActorLocation())<=FMath::Max(100.f,ActivationRadius) &&
  FMath::Abs(Near.Z-Player->GetActorLocation().Z)<650.f;
}

double ABBSBSequentialEncounter::AdvanceAlongPath(double From,double Step,int32& Direction) const
{
 const double Length=Path->GetSplineLength();
 if(Length<1.)return 0.;
 From=FMath::Clamp(From,0.,Length);
 if(Path->IsClosedLoop())
 {
  const double Distance=FMath::Fmod(From+Direction*Step,Length);
  return Distance<0. ? Distance+Length:Distance;
 }

 const double Period=2.*Length;
 const double Phase=FMath::Fmod((Direction>0 ? From:Period-From)+Step,Period);
 Direction=Phase<Length ? 1:-1;
 return Direction>0 ? Phase:Period-Phase;
}

bool ABBSBSequentialEncounter::ValidateSpawn(FVector Point,const ABBSBInkEnemy& Default,FVector& Result) const
{
 const auto* Player=UGameplayStatics::GetPlayerCharacter(this,0);
 if(!Player)return false;
 const float Radius=Default.GetCapsuleComponent()->GetScaledCapsuleRadius();
 const float Half=Default.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBSequentialSpawn),false,this);
 Query.AddIgnoredActor(Player);
 const auto Geometry=BBSBCombatVisuals::GeometryTypes();
 FHitResult Ground;
 if(!GetWorld()->LineTraceSingleByObjectType(Ground,Point+FVector(0,0,GroundProbeUp),Point-FVector(0,0,GroundProbeDown),Geometry,Query) ||
  Ground.ImpactNormal.Z<.75f)return false;
 Result=Ground.ImpactPoint+FVector(0,0,Half+3.f);
 const float PlayerDistance=FVector::Dist(Result,Player->GetActorLocation());
 if(PlayerDistance<FMath::Max3(100.f,MinimumPlayerDistance,Radius*2) ||
  PlayerDistance>FMath::Max(MinimumPlayerDistance+100.f,MaximumPlayerDistance))return false;
 if(GetWorld()->OverlapBlockingTestByChannel(Result,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius+5,Half),Query))return false;

 // The capsule must have support around its base as well as under its centre.
 const float FootOffset=Radius*.7f;
 for(const FVector Offset:{FVector(FootOffset,0,0),FVector(-FootOffset,0,0),FVector(0,FootOffset,0),FVector(0,-FootOffset,0)})
 {
  FHitResult Foot;
  const FVector Base=Ground.ImpactPoint+Offset;
  if(!GetWorld()->LineTraceSingleByObjectType(Foot,Base+FVector(0,0,20),Base-FVector(0,0,45),Geometry,Query) ||
   Foot.ImpactNormal.Z<.7f || FMath::Abs(Foot.ImpactPoint.Z-Ground.ImpactPoint.Z)>25)return false;
 }
 for(const ABBSBInkEnemy* Enemy:ActiveEnemies)
  if(IsValid(Enemy) && !Enemy->bDefeated && FVector::DistSquared(Result,Enemy->GetActorLocation())<FMath::Square(MinimumEnemySpacing))return false;
 return true;
}

const FBBSBEncounterSpawnEntry* ABBSBSequentialEncounter::CurrentEntry() const
{
 if(!Waves.IsValidIndex(WaveIndex))return nullptr;
 const auto& Entries=Waves[WaveIndex].Entries;
 return Entries.IsValidIndex(EntryIndex) ? &Entries[EntryIndex]:nullptr;
}

void ABBSBSequentialEncounter::StartWave(int32 Index,double Now,bool bMoveAnchor)
{
 WaveIndex=Index;
 EntryIndex=RepeatIndex=0;
 const auto& Wave=Waves[Index];
 const double Step=NonNegative(Wave.AdvanceDistance);
 if(bMoveAnchor && Step>0.)CursorDistance=AdvanceAlongPath(CursorDistance,Step,TravelDirection);
 ScheduleEntry(Now+NonNegative(Wave.Delay));
 Status=TEXT("웨이브 준비 중");
}

void ABBSBSequentialEncounter::ScheduleEntry(double BaseTime)
{
 NextActionAt=BaseTime;
 while(const auto* Entry=CurrentEntry())
 {
  if(IsSpawnEntry(*Entry) && RepeatIndex<Entry->Repeat)
  {
   NextActionAt+=NonNegative(Entry->Delay);
   return;
  }
  ++EntryIndex;
  RepeatIndex=0;
 }
}

void ABBSBSequentialEncounter::FinishBurst(double Now)
{
 BatchClass=nullptr;
 ++RepeatIndex;
 ScheduleEntry(Now);
}

bool ABBSBSequentialEncounter::AdvanceWaveIfCleared(double Now)
{
 if(CurrentEntry() || PendingCount>0 || Now<NextActionAt || !ActiveEnemies.IsEmpty() || !RestoreQueue.IsEmpty())return false;
 const int32 NextIndex=WaveIndex+1;
 if(Waves.IsValidIndex(NextIndex))StartWave(NextIndex,Now,true);
 else if(bRepeatWave && !Waves.IsEmpty())StartWave(0,Now,true);
 else
 {
  bCompleted=true;
  Status=TEXT("웨이브 완료");
 }
 return true;
}

void ABBSBSequentialEncounter::FindSpawnPositions(const ABBSBInkEnemy& Default,int32 Limit,TArray<FBBSBPendingEncounterSpawn>& Plans) const
{
 const double Length=Path->GetSplineLength();
 if(Length<1.)return;
 const bool bClosed=Path->IsClosedLoop();
 const double Goal=bClosed ? FMath::Fmod(CursorDistance,Length):FMath::Clamp(CursorDistance,0.,Length);
 const double ProbeStep=FMath::Min(75.,Length/12.);
 const float ReservedSpacing=FMath::Max(MinimumEnemySpacing,Default.GetCapsuleComponent()->GetScaledCapsuleRadius()*2.f+5.f);
 constexpr int32 MaxProbes=48;
 int32 Direction=TravelDirection;

 // A batch reserves distinct points from the wave anchor before drawing warnings.
 for(int32 Pass=0;Pass<(bClosed ? 1:2);++Pass)
 {
  for(int32 Index=Pass;Index<MaxProbes && Plans.Num()<Limit;++Index)
  {
   const double Offset=Index*ProbeStep;
   if(bClosed && Offset>=Length)break;
   const double Distance=bClosed ? FMath::Fmod(Goal+Direction*Offset+Length,Length):FMath::Clamp(Goal+Direction*Offset,0.,Length);
   FVector Position;
   if(ValidateSpawn(Path->GetLocationAtDistanceAlongSpline(Distance,ESplineCoordinateSpace::World),Default,Position) &&
    !Plans.ContainsByPredicate([&](const auto& Plan){return FVector::DistSquared(Position,Plan.Position)<FMath::Square(ReservedSpacing);}))
   {
    auto& Plan=Plans.AddDefaulted_GetRef();
    Plan.Distance=Distance;
    Plan.Position=Position;
   }
   if(!bClosed && (Direction>0 ? Distance>=Length:Distance<=0.))break;
  }
  Direction*=-1;
 }
}

void ABBSBSequentialEncounter::ClearSpawnWarnings()
{
 for(auto& Spawn:Pending)if(Spawn.Warning)Spawn.Warning->DestroyComponent();
 Pending.Reset();
}

ABBSBInkEnemy* ABBSBSequentialEncounter::CreateEnemy(UClass* RequestedClass,FVector Position,float Warmup,int32 SavedVoice,int32 SavedVoiceIndex)
{
 UClass* Class=UBBSBPlacementAuthoringTools::ResolvePlacementClass(RequestedClass);
 if(!Class || !Class->IsChildOf(ABBSBInkEnemy::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))return nullptr;
 const FTransform Pose(Position);
 auto* Enemy=GetWorld()->SpawnActorDeferred<ABBSBInkEnemy>(Class,Pose,this,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
 if(!Enemy)return nullptr;
 Enemy->SequentialEncounter=this;
 Enemy->bFieldBulletHell=true;
 Enemy->bFieldIdleWander=false;
 if(auto* Singer=Cast<ABBSBStage01Singer>(Enemy))
 {
  // The native singer option previously meant the standard encounter guard.
  if(StaticEnum<EBBSBStage01Voice>()->IsValidEnumValue(SavedVoice))Singer->Voice=EBBSBStage01Voice(SavedVoice);
  else if(RequestedClass==ABBSBStage01Singer::StaticClass())Singer->Voice=EBBSBStage01Voice::Guard;
  Singer->Director=Story;
  Singer->SingerId=NAME_None;
  Singer->bParticipatesInStory=false;
  Singer->VoiceIndex=SavedVoiceIndex>=0 ? SavedVoiceIndex:SpawnSerial;
 }
 Enemy->BeginSpawnWarmup(Warmup);
 Enemy->FinishSpawning(Pose);
 ActiveEnemies.Add(Enemy);
 return Enemy;
}

void ABBSBSequentialEncounter::TickBatch(double Now,float WarningTime)
{
 UClass* Class=UBBSBPlacementAuthoringTools::ResolvePlacementClass(BatchClass.Get());
 const auto* Default=Class ? Class->GetDefaultObject<ABBSBInkEnemy>():nullptr;
 if(!Default)
 {
  ClearSpawnWarnings();
  PendingCount=0;
  FinishBurst(Now);
  return;
 }

 if(Pending.IsEmpty())
 {
  if(Now+WarningTime<NextActionAt)return;
  FindSpawnPositions(*Default,PendingCount,Pending);
  if(Pending.IsEmpty())
  {
   NextActionAt=Now+WarningTime+.2f;
   Status=TEXT("안전한 등장 지점을 기다리는 중");
   return;
  }
  NextActionAt=FMath::Max(NextActionAt,Now+WarningTime);
  if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BBSB/Interaction/Materials/M_BunniarAimRing.M_BunniarAimRing")))
  {
   const float Half=Default->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
   for(auto& Spawn:Pending)
   {
    auto* Warning=NewObject<UDecalComponent>(this);
    Warning->SetupAttachment(RootComponent);
    auto* Tint=UMaterialInstanceDynamic::Create(Material,Warning);
    Tint->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.55f,.75f,.95f));
    Warning->SetDecalMaterial(Tint);
    Warning->DecalSize=FVector(6,82,82);
    Warning->SetFadeScreenSize(.0001f);
    Warning->RegisterComponent();
    Warning->SetWorldLocationAndRotation(Spawn.Position-FVector(0,0,Half),FVector::UpVector.Rotation());
    Spawn.Warning=Warning;
   }
  }
  return;
 }
 if(Now<NextActionAt)return;

 // A moved prop or player can invalidate a warned point before the batch appears.
 for(const auto& Spawn:Pending)
 {
  FVector Position;
  const FVector Point=Path->GetLocationAtDistanceAlongSpline(Spawn.Distance,ESplineCoordinateSpace::World);
  if(!ValidateSpawn(Point,*Default,Position) || !Position.Equals(Spawn.Position,8.f) ||
   !CreateEnemy(BatchClass.Get(),Position,FMath::Max(.25f,AttackGraceSeconds)))continue;
  if(SpawnSerial<MAX_int32)++SpawnSerial;
  --PendingCount;
  Status=TEXT("일반 전투 진행 중");
 }
 ClearSpawnWarnings();
 if(PendingCount>0)NextActionAt=Now+WarningTime+.2f;
 else FinishBurst(Now);
}

void ABBSBSequentialEncounter::Tick(float DT)
{
 Super::Tick(DT);
 const double Now=GetWorld()->GetTimeSeconds();
 for(int32 Index=ActiveEnemies.Num()-1;Index>=0;--Index)
 {
  ABBSBInkEnemy* Enemy=ActiveEnemies[Index];
  if(IsValid(Enemy) && !Enemy->bDefeated)continue;
  if(IsValid(Enemy) && Enemy->bDefeated)
  {
   if(Defeats<MAX_int32)++Defeats;
   Enemy->SetLifeSpan(8);
  }
  ActiveEnemies.RemoveAt(Index);
 }

 if(bCompleted)return;
 if(bStarted && AdvanceWaveIfCleared(Now))return;
 if(!CanSpawnNow())
 {
  ClearSpawnWarnings();
  return;
 }
 if(!bStarted)
 {
  bStarted=true;
  StartWave(0,Now,false);
 }
 if(!RestoreQueue.IsEmpty() && Now>=NextRestoreAt)
 {
  NextRestoreAt=Now+.25;
  for(int32 Index=RestoreQueue.Num()-1;Index>=0;--Index)
   if(TryRestoreEnemy(RestoreQueue[Index],true))RestoreQueue.RemoveAt(Index);
 }

 const float WarningTime=FMath::Max(.3f,WarningSeconds);
 if(PendingCount>0)
 {
  TickBatch(Now,WarningTime);
  return;
 }
 const auto* Entry=CurrentEntry();
 if(!Entry || Now+WarningTime<NextActionAt)return;
 if(!IsSpawnEntry(*Entry) || RepeatIndex>=Entry->Repeat)
 {
  ScheduleEntry(Now);
  return;
 }
 const float Chance=FMath::Clamp(NonNegative(Entry->Probability),0.f,1.f);
 if(Chance==0.f || (Chance<1.f && WaveRandom.FRand()>=Chance))
 {
  FinishBurst(FMath::Max(Now,NextActionAt));
  return;
 }
 BatchClass=Entry->MonsterClass;
 PendingCount=Entry->Count;
 TickBatch(Now,WarningTime);
}

void ABBSBSequentialEncounter::ClearRuntime()
{
 ClearSpawnWarnings();
 BatchClass=nullptr;
 PendingCount=0;
 RestoreQueue.Reset();
 NextRestoreAt=0.;
 for(ABBSBInkEnemy* Enemy:ActiveEnemies)if(IsValid(Enemy))Enemy->Destroy();
 ActiveEnemies.Reset();
}

void ABBSBSequentialEncounter::ResetEncounter()
{
 ClearRuntime();
 bStarted=bCompleted=false;
 WaveIndex=INDEX_NONE;
 EntryIndex=RepeatIndex=Defeats=SpawnSerial=0;
 NextActionAt=0.;
 Status.Reset();
 WaveRandom.GenerateNewSeed();
 SetInitialDirection();
}

void ABBSBSequentialEncounter::EndPlay(EEndPlayReason::Type Reason)
{
 ClearRuntime();
 Super::EndPlay(Reason);
}

FBBSBEncounterSave ABBSBSequentialEncounter::CaptureState() const
{
 FBBSBEncounterSave Saved;
 Saved.Path=GetPathName(GetWorld());
 Saved.Cursor=CursorDistance;
 Saved.Direction=TravelDirection;
 Saved.Defeats=Defeats;
 Saved.Serial=SpawnSerial;
 Saved.bStarted=bStarted;
 Saved.WaveFormat=StructuredWaves;
 Saved.WaveIndex=WaveIndex;
 Saved.EntryIndex=EntryIndex;
 Saved.RepeatIndex=RepeatIndex;
 Saved.BatchRemaining=PendingCount;
 Saved.BatchClass=BatchClass.Get();
 Saved.RandomSeed=WaveRandom.GetCurrentSeed();
 Saved.bCompleted=bCompleted;
 const double Now=GetWorld()->GetTimeSeconds();
 Saved.DelayLeft=FMath::GridSnap(float(FMath::Max(0.,NextActionAt-Now)),.1f);
 Saved.Living=RestoreQueue;
 for(const ABBSBInkEnemy* Enemy:ActiveEnemies)
 {
  if(!IsValid(Enemy))continue;
  if(Enemy->bDefeated)
  {
   if(Saved.Defeats<MAX_int32)++Saved.Defeats;
   continue;
  }
  auto& Record=Saved.Living.AddDefaulted_GetRef();
  Record.Class=Enemy->GetClass();
  Record.Transform=Enemy->GetActorTransform();
  Record.Transform.SetLocation(Record.Transform.GetLocation().GridSnap(.1));
  Record.Health=FMath::GridSnap(Enemy->Health,.1f);
  Record.WarmupLeft=FMath::GridSnap(float(FMath::Max(0.,Enemy->SpawnWarmupUntil-Now)),.1f);
  if(const auto* Singer=Cast<ABBSBStage01Singer>(Enemy))
  {
   Record.SingerVoice=int32(Singer->Voice);
   Record.SingerVoiceIndex=Singer->VoiceIndex;
  }
  Record.EcstaticBudget=Enemy->EcstaticRewardBudget;
  Record.NoteBudget=Enemy->NoteRewardBudget;
  Record.EcstaticDropped=Enemy->EcstaticRewardsDropped;
  Record.NotesDropped=Enemy->NoteRewardsDropped;
  Record.DropSerial=Enemy->RewardDropSerial;
  Record.LowestHealthFraction=Enemy->RewardLowestHealthFraction;
 }
 return Saved;
}

bool ABBSBSequentialEncounter::TryRestoreEnemy(const FBBSBEncounterEnemySave& Saved,bool bAllowNearby)
{
 if(!FMath::IsFinite(Saved.Health) || Saved.Health<=0 || Saved.Transform.ContainsNaN())return true;
 UClass* Class=Saved.Class.LoadSynchronous();
 if(!Class || Class->HasAnyClassFlags(CLASS_Abstract))return true;
 int32 Voice=Saved.SingerVoice;
 if(Voice==INDEX_NONE && Class==UBBSBPlacementAuthoringTools::ResolvePlacementClass(ABBSBStage01Singer::StaticClass()))
  Voice=int32(EBBSBStage01Voice::Guard);
 const float Warmup=FMath::Max(NonNegative(AttackGraceSeconds),NonNegative(Saved.WarmupLeft));
 auto* Enemy=CreateEnemy(Class,Saved.Transform.GetLocation(),Warmup,Voice,Saved.SingerVoiceIndex);
 if(!Enemy && bAllowNearby)
 {
  UClass* Resolved=UBBSBPlacementAuthoringTools::ResolvePlacementClass(Class);
  if(const auto* Default=Resolved ? Resolved->GetDefaultObject<ABBSBInkEnemy>():nullptr)
  {
   TArray<FBBSBPendingEncounterSpawn> Plans;
   FindSpawnPositions(*Default,1,Plans);
   if(!Plans.IsEmpty())Enemy=CreateEnemy(Class,Plans[0].Position,Warmup,Voice,Saved.SingerVoiceIndex);
  }
 }
 if(!Enemy)return false;
 Enemy->SetActorRotation(Saved.Transform.Rotator());
 Enemy->Health=FMath::Clamp(Saved.Health,.1f,Enemy->MaximumHealth);
 Enemy->ResetHealthFeedback();
 Enemy->EcstaticRewardBudget=Saved.EcstaticBudget;
 Enemy->NoteRewardBudget=Saved.NoteBudget;
 Enemy->EcstaticRewardsDropped=FMath::Max(0,Saved.EcstaticDropped);
 Enemy->NoteRewardsDropped=FMath::Max(0,Saved.NotesDropped);
 Enemy->RewardDropSerial=FMath::Max(0,Saved.DropSerial);
 Enemy->RewardLowestHealthFraction=FMath::Clamp(Saved.LowestHealthFraction,0.f,1.f);
 return true;
}

void ABBSBSequentialEncounter::RestoreLegacySchedule(const FBBSBEncounterSave& Saved,double Now)
{
 int64 Consumed=SpawnSerial;
 for(int32 Wave=0;Wave<Waves.Num();++Wave)
 {
  const auto& Entries=Waves[Wave].Entries;
  for(int32 Index=0;Index<Entries.Num();++Index)
  {
   const auto& Entry=Entries[Index];
   if(!IsSpawnEntry(Entry))continue;
   const int64 Size=int64(Entry.Count)*Entry.Repeat;
   if(Consumed>=Size)
   {
    Consumed-=Size;
    continue;
   }
   WaveIndex=Wave;
   EntryIndex=Index;
   RepeatIndex=int32(Consumed/Entry.Count);
   const int32 Partial=int32(Consumed%Entry.Count);
   if(Partial>0)
   {
    PendingCount=Entry.Count-Partial;
    BatchClass=Entry.MonsterClass;
   }
   const float InitialDelay=SpawnSerial==0 ? NonNegative(Waves[Wave].Delay):0.f;
   NextActionAt=Now+(Saved.PendingDelays.IsEmpty() ? InitialDelay+NonNegative(Entry.Delay):NonNegative(Saved.PendingDelays[0]));
   return;
  }
 }
 WaveIndex=Waves.Num()-1;
 EntryIndex=Waves.IsEmpty() ? 0:Waves.Last().Entries.Num();
 NextActionAt=Now;
}

void ABBSBSequentialEncounter::RestoreState(const FBBSBEncounterSave& Saved)
{
 ResetEncounter();
 if(!Saved.bStarted)return;
 bStarted=true;
 if(FMath::IsFinite(Saved.Cursor))CursorDistance=FMath::Clamp(Saved.Cursor,0.,double(Path->GetSplineLength()));
 TravelDirection=Saved.Direction<0 ? -1:1;
 Defeats=FMath::Max(0,Saved.Defeats);
 SpawnSerial=FMath::Max(0,Saved.Serial);

 const double Now=GetWorld()->GetTimeSeconds();
 if(Saved.WaveFormat<StructuredWaves)RestoreLegacySchedule(Saved,Now);
 else
 {
  WaveRandom.Initialize(Saved.RandomSeed);
  bCompleted=Saved.bCompleted && Saved.Living.IsEmpty();
  WaveIndex=Waves.IsEmpty() ? INDEX_NONE:FMath::Clamp(Saved.WaveIndex,0,Waves.Num()-1);
  EntryIndex=Waves.IsValidIndex(WaveIndex) ? FMath::Clamp(Saved.EntryIndex,0,Waves[WaveIndex].Entries.Num()):0;
  RepeatIndex=FMath::Max(0,Saved.RepeatIndex);
  NextActionAt=Now+NonNegative(Saved.DelayLeft);
  if(const auto* Entry=CurrentEntry())
  {
   if(!IsSpawnEntry(*Entry) || RepeatIndex>=Entry->Repeat)ScheduleEntry(Now);
   else if(Saved.BatchRemaining>0)
   {
    BatchClass=Saved.BatchClass.LoadSynchronous();
    if(!IsSpawnableEnemyClass(BatchClass))BatchClass=Entry->MonsterClass;
    PendingCount=FMath::Min(Saved.BatchRemaining,Entry->Count);
   }
  }
 }
 for(const auto& Record:Saved.Living)
  if(!TryRestoreEnemy(Record,false))RestoreQueue.Add(Record);
 Status=bCompleted ? TEXT("웨이브 완료"):ActiveEnemies.IsEmpty() ? TEXT("웨이브 준비 중"):TEXT("일반 전투 진행 중");
}
