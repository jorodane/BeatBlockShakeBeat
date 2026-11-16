#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Field/BBSBFieldActions.h"
#include "Character/BBSBTraversal.h"
#include "Puzzle/BBSBPuzzleField.h"
#include "Save/BBSBSession.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
// Create an isolated PIE fixture. Exercise PlayerInput events, not direct
// gesture calls: Slate sends IE_DoubleClick for a rapid second mouse-down.
class FBunniarRapidInputExercise : public IAutomationLatentCommand
{
public:
 explicit FBunniarRapidInputExercise(FAutomationTestBase& InTest):Test(InTest),Started(FPlatformTime::Seconds()) {}
 bool Update() override
 {
  if(FPlatformTime::Seconds()-Started>25) { Test.AddError(TEXT("Rapid input exercise timed out"));return Finish(); }
  if(Stage<=0)
  {
   UWorld* World=nullptr;
   for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::PIE) { World=Context.World();break; }
   auto* Session=World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBBSBSessionSubsystem>():nullptr;
   if(!Session) return false;
   if(Stage==0)
   {
    Session->SaveNamespace=TEXT("BBSBRapidQA_20261116");Session->RefreshSlots();
    if(!Session->LoadSlot(0)) { Test.AddError(TEXT("Seed the isolated QA checkpoint before running the input test"));return true; }
    Stage=-1;return false;
   }
   if(!Session->bGameStarted || Session->bLoading) return false;
   if(Session->SaveNamespace!=TEXT("BBSBRapidQA_20261116")) { Test.AddError(TEXT("Unexpected save namespace"));return true; }
   PC=World->GetFirstPlayerController();
   auto* Player=PC.IsValid() ? Cast<ABBSBCharacter>(PC->GetPawn()):nullptr;
   Actions=Player ? Player->FieldActions:nullptr;
   if(!Player || !Actions.IsValid()) { Test.AddError(TEXT("Playable character is available"));return true; }
   for(TActorIterator<ABBSBPuzzleField> It(World);It;++It) It->BoundaryRadii=FVector2D(1000000,1000000);
   Player->Traversal->SetComponentTickEnabled(false);
   Player->GetCharacterMovement()->StopMovementImmediately();
   Player->SetActorLocation(FVector(100000,100000,100160),false,nullptr,ETeleportType::TeleportPhysics);
   Player->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
   PC->bShowMouseCursor=false;PC->SetControlRotation(FRotator(50,0,0));
   if(!Actions->RequestEquipment(0)) { Test.AddError(TEXT("Bunniar equipment is available"));return true; }
   ArmAt=FPlatformTime::Seconds()+.5;Stage=4;return false;
  }
  if(!PC.IsValid() || !Actions.IsValid()) { Test.AddError(TEXT("Fixture disappeared"));return Finish(); }
  if(Stage==4)
  {
   if(FPlatformTime::Seconds()<ArmAt) return false;
   Buddy=Cast<ABBSBBunniarCompanion>(Actions->InteractionEquipment());
   if(!Buddy.IsValid()) { Test.AddError(TEXT("Bunniar equips before pointer input"));return true; }
   Before=Actions->GestureCounts[0];Send(IE_Pressed);Stage=1;return false;
  }
  if(!Buddy.IsValid()) { Test.AddError(TEXT("Bunniar disappeared"));return Finish(); }
  if(Stage==1)
  {
   const FString Label=FString::Printf(TEXT("Rapid send %d (including double-click events)"),Sent+1);
   Test.TestTrue(*Label,Actions->IsGestureChannelHeld(0) && Buddy->bAirHovering);
   Test.TestEqual(TEXT("Every fresh press emits exactly one Beat"),Actions->GestureCounts[0],Before+Sent+1);
   Send(IE_Released);
   if(++Sent<6) Send(Sent%2 ? IE_DoubleClick:IE_Pressed);
   else Stage=2;
   return false;
  }
  if(Stage==2)
  {
   Test.TestFalse(TEXT("Final release clears held input"),Actions->IsGestureChannelHeld(0));
   Test.TestFalse(TEXT("Final release clears the air placement"),Buddy->bAirHovering);
   // A new action revision must invalidate the equipment-command exemption.
   Actions->InterruptAction(EBBSBActionState::Hit,.5f);Before=Actions->GestureCounts[0];
   Send(IE_DoubleClick);Stage=3;return false;
  }
  Test.TestFalse(TEXT("Hit reaction still blocks a fresh send"),Actions->IsGestureChannelHeld(0));
  Test.TestEqual(TEXT("Rejected input emits no successful Beat"),Actions->GestureCounts[0],Before);
  return Finish();
 }
private:
 FAutomationTestBase& Test;
 TWeakObjectPtr<APlayerController> PC;
 TWeakObjectPtr<UBBSBFieldActionComponent> Actions;
 TWeakObjectPtr<ABBSBBunniarCompanion> Buddy;
 double Started,ArmAt=0;
 int32 Stage=0,Sent=0,Before=0;
 void Send(EInputEvent Event)
 { if(PC.IsValid()) PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton,Event,Event==IE_Released ? 0.f:1.f)); }
 bool Finish() { Send(IE_Released);return true; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBBunniarRapidInputTest,"BBSB.Field.BunniarRapidInput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBBunniarRapidInputTest::RunTest(const FString& Parameters)
{
 UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World():nullptr;
 if(!World || !World->GetName().Contains(TEXT("Stage01_BunniarHalfStepCaravan")))
 { AddError(TEXT("Open Stage01_BunniarHalfStepCaravan before the integration test"));return false; }
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FBunniarRapidInputExercise(*this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
