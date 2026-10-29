#include "Character/BBSBTraversal.h"
#include "Animation/BlendSpace.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "Combat/BBSBCombat.h"
#include "Field/BBSBFieldActions.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

bool UBBSBTraversalPresentation::RebuildGroundDashBlendSpace()
{
#if WITH_EDITOR
 if(!GroundDashBlendSpace) return false;
 GroundDashBlendSpace->ResampleData();GroundDashBlendSpace->MarkPackageDirty();return true;
#else
 return false;
#endif
}
TArray<float> UBBSBTraversalPresentation::GroundDashSampleWeights(float Angle) const
{
 TArray<float> Result;if(!GroundDashBlendSpace) return Result;
 Result.Init(0.f,GroundDashBlendSpace->GetBlendSamples().Num());TArray<FBlendSampleData> Samples;int32 Index=INDEX_NONE;
 if(GroundDashBlendSpace->GetSamplesFromBlendInput(FVector(FRotator::NormalizeAxis(Angle),0,0),Samples,Index,false))
  for(const auto& Sample:Samples) if(Result.IsValidIndex(Sample.SampleDataIndex)) Result[Sample.SampleDataIndex]=Sample.GetClampedWeight();
 return Result;
}

UBBSBTraversalComponent::UBBSBTraversalComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PrePhysics;
 PresentationLibrary=TSoftObjectPtr<UBBSBTraversalPresentation>(FSoftObjectPath(TEXT("/Game/BBSB/AnimationAuthoring/Traversal/DA_TraversalPresentation.DA_TraversalPresentation")));
}
void UBBSBTraversalComponent::BeginPlay()
{
 Super::BeginPlay();Character=Cast<ABBSBCharacter>(GetOwner());
 if (!Character) return;
 WireCharges=2;WireRechargeProgress=0;bWireOnRechargeSurface=bWireRechargeArmed=false;
 if(FMath::IsNearlyEqual(SkyWireElevation,45.f) || FMath::IsNearlyEqual(SkyWireElevation,25.f)) SkyWireElevation=30.f;
 Character->JumpMaxCount=1;
 BaseGravityScale=Character->GetCharacterMovement()->GravityScale;
 if (Character->FieldActions) AddTickPrerequisiteComponent(Character->FieldActions);
 Character->GetCharacterMovement()->AddTickPrerequisiteComponent(this);
 Character->GetMesh()->AddTickPrerequisiteComponent(this);
 if(!Presentation) Presentation=PresentationLibrary.LoadSynchronous();
 auto MakeEffect=[&](FName Name)
 {
  auto* Effect=NewObject<UNiagaraComponent>(Character,Name);
  Effect->SetupAttachment(Character->GetRootComponent());Effect->SetAutoActivate(false);
  Effect->SetCollisionEnabled(ECollisionEnabled::NoCollision);Effect->SetCastShadow(false);
  Character->AddInstanceComponent(Effect);Effect->RegisterComponent();return Effect;
 };
 JumpVortexComponent=MakeEffect(TEXT("DoubleJumpVortex"));DashDrillComponent=MakeEffect(TEXT("AirDashDrill"));
}
void UBBSBTraversalComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 ResetDashSmear();RestoreWallJumpMovement();RestoreParkourRunExit();EndAirWeaponHang();CancelParkour();CancelWireRecall();EndGlide();
 for(const auto& Segment:WireSegments) if(Segment) Segment->DestroyComponent();
 if(JumpVortexComponent) JumpVortexComponent->DestroyComponent();
 if(DashDrillComponent) DashDrillComponent->DestroyComponent();
 Super::EndPlay(Reason);
}
bool UBBSBTraversalComponent::TryJump()
{
 if (!IsValid(Character)) return false;
 const auto* PC=Cast<APlayerController>(Character->GetController());
 if (!PC || !PC->IsLocalController() || PC->IsMoveInputIgnored() || IsWireTraversalActive()) return false;
 if(ParkourMode==EBBSBParkourMode::LedgeHang)
 {
  const FVector Input=Character->FieldActions ? Character->FieldActions->MovementDashDirection():-LedgeNormal;
  return FVector::DotProduct(Input,LedgeNormal)>.3f ? TryWallJump():TryLedgeClimb();
 }
 if(ParkourMode==EBBSBParkourMode::WallSlide) return TryWallJump();
 if(IsParkourActive()) return false;
 if (Character->FieldActions && !Character->FieldActions->CanStartJump()) return false;
 if (Character->Combat && (!Character->Combat->CanAttack() || Character->Combat->bDead)) return false;
 if (IsBunniarTraversalActive()) { ReleaseForFall();return true; }
 auto* Move=Character->GetCharacterMovement();
 if (Move->IsMovingOnGround()) { if(TryLowVault()) return true;Character->Jump();return true; }
 if (!Move->IsFalling()) return false;
 return BeginGlide(!bAirJumpUsed);
}
bool UBBSBTraversalComponent::BeginGlide(bool bApplyLift)
{
 if (!Character || !Character->FieldActions || IsParkourActive()) return false;
 auto* Buddy=Cast<ABBSBBunniarCompanion>(Character->FieldActions->BunniarEquipment);
 if (!IsValid(Buddy)) return false;
 RestoreWallJumpMovement();EndAirWeaponHang();WallJumpAge=-1;
 // Stage-placed companions become player equipment when borrowed for traversal.
 Buddy->SetOwner(Character);
 auto* Move=Character->GetCharacterMovement();
 const int32 Pending=Character->FieldActions->PendingEquipmentId;
 Character->StopJumping();Character->FieldActions->ResetActionState();Character->FieldActions->PendingEquipmentId=Pending;
 SavedGravity=BaseGravityScale;SavedAirControl=Move->AirControl;
 Glider=Buddy;bGliding=true;
 BunniarScale=Buddy->AnimationSet ? Buddy->AnimationSet->EquippedScale:.175f;
 Buddy->BeginGlidePresentation();Move->AirControl=GlideAirControl;
 if (bApplyLift)
 {
  bAirJumpUsed=true;bAirLiftPending=true;AirLiftStartedAt=GetWorld()->GetTimeSeconds();AirLiftStartZ=Character->GetActorLocation().Z;
  DoubleJumpAge=0;GlideHandAlpha=0;
  StartMotionEffect(JumpVortexComponent,Presentation ? Presentation->DoubleJumpVortex.Get():nullptr);
  Move->ClearAccumulatedForces();Move->GravityScale=0;Move->Velocity.Z=-AirAnticipationDrop/FMath::Max(.04f,AirAnticipationSeconds);
 }
 // Redeployment preserves velocity; only the first airborne use applies lift.
 return true;
}
void UBBSBTraversalComponent::ReleaseForFall()
{
 EndGlide();
 if (Character && Character->GetCharacterMovement()->IsFalling())
 {
  auto* Move=Character->GetCharacterMovement();
  Move->ClearAccumulatedForces();Move->Velocity.Z=FMath::Min(0.f,Move->Velocity.Z);
 }
}
bool UBBSBTraversalComponent::TryGlideDash()
{
 // Kept for old Blueprint references. The dedicated air drill has been retired.
 return false;
}
bool UBBSBTraversalComponent::PrepareSurfaceAttachment()
{
 if (!bGliding || !Character || !Character->FieldActions || !Character->FieldActions->CanStartJump()) return false;
 auto* Buddy=Glider.Get();
 if (!IsValid(Buddy) || Character->FieldActions->FieldEquipment!=Buddy) return false;
 ReleaseForFall();Character->FieldActions->ResetActionState();
 // A short unpowered toss buys time to choose an anchor; it does not reset the airborne jump.
 Character->LaunchCharacter(FVector(0,0,DispatchLiftSpeed),false,true);return true;
}
void UBBSBTraversalComponent::ResumeGlideAfterDash()
{
 auto* Move=Character->GetCharacterMovement();
 if (bDashPhysicsSaved)
 {
  Move->BrakingDecelerationFalling=SavedFallingBraking;Move->FallingLateralFriction=SavedFallingFriction;
 }
 bDashPhysicsSaved=false;bGlideDashing=false;bGliding=true;PreviousDashStep=0;
 if(DashDrillComponent) DashDrillComponent->Deactivate();
 Move->AirControl=GlideAirControl;Move->GravityScale=GlideGravityScale;
 Move->Velocity.Z=FMath::Clamp(Move->Velocity.Z,-GlideFallSpeed,0.f);
 const FVector Planar=FVector(Move->Velocity.X,Move->Velocity.Y,0).GetClampedToMaxSize(GlideSpeed);
 Move->Velocity.X=Planar.X;Move->Velocity.Y=Planar.Y;
 if(Move->Velocity.Z<=-GlideFallSpeed) Move->GravityScale=0;
}
void UBBSBTraversalComponent::EndGlide()
{
 if (!IsBunniarTraversalActive()) return;
 if (IsValid(Character))
 {
  auto* Move=Character->GetCharacterMovement();Move->GravityScale=SavedGravity;Move->AirControl=SavedAirControl;
  if (bDashPhysicsSaved)
  {
   Move->BrakingDecelerationFalling=SavedFallingBraking;Move->FallingLateralFriction=SavedFallingFriction;
   Move->Velocity.X=Move->Velocity.Y=0;
  }
 }
 if (IsValid(Glider)) Glider->EndGlidePresentation();
 Glider=nullptr;bGliding=false;bGlideDashing=false;bAirLiftPending=false;bDashPhysicsSaved=false;PreviousDashStep=0;
 DoubleJumpAge=-1;DoubleJumpYaw=0;GlideHandAlpha=1;AirDashPoseAlpha=0;
 if(JumpVortexComponent) JumpVortexComponent->DeactivateImmediate();
 if(DashDrillComponent) DashDrillComponent->DeactivateImmediate();
}
void UBBSBTraversalComponent::Landed(const FHitResult& Hit)
{
 RestoreWallJumpMovement();
 const float Impact=FMath::Max(LastFallSpeed,Character ? -Character->GetCharacterMovement()->Velocity.Z:0.f);
 if(Presentation && Impact>160.f) SpawnSmoke(Presentation->LandingSmoke,Hit.ImpactPoint+Hit.ImpactNormal*3.f,Hit.ImpactNormal,FMath::Clamp(Impact/650.f,.65f,1.45f));
 if(bParkourJumping) FinishParkourJump();
 else if(ParkourMode==EBBSBParkourMode::WallSlide) { CancelParkour();ParkourIgnoreUntil=GetWorld()->GetTimeSeconds()+.1f; }
 EndAirWeaponHang();EndGlide();bAirJumpUsed=false;LastFallSpeed=0;bWasFalling=false;
}
void UBBSBTraversalComponent::SpawnSmoke(UNiagaraSystem* System,const FVector& Position,const FVector& Normal,float Strength)
{
 if(!System || !Character || !Presentation || Presentation->EffectScale<=0.f) return;
 UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),System,Position,FRotationMatrix::MakeFromZ(Normal).Rotator(),FVector(Presentation->EffectScale*Strength),true,true,ENCPoolMethod::AutoRelease,true);
}
void UBBSBTraversalComponent::StartMotionEffect(UNiagaraComponent* Component,UNiagaraSystem* System)
{
 if(!Component || !System || !Presentation || Presentation->EffectScale<=0.f) return;
 Component->DeactivateImmediate();Component->SetAsset(System);Component->SetRelativeLocation(FVector::ZeroVector);
 Component->SetRelativeRotation(FRotator::ZeroRotator);Component->SetRelativeScale3D(FVector(Presentation->EffectScale));Component->Activate(true);
}
float UBBSBTraversalComponent::DashHeightAt(float Age) const
{
 const float Duration=FMath::Max(.2f,GlideDashLeadSeconds+GlideDashSeconds);
 const float T=FMath::Clamp(Age/Duration,0.f,1.f);
 auto Smooth=[](float X) { X=FMath::Clamp(X,0.f,1.f);return X*X*(3.f-2.f*X); };
 // Small float, a long downward swoop, then only a small recovery at the end.
 if(T<.24f) return AirDashRise*Smooth(T/.24f);
 if(T<.80f) return FMath::Lerp(AirDashRise,-AirDashDrop,Smooth((T-.24f)/.56f));
 return FMath::Lerp(-AirDashDrop,-AirDashDrop+FMath::Min(AirDashRecovery,AirDashDrop*.4f),Smooth((T-.80f)/.20f));
}
FVector UBBSBTraversalComponent::GlideGrip(bool bLeft) const
{
 if (!IsValid(Character)) return FVector::ZeroVector;
 return Character->GetActorTransform().TransformPosition(bLeft ? LeftGripOffset:RightGripOffset)+(bLeft ? GlideLeadOffset:FVector::ZeroVector);
}
void UBBSBTraversalComponent::UpdateHangingPresentation(float DT)
{
 const bool Active=IsBunniarTraversalActive();
 GlidePoseTime+=FMath::Max(0.f,DT);
 if(Active && DoubleJumpAge>=0)
 {
  DoubleJumpAge=GetWorld()->GetTimeSeconds()-AirLiftStartedAt;
  const float Phase=FMath::Clamp(DoubleJumpAge/FMath::Max(.04f,AirAnticipationSeconds),0.f,1.f);
  DoubleJumpYaw=-DoubleJumpSpinDegrees*Phase*Phase*(3.f-2.f*Phase);
  const float Catch=FMath::Clamp((Phase-.76f)/.24f,0.f,1.f);GlideHandAlpha=Catch*Catch*(3.f-2.f*Catch);
  if(DoubleJumpAge>AirAnticipationSeconds+.45f) { DoubleJumpAge=-1;DoubleJumpYaw=0; }
 }
 const float DashDuration=FMath::Max(.2f,GlideDashLeadSeconds+GlideDashSeconds);
 AirDashPoseAlpha=bGlideDashing ? FMath::SmoothStep(0.f,.12f,GlideDashElapsed)*FMath::SmoothStep(0.f,.13f,DashDuration-GlideDashElapsed):0.f;
 const FVector Lead=bGlideDashing ? GlideDashDirection*(GlideDashLeadDistance*FMath::Clamp(GlideDashElapsed/FMath::Max(.01f,GlideDashLeadSeconds),0.f,1.f)):FVector::ZeroVector;
 GlideLeadOffset=FMath::VInterpTo(GlideLeadOffset,Lead,DT,14.f);
 if(Active && !GlideLeadOffset.IsNearlyZero())
 {
  const FVector Start=Character->GetActorTransform().TransformPosition(BunniarOffset);
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BunniarGlideLead),false,Character);Query.AddIgnoredActor(Glider.Get());FHitResult Hit;
  if(GetWorld()->SweepSingleByChannel(Hit,Start,Start+GlideLeadOffset,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(22),Query))
   GlideLeadOffset=Hit.bStartPenetrating ? FVector::ZeroVector:Hit.Location-Start;
 }
 const FVector Direction=bGlideDashing ? GlideDashDirection:Character->GetCharacterMovement()->Velocity.GetSafeNormal2D();
 const FVector Local=Character->GetActorQuat().UnrotateVector(Direction);
 const float Lean=bGlideDashing ? FMath::Lerp(TravelLeanDegrees,PullLeanDegrees-GlideDashTangent.Rotation().Pitch,AirDashPoseAlpha):TravelLeanDegrees*FMath::Clamp(Character->GetVelocity().Size2D()/FMath::Max(GlideSpeed,1.f),0.f,1.f);
 const FVector2D Goal=Active ? FVector2D(Local.X*Lean,Local.Y*Lean+FMath::Sin(GlidePoseTime*3.f)*GlideSwayDegrees):FVector2D::ZeroVector;
 const int32 Steps=FMath::Max(1,FMath::CeilToInt(FMath::Max(0.f,DT)*120.f));const float H=FMath::Max(0.f,DT)/Steps;
 for(int32 I=0;I<Steps;++I)
 {
  const float Omega=bGlideDashing ? 26.f:14.f;
  GlideLeanVelocity+=((Goal-GlideBodyLean)*Omega*Omega-GlideLeanVelocity*(2.f*Omega))*H;
  GlideBodyLean+=GlideLeanVelocity*H;
 }
 if(DashDrillComponent && bGlideDashing)
 {
  const FQuat Spin(GlideDashTangent,GlideDashElapsed*GlideDashSpinHz*2.f*PI);
  DashDrillComponent->SetWorldRotation(Spin*GlideDashTangent.Rotation().Quaternion());
  DashDrillComponent->SetWorldLocation(Character->GetActorLocation()+FVector(0,0,38)+GlideDashTangent*15.f);
 }
 if(JumpVortexComponent && DoubleJumpAge>=0)
  JumpVortexComponent->SetWorldRotation(FRotator(0,-DoubleJumpAge*540.f,0));
}
void UBBSBTraversalComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DT,Type,Function);
 if (!IsValid(Character)) return;
 RestoreParkourRunExit();TickWallJumpMovement();auto* Move=Character->GetCharacterMovement();
 TickWireMovement(DT);TickWireRecharge(DT);UpdateWirePresentation();
 TickParkour(DT);
 TickMovementEffects(DT);
 ParkourPoseAlpha=FMath::FInterpConstantTo(ParkourPoseAlpha,IsParkourActive() ? 1.f:0.f,DT,IsParkourActive() ? 16.f:10.f);
 if(WallJumpAge>=0) { WallJumpAge=GetWorld()->GetTimeSeconds()-WallJumpStartedAt;if(WallJumpAge>.5f) WallJumpAge=-1; }
 if(IsParkourActive())
 {
  WirePoseAlpha=FMath::FInterpConstantTo(WirePoseAlpha,0.f,DT,14.f);
  UpdateHangingPresentation(DT);GlidePoseAlpha=FMath::FInterpTo(GlidePoseAlpha,0.f,DT,20.f);return;
 }
 WirePoseAlpha=FMath::FInterpConstantTo(WirePoseAlpha,IsWireTraversalActive() ? 1.f:0.f,DT,IsWireTraversalActive() ? 14.f:10.f);
 if(IsWireTraversalActive()) { UpdateHangingPresentation(DT);GlidePoseAlpha=FMath::FInterpTo(GlidePoseAlpha,0.f,DT,20.f);return; }
 if(Move->IsFalling())
 {
  if(!bWasFalling && Move->Velocity.Z>50.f && Presentation)
   SpawnSmoke(Presentation->JumpSmoke,Character->GetActorLocation()-FVector(0,0,Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-4.f),FVector::UpVector,1.f);
  LastFallSpeed=FMath::Max(0.f,-Move->Velocity.Z);
 }
 bWasFalling=Move->IsFalling();
 if (Move->IsMovingOnGround()) { EndGlide();bAirJumpUsed=false; }
 if (IsBunniarTraversalActive())
 {
  const auto* PC=Cast<APlayerController>(Character->GetController());
  const bool Interrupted=Character->FieldActions && !Character->FieldActions->CanMove() && !Character->FieldActions->IsDodging() && !Character->FieldActions->bEquipmentWheelOpen;
  if (!IsValid(Glider) || !PC || !PC->IsLocalController() || !Move->IsFalling() || Interrupted || (Character->Combat && Character->Combat->bDead)) EndGlide();
  else
  {
   BunniarScale=Glider->AnimationSet ? Glider->AnimationSet->EquippedScale:.175f;
   if (bGliding)
   {
    Move->AirControl=GlideAirControl;
    if(bAirLiftPending)
    {
     const float Age=GetWorld()->GetTimeSeconds()-AirLiftStartedAt;
     if(Age>=AirAnticipationSeconds)
     {
      bAirLiftPending=false;Move->GravityScale=SavedGravity;
      Character->LaunchCharacter(FVector(0,0,AirJumpVelocity),false,true);
     }
     else
     {
      const float Phase=FMath::Clamp((Age+DT)/FMath::Max(.04f,AirAnticipationSeconds),0.f,1.f);
      const float Goal=AirLiftStartZ-AirAnticipationDrop*Phase*Phase*(3-2*Phase);
      Move->GravityScale=0;Move->Velocity.Z=FMath::Clamp((Goal-Character->GetActorLocation().Z)/FMath::Max(DT,.0001f),-300.f,0.f);
     }
    }
    else
    {
     Move->GravityScale=Move->Velocity.Z>0 ? SavedGravity:Move->Velocity.Z<=-GlideFallSpeed ? 0.f:GlideGravityScale;
     Move->Velocity.Z=FMath::Max(Move->Velocity.Z,-GlideFallSpeed);
    }
   }
   else
   {
    GlideDashElapsed=GetWorld()->GetTimeSeconds()-GlideDashStartedAt;
    const FVector Position=Character->GetActorLocation();
    const float Progress=FVector::DotProduct(Position-PreviousDashPosition,GlideDashDirection);
    DashDistanceTravelled+=Progress;
    const bool Blocked=PreviousDashStep>4.f && Progress<PreviousDashStep*.15f;
    if ((GlideDashElapsed>=GlideDashLeadSeconds+GlideDashSeconds && DashDistanceTravelled>=GlideDashDistance-1.f) || Blocked) ResumeGlideAfterDash();
    else
    {
     PreviousDashPosition=Position;
     const float NextAge=GlideDashElapsed+DT;
     const float Height=DashHeightAt(NextAge);
     Move->GravityScale=0;
     Move->Velocity.Z=FMath::Clamp((GlideDashStartZ+Height-Position.Z)/FMath::Max(DT,.0001f),-900.f,900.f);
     if (GlideDashElapsed<GlideDashLeadSeconds)
     {
      Move->Velocity.X=DashInitialVelocity.X;Move->Velocity.Y=DashInitialVelocity.Y;PreviousDashStep=0;
     }
     else
     {
      const float T1=FMath::Clamp((GlideDashElapsed+DT-GlideDashLeadSeconds)/GlideDashSeconds,0.f,1.f);
      // Integrate toward the absolute eased distance so a long frame cannot
      // carry the previous frame's peak dash velocity beyond the destination.
      PreviousDashStep=FMath::Max(0.f,GlideDashDistance*(1.f-FMath::Cos(PI*T1))*.5f-DashDistanceTravelled);
      const FVector Velocity=GlideDashDirection*(PreviousDashStep/FMath::Max(DT,.0001f));
      Move->Velocity.X=Velocity.X;Move->Velocity.Y=Velocity.Y;
     }
     const float ForwardSpeed=FMath::Max(250.f,Move->Velocity.Size2D());
     const float Pitch=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Move->Velocity.Z,ForwardSpeed)),-55.f,40.f);
     GlideDashTangent=(GlideDashDirection*FMath::Cos(FMath::DegreesToRadians(Pitch))+FVector::UpVector*FMath::Sin(FMath::DegreesToRadians(Pitch))).GetSafeNormal();
    }
   }
  }
 }
 if(!IsWireTraversalActive() && !bGlideDashing && !bAirLiftPending && !bAirWeaponHanging) TryAutoLedge();
 if(bAirWeaponHanging)
 {
  const bool Attacking=Character->FieldActions && Character->FieldActions->ActiveState==EBBSBActionState::Attack && !Character->FieldActions->IsDodging();
  const auto* LocalPC=Cast<APlayerController>(Character->GetController());
  if(!LocalPC || !LocalPC->IsLocalController() || !Move->IsFalling() || !Attacking || IsBunniarTraversalActive() || GetWorld()->GetTimeSeconds()>=AirWeaponHangUntil || (Character->Combat && (Character->Combat->bDead || Character->Combat->BlocksMovement()))) EndAirWeaponHang();
  else
  {
   const float T=FMath::Clamp(float(GetWorld()->GetTimeSeconds()-AirWeaponStartedAt+DT)/FMath::Max(.05f,AirWeaponLiftSeconds),0.f,1.f);
   const float Z=AirWeaponStartZ+AirWeaponLift*T*T*(3.f-2.f*T);
   Move->GravityScale=0;Move->Velocity.Z=FMath::Clamp((Z-Character->GetActorLocation().Z)/FMath::Max(DT,.0001f),-250.f,250.f);
  }
 }
 if(!IsBunniarTraversalActive() && !bAirWeaponHanging && !IsParkourActive())
  Move->GravityScale=BaseGravityScale*(Move->IsFalling() && Move->Velocity.Z<=0 ? FMath::Max(1.f,FallGravityMultiplier):1.f);
 UpdateHangingPresentation(DT);
 GlidePoseAlpha=FMath::FInterpTo(GlidePoseAlpha,IsBunniarTraversalActive() ? 1.f:0.f,DT,12.f);
 if (!IsBunniarTraversalActive() && GlidePoseAlpha<.001f) GlidePoseAlpha=0;
}

void UBBSBTraversalComponent::BeginAirWeaponHang(float Duration)
{
 if(!Character || IsBunniarTraversalActive() || IsWireTraversalActive() || !Character->GetCharacterMovement()->IsFalling()) return;
 RestoreWallJumpMovement();
 bAirWeaponHanging=true;AirWeaponStartedAt=GetWorld()->GetTimeSeconds();AirWeaponHangUntil=AirWeaponStartedAt+FMath::Max(.1f,Duration);
 AirWeaponStartZ=Character->GetActorLocation().Z;auto* Move=Character->GetCharacterMovement();Move->ClearAccumulatedForces();Move->GravityScale=0;Move->Velocity.Z=0;
}
void UBBSBTraversalComponent::EndAirWeaponHang()
{
 if(!bAirWeaponHanging) return;bAirWeaponHanging=false;
 if(Character) Character->GetCharacterMovement()->GravityScale=BaseGravityScale;
}
