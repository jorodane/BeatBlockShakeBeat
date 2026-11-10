#include "Field/BBSBRepairTool.h"
#include "BBSBCharacter.h"
#include "Field/BBSBFieldActions.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBItem.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Character/BBSBRespawn.h"
#include "Interaction/BBSBInteraction.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
UBBSBRepairToolComponent::UBBSBRepairToolComponent() { PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;StrikeAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BBSB/AnimationAuthoring/Sword/A_Sword_Beat1.A_Sword_Beat1"))); }
void UBBSBRepairToolComponent::BeginPlay() { Super::BeginPlay();Player=Cast<ABBSBCharacter>(GetOwner());Clip=StrikeAnimation.LoadSynchronous(); }
void UBBSBRepairToolComponent::EndPlay(const EEndPlayReason::Type Reason) { for(auto* C:{Board.Get(),Head.Get(),Handle.Get()})if(C)C->DestroyComponent();Super::EndPlay(Reason); }
bool UBBSBRepairToolComponent::IsRepairToolEquipped() const
{
 if(!Player || !Player->Hunting || !Player->FieldActions || Player->FieldActions->IsBunniarControlActive())return false;
 const auto* E=Player->Hunting->FindOwnedEntry(Player->FieldActions->EquippedWeaponInstance);
 return E && E->Item && E->Item->bRepairTool;
}
void UBBSBRepairToolComponent::EnsureVisuals()
{
 if(Board || !Player)return;
 auto Make=[&](FName Name,FVector Scale,FLinearColor Color)
 {
  auto* C=NewObject<UStaticMeshComponent>(Player,Name);Player->AddInstanceComponent(C);C->SetupAttachment(Player->GetRootComponent());C->SetAbsolute(true,true,true);
  C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));C->SetMobility(EComponentMobility::Movable);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->RegisterComponent();C->SetWorldScale3D(Scale);BBSBCombatVisuals::Tint(C,Color);return C;
 };
 Board=Make(TEXT("RepairWoodBoard"),FVector(.16,.80,1.0),FLinearColor(.42,.26,.14));
 Handle=Make(TEXT("RepairMalletHandle"),FVector(.10,.10,.85),FLinearColor(.63,.43,.25));
 Head=Make(TEXT("RepairRubberHead"),FVector(.45,.70,.38),FLinearColor(.22,.17,.21));
}
FVector UBBSBRepairToolComponent::Aim() const
{
 APlayerController* PC=Cast<APlayerController>(Player->GetController());if(!PC)return Player->GetActorLocation();
 FVector P=Player->FieldActions->VisualAimPoint();
 if(Player->Combat->HasTargetLock())P=Player->Combat->FieldLockedTarget->GetActorLocation();
 const FVector Start=Player->GetActorLocation();return Start+(P-Start).GetClampedToMaxSize(Reach);
}
bool UBBSBRepairToolComponent::Gesture(EBBSBFieldGesture G,int32 Channel,bool Initial)
{
 if(!IsRepairToolEquipped() || !Player->Combat->CanAttack() || Player->Respawn->IsSequenceActive() || Player->Interaction->bDialogueActive)return false;
 const double Now=GetWorld()->GetTimeSeconds();
 if(Channel==1)
 {
  if(G==EBBSBFieldGesture::Beat && Initial) { bBoardPlaced=true;BoardHits=0;LastBoardBeat=-100;BoardEpoch=Now+BoardBeatSeconds;Target=Aim();Feedback=TEXT("판 고정 · 빛이 모일 때 좌클릭, 최대 3회");return true; }
  return false;
 }
 if(G!=EBBSBFieldGesture::Beat || !Initial || Now<ReadyAt)return false;
 if(Player->FieldActions->IsDodging() || !Player->Combat->CanAttack())return false;
 const auto Snapshot=Player->EquipmentGrowth->WeaponSnapshot(Player->FieldActions->EquippedWeaponInstance);
 const float Recovery=(bBoardPlaced ? .28f:DirectRecovery)*Snapshot.CooldownMultiplier;
 if(bBoardPlaced)
 {
  const double Beat=FMath::RoundToDouble((Now-BoardEpoch)/BoardBeatSeconds);
  if(Beat<0 || FMath::Abs(Now-(BoardEpoch+Beat*BoardBeatSeconds))>BoardTimingWindow || Beat==LastBoardBeat || BoardHits>=3)
  { Feedback=TEXT("판 타격 타이밍을 기다려주세요");return false; }
  if(!Player->FieldActions->BeginItemAttack(Recovery))return false;LastBoardBeat=Beat;++BoardHits;
 }
 else { if(!Player->FieldActions->BeginItemAttack(Recovery))return false;Target=Aim(); }
 bStrikeBoard=bBoardPlaced;StartedAt=Now;HitAt=Now+.12;ReadyAt=Now+Recovery;bHitApplied=false;
 From=Head ? Head->GetComponentLocation():Player->GetActorLocation();
 Feedback=bStrikeBoard ? FString::Printf(TEXT("판 타격 %d / 3"),BoardHits):TEXT("직접 타격 · 회복 중");
 Player->Combat->OnAttackStarted();return true;
}
void UBBSBRepairToolComponent::ReleaseBoard() { bBoardPlaced=false;BoardHits=0;BoardEpoch=LastBoardBeat=-100; }
float UBBSBRepairToolComponent::NextBeatProgress() const { return bBoardPlaced ? FMath::Frac(float((GetWorld()->GetTimeSeconds()-BoardEpoch+BoardBeatSeconds)/BoardBeatSeconds)):0; }
void UBBSBRepairToolComponent::ApplyHit()
{
 bHitApplied=true;
 const auto Snapshot=Player->EquipmentGrowth->WeaponSnapshot(Player->FieldActions->EquippedWeaponInstance);
 const float Scale=Snapshot.DamageMultiplier*Player->EquipmentGrowth->ConditionalDamageMultiplier(Snapshot);
 for(TActorIterator<ABBSBInkEnemy> It(GetWorld());It;++It)
 {
  if(It->bDefeated || FVector::DistSquared(It->GetActorLocation(),Target)>FMath::Square(HitRadius+45))continue;
  FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(RepairToolStrike),false,Player);Q.AddIgnoredActor(*It);
  if(GetWorld()->LineTraceSingleByObjectType(Hit,Target,It->GetActorLocation(),BBSBCombatVisuals::GeometryTypes(),Q))continue;
  const float Applied=UGameplayStatics::ApplyDamage(*It,(Damage*FMath::Max(.1f,Scale)+Snapshot.FlatDamage)*(bStrikeBoard ? .9f:1.f),Cast<APlayerController>(Player->GetController()),Player,UDamageType::StaticClass());
  if(Applied>0) { ++HitsLanded;Player->EquipmentGrowth->NotifyWeaponHit(Snapshot,*It,Applied,It->GetActorLocation()); }
 }
}
void UBBSBRepairToolComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DT,Type,Function);if(!Player)return;const bool Active=IsRepairToolEquipped() && !Player->Respawn->IsSequenceActive();
 if(!Active) { if(Board)for(auto* C:{Board.Get(),Head.Get(),Handle.Get()})C->SetVisibility(false);ReleaseBoard();bHitApplied=true;return; }
 EnsureVisuals();for(auto* C:{Board.Get(),Head.Get(),Handle.Get()})C->SetVisibility(true);
 auto* PC=Cast<APlayerController>(Player->GetController());if(!PC)return;FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 const FVector F=View.Vector(),R=FRotationMatrix(View).GetUnitAxis(EAxis::Y),U=FRotationMatrix(View).GetUnitAxis(EAxis::Z);
 const FVector Rest=Eye+F*260+R*95-U*90;const double Now=GetWorld()->GetTimeSeconds();
 if(Player->Combat->bDead || Player->FieldActions->IsDodging()) { bHitApplied=true;ReleaseBoard(); }
 if(!bHitApplied && Now>=HitAt)ApplyHit();
 const float Age=Now-StartedAt;FVector P=Rest;FRotator Rot=View;
 if(Age>=0 && Age<.48f)
 {
  const float T=FMath::Clamp(Age/.12f,0.f,1.f),Back=FMath::SmoothStep(.18f,.48f,Age);
  const FVector Contact=Target-F*25;P=FMath::Lerp(FMath::Lerp(From,Contact,T),Rest,Back);P+=U*FMath::Sin(T*PI)*60*(1-Back);Rot.Pitch+=FMath::Lerp(-65.f,25.f,T)*(1-Back);
 }
 const FQuat Frame=Rot.Quaternion();Handle->SetWorldLocationAndRotation(P-Frame.GetUpVector()*45,Frame);Head->SetWorldLocationAndRotation(P,Frame);
 Board->SetWorldLocationAndRotation(bBoardPlaced ? Target+F*8:Rest-R*45-F*15,View);
 if(bBoardPlaced)BBSBCombatVisuals::Tint(Board,FMath::Abs(NextBeatProgress()-.5f)>.35f ? FLinearColor(.67,.50,.26):FLinearColor(.42,.26,.14));
}
void UBBSBRepairToolComponent::GetBodyPose(UAnimSequence*& Animation,float& Time,float& Weight) const
{
 if(!IsRepairToolEquipped() || !Clip)return;const float Age=GetWorld()->GetTimeSeconds()-StartedAt;
 if(Age<0 || Age>.48)return;Animation=Clip;Time=FMath::Clamp(Age/.48f,0.f,1.f)*Clip->GetPlayLength();Weight=FMath::SmoothStep(0.f,.05f,Age)*(1-FMath::SmoothStep(.30f,.48f,Age));
}
