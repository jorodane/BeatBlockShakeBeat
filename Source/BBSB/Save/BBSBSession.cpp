#include "Save/BBSBSession.h"
#include "BBSBCharacter.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Character/BBSBTraversal.h"
#include "Character/BBSBRespawn.h"
#include "Character/BBSBWorldMap.h"
#include "Field/BBSBFieldActions.h"
#include "Combat/BBSBCombat.h"
#include "Interaction/BBSBInteraction.h"
#include "Caravan/BBSBCaravan.h"
#include "Stage01/BBSBStage01.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"

UBBSBSessionSubsystem* UBBSBSessionSubsystem::Get(const UObject* Context)
{
 const UWorld* W=Context ? Context->GetWorld():nullptr;const auto* GI=W ? W->GetGameInstance():nullptr;
 return GI ? GI->GetSubsystem<UBBSBSessionSubsystem>():nullptr;
}
bool UBBSBSessionSubsystem::Owns(const UObject* Context) { const auto* S=Get(Context);return S && S->bEnabled; }
UWorld* UBBSBSessionSubsystem::GetTickableGameObjectWorld() const { return GetGameInstance() ? GetGameInstance()->GetWorld():nullptr; }
void UBBSBSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);TearDownHandle=FWorldDelegates::OnWorldBeginTearDown.AddUObject(this,&UBBSBSessionSubsystem::OnWorldTearDown);
}
void UBBSBSessionSubsystem::Deinitialize()
{
 FWorldDelegates::OnWorldBeginTearDown.Remove(TearDownHandle);if(Menu)Menu->RemoveFromParent();Menu=nullptr;Super::Deinitialize();
}
bool UBBSBSessionSubsystem::BindPlayer()
{
 auto* W=GetTickableGameObjectWorld();if(!W || !W->IsGameWorld() || W->bIsTearingDown)return false;
 auto* P=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(W,0));if(!P || !P->IsLocallyControlled() || !P->Hunting->bHuntingInitialized || !P->FieldActions->BunniarEquipment)return false;
 for(TActorIterator<ABBSBStage01Director> It(W);It;++It)if(It->PlayerCharacter()!=P)return false;
 Player=P;BoundWorld=W;
 auto* PC=Cast<APlayerController>(P->GetController());Controls=NewObject<UInputComponent>(P,TEXT("SessionMenuControls"));Controls->RegisterComponent();Controls->Priority=70;
 auto& Key=Controls->BindKey(EKeys::Escape,IE_Pressed,this,&UBBSBSessionSubsystem::OpenMenu);Key.bExecuteWhenPaused=true;PC->PushInputComponent(Controls);
 bSwitching=false;NextCheck=FPlatformTime::Seconds()+FMath::Max(.1f,AutoSaveIntervalSeconds);
 if(Pending)
 {
  bApplying=true;const bool OK=Restore(Pending);bApplying=false;Pending=nullptr;bLoading=false;
  if(!OK) { StatusText=TEXT("이 기록을 불러오지 못했어요. 다른 기록을 선택해 주세요.");OpenMenu();return true; }
  bGameStarted=true;LastStateHash=Capture()->StateHash();
 }
 else if(bNewGamePending)
 {
  bNewGamePending=false;bLoading=false;bGameStarted=true;LastStateHash.Empty();SaveNow(TEXT("처음부터"),true);
 }
 else if(!bGameStarted)OpenMenu();
 return true;
}
void UBBSBSessionSubsystem::Tick(float DeltaTime)
{
 auto* W=GetTickableGameObjectWorld();if(!W || !W->IsGameWorld())return;
 if(!Player.IsValid() || BoundWorld.Get()!=W) { if(!bSwitching || FPlatformTime::Seconds()>BindAfter)BindPlayer();return; }
 if(!bGameStarted || bMenuOpen || bLoading || bApplying || W->IsPaused() || Player->Respawn->IsSequenceActive() || Player->Combat->bDead)return;
 const double Now=FPlatformTime::Seconds();
 if(!PendingReason.IsEmpty() && Now-SaveRequestedAt>.35 && IsStable()) { const FString Why=PendingReason;SaveNow(Why); }
 if(Now>=NextCheck) { NextCheck=Now+FMath::Max(.1f,AutoSaveIntervalSeconds);CheckAutoSave(); }
}
bool UBBSBSessionSubsystem::IsStable() const
{
 if(!Player.IsValid() || Player->Combat->bDead || Player->Respawn->IsSequenceActive() || Player->Interaction->bDialogueActive)return false;
 const auto* A=Player->FieldActions.Get();const auto* T=Player->Traversal.Get();
 return !Player->EquipmentGrowth->bConcertoActive && !A->IsDodging() && !A->IsGestureCaptured() && A->ActiveState==EBBSBActionState::Idle && !T->IsWireTraversalActive() &&
  T->ParkourMode!=EBBSBParkourMode::Mantle && T->ParkourMode!=EBBSBParkourMode::Vault;
}
void UBBSBSessionSubsystem::RequestAutoSave(FString Reason)
{
 if(!bGameStarted || bApplying || bLoading)return;PendingReason=Reason;SaveRequestedAt=FPlatformTime::Seconds();bChanged=true;
}
void UBBSBSessionSubsystem::CheckAutoSave()
{
 if(!bGameStarted || bApplying || bLoading || !Player.IsValid())return;
 auto* Record=Capture();if(!Record || Record->StateHash()==LastStateHash)return;
 if(!IsStable()) { RequestAutoSave(TEXT("자동 저장"));return; }SaveNow(TEXT("자동 저장"));
}
bool UBBSBSessionSubsystem::SaveNow(FString Reason,bool Force)
{
 if(!bEnabled || !bGameStarted || bApplying || bLoading || !Player.IsValid() || Player->Combat->bDead || Player->Respawn->IsSequenceActive())return false;
 if(!Force && !IsStable()) { RequestAutoSave(Reason);return false; }
 auto* Record=Capture();if(!Record)return false;const FString Hash=Record->StateHash();
 if(!Force && Hash==LastStateHash) { PendingReason.Empty();bChanged=false;return true; }
 Record->Reason=Reason;
 if(!WriteRecord(Record)) { StatusText=TEXT("저장하지 못했어요. 기존 기록은 유지돼요.");return false; }
 LastStateHash=Hash;PendingReason.Empty();bChanged=false;StatusText=TEXT("저장했어요");return true;
}
void UBBSBSessionSubsystem::OnWorldTearDown(UWorld* W)
{
 if(bEnabled && W==BoundWorld.Get() && !bSwitching && bGameStarted && !bLoading)
 {
  if(auto* Record=Capture();Record && Record->StateHash()!=LastStateHash)SaveNow(TEXT("마지막 위치"),true);
 }
 if(W==BoundWorld.Get()) { if(Menu)Menu->RemoveFromParent();Menu=nullptr;bMenuOpen=bOwnPause=bOwnMoveIgnore=bOwnLookIgnore=false;Player.Reset();Controls=nullptr; }
}
bool UBBSBSessionSubsystem::HasLegacy() const
{
 return Player.IsValid() && UGameplayStatics::DoesSaveGameExist(Player->Hunting->SaveSlotName,0);
}
bool UBBSBSessionSubsystem::CanContinue() const { return bGameStarted || Slots.ContainsByPredicate([](const auto& S){return S.bValid;}) || HasLegacy(); }
bool UBBSBSessionSubsystem::ContinueGame()
{
 if(bGameStarted) { CloseMenu();return true; }RefreshSlots();for(const auto& S:Slots)if(S.bValid)return LoadSlot(S.Index);return ImportLegacy();
}
void UBBSBSessionSubsystem::BeginMap(const FString& Map)
{
 CloseMenu();bSwitching=true;bLoading=true;BindAfter=FPlatformTime::Seconds()+.1;Player.Reset();BoundWorld.Reset();LastStateHash.Empty();
 UGameplayStatics::OpenLevel(GetGameInstance(),FName(*Map));
}
bool UBBSBSessionSubsystem::StartNewGame()
{
 if(!Player.IsValid())return false;
 const FString Map=FPackageName::GetLongPackagePath(GetWorld()->GetOutermost()->GetName())+TEXT("/")+UGameplayStatics::GetCurrentLevelName(GetWorld(),true);
 if(bGameStarted)SaveNow(TEXT("처음부터 시작 전"),true);
 Pending=nullptr;bNewGamePending=true;bGameStarted=false;BeginMap(Map);return true;
}
bool UBBSBSessionSubsystem::LoadSlot(int32 Index)
{
 auto* Record=ReadRecord(Index);
 if(!Record || !FPackageName::DoesPackageExist(Record->MapPackage)) { StatusText=TEXT("기록이나 지도를 읽을 수 없어요.");if(Menu)Menu->Refresh();return false; }
 Pending=Record;if(bGameStarted)SaveNow(TEXT("불러오기 전"),true);
 bNewGamePending=false;BeginMap(Record->MapPackage);return true;
}
bool UBBSBSessionSubsystem::ImportLegacy()
{
 if(!Player.IsValid() || !HasLegacy())return false;bApplying=true;
 const bool OK=Player->Hunting->LoadProgress();
 if(OK)
 {
  for(TActorIterator<ABBSBStage01Director> It(GetWorld());It;++It)
  {
   auto* D=*It;if(auto* S=Cast<UBBSBStage01Save>(UGameplayStatics::LoadGameFromSlot(D->SaveSlotName,0));S && S->Version==1)
   { D->Phase=S->Phase;D->Solved=S->Solved;D->Quieted=S->Quieted;D->Tea=S->Tea;D->bRewardGranted=S->bRewardGranted;D->ApplyProgress();D->UpdateTutorialGate(); }
  }
  Player->Hunting->TickEquipmentLoadout();bGameStarted=true;
 }
 bApplying=false;
 if(!OK) { StatusText=TEXT("기존 기록을 읽을 수 없어요.");if(Menu)Menu->Refresh();return false; }
 if(auto* B=Cast<ABBSBBunniarCompanion>(Player->FieldActions->BunniarEquipment))B->EndOpeningRest();
 CloseMenu();SaveNow(TEXT("기존 진행 이어서"),true);return true;
}
void UBBSBSessionSubsystem::OpenMenu()
{
 if(!Player.IsValid() || bLoading)return;
 if(bMenuOpen) { if(bGameStarted)CloseMenu();return; }
 if(Player->Hunting->bInventoryOpen || Player->WorldMap->bMapOpen || Player->Interaction->bDialogueActive || Player->Respawn->IsSequenceActive())return;
 auto* PC=Cast<APlayerController>(Player->GetController());if(!PC)return;RefreshSlots();
 bPreviousCursor=PC->bShowMouseCursor;PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bOwnMoveIgnore=bOwnLookIgnore=true;
 bOwnPause=!GetWorld()->IsPaused() && UGameplayStatics::SetGamePaused(GetWorld(),true);
 Menu=CreateWidget<UBBSBSessionMenu>(PC,UBBSBSessionMenu::StaticClass());Menu->Session=this;Menu->AddToViewport(2000);bMenuOpen=true;
 PC->bShowMouseCursor=true;FInputModeUIOnly Input;Input.SetWidgetToFocus(Menu->TakeWidget());Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Input);Menu->Refresh();
}
void UBBSBSessionSubsystem::CloseMenu()
{
 if(Menu)Menu->RemoveFromParent();Menu=nullptr;
 if(auto* PC=Player.IsValid() ? Cast<APlayerController>(Player->GetController()):nullptr)
 {
  if(bOwnMoveIgnore)PC->SetIgnoreMoveInput(false);if(bOwnLookIgnore)PC->SetIgnoreLookInput(false);
  PC->bShowMouseCursor=bPreviousCursor;FInputModeGameOnly Input;PC->SetInputMode(Input);
 }
 if(bOwnPause)UGameplayStatics::SetGamePaused(GetWorld(),false);
 bMenuOpen=bOwnPause=bOwnMoveIgnore=bOwnLookIgnore=false;NextCheck=FPlatformTime::Seconds()+FMath::Max(.1f,AutoSaveIntervalSeconds);
}
