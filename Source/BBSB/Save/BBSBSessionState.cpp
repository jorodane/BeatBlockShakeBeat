#include "Save/BBSBSession.h"
#include "BBSBCharacter.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBLootPickup.h"
#include "Character/BBSBTraversal.h"
#include "Character/BBSBRespawn.h"
#include "Field/BBSBFieldActions.h"
#include "Combat/BBSBCombat.h"
#include "Interaction/BBSBInteraction.h"
#include "Caravan/BBSBCaravan.h"
#include "Stage01/BBSBStage01.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

namespace
{
 FString ActorKey(const AActor* A) { return A ? A->GetPathName(A->GetWorld()):FString(); }
 AActor* ResolveActor(UWorld* W,const FString& Key)
 {
  if(Key.IsEmpty())return nullptr;for(TActorIterator<AActor> It(W);It;++It)if(ActorKey(*It)==Key)return *It;return nullptr;
 }
 FVector Rounded(FVector V,float Grid=1.f) { return FVector(FMath::GridSnap(V.X,Grid),FMath::GridSnap(V.Y,Grid),FMath::GridSnap(V.Z,Grid)); }
 FTransform Rounded(FTransform T)
 {
  T.SetLocation(Rounded(T.GetLocation()));FRotator R=T.Rotator();R.Pitch=FMath::GridSnap(R.Pitch,.1);R.Yaw=FMath::GridSnap(R.Yaw,.1);R.Roll=FMath::GridSnap(R.Roll,.1);T.SetRotation(R.Quaternion());return T;
 }
}
UBBSBSessionSave* UBBSBSessionSubsystem::Capture() const
{
 auto* P=Player.Get();if(!P || !P->Hunting->bHuntingInitialized || P->Combat->bDead || P->Respawn->IsSequenceActive())return nullptr;
 auto* S=NewObject<UBBSBSessionSave>();auto* T=P->Traversal.Get();auto* Move=P->GetCharacterMovement();
 S->MapPackage=FPackageName::GetLongPackagePath(GetWorld()->GetOutermost()->GetName())+TEXT("/")+UGameplayStatics::GetCurrentLevelName(GetWorld(),true);
 S->PlayerTransform=Rounded(P->GetActorTransform());S->ViewRotation=P->GetControlRotation().GetDenormalized();
 S->ViewRotation.Pitch=FMath::GridSnap(S->ViewRotation.Pitch,.1);S->ViewRotation.Yaw=FMath::GridSnap(S->ViewRotation.Yaw,.1);S->ViewRotation.Roll=0;
 S->Velocity=Rounded(Move->Velocity);S->Health=FMath::GridSnap(P->Combat->Health,.1f);S->bAirJumpUsed=T->bAirJumpUsed;
 S->WireCharges=T->WireCharges;S->WireRecharge=FMath::GridSnap(T->WireRechargeProgress,.001f);S->bWireRechargeArmed=T->bWireRechargeArmed;
 if(const auto* B=Cast<ABBSBBunniarCompanion>(P->FieldActions->BunniarEquipment))S->bBunniarOpeningRest=B->bOpeningRest;
 UGameplayStatics::SaveGameToMemory(P->Hunting->CaptureProfile(),S->Inventory);
 for(TActorIterator<ABBSBStage01Director> It(GetWorld());It;++It)
 {
  auto* D=*It;auto* Story=NewObject<UBBSBStage01Save>();Story->Phase=D->Phase;Story->Solved=D->Solved;Story->Quieted=D->Quieted;Story->Tea=D->Tea;Story->bRewardGranted=D->bRewardGranted;
  UGameplayStatics::SaveGameToMemory(Story,S->Story);S->StoryTool=D->ToolInstance;S->bTutorialLoadoutEnforced=D->bEnforceTutorialLoadout;break;
 }
 S->Movement=Move->IsMovingOnGround() ? EBBSBSavedMovement::Ground:EBBSBSavedMovement::Falling;
 if(T->bGliding)S->Movement=EBBSBSavedMovement::Glide;
 if(T->IsOnLadder())
 {
  S->Movement=EBBSBSavedMovement::Ladder;S->SurfaceActor=ActorKey(T->LadderSource);S->LadderBottom=T->LadderBottom;S->LadderTop=T->LadderTop;S->LadderExit=T->LadderExit;S->SurfaceNormal=T->LadderOut;S->LadderRungSpacing=T->LadderRungSpacing;
 }
 else if(T->ParkourMode==EBBSBParkourMode::WallSlide || T->ParkourMode==EBBSBParkourMode::LedgeHang)
 {
  S->Movement=T->ParkourMode==EBBSBParkourMode::WallSlide ? EBBSBSavedMovement::WallSlide:EBBSBSavedMovement::LedgeHang;
  if(auto* Part=T->ParkourSurface.Get()) { S->SurfaceActor=ActorKey(Part->GetOwner());S->SurfaceComponent=Part->GetName(); }
  S->SurfacePoint=T->LedgePoint;S->SurfaceNormal=T->LedgeNormal;
  if(auto* Part=T->ParkourSurface.Get())S->LedgeStand=Part->GetComponentTransform().TransformPosition(T->LedgeLocalStand);
  S->LedgeTopNormal=T->LedgeTopNormal;
 }
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  auto* A=*It;FBBSBSessionActor R;R.Path=ActorKey(A);R.Transform=Rounded(A->GetActorTransform());
  if(auto* D=Cast<ABBSBStage01Device>(A)) { R.Kind=1;R.Value=FMath::GridSnap(D->Painted,.001f);R.bFlag=D->bSolved; }
  else if(auto* E=Cast<ABBSBInkEnemy>(A)) { R.Kind=2;R.Value=FMath::GridSnap(E->Health,.1f);R.bFlag=E->bDefeated; }
  else if(auto* C=Cast<ABBSBCarryObject>(A))
  {
   R.Kind=3;R.Value=FMath::GridSnap(C->Health,.1f);R.bFlag=C->bBroken;R.bSimulating=C->Body->IsSimulatingPhysics() || C->bCarried;
   if(!C->bCarried) { R.Velocity=Rounded(C->Body->GetPhysicsLinearVelocity());R.AngularVelocity=Rounded(C->Body->GetPhysicsAngularVelocityInDegrees()); }
  }
  else if(auto* L=Cast<ABBSBLootPickup>(A);L && L->IsAvailable())
  {
   FBBSBSessionLoot Loot;Loot.Entry=L->WeaponPayload;Loot.Entry.Item=L->Item;Loot.Entry.Quantity=L->Quantity;Loot.Location=Rounded(L->GetActorLocation());Loot.bConcertoReward=L->bConcertoReward;S->Loot.Add(Loot);continue;
  }
  else continue;
  S->Actors.Add(R);
 }
 S->Actors.Sort([](const auto& A,const auto& B){return A.Path<B.Path;});
 S->Loot.Sort([](const auto& A,const auto& B){const FString X=A.Entry.Item ? A.Entry.Item->GetPathName():FString(),Y=B.Entry.Item ? B.Entry.Item->GetPathName():FString();return X==Y ? A.Location.X<B.Location.X:X<Y;});
 return S;
}
bool UBBSBSessionSubsystem::Restore(UBBSBSessionSave* S)
{
 auto* P=Player.Get();if(!P || !S || !S->IsValidRecord())return false;
 const FString Map=FPackageName::GetLongPackagePath(GetWorld()->GetOutermost()->GetName())+TEXT("/")+UGameplayStatics::GetCurrentLevelName(GetWorld(),true);
 if(S->MapPackage!=Map)return false;
 TGuardValue<bool> ApplyingGuard(bApplying,true);
 auto* Profile=Cast<UBBSBHuntingSave>(UGameplayStatics::LoadGameFromMemory(S->Inventory));
 auto* Story=S->Story.IsEmpty() ? nullptr:Cast<UBBSBStage01Save>(UGameplayStatics::LoadGameFromMemory(S->Story));
 if(!Profile || (!S->Story.IsEmpty() && (!Story || Story->Version!=1)))return false;
 auto* T=P->Traversal.Get();auto* Move=P->GetCharacterMovement();P->Interaction->EndDialogue();T->CancelWireRecall();T->DropParkour();T->EndGlide();P->FieldActions->ResetActionState();
 if(auto* B=Cast<ABBSBBunniarCompanion>(P->FieldActions->BunniarEquipment))B->ReleaseSurface();
 if(!P->Hunting->ApplyProfile(Profile))return false;P->Hunting->TickEquipmentLoadout();
 if(Story)for(TActorIterator<ABBSBStage01Director> It(GetWorld());It;++It)
 {
  auto* D=*It;D->Phase=Story->Phase;D->Solved=Story->Solved;D->Quieted=Story->Quieted;D->Tea=Story->Tea;D->bRewardGranted=Story->bRewardGranted;D->ToolInstance=S->StoryTool;D->bEnforceTutorialLoadout=S->bTutorialLoadoutEnforced;D->ApplyProgress();D->UpdateTutorialGate();
  if(!S->bBunniarOpeningRest || S->Movement==EBBSBSavedMovement::Glide)if(auto* B=Cast<ABBSBBunniarCompanion>(P->FieldActions->BunniarEquipment))B->EndOpeningRest();
 }
 for(const auto& R:S->Actors)
 {
  auto* A=ResolveActor(GetWorld(),R.Path);if(!A || R.Transform.ContainsNaN() || !FMath::IsFinite(R.Value))continue;
  if(auto* D=Cast<ABBSBStage01Device>(A);D && R.Kind==1)D->RestoreSessionState(R.Value,R.bFlag);
  else if(auto* E=Cast<ABBSBInkEnemy>(A);E && R.Kind==2)
  {
   E->SetActorTransform(R.Transform,false,nullptr,ETeleportType::TeleportPhysics);E->Health=FMath::Clamp(R.Value,0.f,E->MaximumHealth);E->bDefeated=R.bFlag;E->bHuntingRewardsGranted=R.bFlag;E->SetActorEnableCollision(!R.bFlag);E->SetActorHiddenInGame(R.bFlag);
  }
  else if(auto* C=Cast<ABBSBCarryObject>(A);C && R.Kind==3)
  {
   C->Body->SetSimulatePhysics(false);C->SetActorTransform(R.Transform,false,nullptr,ETeleportType::TeleportPhysics);C->Health=FMath::Clamp(R.Value,0.f,C->MaximumHealth);C->bBroken=R.bFlag;C->SetActorHiddenInGame(R.bFlag);C->SetActorEnableCollision(!R.bFlag);
   C->Body->SetSimulatePhysics(R.bSimulating && !R.bFlag);if(C->Body->IsSimulatingPhysics()) { C->Body->SetPhysicsLinearVelocity(R.Velocity);C->Body->SetPhysicsAngularVelocityInDegrees(R.AngularVelocity); }
  }
 }
 // Restore unclaimed drops, including individual resources. Dead enemies stay
 // rewarded, so loading cannot manufacture another copy of the same reward.
 for(TActorIterator<ABBSBLootPickup> It(GetWorld());It;++It)It->Destroy();
 for(const auto& R:S->Loot)if(R.Entry.Item && R.Entry.Quantity>0 && !R.Location.ContainsNaN())
 {
  const FTransform TF(R.Location);auto* L=GetWorld()->SpawnActorDeferred<ABBSBLootPickup>(ABBSBLootPickup::StaticClass(),TF,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(L) { L->Item=R.Entry.Item;L->Quantity=R.Entry.Quantity;L->WeaponPayload=R.Entry;L->bConcertoReward=R.bConcertoReward;L->FinishSpawning(TF); }
 }
 Move->StopMovementImmediately();Move->ClearAccumulatedForces();P->Combat->Health=FMath::Clamp(S->Health,1.f,P->Combat->MaximumHealth);
 FVector Position=S->PlayerTransform.GetLocation();FRotator Rotation=S->PlayerTransform.Rotator();
 if(!GetWorld()->FindTeleportSpot(P,Position,Rotation)) { Position=P->Respawn->RespawnTransform().GetLocation();Rotation=P->GetActorRotation(); }
 P->SetActorLocationAndRotation(Position,Rotation,false,nullptr,ETeleportType::TeleportPhysics);
 if(auto* PC=Cast<APlayerController>(P->GetController()))PC->SetControlRotation(S->ViewRotation);
 Move->SetMovementMode(MOVE_Falling);Move->Velocity=S->Velocity;T->ParkourIgnoreUntil=0;bool Restored=true;
 if(S->Movement==EBBSBSavedMovement::Ground)Move->SetMovementMode(MOVE_Walking);
 else if(S->Movement==EBBSBSavedMovement::Glide)Restored=T->BeginGlide(false);
 else if(S->Movement==EBBSBSavedMovement::Ladder)
 {
  auto* Source=ResolveActor(GetWorld(),S->SurfaceActor);T->LadderRungSpacing=FMath::Clamp(S->LadderRungSpacing,20.f,50.f);
  Restored=T->BeginLadder(Source,S->LadderBottom,S->LadderTop,S->LadderExit,S->SurfaceNormal);
  if(Restored) { T->bLadderMounting=T->bLadderExiting=T->bLadderStepping=T->bLadderHopping=false;T->LadderStepFrom=T->LadderStepTo=T->LadderDistance;T->ParkourHandAlpha=1;T->ParkourPoseAlpha=1;T->ParkourIgnoreUntil=0; }
 }
 else if(S->Movement==EBBSBSavedMovement::WallSlide || S->Movement==EBBSBSavedMovement::LedgeHang)
 {
  UPrimitiveComponent* Part=nullptr;if(auto* A=ResolveActor(GetWorld(),S->SurfaceActor)) { TArray<UPrimitiveComponent*> Parts;A->GetComponents(Parts);for(auto* C:Parts)if(C->GetName()==S->SurfaceComponent)Part=C; }
  if(S->Movement==EBBSBSavedMovement::WallSlide)Restored=T->BeginWallSlide(S->SurfacePoint,S->SurfaceNormal,Part);
  else if(Part)
  {
   UBBSBTraversalComponent::FLedgeCandidate L;L.Surface=Part;L.Point=S->SurfacePoint;L.Normal=S->SurfaceNormal;L.Hang=Position;L.Stand=S->LedgeStand;L.TopNormal=S->LedgeTopNormal;T->BeginLedgeHang(L);
  }
  else Restored=false;
 }
 T->bAirJumpUsed=S->bAirJumpUsed;T->WireCharges=S->WireCharges;T->WireRechargeProgress=FMath::Clamp(S->WireRecharge,0.f,1.f);T->bWireRechargeArmed=S->bWireRechargeArmed;
 if(!T->IsOnLadder() && !T->IsParkourActive())Move->Velocity=S->Velocity;
 if(!Restored) { T->CancelParkour();Move->SetMovementMode(MOVE_Falling);Move->Velocity=S->Velocity;StatusText=TEXT("자세를 유지할 수 없어 안전하게 공중 상태로 돌아왔어요."); }
 return true;
}
