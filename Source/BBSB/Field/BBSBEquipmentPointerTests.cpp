#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Field/BBSBFieldActions.h"
#include "Puzzle/BBSBPuzzleField.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Widgets/SWindow.h"

namespace
{
// Exercise Slate -> viewport -> PlayerInput axes -> shared weapon recognizer.
// FieldGestureMotion and direct calls to the puzzle interface are deliberately unused.
class FBunniarPointerExercise : public IAutomationLatentCommand
{
public:
 explicit FBunniarPointerExercise(FAutomationTestBase& InTest,UWorld* InWorld)
  : Test(InTest),World(InWorld),Started(FPlatformTime::Seconds()) {}

 virtual bool Update() override
 {
  const double Now=FPlatformTime::Seconds();
  if (Now-Started>35) { Check(false,TEXT("Pointer exercise finishes within 35 seconds"));return Finish(); }
  if (Now<NextAt) return false;
  if (!World.IsValid() && Stage==0)
   for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::PIE) { World=Context.World();break; }
  if (!World.IsValid()) { Check(false,TEXT("PIE world remains available"));return Finish(); }
  if(Stage<=2 || Stage==20)
  {
   // PIE can restore the floating message log over the scene. It is a diagnostic
   // window, not part of the input fixture; retain every gameplay assertion.
   TArray<TSharedRef<SWindow>> Windows;FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
   bool ClosedLog=false;
   for(const auto& Window:Windows)
    if(const FString Title=Window->GetTitle().ToString();Title==TEXT("Message Log") || Title==TEXT("메시지 로그")) { Window->RequestDestroyWindow();ClosedLog=true; }
   if(ClosedLog) { NextAt=Now+.05;return false; }
  }
  switch (Stage)
  {
  case 0:
   PC=World->GetFirstPlayerController();Player=PC.IsValid() ? Cast<ABBSBCharacter>(PC->GetPawn()):nullptr;
   if (!Player.IsValid() || !World->GetGameViewport()) { Check(false,TEXT("Playable meadow and viewport are available"));return Finish(); }
   Actions=Player->FieldActions;Viewport=World->GetGameViewport();Scene=Viewport->GetGameViewport();
   if (!Scene) { Check(false,TEXT("PIE scene viewport is available"));return Finish(); }
   OriginalTransform=Player->GetActorTransform();OriginalView=PC->GetControlRotation();OriginalEquipment=Actions->EquippedEquipmentId();
   OriginalCursor=PC->bShowMouseCursor;OriginalCapture=Viewport->GetMouseCaptureMode();OriginalLock=Viewport->GetMouseLockMode();
   // Establish normal viewport focus once, before any equipment action.
   Scene->SetUserFocus(true);Scene->CaptureMouse(true);
   Check(Actions->RequestEquipment(0),TEXT("Bunniar uses the normal equipment selection"));
   Advance(1,.3);return false;
  case 1:
   Buddy=Cast<ABBSBBunniarCompanion>(Actions->FieldEquipment);
   if (!Buddy.IsValid()) { Check(false,TEXT("Bunniar equips"));return Finish(); }
   BeginCase();return false;
  case 2:
  {
   const FVector Point=Case==3 ? Station->Payload->GetComponentLocation():Station->GetActorLocation()+FVector(-33,0,110);
   const FRotator Wanted=(Point-PC->PlayerCameraManager->GetCameraLocation()).Rotation();
   const FRotator Difference=(Wanted-PC->PlayerCameraManager->GetCameraRotation()).GetNormalized();
   PC->SetControlRotation((PC->GetControlRotation()+Difference).GetNormalized());
   if (++AimFrames<18) return false;
   const FGeometry& Geometry=Scene->GetCachedGeometry();
   Position=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5f);
   FSlateApplication::Get().SetCursorPos(Position);
   // Warping the OS cursor does not itself enter the viewport or update its cached mouse position.
   // Send the same hover event that precedes a human click, then let Slate process a frame.
   FSlateApplication::Get().ProcessMouseMoveEvent(FPointerEvent(0,Position,Position,TSet<FKey>(),FKey(),0,FModifierKeysState()));
   Advance(20,.05);return false;
  }
  case 20:
  {
   float MouseX=0,MouseY=0;
   const bool Inside=PC->GetMousePosition(MouseX,MouseY);
   Test.AddInfo(FString::Printf(TEXT("Pointer hover: viewport=%dx%d, mouse=(%.1f,%.1f), inside=%d, capture=%d"),Scene->GetSizeXY().X,Scene->GetSizeXY().Y,MouseX,MouseY,Inside,Scene->HasMouseCapture()));
   ViewAtPress=PC->GetControlRotation();
   for (int32 I=0;I<5;++I) CountsBefore[I]=Actions->GestureCounts[I];
   PayloadBefore=Station->Payload->GetComponentLocation();
   PointerButton(true);Advance(3,.15);return false;
  }
  case 3:
   Check(Buddy->bSurfaceAttached && Buddy->SurfaceComponent && Buddy->SurfaceComponent->GetOwner()==Station.Get(),TEXT("Pointer-down attaches to the aimed station"));
   Check(PC->bShowMouseCursor==OriginalCursor,TEXT("Attachment preserves the weapon cursor state"));
   Check(Viewport->GetMouseCaptureMode()==OriginalCapture && Viewport->GetMouseLockMode()==OriginalLock,TEXT("Attachment preserves the weapon input mode"));
   Check(Scene->HasMouseCapture(),TEXT("Attachment keeps actual viewport mouse capture"));
   Check(Actions->GetLookInputScale()==0,TEXT("Attachment holds the view while still accepting gesture motion"));
   if (!Buddy->bSurfaceAttached || !Scene->HasMouseCapture()) return Finish();
   Advance(4,.35);return false;
  case 4:
   Check(Actions->GestureCounts[1]>CountsBefore[1],TEXT("Real held button emits Hold"));
   MotionIndex=0;Advance(5);return false;
  case 5:
   if (Case==0) PointerMotion(FVector2D(0,50));
   else if (Case==1) PointerMotion(FVector2D(MotionIndex%2 ? -65:65,0));
   else if (Case==2) PointerMotion(FVector2D(65,0));
   else PointerMotion(FVector2D(45,-40));
   if (++MotionIndex<(Case==0 ? 4:Case==1 ? 10:Case==2 ? 3:1)) Advance(5,Case==2 ? .015:.045);
   else Advance(6,Case==2 ? .015:.15);
   return false;
  case 6:
   Check((PC->GetControlRotation()-ViewAtPress).GetNormalized().IsNearlyZero(.1f),TEXT("Gesture motion does not rotate the view"));
   if (Case==0) Check(Station->bSolved && Station->Drag.Y>=160,TEXT("Viewport drag physically lowers the ladder"));
   if (Case==1) Check(Actions->GestureCounts[2]>CountsBefore[2] && Station->bLoose,TEXT("Viewport reversals emit Shake and detach the object"));
   if (Case==3) Check(Station->bCarrying && FVector::Dist(PayloadBefore,Station->Payload->GetComponentLocation())>20,TEXT("Viewport drag physically moves a held object"));
   PointerButton(false);Advance(7,Case==2 ? 1.2:.25);return false;
  case 7:
   if (Case==2)
   {
    Check(Actions->GestureCounts[3]>CountsBefore[3],TEXT("Fast viewport motion and release emit Flick"));
    Check(Station->bSolved,TEXT("Directional Flick reaches the puzzle destination"));
   }
   else Check(Actions->GestureCounts[4]>CountsBefore[4],TEXT("Settled viewport release emits Dive"));
   Check(!Buddy->bSurfaceAttached && !Actions->bEquipmentPointerCaptured && !Actions->IsGestureCaptured(),TEXT("Mouse-up releases equipment and gesture state"));
   Check(PC->bShowMouseCursor==OriginalCursor && Viewport->GetMouseCaptureMode()==OriginalCapture,TEXT("Mouse-up preserves the weapon cursor and input mode"));
   Station->ResetPuzzle();
   if (++Case<4) { BeginCase();return false; }
   return Finish();
  }
  return false;
 }

private:
 FAutomationTestBase& Test;
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<APlayerController> PC;
 TWeakObjectPtr<ABBSBCharacter> Player;
 TWeakObjectPtr<UBBSBFieldActionComponent> Actions;
 TWeakObjectPtr<ABBSBBunniarCompanion> Buddy;
 TWeakObjectPtr<ABBSBPuzzleStation> Station;
 UGameViewportClient* Viewport=nullptr;
 FSceneViewport* Scene=nullptr;
 FTransform OriginalTransform;
 FRotator OriginalView,ViewAtPress;
 FVector PayloadBefore;
 FVector2D Position;
 double Started,NextAt=0;
 int32 Stage=0,Case=0,AimFrames=0,MotionIndex=0,OriginalEquipment=4,Failures=0;
 int32 CountsBefore[5]={};
 bool OriginalCursor=false,bButtonDown=false;
 EMouseCaptureMode OriginalCapture=EMouseCaptureMode::NoCapture;
 EMouseLockMode OriginalLock=EMouseLockMode::DoNotLock;
 TArray<FString> Checks;

 void Advance(int32 Next,double Delay=0) { Stage=Next;NextAt=FPlatformTime::Seconds()+Delay; }
 void Check(bool Passed,const TCHAR* Message)
 {
  Test.TestTrue(FString::Printf(TEXT("Case %d: %s"),Case,Message),Passed);
  if (!Passed) ++Failures;
  Checks.Add(FString::Printf(TEXT("{\"case\":%d,\"name\":\"%s\",\"passed\":%s}"),Case,Message,Passed ? TEXT("true"):TEXT("false")));
 }
 void BeginCase()
 {
  const EBBSBPuzzleKind Kinds[]={EBBSBPuzzleKind::Ladder,EBBSBPuzzleKind::ShakeLoose,EBBSBPuzzleKind::FlickRight,EBBSBPuzzleKind::Balance};
  Station=nullptr;
  for (TActorIterator<ABBSBPuzzleStation> It(World.Get());It;++It) if (It->Kind==Kinds[Case]) { Station=*It;break; }
  if (!Station.IsValid()) { Check(false,TEXT("Required puzzle station exists"));Stage=99;return; }
  Station->ResetPuzzle();
  const double Y=Case==3 ? Station->Payload->GetRelativeLocation().Y:0;
  Player->GetCharacterMovement()->StopMovementImmediately();
  Player->SetActorLocation(Station->GetActorLocation()+FVector(-380,Y,110),false,nullptr,ETeleportType::TeleportPhysics);
  PC->SetControlRotation(FRotator(-8,0,0));AimFrames=0;Advance(2,.25);
 }
 void PointerButton(bool Down)
 {
  TSet<FKey> Buttons;if (Down) Buttons.Add(EKeys::LeftMouseButton);
  const FPointerEvent Event(0,Position,Position,Buttons,EKeys::LeftMouseButton,0,FModifierKeysState());
  auto& Slate=FSlateApplication::Get();
  if (Down)
  {
   const auto Window=Slate.GetActiveTopLevelWindow();
   Slate.ProcessMouseButtonDownEvent(Window.IsValid() ? Window->GetNativeWindow():nullptr,Event);
  }
  else Slate.ProcessMouseButtonUpEvent(Event);
  bButtonDown=Down;
 }
 void PointerMotion(FVector2D Delta)
 {
  TSet<FKey> Buttons;Buttons.Add(EKeys::LeftMouseButton);
  const FVector2D Previous=Position;Position+=Delta;
  FSlateApplication::Get().ProcessMouseMoveEvent(FPointerEvent(0,Position,Previous,Buttons,FKey(),0,FModifierKeysState()));
 }
 bool Finish()
 {
  if (bButtonDown) PointerButton(false);
  if (Buddy.IsValid()) Buddy->ReleaseSurface();
  if (Station.IsValid()) Station->ResetPuzzle();
  if (Player.IsValid()) { Player->SetActorTransform(OriginalTransform,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation(OriginalView);Actions->RequestEquipment(OriginalEquipment); }
  const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation"));
  IFileManager::Get().MakeDirectory(*Directory,true);
  const FString Json=FString::Printf(TEXT("{\"complete\":%s,\"input\":\"Slate pointer events through viewport and PlayerInput; no synthetic gesture motion\",\"checks\":[%s]}"),Failures ? TEXT("false"):TEXT("true"),*FString::Join(Checks,TEXT(",\n")));
  FFileHelper::SaveStringToFile(Json,*FPaths::Combine(Directory,TEXT("BunniarPointer.json")));
  return true;
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBEquipmentPointerSlateTest,"BBSB.Field.Equipment.BunniarSharedPointer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBEquipmentPointerSlateTest::RunTest(const FString& Parameters)
{
 UWorld* EditorWorld=GEditor ? GEditor->GetEditorWorldContext().World():nullptr;
 if (!EditorWorld || !EditorWorld->GetName().Contains(TEXT("BunniarMeadow"))) { AddError(TEXT("Open L_BunniarMeadow before this integration test."));return false; }
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
 ADD_LATENT_AUTOMATION_COMMAND(FBunniarPointerExercise(*this,nullptr));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
#endif
