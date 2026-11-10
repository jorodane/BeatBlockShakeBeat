#include "Character/BBSBTraversal.h"
#include "BBSBCharacter.h"
#include "Combat/BBSBCombat.h"
#include "Field/BBSBFieldActions.h"
#include "Interaction/BBSBInteraction.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Stage01/BBSBStage01.h"
#include "EngineUtils.h"

bool UBBSBTraversalComponent::TryAutoLadder(const FVector& Direction)
{
 if(!Character || !Character->FieldActions || (!Character->GetCharacterMovement()->IsFalling() && !Character->FieldActions->IsDodging()))return false;
 const FVector Position=Character->GetActorLocation();const float Reach=Character->GetCapsuleComponent()->GetScaledCapsuleRadius()+65.f;
 for(TActorIterator<ABBSBStage01Device> It(GetWorld());It;++It)
 {
  auto* D=*It;if(D->Kind!=EBBSBStage01Node::ClimbLadder && (D->Kind!=EBBSBStage01Node::LowerLadder || !D->bSolved || D->Painted<.98f))continue;
  const FVector N=D->LadderNormal(),B=D->LadderBottomPoint(),T=D->LadderTopPoint(),Goal=FMath::ClosestPointOnSegment(Position,B,T);
  if(FVector::DotProduct(Direction,-N)<.5f || FVector::DotProduct(Position-Goal,N)<-12.f || FVector::DistSquared(Position,Goal)>FMath::Square(Reach))continue;
  if(D->MountLadder())return true;
 }
 return false;
}
float UBBSBTraversalComponent::LadderInput() const
{
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 if(!PC || PC->IsMoveInputIgnored())return 0;
 const float Up=(bLadderUpHeld || PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up)) ? 1.f:0.f;
 const float Down=(bLadderDownHeld || PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down)) ? 1.f:0.f;
 return FMath::Clamp(Up-Down+PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY),-1.f,1.f);
}
bool UBBSBTraversalComponent::BeginLadder(AActor* Source,FVector Bottom,FVector Top,FVector Exit,FVector Normal)
{
 if(IsOnLadder())return LadderSource==Source;
 if(!Character || !IsValid(Source) || !Character->FieldActions || IsWireTraversalActive() || bGlideDashing || GetWorld()->GetTimeSeconds()<ParkourIgnoreUntil)return false;
 if(Character->Combat && (Character->Combat->bDead || Character->Combat->BlocksMovement()))return false;
 if(Character->Interaction && Character->Interaction->bDialogueActive)return false;
 const auto* PC=Cast<APlayerController>(Character->GetController());if(!PC || !PC->IsLocalController() || PC->IsMoveInputIgnored())return false;
 const FVector Line=Top-Bottom;const float Length=Line.Size();if(Length<60.f || Line.Z<40.f)return false;
 const FVector Axis=Line/Length,Position=Character->GetActorLocation();
 const float Distance=FMath::Clamp(float(FVector::DotProduct(Position-Bottom,Axis)),0.f,Length);
 const FVector Goal=Bottom+Axis*Distance;
 if(FVector::Distance(Position,Goal)>170.f || !CapsuleClear(Goal))return false;
 const float MountRise=Distance>Length-120.f ? 60.f:0.f;
 const FVector Lift=FVector::UpVector*MountRise;
 if(!(CapsulePathClear(Position,Position+Lift) && CapsulePathClear(Position+Lift,Goal+Lift) && CapsulePathClear(Goal+Lift,Goal)))return false;
 if(IsParkourActive())CancelParkour();
 if(!Character->FieldActions->CanStartLadder())return false;
 BeginParkour(EBBSBParkourMode::Ladder);
 LadderSource=Source;LadderBottom=Bottom;LadderTop=Top;LadderExit=Exit;LadderAxis=Axis;LadderLength=Length;LadderOut=Normal.GetSafeNormal2D();
 if(LadderOut.IsNearlyZero())LadderOut=-(Exit-Bottom).GetSafeNormal2D();if(LadderOut.IsNearlyZero())LadderOut=-Character->GetActorForwardVector();
 LedgeNormal=LadderOut;LedgePoint=Goal;LadderDistance=Distance;LadderStepFrom=LadderStepTo=Distance;
 bLadderStepping=bLadderHopping=bLadderSliding=bLadderExiting=false;bLadderMounting=true;
 LadderMountFrom=Position;LadderMountTo=Goal;LadderMountAt=GetWorld()->GetTimeSeconds();LadderMotionSeconds=MountRise>0 ? .28f:.16f;
 ParkourHandAlpha=1;Character->SetActorRotation((-LadderOut).Rotation());return true;
}
bool UBBSBTraversalComponent::JumpFromLadder()
{
 if(!Character || !IsOnLadder() || (Character->Combat && (Character->Combat->bDead || Character->Combat->BlocksMovement())))return false;
 const FVector Out=LadderOut,Input=Character->FieldActions ? Character->FieldActions->MovementIntent():FVector::ZeroVector;
 CancelParkour();Character->StopJumping();
 auto* Move=Character->GetCharacterMovement();Move->ClearAccumulatedForces();
 // This is an ordinary upward jump, not the downward drop used by DropParkour.
 FVector Planar=Input*Move->MaxWalkSpeed;
 if(FVector::DotProduct(Planar,Out)<0)Planar=FVector::VectorPlaneProject(Planar,Out);
 Character->LaunchCharacter(Planar+Out*80.f+FVector::UpVector*Move->JumpZVelocity,true,true);
 bAirJumpUsed=false;LastFallSpeed=0;bWasFalling=true;
 ParkourIgnoreUntil=GetWorld()->GetTimeSeconds()+FMath::Max(.25f,ParkourRegrabDelay);
 if(Presentation)SpawnSmoke(Presentation->JumpSmoke,Character->GetActorLocation()-FVector::UpVector*Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),FVector::UpVector,.7f);
 return true;
}
void UBBSBTraversalComponent::StartLadderStep(bool Hop)
{
 bLadderStepping=true;bLadderHopping=Hop;bLadderSliding=false;LadderStepFrom=LadderDistance;
 LadderStepTo=FMath::Min(LadderLength,LadderStepFrom+FMath::Max(20.f,LadderRungSpacing)*(Hop ? FMath::Max(2,LadderHopRungs):1));
 LadderStepAt=GetWorld()->GetTimeSeconds();LadderMotionSeconds=FMath::Max(.12f,Hop ? LadderHopSeconds:LadderStepSeconds);LadderStepPhase=0;
}
bool UBBSBTraversalComponent::TryLadderHop()
{
 if(!IsOnLadder() || bLadderMounting || bLadderExiting || bLadderHopping)return false;
 if(LadderInput()<-.2f)return false;
 StartLadderStep(true);return true;
}
void UBBSBTraversalComponent::ExitLadderTop()
{
 if(bLadderExiting)return;
 const FVector From=Character->GetActorLocation();const float Apex=FMath::Max(float(From.Z),float(LadderExit.Z))+65.f;
 if(!PathClear(From,LadderExit,Apex))return;
 LadderMountFrom=From;LadderMountTo=LadderExit;LadderMountAt=GetWorld()->GetTimeSeconds();LadderMotionSeconds=.42f;
 bLadderExiting=true;bLadderStepping=false;bLadderHopping=true;LadderStepPhase=0;
}
void UBBSBTraversalComponent::TickLadder(float DT)
{
 auto* Move=Character->GetCharacterMovement();const auto* PC=Cast<APlayerController>(Character->GetController());
 if(!IsValid(LadderSource) || !PC || PC->IsMoveInputIgnored() || (Character->Combat && (Character->Combat->bDead || Character->Combat->BlocksMovement())) || (Character->Interaction && Character->Interaction->bDialogueActive)) { CancelParkour();return; }
 Move->StopMovementImmediately();Move->GravityScale=0;Character->ConsumeMovementInputVector();ClimbFootAlpha=0;
 if(FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive()) { bLadderUpHeld=bLadderDownHeld=bLadderHopHeld=false;return; }
 const double Now=GetWorld()->GetTimeSeconds();ParkourAge=Now-ParkourStartedAt;
 FVector Goal=Character->GetActorLocation();bool FinishMount=false,FinishExit=false;
 if(bLadderMounting || bLadderExiting)
 {
  const float T=FMath::Clamp(float((Now-LadderMountAt)/FMath::Max(.1f,LadderMotionSeconds)),0.f,1.f);LadderStepPhase=T;
  if(bLadderExiting || LadderMotionSeconds>.2f)
  {
   const float Apex=FMath::Max(float(LadderMountFrom.Z),float(LadderMountTo.Z))+65.f;
   const FVector A(LadderMountFrom.X,LadderMountFrom.Y,Apex),B(LadderMountTo.X,LadderMountTo.Y,Apex);
   Goal=T<.25f ? FMath::Lerp(LadderMountFrom,A,FMath::SmoothStep(0.f,.25f,T)):T<.7f ? FMath::Lerp(A,B,FMath::SmoothStep(.25f,.7f,T)):FMath::Lerp(B,LadderMountTo,FMath::SmoothStep(.7f,1.f,T));
  }
  else Goal=FMath::Lerp(LadderMountFrom,LadderMountTo,FMath::SmoothStep(0.f,1.f,T));
  FinishMount=bLadderMounting && T>=1;FinishExit=bLadderExiting && T>=1;
 }
 else
 {
  const float Input=LadderInput();bLadderSliding=Input<-.2f;
  if(bLadderSliding)
  {
   bLadderStepping=bLadderHopping=false;LadderDistance=FMath::Max(0.f,LadderDistance-FMath::Max(100.f,LadderSlideSpeed)*DT);LadderStepFrom=LadderStepTo=LadderDistance;
  }
  else
  {
   const bool HopHeld=bLadderHopHeld || PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
   if(!bLadderStepping && (Input>.2f || HopHeld) && LadderDistance<LadderLength-.1f)StartLadderStep(HopHeld);
   if(bLadderStepping)
   {
    LadderStepPhase=FMath::Clamp(float((Now-LadderStepAt)/LadderMotionSeconds),0.f,1.f);
    if(bLadderHopping)
    {
     // A longer plant launches six rungs. Constant downward acceleration
     // slows the ascent to zero as the hands catch the destination rungs.
     const float T=LadderStepPhase,Crouch=FMath::Min(8.f,LadderStepFrom);
     if(T<.30f)LadderDistance=LadderStepFrom-Crouch*FMath::SmoothStep(0.f,.30f,T);
     else
     {
      const float U=FMath::Clamp((T-.30f)/.58f,0.f,1.f);
      LadderDistance=FMath::Lerp(LadderStepFrom-Crouch,LadderStepTo,2.f*U-U*U);
     }
    }
    else LadderDistance=FMath::Lerp(LadderStepFrom,LadderStepTo,FMath::SmoothStep(0.f,1.f,LadderStepPhase));
    if(LadderStepPhase>=1)bLadderStepping=bLadderHopping=false;
   }
   if(!bLadderStepping && LadderDistance>=LadderLength-.1f && (Input>.2f || HopHeld))ExitLadderTop();
  }
  Goal=LadderBottom+LadderAxis*LadderDistance;
  if(bLadderHopping && bLadderStepping)
  {
   const float Flight=FMath::Clamp((LadderStepPhase-.30f)/.58f,0.f,1.f);
   Goal+=LadderOut*(12.f*FMath::Square(FMath::Sin(PI*Flight)));
  }
 }
 const bool HopInFlight=bLadderHopping && bLadderStepping && !bLadderMounting && !bLadderExiting;
 ParkourHandAlpha=HopInFlight ? 1.f-FMath::SmoothStep(.22f,.36f,LadderStepPhase)+FMath::SmoothStep(.80f,.92f,LadderStepPhase):1.f;
 FHitResult Hit;Character->SetActorLocation(Goal,true,&Hit,ETeleportType::None);LedgePoint=Goal;Character->SetActorRotation((-LadderOut).Rotation());
 if(Hit.bBlockingHit && FVector::Distance(Character->GetActorLocation(),Goal)>3.f)
 {
  LadderDistance=FMath::Clamp(float(FVector::DotProduct(Character->GetActorLocation()-LadderBottom,LadderAxis)),0.f,LadderLength);
  bLadderStepping=bLadderHopping=false;bLadderMounting=bLadderExiting=false;LadderStepFrom=LadderStepTo=LadderDistance;ParkourHandAlpha=1;return;
 }
 if(FinishMount)bLadderMounting=false;
 if(FinishExit || (bLadderSliding && LadderDistance<=.01f))
 {
  const FVector Out=LadderOut;CancelParkour();Move->Velocity=FinishExit ? FVector::ZeroVector:Out*70.f;ParkourIgnoreUntil=Now+.35f;
 }
}
float UBBSBTraversalComponent::LadderClipPhase() const
{
 if(bLadderHopping)
 {
  // Retime the editable launch/tuck/catch clip without changing its source.
  const float T=LadderStepPhase;
  if(T<.30f)return FMath::Lerp(0.f,.18f,T/.30f);
  if(T<.46f)return FMath::Lerp(.18f,.32f,(T-.30f)/.16f);
  if(T<.80f)return FMath::Lerp(.32f,.70f,(T-.46f)/.34f);
  if(T<.94f)return FMath::Lerp(.70f,.96f,(T-.80f)/.14f);
  return FMath::Lerp(.96f,1.f,(T-.94f)/.06f);
 }
 return FMath::Fmod(LadderDistance/FMath::Max(20.f,LadderRungSpacing),2.f)*.5f;
}
FVector UBBSBTraversalComponent::LadderHand(bool Left) const
{
 const auto* Device=Cast<ABBSBStage01Device>(LadderSource);const float Grip=Device && Device->Kind==EBBSBStage01Node::LowerLadder ? 29.f:32.f;
 const float Spacing=FMath::Max(20.f,LadderRungSpacing);const FVector Side=FVector::CrossProduct(FVector::UpVector,-LadderOut).GetSafeNormal();
 if(bLadderSliding)return Character->GetActorLocation()-LadderOut*Grip+Side*(Left ? -24.f:24.f)+FVector::UpVector*20.f+LadderAxis*40.f;
 auto Rung=[&](float D) { const int32 N=FMath::FloorToInt((D+.01f)/Spacing);return Left ? 2*((N+1)/2):2*(N/2)+1; };
 // Airborne hands reach the destination rungs; only the contact weight plants them.
 const float T=bLadderStepping ? (bLadderHopping ? FMath::SmoothStep(.30f,.80f,LadderStepPhase):FMath::SmoothStep(0.f,.82f,LadderStepPhase)):1.f;
 const float N=FMath::Lerp(float(Rung(LadderStepFrom)),float(Rung(LadderStepTo)),T);
 return LadderBottom+LadderAxis*(N*Spacing)-LadderOut*Grip+Side*(Left ? -18.f:18.f)+FVector::UpVector*20.f;
}
FVector UBBSBTraversalComponent::LadderFoot(bool Left) const
{
 const auto* Device=Cast<ABBSBStage01Device>(LadderSource);const float Grip=Device && Device->Kind==EBBSBStage01Node::LowerLadder ? 29.f:32.f;
 const float Spacing=FMath::Max(20.f,LadderRungSpacing);const FVector Side=FVector::CrossProduct(FVector::UpVector,-LadderOut).GetSafeNormal();
 auto Rung=[&](float D) { const int32 N=FMath::FloorToInt((D+.01f)/Spacing);return Left ? 2*(N/2):2*((N+1)/2)-1; };
 const float T=bLadderStepping ? (bLadderHopping ? FMath::SmoothStep(.35f,.84f,LadderStepPhase):FMath::SmoothStep(.12f,1.f,LadderStepPhase)):1.f;
 const float N=FMath::Lerp(float(Rung(LadderStepFrom)),float(Rung(LadderStepTo)),T);
 return LadderBottom+LadderAxis*((N-2.f)*Spacing)-LadderOut*Grip+Side*(Left ? -14.f:14.f)+FVector::UpVector*20.f;
}
