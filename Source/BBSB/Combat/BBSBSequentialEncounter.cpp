#include "Combat/BBSBSequentialEncounter.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "BBSBCharacter.h"
#include "Stage01/BBSBStage01.h"
#include "Character/BBSBRespawn.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Save/BBSBSession.h"
#include "UI/BBSBGuides.h"
#include "Components/SplineComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

ABBSBEncounterSamplePoint::ABBSBEncounterSamplePoint()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Sample")));SetActorEnableCollision(false);
#if WITH_EDITORONLY_DATA
 bIsEditorOnlyActor=true;
#endif
}
ABBSBSequentialEncounter::ABBSBSequentialEncounter()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("EncounterOrigin")));
 Path=CreateDefaultSubobject<USplineComponent>(TEXT("SpawnPath"));Path->SetupAttachment(RootComponent);
 Path->ClearSplinePoints(false);for(const FVector P:{FVector(600,0,0),FVector(0,600,0),FVector(-600,0,0),FVector(0,-600,0)})Path->AddSplinePoint(P,ESplineCoordinateSpace::Local,false);Path->SetClosedLoop(true,true);
 GenerationBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("SamplingBounds"));GenerationBounds->SetupAttachment(RootComponent);GenerationBounds->SetBoxExtent(FVector(8000,8000,3000));GenerationBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);GenerationBounds->SetCanEverAffectNavigation(false);
 SampleGenerator=CreateDefaultSubobject<UPCGComponent>(TEXT("SpawnSamplesPCG"));SampleGenerator->GenerationTrigger=EPCGComponentGenerationTrigger::GenerateOnDemand;
 SamplingGraph=TSoftObjectPtr<UPCGGraph>(FSoftObjectPath(TEXT("/Game/BBSB/Encounters/PCG/PCG_SequentialEncounterSamples.PCG_SequentialEncounterSamples")));
 EnemyClasses={ABBSBInkEnemy::StaticClass()};Tags.Add(TEXT("BBSBSequentialEncounter"));
}
void ABBSBSequentialEncounter::RebuildSpawnSamples()
{
 if(auto* Graph=SamplingGraph.LoadSynchronous())
 {
  SampleGenerator->OnPCGGraphGeneratedDelegate.RemoveAll(this);SampleGenerator->OnPCGGraphGeneratedDelegate.AddUObject(this,&ABBSBSequentialEncounter::BakeSamples);
  SampleGenerator->SetGraph(Graph);SampleGenerator->GenerateLocal(true);Status=TEXT("스폰 경로 생성 중");
 }
 else Status=TEXT("스폰 경로 그래프를 지정하세요");
}
void ABBSBSequentialEncounter::BakeSamples(UPCGComponent* Component)
{
 TArray<AActor*> SampleActors;GetAttachedActors(SampleActors,true,true);TArray<FBBSBEncounterSpawnSample> Samples;
 for(AActor* A:SampleActors)if(auto* Point=Cast<ABBSBEncounterSamplePoint>(A))
 { FBBSBEncounterSpawnSample S;S.Position=Point->GetActorLocation();S.Distance=Point->DistanceAlongPath;Samples.Add(S); }
 Samples.Sort([](const auto& A,const auto& B){return A.Distance<B.Distance;});SpawnSamples.Reset();
 for(const auto& S:Samples)if(SpawnSamples.IsEmpty() || !FMath::IsNearlyEqual(SpawnSamples.Last().Distance,S.Distance,.01))SpawnSamples.Add(S);
 Status=FString::Printf(TEXT("스폰 후보 %d개"),SpawnSamples.Num());
#if WITH_EDITOR
 if(!GetWorld()->IsGameWorld())MarkPackageDirty();
#endif
}
void ABBSBSequentialEncounter::BeginPlay() { Super::BeginPlay();SetInitialDirection(); }
void ABBSBSequentialEncounter::SetInitialDirection()
{
 TravelDirection=bReverseOpenPath ? -1:1;CursorDistance=FMath::Clamp(double(StartDistance),0.,double(Path->GetSplineLength()));
 if(Path->IsClosedLoop() && SpawnSamples.Num()>2)
 {
  double Area=0;for(int32 I=0;I<SpawnSamples.Num();++I) { const FVector A=SpawnSamples[I].Position,B=SpawnSamples[(I+1)%SpawnSamples.Num()].Position;Area+=A.X*B.Y-B.X*A.Y; }
  TravelDirection=(Area>=0 ? -1:1)*(bClockwise ? 1:-1);
 }
}
bool ABBSBSequentialEncounter::CanSpawnNow() const
{
 const auto* P=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if(!bEnabled || SpawnSamples.Num()<2 || !P || P->Combat->bDead || P->Respawn->IsSequenceActive() || P->Interaction->bDialogueActive || P->EquipmentGrowth->bConcertoActive)return false;
 const auto* Session=UBBSBSessionSubsystem::Get(this);if(Session && (!Session->bGameStarted || Session->bLoading || Session->bMenuOpen))return false;
 if(const auto* Guides=UBBSBGuideSubsystem::Get(this);Guides && (Guides->bOpeningActive || Guides->bGuideOpen))return false;
 if(Story && ((bRequireFirstWeapon && !Story->HasTool()) || Story->Phase==EBBSBStage01Phase::ShopBattle || Story->Phase==EBBSBStage01Phase::Trio || Story->Phase==EBBSBStage01Phase::Captain))return false;
 const FVector Near=Path->FindLocationClosestToWorldLocation(P->GetActorLocation(),ESplineCoordinateSpace::World);
 return FVector::Dist2D(Near,P->GetActorLocation())<=FMath::Max(100.f,ActivationRadius) && FMath::Abs(Near.Z-P->GetActorLocation().Z)<650;
}
double ABBSBSequentialEncounter::NextDistance(double Step,int32& Direction) const
{
 const double Length=Path->GetSplineLength();if(Length<1)return 0;
 double At=CursorDistance+Direction*FMath::Fmod(FMath::Max(0.,Step),Length*2);
 if(Path->IsClosedLoop()) { At=FMath::Fmod(At,Length);return At<0 ? At+Length:At; }
 while(At<0 || At>Length) { if(At<0) { At=-At;Direction=1; }if(At>Length) { At=2*Length-At;Direction=-1; } }
 return At;
}
bool ABBSBSequentialEncounter::ValidateSpawn(FVector Point,FVector& Result,const ABBSBInkEnemy* Ignore) const
{
 const auto* Player=UGameplayStatics::GetPlayerCharacter(this,0);if(!Player)return false;
 UClass* Class=EnemyClasses.IsEmpty() ? nullptr:EnemyClasses[SpawnSerial%EnemyClasses.Num()].Get();
 const auto* Default=Class ? Cast<ABBSBInkEnemy>(Class->GetDefaultObject()):nullptr;if(!Default)return false;
 const float Radius=Default->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half=Default->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBSequentialSpawn),false,this);Query.AddIgnoredActor(Player);Query.AddIgnoredActor(Ignore);
 FHitResult Ground;
 if(!GetWorld()->LineTraceSingleByObjectType(Ground,Point+FVector(0,0,GroundProbeUp),Point-FVector(0,0,GroundProbeDown),BBSBCombatVisuals::GeometryTypes(),Query) || Ground.ImpactNormal.Z<.75f)return false;
 Result=Ground.ImpactPoint+FVector(0,0,Half+3.f);
 const float PlayerDistance=FVector::Dist(Result,Player->GetActorLocation());if(PlayerDistance<FMath::Max(100.f,MinimumPlayerDistance) || PlayerDistance<FMath::Max(100.f,Radius*2) || PlayerDistance>FMath::Max(MinimumPlayerDistance+100.f,MaximumPlayerDistance))return false;
 if(GetWorld()->OverlapBlockingTestByChannel(Result,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius+5,Half),Query))return false;
 // A supporting centre alone is not enough at a platform edge.
 for(const FVector Offset:{FVector(Radius*.7f,0,0),FVector(-Radius*.7f,0,0),FVector(0,Radius*.7f,0),FVector(0,-Radius*.7f,0)})
 {
  FHitResult Foot;const FVector P=Ground.ImpactPoint+Offset;
  if(!GetWorld()->LineTraceSingleByObjectType(Foot,P+FVector(0,0,20),P-FVector(0,0,45),BBSBCombatVisuals::GeometryTypes(),Query) || Foot.ImpactNormal.Z<.7f || FMath::Abs(Foot.ImpactPoint.Z-Ground.ImpactPoint.Z)>25)return false;
 }
 for(const ABBSBInkEnemy* E:ActiveEnemies)if(IsValid(E) && E!=Ignore && !E->bDefeated && FVector::DistSquared(Result,E->GetActorLocation())<FMath::Square(MinimumEnemySpacing))return false;
 for(const auto& P:Pending)if(P.bPrepared && FVector::DistSquared(Result,P.Position)<FMath::Square(MinimumEnemySpacing))return false;
 return true;
}
bool ABBSBSequentialEncounter::FindSpawn(FVector& Position,double& Distance,int32& Direction) const
{
 const bool First=SpawnSerial==0 && !Pending.ContainsByPredicate([](const auto& P){return P.bPrepared;});
 Direction=TravelDirection;const double Goal=NextDistance(First ? 0:FMath::Max(50.f,AdvanceDistance),Direction),Length=Path->GetSplineLength();
 for(int32 Pass=0;Pass<(Path->IsClosedLoop() ? 1:2);++Pass)
 {
  TArray<int32> Order;for(int32 I=0;I<SpawnSamples.Num();++I)Order.Add(I);
  auto Score=[&](int32 Index)
  {
   double D=(SpawnSamples[Index].Distance-Goal)*Direction;
   if(Path->IsClosedLoop())
   {
    D=FMath::Fmod(D+Length,Length);
    const double Moved=FMath::Fmod((SpawnSamples[Index].Distance-CursorDistance)*Direction+Length,Length);
    if(!First && Moved<FMath::Min(double(AdvanceDistance)*.75,Length*.25))return DBL_MAX;
   }
   return D<-.1 ? DBL_MAX:D;
  };
  Order.Sort([&](int32 A,int32 B){return Score(A)<Score(B);});
  int32 Probes=0;for(int32 I:Order)
  { if(Score(I)==DBL_MAX || ++Probes>48)break;if(ValidateSpawn(SpawnSamples[I].Position,Position)) { Distance=SpawnSamples[I].Distance;return true; } }
  Direction*=-1;
 }
 return false;
}
void ABBSBSequentialEncounter::QueueSpawn(float Delay)
{ auto& P=Pending.AddDefaulted_GetRef();P.ReadyAt=GetWorld()->GetTimeSeconds()+FMath::Max(FMath::Max(.3f,WarningSeconds),Delay);PendingCount=Pending.Num(); }
ABBSBInkEnemy* ABBSBSequentialEncounter::CreateEnemy(UClass* Class,FVector Position,float Warmup)
{
 if(!Class || !Class->IsChildOf(ABBSBInkEnemy::StaticClass()))return nullptr;
 const FTransform Pose(Position);auto* Enemy=GetWorld()->SpawnActorDeferred<ABBSBInkEnemy>(Class,Pose,this,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);if(!Enemy)return nullptr;
 Enemy->SequentialEncounter=this;Enemy->bFieldBulletHell=true;Enemy->bFieldIdleWander=false;
 if(auto* Singer=Cast<ABBSBStage01Singer>(Enemy)) { Singer->Director=Story;Singer->Voice=EBBSBStage01Voice::Guard;Singer->SingerId=NAME_None;Singer->bParticipatesInStory=false;Singer->VoiceIndex=SpawnSerial; }
 Enemy->BeginSpawnWarmup(Warmup);Enemy->FinishSpawning(Pose);ActiveEnemies.Add(Enemy);++SpawnSerial;return Enemy;
}
void ABBSBSequentialEncounter::Tick(float DT)
{
 Super::Tick(DT);const double Now=GetWorld()->GetTimeSeconds();
 for(int32 I=ActiveEnemies.Num()-1;I>=0;--I)
 {
  ABBSBInkEnemy* Enemy=ActiveEnemies[I];if(IsValid(Enemy) && !Enemy->bDefeated)continue;
  if(IsValid(Enemy) && Enemy->bDefeated) { ++Defeats;Enemy->SetLifeSpan(8);QueueSpawn(SpawnDelay); }
  ActiveEnemies.RemoveAt(I);
 }
 if(!CanSpawnNow())return;
 if(!bStarted) { bStarted=true;for(int32 I=0;I<FMath::Clamp(InitialCount,1,8);++I)QueueSpawn(SpawnDelay+I*.3f); }
 for(int32 I=0;I<Pending.Num();++I)
 {
  auto& P=Pending[I];if(Now<P.ReadyAt-FMath::Max(.3f,WarningSeconds))continue;
  if(!P.bPrepared)
  {
   double Distance;int32 Direction;FVector At;
   if(!FindSpawn(At,Distance,Direction)) { P.ReadyAt=Now+FMath::Max(.3f,WarningSeconds)+.2f;Status=TEXT("안전한 등장 지점을 기다리는 중");continue; }
   P.Position=At;P.bPrepared=true;P.ReadyAt=FMath::Max(P.ReadyAt,Now+FMath::Max(.3f,WarningSeconds));CursorDistance=Distance;TravelDirection=Direction;
   if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BBSB/Interaction/Materials/M_BunniarAimRing.M_BunniarAimRing")))
   {
    auto* Warning=NewObject<UDecalComponent>(this);AddInstanceComponent(Warning);Warning->SetupAttachment(RootComponent);
    auto* Tint=UMaterialInstanceDynamic::Create(Material,Warning);Tint->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.55f,.75f,.95f));Warning->SetDecalMaterial(Tint);
    Warning->DecalSize=FVector(6,82,82);Warning->SetFadeScreenSize(.0001f);Warning->RegisterComponent();
    const auto* Default=EnemyClasses.IsEmpty() ? nullptr:Cast<ABBSBInkEnemy>(EnemyClasses[SpawnSerial%EnemyClasses.Num()].GetDefaultObject());
    const float Half=Default ? Default->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():72.f;
    Warning->SetWorldLocationAndRotation(At-FVector(0,0,Half),FVector::UpVector.Rotation());P.Warning=Warning;
   }
  }
  if(Now<P.ReadyAt)continue;
  // Recheck after the warning; the player or a moved crate may now occupy it.
  const FVector Sample=P.Position-FVector(0,0,75);FVector At;
  const bool WasPrepared=P.bPrepared;P.bPrepared=false;const bool Safe=ValidateSpawn(Sample,At);P.bPrepared=WasPrepared;
  UClass* Class=EnemyClasses.IsEmpty() ? nullptr:EnemyClasses[SpawnSerial%EnemyClasses.Num()].Get();
  if(!Safe || !CreateEnemy(Class,At,FMath::Max(.25f,AttackGraceSeconds))) { if(P.Warning)P.Warning->DestroyComponent();P.Warning=nullptr;P.bPrepared=false;P.ReadyAt=Now+FMath::Max(.3f,WarningSeconds)+.2f;continue; }
  if(P.Warning)P.Warning->DestroyComponent();Pending.RemoveAt(I--);Status=TEXT("일반 전투 진행 중");
 }
 PendingCount=Pending.Num();
}
void ABBSBSequentialEncounter::ClearRuntime()
{
 for(auto& P:Pending)if(P.Warning)P.Warning->DestroyComponent();Pending.Reset();PendingCount=0;
 for(ABBSBInkEnemy* E:ActiveEnemies)if(IsValid(E))E->Destroy();ActiveEnemies.Reset();
}
void ABBSBSequentialEncounter::ResetEncounter() { ClearRuntime();bStarted=false;Defeats=SpawnSerial=0;SetInitialDirection(); }
void ABBSBSequentialEncounter::EndPlay(EEndPlayReason::Type Reason) { ClearRuntime();Super::EndPlay(Reason); }

FBBSBEncounterSave ABBSBSequentialEncounter::CaptureState() const
{
 FBBSBEncounterSave S;S.Path=GetPathName(GetWorld());S.Cursor=CursorDistance;S.Direction=TravelDirection;S.Defeats=Defeats;S.Serial=SpawnSerial;S.bStarted=bStarted;
 const double Now=GetWorld()->GetTimeSeconds();for(const auto& P:Pending)S.PendingDelays.Add(FMath::GridSnap(float(FMath::Max(0.,P.ReadyAt-Now)),.1f));
 for(const ABBSBInkEnemy* E:ActiveEnemies)if(IsValid(E))
 {
  if(E->bDefeated) { ++S.Defeats;S.PendingDelays.Add(SpawnDelay);continue; }
  FBBSBEncounterEnemySave R;R.Class=E->GetClass();R.Transform=E->GetActorTransform();R.Transform.SetLocation(R.Transform.GetLocation().GridSnap(.1));R.Health=FMath::GridSnap(E->Health,.1f);R.WarmupLeft=FMath::GridSnap(float(FMath::Max(0.,E->SpawnWarmupUntil-Now)),.1f);
  R.EcstaticBudget=E->EcstaticRewardBudget;R.NoteBudget=E->NoteRewardBudget;R.EcstaticDropped=E->EcstaticRewardsDropped;R.NotesDropped=E->NoteRewardsDropped;R.DropSerial=E->RewardDropSerial;R.LowestHealthFraction=E->RewardLowestHealthFraction;S.Living.Add(R);
 }
 return S;
}
void ABBSBSequentialEncounter::RestoreState(const FBBSBEncounterSave& S)
{
 ClearRuntime();bStarted=S.bStarted;CursorDistance=FMath::IsFinite(S.Cursor) ? FMath::Clamp(S.Cursor,0.,double(Path->GetSplineLength())):0;TravelDirection=S.Direction<0 ? -1:1;Defeats=FMath::Max(0,S.Defeats);SpawnSerial=FMath::Max(0,S.Serial);
 for(const auto& R:S.Living)
 {
  if(ActiveEnemies.Num()+Pending.Num()>=FMath::Clamp(InitialCount,1,8))break;
  if(!FMath::IsFinite(R.Health) || R.Health<=0 || R.Transform.ContainsNaN())continue;
  auto* Enemy=CreateEnemy(R.Class.LoadSynchronous(),R.Transform.GetLocation(),FMath::Max(AttackGraceSeconds,R.WarmupLeft));
  if(!Enemy) { QueueSpawn(SpawnDelay);continue; }
  Enemy->SetActorRotation(R.Transform.Rotator());Enemy->Health=FMath::Clamp(R.Health,.1f,Enemy->MaximumHealth);Enemy->ResetHealthFeedback();
  Enemy->EcstaticRewardBudget=R.EcstaticBudget;Enemy->NoteRewardBudget=R.NoteBudget;Enemy->EcstaticRewardsDropped=FMath::Max(0,R.EcstaticDropped);Enemy->NoteRewardsDropped=FMath::Max(0,R.NotesDropped);Enemy->RewardDropSerial=FMath::Max(0,R.DropSerial);Enemy->RewardLowestHealthFraction=FMath::Clamp(R.LowestHealthFraction,0.f,1.f);
 }
 SpawnSerial=FMath::Max(0,S.Serial);
 for(float Delay:S.PendingDelays)if(ActiveEnemies.Num()+Pending.Num()<FMath::Clamp(InitialCount,1,8) && FMath::IsFinite(Delay))QueueSpawn(Delay);
}
