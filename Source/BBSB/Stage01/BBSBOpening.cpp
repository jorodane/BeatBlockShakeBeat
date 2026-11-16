#include "Stage01/BBSBOpening.h"
#include "Stage01/BBSBStage01.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "Character/BBSBAimGaze.h"
#include "Character/BBSBTraversal.h"
#include "Field/BBSBFieldActions.h"
#include "Field/BBSBWeaponAnimation.h"
#include "Save/BBSBSession.h"
#include "UI/BBSBGuides.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "UI/BBSBPaperTheme.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"
#include "Materials/MaterialInterface.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UBBSBOpeningSkipPrompt::RebuildWidget()
{
 SetVisibility(ESlateVisibility::HitTestInvisible);
 auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
 auto* Label=WidgetTree->ConstructWidget<UTextBlock>();Label->SetText(FText::FromString(TEXT("[Enter] 컷씬 건너뛰기")));
 FSlateFontInfo Font=FCoreStyle::GetDefaultFontStyle("Regular",17);Font.OutlineSettings.OutlineSize=2;Font.OutlineSettings.OutlineColor=BBSBPaper::Ink();
 Label->SetFont(Font);Label->SetColorAndOpacity(BBSBPaper::Paper());
 auto* LabelSlot=Root->AddChildToCanvas(Label);LabelSlot->SetAnchors(FAnchors(1,1));LabelSlot->SetAlignment(FVector2D(1,1));LabelSlot->SetPosition(FVector2D(-38,-32));LabelSlot->SetAutoSize(true);
 return Super::RebuildWidget();
}

ABBSBStage01Opening::ABBSBStage01Opening()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("OpeningOrigin")));SetActorEnableCollision(false);
}
UStaticMeshComponent* ABBSBStage01Opening::Prop(FName Name,FVector Position,FVector Scale,const TCHAR* Material,const TCHAR* Shape)
{
 auto* C=NewObject<UStaticMeshComponent>(this,Name);AddInstanceComponent(C);C->SetupAttachment(RootComponent);C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Shape));C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->RegisterComponent();C->SetWorldLocation(Position);C->SetWorldScale3D(Scale);if(auto* M=LoadObject<UMaterialInterface>(nullptr,Material))C->SetMaterial(0,M);Props.Add(C);return C;
}
bool ABBSBStage01Opening::Start(ABBSBStage01Director* Story,ABBSBCharacter* Character)
{
 if(bActive || !Story || !Character)return false;
 Presentation=LoadObject<UBBSBOpeningPresentation>(nullptr,TEXT("/Game/BBSB/AnimationAuthoring/Opening/DA_Opening.DA_Opening"));
 if(!Presentation || !Presentation->Writing || !Presentation->Tap || !Presentation->Bandage || !Presentation->Stand)return false;
 Player=Character;Director=Story;Buddy=Cast<ABBSBBunniarCompanion>(Player->FieldActions->BunniarEquipment);
 auto* PC=Cast<APlayerController>(Player->GetController());if(!PC || !Buddy)return false;
 bActive=true;Squeaks=0;bBandageApplied=false;Director->Caption.Empty();
 if(auto* G=UBBSBGuideSubsystem::Get(this))G->bOpeningActive=true;
 // The authored bookstand is 155 cm above this carriage's floor.
 DeskOrigin=Director->BookstandPose.GetLocation()-FVector(0,0,155);
 PreviousView=PC->GetViewTarget();bSavedCursor=PC->bShowMouseCursor;PC->bShowMouseCursor=false;
 if(auto* HUD=PC->GetHUD()) { bSavedHUD=HUD->bShowHUD;HUD->bShowHUD=false; }
 PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bOwnMoveIgnore=bOwnLookIgnore=true;PC->RotationInput=FRotator::ZeroRotator;
 bSavedEquipmentBlock=Player->FieldActions->bEquipmentInputBlocked;Player->FieldActions->ResetActionState();Player->FieldActions->bEquipmentInputBlocked=true;Player->FieldActions->bStoryPresentationHidden=true;
 bSavedGaze=Player->AimGaze->bEnableHeadGaze;Player->AimGaze->bEnableHeadGaze=false;
 Player->Traversal->EndGlide();Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->DisableMovement();Player->ConsumeMovementInputVector();
 Player->SetActorLocationAndRotation(DeskOrigin+Presentation->SeatOffset,FRotator(0,Presentation->FacingYaw,0),false,nullptr,ETeleportType::TeleportPhysics);
 auto* Mesh=Player->GetMesh();SavedMeshTransform=Mesh->GetRelativeTransform();SavedAnimClass=Mesh->GetAnimClass();SavedAnimationMode=uint8(Mesh->GetAnimationMode());bSavedPostProcess=Mesh->GetDisablePostProcessBlueprint();
 // Switching animation modes destroys the instance. Retain the gameplay root-motion
 // policy as well as its class, otherwise command montages can launch the capsule.
 if(const auto* Anim=Mesh->GetAnimInstance()) { SavedRootMotionMode=uint8(Anim->RootMotionMode.GetValue());bSavedRootMotionMode=true; }
 Mesh->SetDisablePostProcessBlueprint(true);Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
 SceneCamera=GetWorld()->SpawnActor<ACameraActor>();SceneCamera->GetCameraComponent()->SetFieldOfView(Presentation->CameraFOV);SceneCamera->GetCameraComponent()->bConstrainAspectRatio=false;UpdateCamera(0);PC->SetViewTarget(SceneCamera);PC->PlayerCameraManager->StartCameraFade(1,0,.45f,FLinearColor::Black,false,false);
 Controls=NewObject<UInputComponent>(this,TEXT("OpeningClickControls"));Controls->RegisterComponent();Controls->Priority=100;Controls->bBlockInput=true;
 Controls->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&ABBSBStage01Opening::Squeak);Controls->BindKey(EKeys::LeftMouseButton,IE_DoubleClick,this,&ABBSBStage01Opening::Squeak);
 Controls->BindKey(EKeys::Enter,IE_Pressed,this,&ABBSBStage01Opening::Skip);Controls->BindKey(EKeys::Gamepad_Special_Right,IE_Pressed,this,&ABBSBStage01Opening::Skip);PC->PushInputComponent(Controls);
 SkipPrompt=CreateWidget<UBBSBOpeningSkipPrompt>(PC,UBBSBOpeningSkipPrompt::StaticClass());if(SkipPrompt)SkipPrompt->AddToViewport(1000);
 // Capture Escape above the scene blocker so the ordinary pause menu remains available.
 auto& Pause=Controls->BindKey(EKeys::Escape,IE_Pressed,UBBSBSessionSubsystem::Get(this),&UBBSBSessionSubsystem::OpenMenu);Pause.bExecuteWhenPaused=true;
 Prop(TEXT("OpenNotebook"),DeskOrigin+FVector(34,-50,85),FVector(.64,.42,.012),TEXT("/Game/BBSB/Stage01/Materials/MI_Paper.MI_Paper"));
 Prop(TEXT("NotebookSpine"),DeskOrigin+FVector(34,-50,86),FVector(.018,.42,.014),TEXT("/Game/BBSB/Stage01/Materials/MI_Rose.MI_Rose"));
 for(int I=0;I<6;++I)Prop(FName(*FString::Printf(TEXT("InkLine%d"),I)),DeskOrigin+FVector(42,-64+I*4,86),FVector(.25,.006,.002),TEXT("/Game/BBSB/Stage01/Materials/MI_Dark.MI_Dark"));
 Pen=Prop(TEXT("WritingPen"),DeskOrigin+FVector(30,-55,92),FVector(.018,.018,.14),TEXT("/Game/BBSB/Stage01/Materials/MI_Dark.MI_Dark"),TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 FTransform Rest=Director->BookstandPose;Rest.SetLocation(DeskOrigin+Presentation->BunniarOffset);Rest.SetScale3D(FVector(.12));Buddy->BeginOpeningRest(Rest);Buddy->SetOpeningBandaged(false);
 ChangeBeat(EBBSBOpeningBeat::Writing);return true;
}
void ABBSBStage01Opening::ChangeBeat(EBBSBOpeningBeat Next)
{
 Beat=Next;BeatStarted=GetWorld()->GetTimeSeconds();bWritingReady=false;TapCueCount=0;
 UAnimSequence* Clip=Beat==EBBSBOpeningBeat::Writing ? Presentation->Writing:Beat==EBBSBOpeningBeat::Tap ? Presentation->Tap:Beat==EBBSBOpeningBeat::Bandage ? Presentation->Bandage:Presentation->Stand;
 Player->GetMesh()->PlayAnimation(Clip,Beat==EBBSBOpeningBeat::Writing);NextScribble=BeatStarted+.45;
}
void ABBSBStage01Opening::Squeak()
{
 if(!bActive || Beat!=EBBSBOpeningBeat::Writing || !bWritingReady || GetWorld()->IsPaused())return;
 ++Squeaks;PlaySoundCue(0);ChangeBeat(Squeaks==1 ? EBBSBOpeningBeat::Tap:Squeaks==2 ? EBBSBOpeningBeat::Bandage:EBBSBOpeningBeat::Stand);
}
void ABBSBStage01Opening::Skip()
{
 if(!bActive || !Player || !Presentation || GetWorld()->IsPaused())return;
 // Commit the same story outcome and exit position as the completed performance.
 bBandageApplied=true;if(Buddy)Buddy->SetOpeningBandaged(true);
 Player->SetActorLocation(DeskOrigin+Presentation->StandExitOffset,false,nullptr,ETeleportType::TeleportPhysics);
 Player->StopJumping();Player->ConsumeMovementInputVector();Finish(true);
}
void ABBSBStage01Opening::Tick(float DT)
{
 Super::Tick(DT);if(!bActive || !Player || !Buddy || !Presentation)return;
 const float Age=GetWorld()->GetTimeSeconds()-BeatStarted;UpdateCamera(Age);UpdateBuddy(Age);
 if(auto* PC=Cast<APlayerController>(Player->GetController()))if(auto* HUD=PC->GetHUD())HUD->bShowHUD=false;
 Player->ConsumeMovementInputVector();
 if(Pen && !(Beat==EBBSBOpeningBeat::Stand && Age>.62f))
 {
  const FVector Hand=Player->GetMesh()->GetSocketLocation(TEXT("hand_r"));Pen->SetWorldLocation(Hand+FVector(0,0,-3));Pen->SetWorldRotation(FRotator(20,20,0));
 }
 if(Beat==EBBSBOpeningBeat::Writing)
 {
  bWritingReady=Age>=.55f;
  if(GetWorld()->GetTimeSeconds()>NextScribble) { PlaySoundCue(1);NextScribble=GetWorld()->GetTimeSeconds()+1.6; }
 }
 else if(Beat==EBBSBOpeningBeat::Tap)
 {
  if(Age>.35f && TapCueCount==0) { PlaySoundCue(2);++TapCueCount; }
  if(Age>.62f && TapCueCount==1) { PlaySoundCue(2);++TapCueCount; }
  if(Age>=Presentation->Tap->GetPlayLength())ChangeBeat(EBBSBOpeningBeat::Writing);
 }
 else if(Beat==EBBSBOpeningBeat::Bandage)
 {
  if(Age>.78f && !bBandageApplied) { bBandageApplied=true;Buddy->SetOpeningBandaged(true);PlaySoundCue(2); }
  if(Age>=Presentation->Bandage->GetPlayLength())ChangeBeat(EBBSBOpeningBeat::Writing);
 }
 else if(Beat==EBBSBOpeningBeat::Stand)
 {
  if(Age>.62f && Pen) { Pen->SetWorldLocation(DeskOrigin+FVector(6,-55,87));Pen->SetWorldRotation(FRotator(90,20,0)); }
  if(Age>.8f)
  {
   const float T=FMath::SmoothStep(.8f,2.45f,Age);Player->SetActorLocation(FMath::Lerp(DeskOrigin+Presentation->SeatOffset,DeskOrigin+Presentation->StandExitOffset,T),false,nullptr,ETeleportType::TeleportPhysics);
  }
  if(Age>.25f && !bStartedTransition) { PlaySoundCue(3);bStartedTransition=true; }
  if(Age>=Presentation->Stand->GetPlayLength())Finish(true);
 }
}
void ABBSBStage01Opening::UpdateBuddy(float Age)
{
 FTransform Pose=Director->BookstandPose;Pose.SetScale3D(FVector(.12));Pose.SetRotation(FRotator(0,Presentation->BunniarYaw,0).Quaternion());FVector At=DeskOrigin+Presentation->BunniarOffset;
 // The little involuntary hop is retained even after the mouth is taped shut.
 if(Beat!=EBBSBOpeningBeat::Writing)At.Z+=FMath::Sin(FMath::Clamp(Age/.22f,0.f,1.f)*PI)*4;
 if(Beat==EBBSBOpeningBeat::Tap)At.Z+=FMath::Sin(FMath::Clamp((Age-.32f)/.5f,0.f,1.f)*2*PI)*2;
 if(Beat==EBBSBOpeningBeat::Stand && Age>1.1f)
 {
  const float T=FMath::SmoothStep(1.1f,1.65f,Age);At=FMath::Lerp(At,Player->GetMesh()->GetSocketLocation(TEXT("hand_l"))+FVector(0,0,5),T);
 }
 Pose.SetLocation(At);Buddy->OpeningRestTransform=Pose;Buddy->SetActorTransform(Pose);
}
void ABBSBStage01Opening::UpdateCamera(float Age)
{
 if(!SceneCamera || !Presentation)return;
 // Preserve the scene-texture producer required by the shared outline compositor.
 SceneCamera->GetCameraComponent()->PostProcessSettings=Player->GetFollowCamera()->PostProcessSettings;
 SceneCamera->GetCameraComponent()->PostProcessBlendWeight=Player->GetFollowCamera()->PostProcessBlendWeight;
 FVector At=DeskOrigin+Presentation->CameraOffset,Focus=DeskOrigin+Presentation->CameraLookAt;
 FTransform Curve;if(Presentation->CameraSequence && UBBSBWeaponAnimationSet::SampleTransform(Presentation->CameraSequence,TEXT("CameraMotion"),FMath::Clamp(Age/3.f,0.f,1.f),Curve))At+=Curve.GetLocation();
 if(Beat==EBBSBOpeningBeat::Stand) { const float T=FMath::SmoothStep(1.f,2.6f,Age);At+=FVector(35,-65,30)*T;Focus+=FVector(35,-65,32)*T; }
 SceneCamera->SetActorLocationAndRotation(At,(Focus-At).Rotation());
}
void ABBSBStage01Opening::PlaySoundCue(int Kind)
{
 // Short authored procedural cues keep the prototype self-contained and editable in code.
 auto* Wave=NewObject<USoundWaveProcedural>(this);Wave->SetSampleRate(24000);Wave->NumChannels=1;const float Duration=Kind==0 ? .19f:Kind==1 ? .18f:Kind==2 ? .055f:.38f;Wave->Duration=Duration;
 const int Count=FMath::RoundToInt(Duration*24000);TArray<int16> Samples;Samples.SetNumUninitialized(Count);float Phase=0;
 for(int I=0;I<Count;++I)
 {
  const float T=float(I)/24000,Envelope=FMath::Sin(PI*I/Count);float Value=0;
  if(Kind==0) { const float Hz=1250.f+900.f*FMath::Sin(T/Duration*PI);Phase+=2*PI*Hz/24000;Value=(FMath::Sin(Phase)+.25f*FMath::Sin(Phase*2.07f))*.21f; }
  else if(Kind==2)Value=FMath::Sin(T*2*PI*370)*FMath::Exp(-T*65)*.3f;
  else Value=(FMath::Sin(I*12.9898f)*FMath::Sin(I*4.1414f))*(Kind==1 ? .025f:.045f);
  Samples[I]=FMath::RoundToInt(FMath::Clamp(Value*Envelope,-1.f,1.f)*32767);
 }
 Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Count*sizeof(int16));
 if(auto* Audio=UGameplayStatics::SpawnSound2D(this,Wave,.65f,1.f,0,nullptr,false,false)) { Sounds.Add(Audio);Audio->FadeOut(Duration+.02f,0); }
 for(int I=Sounds.Num()-1;I>=0;--I)if(!IsValid(Sounds[I]) || !Sounds[I]->IsPlaying()) { if(IsValid(Sounds[I]))Sounds[I]->DestroyComponent();Sounds.RemoveAt(I); }
}
void ABBSBStage01Opening::Finish(bool Completed)
{
 if(SkipPrompt) { SkipPrompt->RemoveFromParent();SkipPrompt=nullptr; }
 if(!bActive)return;bActive=false;Beat=EBBSBOpeningBeat::Complete;
 if(auto* G=UBBSBGuideSubsystem::Get(this))G->bOpeningActive=false;
 if(Player)
 {
  auto* PC=Cast<APlayerController>(Player->GetController());
  if(PC)
  {
   if(Controls)PC->PopInputComponent(Controls);if(bOwnMoveIgnore)PC->SetIgnoreMoveInput(false);if(bOwnLookIgnore)PC->SetIgnoreLookInput(false);bOwnMoveIgnore=bOwnLookIgnore=false;
   PC->bShowMouseCursor=bSavedCursor;if(auto* HUD=PC->GetHUD())HUD->bShowHUD=bSavedHUD;
   if(Completed)
   {
    // Match a sensible outgoing gameplay view before blending back to its spring arm.
    PC->SetControlRotation(FRotator(-4,70,0));PC->SetViewTargetWithBlend(Player,Presentation->CameraReturnSeconds,VTBlend_Cubic);
   }
   else if(PreviousView.IsValid())PC->SetViewTarget(PreviousView.Get());
  }
  auto* Mesh=Player->GetMesh();Mesh->SetAnimationMode(EAnimationMode::Type(SavedAnimationMode));if(SavedAnimationMode==uint8(EAnimationMode::AnimationBlueprint))Mesh->SetAnimInstanceClass(SavedAnimClass);Mesh->SetRelativeTransform(SavedMeshTransform);Mesh->SetDisablePostProcessBlueprint(bSavedPostProcess);
  if(bSavedRootMotionMode)if(auto* Anim=Mesh->GetAnimInstance())Anim->SetRootMotionMode(ERootMotionMode::Type(SavedRootMotionMode));
  Player->AimGaze->bEnableHeadGaze=bSavedGaze;Player->FieldActions->bEquipmentInputBlocked=bSavedEquipmentBlock;Player->FieldActions->bStoryPresentationHidden=false;Player->FieldActions->ResetActionState();
  Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
 }
 if(Buddy)Buddy->EndOpeningRest();
 if(Completed)
 {
  if(Director)Director->Caption.Empty();if(auto* S=UBBSBSessionSubsystem::Get(this))S->RequestAutoSave(TEXT("도입 완료"));
  // Allow the view-target blend to finish before destroying the outgoing camera.
  SetLifeSpan(Presentation->CameraReturnSeconds+.2f);
 }
}
void ABBSBStage01Opening::EndPlay(const EEndPlayReason::Type Reason)
{
 Finish(false);if(SceneCamera)SceneCamera->Destroy();for(UAudioComponent* A:Sounds)if(IsValid(A))A->DestroyComponent();Super::EndPlay(Reason);
}
