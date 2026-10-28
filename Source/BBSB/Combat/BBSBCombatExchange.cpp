#include "BBSBCombat.h"
#include "BBSBCombatVisuals.h"
#include "BBSB.h"
#include "Engine/World.h"

bool ABBSBCombatStage::IsCounterWindow() const
{
 return bUseCombatExchanges && ExperimentPreset==0 && Phase==EBBSBStagePhase::Fighting && bCounterAttackOpen;
}
float ABBSBCombatStage::CounterSecondsRemaining() const
{
 if(!IsCounterWindow() || !GetWorld()) return 0;
 const double Beat=BBSBCombatRules::BeatAt(GetWorld()->GetTimeSeconds(),MusicEpoch,BPM);
 return FMath::Max(0.f,float((CounterStartedBeat+FMath::Max(2.f,CounterWindowBeats)-Beat)*60./BPM));
}
int32 ABBSBCombatStage::UnresolvedAttackCount() const
{
 int32 Count=0;for(const auto& Shot:ExchangeProjectiles) if(Shot.IsValid() && !Shot->bResolved) ++Count;return Count;
}
FString ABBSBCombatStage::ExchangeHint() const
{
 if(IsCounterWindow()) return FString::Printf(TEXT("COUNTER!  %.1fs  |  damage x%.2f"),CounterSecondsRemaining(),CounterDamageMultiplier);
 return FString::Printf(TEXT("%s  |  %s"),CurrentBeat<ExchangeStartedBeat+FMath::Max(2.f,EnemyWindupBeats) ? TEXT("READ"):TEXT("DEFEND"),*EnemyPatternHint);
}
void ABBSBCombatStage::RefreshExchangeLabels()
{
 for(ABBSBInkEnemy* Member:Members) if(IsValid(Member) && !Member->bDefeated) Member->UpdateLabel();
}
void ABBSBCombatStage::BeginExchange()
{
 if(!IsValid(Front) || Front->bDefeated) return;
 ++ExchangeNumber;bCounterAttackOpen=false;CounterStartedBeat=-1;ExchangeStartedBeat=CurrentBeat;PendingNotes.Reset();
 const float Windup=FMath::Max(2.f,EnemyWindupBeats);
 BBSBCombatRules::FSocketLedger Ledger;EnemyPatternHint.Reset();
 const int32 Count=FMath::Clamp(Front->AttackDirections.Num(),1,4);
 float LastImpact=Windup;
 for(int32 Index=0;Index<Count;++Index)
 {
  const auto Pattern=Front->MainPattern;
  if(Pattern==EBBSBInkPattern::Area && Index==1) continue;
  const int32 Direction=Front->AttackDirections.IsValidIndex(Index) ? int32(Front->AttackDirections[Index]):0;
  const uint8 Mask=1<<FMath::Clamp(Direction,0,3);const float Offset=Windup+Index;
  const bool Hold=Pattern==EBBSBInkPattern::Hold && Index==1;
  if(!Ledger.Reserve(Offset,Mask,Hold ? 1.f:0.f)) continue;
  if(!EnemyPatternHint.IsEmpty()) EnemyPatternHint+=TEXT(" > ");
  EnemyPatternHint+=Pattern==EBBSBInkPattern::Area ? TEXT("GROUND"):FString::Printf(TEXT("%s%s"),Hold ? TEXT("HOLD "):TEXT(""),BBSBCombatVisuals::DirectionName(Direction));
  const int32 Group=Hold ? NextHoldGroup++:INDEX_NONE;
  for(int32 Pulse=0;Pulse<(Hold ? 3:1);++Pulse)
  {
   FNote Note;Note.ImpactBeat=ExchangeStartedBeat+Offset+Pulse*.5f;Note.Directions=Mask;Note.Source=Front;Note.HoldGroup=Group;
   Note.Pattern=Hold ? EBBSBInkPattern::Hold:Pattern==EBBSBInkPattern::Hold ? EBBSBInkPattern::Tap:Pattern;
   PendingNotes.Add(Note);LastImpact=FMath::Max(LastImpact,Offset+Pulse*.5f);
  }
 }
 // Group fights still share one packet and one recovery; a backer cannot fill the player's opening.
 TArray<ABBSBInkEnemy*> Backers;for(ABBSBInkEnemy* Member:Members) if(IsValid(Member) && !Member->bDefeated && Member!=Front) Backers.Add(Member);
 if(!Backers.IsEmpty())
 {
  const float Offset=Windup+.5f;const uint8 Mask=1<<((ExchangeNumber+1)%4);
  if(Ledger.Reserve(Offset,Mask))
  { FNote Note;Note.ImpactBeat=ExchangeStartedBeat+Offset;Note.Directions=Mask;Note.Source=Backers[(ExchangeNumber-1)%Backers.Num()];PendingNotes.Add(Note);LastImpact=FMath::Max(LastImpact,Offset); }
 }
 EarliestCounterBeat=ExchangeStartedBeat+LastImpact+1.f;RefreshExchangeLabels();
 UE_LOG(LogBBSB,Display,TEXT("Exchange %d enemy=%s phrase=%s notes=%d"),ExchangeNumber,*GetNameSafe(Front),*EnemyPatternHint,PendingNotes.Num());
}
void ABBSBCombatStage::TickExchange()
{
 ExchangeProjectiles.RemoveAll([](const auto& Shot){return !Shot.IsValid() || Shot->bResolved;});
 if(ExchangeNumber==0) BeginExchange();
 if(bCounterAttackOpen)
 {
  if(CurrentBeat>=CounterStartedBeat+FMath::Max(2.f,CounterWindowBeats)) BeginExchange();
  return;
 }
 for(int32 Index=0;Index<PendingNotes.Num();)
 {
  if(PendingNotes[Index].ImpactBeat-2.<=CurrentBeat)
  { const FNote Note=PendingNotes[Index];PendingNotes.RemoveAt(Index);Launch(Note); }
  else ++Index;
 }
 if(BBSBCombatRules::CanOpenCounter(CurrentBeat,EarliestCounterBeat,!PendingNotes.IsEmpty(),UnresolvedAttackCount()))
 {
  bCounterAttackOpen=true;CounterStartedBeat=CurrentBeat;RefreshExchangeLabels();
  if(Player) Player->SetFeedback(TEXT("COUNTER!  |  move in and strike"));
  UE_LOG(LogBBSB,Display,TEXT("Counter open exchange=%d duration=%.2fs incoming=%d"),ExchangeNumber,CounterWindowBeats*60.f/BPM,UnresolvedAttackCount());
 }
}
