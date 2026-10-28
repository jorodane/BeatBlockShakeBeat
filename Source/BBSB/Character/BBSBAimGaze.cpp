#include "Character/BBSBAimGaze.h"
#include "Character/BBSBTraversal.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "TwoBoneIK.h"
#include "BBSBCharacter.h"
#include "Combat/BBSBCombat.h"
#include "Field/BBSBFieldActions.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"

UBBSBAimGazeComponent::UBBSBAimGazeComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.TickGroup=TG_PrePhysics;
 HeadPostProcess=TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/BBSB/AnimationAuthoring/Character/ABP_PlayerAimPostProcess.ABP_PlayerAimPostProcess_C")));
}
void UBBSBAimGazeComponent::BeginPlay()
{
 Super::BeginPlay();
 if (auto* Player=Cast<ABBSBCharacter>(GetOwner()))
 {
  Player->GetMesh()->AddTickPrerequisiteComponent(this);
  if (!Player->GetMesh()->GetPostProcessInstance())
   if (auto* Class=HeadPostProcess.LoadSynchronous()) Player->GetMesh()->SetOverridePostProcessAnimBP(Class,true);
 }
}
void UBBSBAimGazeComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DT,Type,Function);
 const auto* Player=Cast<ABBSBCharacter>(GetOwner());
 if (!Player || !Player->FieldActions || !Player->GetMesh()) return;
 const APlayerController* PC=Cast<APlayerController>(Player->GetController());
 const bool HasLocalPlayerView=IsValid(PC) && PC->IsLocalController();
 // Keep ticking while unpossessed so a later player possession can resume head gaze.
 AimPoint=HasLocalPlayerView ? Player->FieldActions->VisualAimPoint():Player->GetMesh()->GetSocketLocation(TEXT("head"))+Player->GetActorForwardVector()*100.f;
 const FRotator Relative=(Player->GetActorQuat().UnrotateVector(AimPoint-Player->GetMesh()->GetSocketLocation(TEXT("head")))).Rotation();
 const bool Enabled=HasLocalPlayerView && bEnableHeadGaze && !(Player->Combat && Player->Combat->bDead) && !Player->FieldActions->IsDodging();
 const float Weight=Enabled ? FMath::Clamp(AimWeight,0.f,1.f):0;
 const FRotator Desired(FMath::Clamp(Relative.Pitch,-MaximumPitch,MaximumPitch)*Weight,FMath::Clamp(FRotator::NormalizeAxis(Relative.Yaw),-MaximumYaw,MaximumYaw)*Weight,0);
 HeadAimRotation=FMath::RInterpTo(HeadAimRotation,Desired,DT,InterpSpeed);
 const FQuat Actor=Player->GetActorQuat(),Mesh=Player->GetMesh()->GetComponentQuat();
 ComponentAimRotation=(Mesh.Inverse()*Actor*HeadAimRotation.Quaternion()*Actor.Inverse()*Mesh).GetNormalized();
}

namespace
{
struct FHeadGazeProxy final : FAnimInstanceProxy
{
 FHeadGazeProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance) {}
 FAnimNode_LinkedInputPose* Input=nullptr;
 FQuat Offset=FQuat::Identity;
 float GlideWeight=0,WireWeight=0,ParkourWeight=0,EdgeHandWeight=0;
 FVector EdgeHands[2],EdgeElbows[2],ClimbFoot,ClimbKnee;
 float FootPlantWeight=0;
 FQuat WireTilt=FQuat::Identity;
 float HangWeight=0,MotionWeight=0,MotionTime=0;
 FQuat AirSpin=FQuat::Identity;
 UAnimSequence* DefenseClip=nullptr;float DefenseTime=0,DefenseWeight=0;
 UAnimSequence* MotionClip=nullptr;
 UBlendSpace* DashBlend=nullptr;
 float DashPhase=0,DashAngle=0,DashWeight=0;
 double MatchedDashStarted=-100;float MatchedDashAngle=0,MatchedDashInput=0,MatchedDashTime=0;bool bMatchedDash=false;
 bool bGlideHands=false;
 bool bRelaxedGlide=false;
 float LimbExtension=.99f;
 FVector Down,Forward,Right,Lead;
 FQuat BodyTilt=FQuat::Identity;
 FVector Grips[2],Elbows[2];
 UAnimSequence* GlidePose=nullptr;
 UAnimSequence* GlideAttackClip=nullptr;
 float GlideAttackTime=0,GlideAttackWeight=0;
 FVector WeaponHand,WeaponElbow;
 float WeaponArmWeight=0,WeaponBodyTurn=0;
 UAnimSequence* WeaponStepClip=nullptr;
 float WeaponStepTime=0,WeaponStepWeight=0;
 virtual void PreUpdate(UAnimInstance* Instance,float DT) override
 {
  FAnimInstanceProxy::PreUpdate(Instance,DT);
  Input=Instance->GetLinkedInputPoseNode();
  const auto* Player=Cast<ABBSBCharacter>(Instance->GetOwningActor());
  Offset=Player && Player->AimGaze ? Player->AimGaze->ComponentAimRotation:FQuat::Identity;
  const auto* Traversal=Player ? Player->Traversal.Get():nullptr;
  DefenseClip=nullptr;DefenseTime=DefenseWeight=0;
  if(Player && Player->Combat) Player->Combat->GetDefensePose(DefenseClip,DefenseTime,DefenseWeight);
  WeaponArmWeight=WeaponBodyTurn=0;
  WeaponStepClip=nullptr;WeaponStepTime=WeaponStepWeight=0;
  if(Player && Player->FieldActions)
  {
   Player->FieldActions->GetWeaponStepPose(WeaponStepClip,WeaponStepTime,WeaponStepWeight);
   FVector Hand,Elbow;
   if(Player->FieldActions->GetWeaponArmPose(Hand,Elbow,WeaponArmWeight,WeaponBodyTurn))
   {
    const FTransform Mesh=Player->GetMesh()->GetComponentTransform();WeaponHand=Mesh.InverseTransformPosition(Hand);WeaponElbow=Mesh.InverseTransformPosition(Elbow);
   }
  }
  GlideWeight=Traversal ? Traversal->GlidePoseAlpha:0;
  HangWeight=GlideWeight*(Traversal ? Traversal->GlideHandAlpha:1.f);
  MotionClip=nullptr;MotionWeight=0;MotionTime=0;AirSpin=FQuat::Identity;DashBlend=nullptr;DashWeight=0;
  bGlideHands=Traversal && Traversal->bUseGlideHandIK;
  GlidePose=Traversal ? Traversal->GlideBodyPose.Get():nullptr;
  bRelaxedGlide=Traversal && Traversal->bUseRelaxedGlidePose;
  LimbExtension=Traversal ? FMath::Clamp(Traversal->RelaxedLimbExtension,.9f,.999f):.99f;
  GlideAttackClip=nullptr;GlideAttackTime=GlideAttackWeight=0;
  if(Player && Player->FieldActions) Player->FieldActions->GetGlideAttackPose(GlideAttackClip,GlideAttackTime,GlideAttackWeight);
  if (Traversal && (GlideWeight>.0001f || Traversal->bWireRecovering))
  {
   const FTransform Mesh=Player->GetMesh()->GetComponentTransform();
   Forward=Mesh.InverseTransformVectorNoScale(Player->GetActorForwardVector());
   Right=Mesh.InverseTransformVectorNoScale(Player->GetActorRightVector());
   Down=Mesh.InverseTransformVectorNoScale(-FVector::UpVector);
   AirSpin=FQuat(-Down,FMath::DegreesToRadians(Traversal->DoubleJumpYaw));
   if(GlideAttackWeight>0) AirSpin=FQuat::Identity;
   if(const auto* Presentation=Traversal->Presentation.Get())
   {
    if(Traversal->DoubleJumpAge>=0 && Presentation->DoubleJump)
    {
     MotionClip=Presentation->DoubleJump;
     const float Age=Traversal->DoubleJumpAge,Duration=Traversal->AirAnticipationSeconds+.45f;
     MotionTime=FMath::Clamp(Age/Duration,0.f,1.f)*MotionClip->GetPlayLength();
     MotionWeight=GlideWeight*FMath::SmoothStep(0.f,.035f,Age)*(1.f-FMath::SmoothStep(Traversal->AirAnticipationSeconds+.13f,Duration,Age));
    }
    else if(Traversal->bGlideDashing && Presentation->AirDash)
    {
     MotionClip=Presentation->AirDash;
     MotionTime=FMath::Clamp(Traversal->GlideDashElapsed/FMath::Max(.2f,Traversal->GlideDashLeadSeconds+Traversal->GlideDashSeconds),0.f,1.f)*MotionClip->GetPlayLength();
     MotionWeight=GlideWeight*Traversal->AirDashPoseAlpha;
    }
   }
   Lead=Mesh.InverseTransformVector(Traversal->GlideLeadOffset);
   BodyTilt=FQuat(Right,FMath::DegreesToRadians(Traversal->GlideBodyLean.X))*FQuat(Forward,FMath::DegreesToRadians(-Traversal->GlideBodyLean.Y));
   for (int32 I=0;I<2;++I)
   {
    Grips[I]=Mesh.InverseTransformPosition(Traversal->GlideGrip(I==0));
    Elbows[I]=Mesh.InverseTransformPosition(Player->GetActorTransform().TransformPosition(FVector(20,I==0 ? -45:45,35)))+Lead;
   }
  }
  WireWeight=Traversal ? Traversal->WirePoseAlpha:0;WireTilt=FQuat::Identity;
  if(Traversal && WireWeight>.0001f)
  {
   HangWeight*=1.f-WireWeight;
   if(const auto* Library=Traversal->Presentation.Get())
   {
    MotionClip=Traversal->bWirePulling || Traversal->WireArrival==EBBSBWireArrival::Air ? Library->WirePull.Get():
     Traversal->WireArrival==EBBSBWireArrival::Floor ? Library->WireLanding.Get():Library->WireWallLatch.Get();
    MotionWeight=WireWeight;MotionTime=MotionClip ? FMath::Clamp(Traversal->WireMotionAge,0.f,MotionClip->GetPlayLength()):0;
    if(Traversal->bWireArrival && Traversal->WireArrival==EBBSBWireArrival::Wall && Traversal->WireMotionAge>=Traversal->WireWallSlideStartsAt && Library->WallSlide)
    { MotionClip=Library->WallSlide;MotionTime=FMath::Clamp(Traversal->WireMotionAge-Traversal->WireWallSlideStartsAt,0.f,MotionClip->GetPlayLength()); }
   }
   if(Traversal->bWireRecovering)
   {
    const auto* Library=Traversal->Presentation.Get();const float Age=Traversal->WireRecoveryAge,Duration=FMath::Max(.35f,Traversal->WireRecoverySeconds);
    MotionClip=Library ? Library->DoubleJump.Get():nullptr;
    MotionTime=MotionClip ? FMath::Clamp(Age/Duration,0.f,1.f)*MotionClip->GetPlayLength():0;
    MotionWeight=WireWeight*(1.f-FMath::SmoothStep(Duration-.12f,Duration,Age));
    HangWeight=WireWeight*Traversal->WireCatchAlpha;GlideWeight=FMath::Max(GlideWeight,HangWeight);
    const FVector Up=Player->GetMesh()->GetComponentTransform().InverseTransformVectorNoScale(FVector::UpVector);
    AirSpin=FQuat(Up,FMath::DegreesToRadians(Traversal->WireRecoveryYaw));
   }
   else if(Traversal->bWirePulling || Traversal->WireArrival==EBBSBWireArrival::Air)
   {
    const FTransform Mesh=Player->GetMesh()->GetComponentTransform();
    const FVector Side=Mesh.InverseTransformVectorNoScale(Player->GetActorRightVector());
    const FVector Front=Mesh.InverseTransformVectorNoScale(Player->GetActorForwardVector());
    const FVector Direction=Player->GetActorQuat().UnrotateVector(Traversal->WireDirection);
    const float Lean=FMath::Clamp(70.f-Traversal->WireDirection.Rotation().Pitch,-15.f,145.f);
    WireTilt=FQuat(Side,FMath::DegreesToRadians(Lean*Direction.X))*FQuat(Front,FMath::DegreesToRadians(-Lean*Direction.Y));
   }
  }
  ParkourWeight=Traversal ? Traversal->ParkourPoseAlpha:0;EdgeHandWeight=Traversal ? ParkourWeight*Traversal->ParkourHandAlpha:0;
  FootPlantWeight=Traversal ? ParkourWeight*Traversal->ClimbFootAlpha:0;
  if(Traversal && ParkourWeight>.0001f)
  {
   HangWeight=0;WireWeight*=1.f-ParkourWeight;
   if(const auto* Library=Traversal->Presentation.Get())
   {
    const auto Mode=Traversal->ParkourVisualMode;
    MotionClip=Mode==EBBSBParkourMode::LedgeHang ? Library->LedgeHang.Get():Mode==EBBSBParkourMode::Mantle ? Library->LedgeClimb.Get():Mode==EBBSBParkourMode::WallSlide ? (Library->WallSlide ? Library->WallSlide.Get():Library->WireWallLatch.Get()):Library->LowVault.Get();
    MotionTime=MotionClip && Mode!=EBBSBParkourMode::LedgeHang ? Traversal->ParkourMotionPhase()*MotionClip->GetPlayLength():0;
    if(Mode==EBBSBParkourMode::WallSlide && MotionClip) MotionTime=FMath::Clamp(Traversal->ParkourAge,0.f,MotionClip->GetPlayLength());
    MotionWeight=ParkourWeight;
   }
   const FTransform Mesh=Player->GetMesh()->GetComponentTransform();
   for(int32 I=0;I<2;++I)
   {
    EdgeHands[I]=Mesh.InverseTransformPosition(Traversal->LedgeHand(I==0));
    const FVector EdgeRight=FVector::CrossProduct(FVector::UpVector,-Traversal->LedgeNormal).GetSafeNormal();
    EdgeElbows[I]=Mesh.InverseTransformPosition(Traversal->LedgePoint+Traversal->LedgeNormal*20.f+EdgeRight*(I==0 ? -45.f:45.f)+FVector::UpVector*8.f);
   }
   ClimbFoot=Mesh.InverseTransformPosition(Traversal->ClimbFootPoint);
   ClimbKnee=Mesh.InverseTransformPosition(Traversal->ClimbFootPoint-Traversal->LedgeNormal*20.f+FVector::UpVector*30.f);
  }
  if(Traversal && Traversal->WallJumpAge>=0 && !Traversal->IsBunniarTraversalActive() && !Traversal->IsParkourActive() && !Traversal->IsWireTraversalActive())
  {
   const float Age=Traversal->WallJumpAge;
   if(const auto* Library=Traversal->Presentation.Get();Library && Library->WallJump)
   {
    MotionClip=Library->WallJump;MotionTime=FMath::Clamp(Age,0.f,MotionClip->GetPlayLength());
    MotionWeight=FMath::SmoothStep(0.f,.04f,Age)*(1.f-FMath::SmoothStep(.3f,.5f,Age));
    HangWeight=WireWeight=EdgeHandWeight=FootPlantWeight=0;WireTilt=FQuat::Identity;
   }
  }
  if(Player && Player->FieldActions && Traversal && Traversal->Presentation)
  {
   const auto* Actions=Player->FieldActions.Get();const float Age=Actions->GetDodgeVisualAge();
   if(Age>=0)
   {
    DashBlend=Traversal->Presentation->GroundDashBlendSpace;const float Duration=FMath::Max(.1f,Actions->DodgeDuration);
    const double Started=Player->GetWorld()->GetTimeSeconds()-Age;const float InputAngle=Actions->GetDodgeDirectionAngle();
    if(Actions->IsDodging() && FMath::Abs(Started-MatchedDashStarted)>.05)
    {
     MatchedDashStarted=Started;bMatchedDash=false;MatchedDashInput=InputAngle;MatchedDashAngle=InputAngle;MatchedDashTime=0;
     auto* BodyInstance=Player->GetMesh()->GetAnimInstance();const auto* Library=Traversal->Presentation.Get();
     const auto* Collector=BodyInstance ? UPoseSearchLibrary::FindPoseHistoryNode(TEXT("BBSB_MoveHistory"),BodyInstance):nullptr;
     if(Library->bUseDashMotionMatching && Library->GroundDashDatabase && Collector && Collector->GetPoseHistoryPtr())
     {
      using namespace UE::PoseSearch;
      FPoseHistory History;History=*Collector->GetPoseHistoryPtr();FTransformTrajectory Trajectory;
      const FTransform Mesh=Player->GetMesh()->GetComponentTransform();const FVector Direction=FRotator(0,Player->GetActorRotation().Yaw+InputAngle,0).Vector();
      for(int32 Step=-2;Step<=4;++Step)
      {
       auto& Sample=Trajectory.Samples.AddDefaulted_GetRef();const float T=Step*.1f;Sample.TimeInSeconds=T;Sample.Facing=Mesh.GetRotation();
       const float Distance=T>=0 ? Actions->DodgeDistance*.5f*(1.f-FMath::Cos(PI*FMath::Clamp(T/Duration,0.f,1.f))):T*Player->GetVelocity().Size2D();Sample.Position=Mesh.GetLocation()+Direction*Distance;
      }
      History.SetTrajectory(Trajectory);const UObject* Contexts[]={BodyInstance};const FRole Roles[]={DefaultRole};const IPoseHistory* Histories[]={&History};const UObject* Assets[]={Library->GroundDashDatabase};
      const auto Found=UPoseSearchLibrary::MotionMatch(MakeArrayView(Contexts),MakeArrayView(Roles),MakeArrayView(Histories),MakeArrayView(Assets),FPoseSearchContinuingProperties(),FPoseSearchFutureProperties(),FPoseSearchEvent());
      FPoseSearchBlueprintResult Result;
      if(Found.IsValid() && Result.InitFrom(Found,1.f) && Cast<UBlendSpace>(Result.SelectedAnim))
      {
       MatchedDashAngle=Result.BlendParameters.X;bMatchedDash=FMath::Abs(FRotator::NormalizeAxis(MatchedDashAngle-InputAngle))<23.f;MatchedDashTime=bMatchedDash ? FMath::Clamp(Result.SelectedTime*.4f,0.f,.035f):0.f;
       if(auto* Debug=Cast<UBBSBHeadGazeAnimInstance>(Instance)) Debug->DashMatchCost=Result.SearchCost;
      }
     }
     if(auto* Debug=Cast<UBBSBHeadGazeAnimInstance>(Instance)) { Debug->bDashMotionMatched=bMatchedDash;Debug->DashMatchedAngle=MatchedDashAngle; }
    }
    DashAngle=FRotator::NormalizeAxis(InputAngle+(bMatchedDash ? FRotator::NormalizeAxis(MatchedDashAngle-MatchedDashInput):0.f));
    const float ClipTime=Age<=Duration ? MatchedDashTime+Age/Duration*(.3f-MatchedDashTime):.3f+Age-Duration;DashPhase=FMath::Clamp(ClipTime/.4f,0.f,1.f);
    const bool Running=Actions->IsDodging() ? !Actions->MovementIntent().IsNearlyZero():Actions->IsDodgeRunExit();const float Fade=Running ? FMath::SmoothStep(Duration,Duration+.06f,Age):FMath::SmoothStep(Duration+.065f,Duration+.1f,Age);
    DashWeight=DashBlend ? FMath::SmoothStep(0.f,.025f,Age)*(1.f-Fade):0.f;
    if(DashWeight>0) { GlideWeight=HangWeight=WireWeight=MotionWeight=ParkourWeight=EdgeHandWeight=FootPlantWeight=0; }
   }
  }
 }
 virtual bool Evaluate(FPoseContext& Output) override
 {
  if (!Input) return false;
  Input->Evaluate_AnyThread(Output);
  if (Offset.Equals(FQuat::Identity,.00001f) && GlideWeight<.0001f && WireWeight<.0001f && MotionWeight<.0001f && DashWeight<.0001f && WeaponArmWeight<.0001f && WeaponStepWeight<.0001f && DefenseWeight<.0001f && FMath::IsNearlyZero(WeaponBodyTurn)) return true;
  if (GlidePose && GlideWeight>.0001f)
  {
   FPoseContext Authored(this);Authored.ResetToRefPose();
   FAnimationPoseData Data(Authored),Result(Output);
   GlidePose->GetAnimationPose(Data,FAnimExtractContext(0.0,false));
   FAnimationRuntime::BlendTwoPosesTogetherInPlace(Result,Data,1.f-GlideWeight);
  }
  if(MotionClip && MotionWeight>.0001f)
  {
   FPoseContext Authored(this);Authored.ResetToRefPose();FAnimationPoseData Data(Authored),Result(Output);
   MotionClip->GetAnimationPose(Data,FAnimExtractContext(double(MotionTime),false));
   FAnimationRuntime::BlendTwoPosesTogetherInPlace(Result,Data,1.f-MotionWeight);
  }
  if(DashBlend && DashWeight>.0001f)
  {
   TArray<FBlendSampleData> Samples;int32 Index=INDEX_NONE;
   if(DashBlend->GetSamplesFromBlendInput(FVector(DashAngle,0,0),Samples,Index,true) && Samples.Num()>0)
   {
    DashBlend->ResetBlendSamples(Samples,DashPhase,false,false);
    FPoseContext Authored(this);Authored.ResetToRefPose();FAnimationPoseData Data(Authored),Result(Output);
    DashBlend->GetAnimationPose(Samples,FAnimExtractContext(0.0,false),Data);
    FAnimationRuntime::BlendTwoPosesTogetherInPlace(Result,Data,1.f-DashWeight);
   }
  }
  // Only the torso and free arm attack; the supporting hand and legs retain glide authority.
  if(GlideAttackClip && GlideAttackWeight>.0001f)
  {
   FPoseContext Authored(this);Authored.ResetToRefPose();FAnimationPoseData Data(Authored);
   GlideAttackClip->GetAnimationPose(Data,FAnimExtractContext(double(GlideAttackTime),false));
   const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
   for(const FName Name:{FName(TEXT("spine_01")),FName(TEXT("spine_02")),FName(TEXT("spine_03")),FName(TEXT("clavicle_r")),FName(TEXT("upperarm_r")),FName(TEXT("lowerarm_r")),FName(TEXT("hand_r"))})
   {
    const int32 Bone=Bones.GetPoseBoneIndexForBoneName(Name);if(Bone==INDEX_NONE) continue;
    const FCompactPoseBoneIndex Index=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bone));if(Index.GetInt()==INDEX_NONE) continue;
    FTransform Blended;Blended.Blend(Output.Pose[Index],Authored.Pose[Index],GlideAttackWeight);Output.Pose[Index]=Blended;
   }
  }
  if(WeaponStepClip && WeaponStepWeight>.0001f)
  {
   FPoseContext Step(this);Step.ResetToRefPose();FAnimationPoseData Data(Step);
   WeaponStepClip->GetAnimationPose(Data,FAnimExtractContext(double(WeaponStepTime),false));
   const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
   for(const FName Name:{FName(TEXT("thigh_l")),FName(TEXT("calf_l")),FName(TEXT("foot_l")),FName(TEXT("ball_l")),FName(TEXT("thigh_r")),FName(TEXT("calf_r")),FName(TEXT("foot_r")),FName(TEXT("ball_r"))})
   {
    const int32 Bone=Bones.GetPoseBoneIndexForBoneName(Name);if(Bone==INDEX_NONE) continue;
    const FCompactPoseBoneIndex Index=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bone));if(Index.GetInt()==INDEX_NONE) continue;
    auto& Transform=Output.Pose[Index];Transform.SetRotation(FQuat::Slerp(Transform.GetRotation(),Step.Pose[Index].GetRotation(),WeaponStepWeight).GetNormalized());
   }
  }
  if(DefenseClip && DefenseWeight>.0001f)
  {
   FPoseContext Authored(this);Authored.ResetToRefPose();FAnimationPoseData Data(Authored);
   DefenseClip->GetAnimationPose(Data,FAnimExtractContext(double(DefenseTime),false));
   const FBoneContainer& Bones=Output.Pose.GetBoneContainer();const int32 SpineBone=Bones.GetPoseBoneIndexForBoneName(TEXT("spine_01"));
   const FCompactPoseBoneIndex Spine=SpineBone!=INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SpineBone)):FCompactPoseBoneIndex(INDEX_NONE);
   if(Spine.GetInt()!=INDEX_NONE) for(int32 Bone=0;Bone<Bones.GetCompactPoseNumBones();++Bone)
   {
    const FCompactPoseBoneIndex Index(Bone);bool Upper=false;
    for(auto Parent=Index;Parent.GetInt()!=INDEX_NONE;Parent=Bones.GetParentBoneIndex(Parent)) if(Parent==Spine) { Upper=true;break; }
    if(Upper) { FTransform Mixed;Mixed.Blend(Output.Pose[Index],Authored.Pose[Index],DefenseWeight);Output.Pose[Index]=Mixed; }
   }
   WeaponArmWeight*=1.f-DefenseWeight;
  }
  FCSPose<FCompactPose> Pose;Pose.InitPose(Output.Pose);
  const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
 if(bRelaxedGlide && HangWeight>.0001f)
  {
   const int32 Pelvis=Bones.GetPoseBoneIndexForBoneName(TEXT("pelvis"));
   if(Pelvis!=INDEX_NONE)
   {
    const FCompactPoseBoneIndex Index=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Pelvis));
    if(Index.GetInt()!=INDEX_NONE)
    {
     FTransform Transform=Pose.GetComponentSpaceTransform(Index);const FVector Pivot=Grips[0]-Lead;
     Transform.SetLocation(Pivot+BodyTilt.RotateVector(Transform.GetLocation()-Pivot)+Lead);
     Transform.SetRotation((BodyTilt*Transform.GetRotation()).GetNormalized());
     const FBoneTransform Change(Index,Transform);Pose.LocalBlendCSBoneTransforms(MakeArrayView(&Change,1),HangWeight);
    }
   }
   if(!GlidePose)
   {
    // Settle the body's weight below its supporting hand, rather than bending
    // the elbow to compensate for differences in the incoming falling pose.
    if(bGlideHands && Pelvis!=INDEX_NONE)
    {
     const int32 ShoulderBone=Bones.GetPoseBoneIndexForBoneName(TEXT("upperarm_l")),ElbowBone=Bones.GetPoseBoneIndexForBoneName(TEXT("lowerarm_l")),HandBone=Bones.GetPoseBoneIndexForBoneName(TEXT("hand_l"));
     if(ShoulderBone!=INDEX_NONE && ElbowBone!=INDEX_NONE && HandBone!=INDEX_NONE)
     {
      const FCompactPoseBoneIndex Shoulder=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(ShoulderBone)),Elbow=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(ElbowBone)),Hand=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandBone)),Root=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Pelvis));
      if(Shoulder.GetInt()!=INDEX_NONE && Elbow.GetInt()!=INDEX_NONE && Hand.GetInt()!=INDEX_NONE && Root.GetInt()!=INDEX_NONE)
      {
       const FVector S=Pose.GetComponentSpaceTransform(Shoulder).GetLocation(),E=Pose.GetComponentSpaceTransform(Elbow).GetLocation(),H=Pose.GetComponentSpaceTransform(Hand).GetLocation();
       const FVector Away=S-Grips[0];const float Length=(E-S).Size()+(H-E).Size();
       const FVector Settle=(Away.GetSafeNormal()*(Length*LimbExtension-Away.Size())).GetClampedToMaxSize(10.f);
       FTransform Transform=Pose.GetComponentSpaceTransform(Root);Transform.AddToTranslation(Settle);
       const FBoneTransform Change(Root,Transform);Pose.LocalBlendCSBoneTransforms(MakeArrayView(&Change,1),HangWeight);
      }
     }
    }
    // Let the free arm and both legs hang at their natural bone lengths.
    // An authored body clip replaces these defaults; no original animation is rewritten.
    const FName Chains[3][3]={{TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")},
      {TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l")},{TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r")}};
    const FVector HangingDown=FQuat::Slerp(FQuat::Identity,BodyTilt,GlideWeight).RotateVector(Down);
    for(int32 Chain=0;Chain<3;++Chain)
    {
     TArray<FCompactPoseBoneIndex> Indices;
     for(const FName Name:Chains[Chain])
     {
      const int32 Index=Bones.GetPoseBoneIndexForBoneName(Name);
      if(Index!=INDEX_NONE) Indices.Add(Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index)));
     }
     if(Indices.Num()!=3 || Indices.Contains(FCompactPoseBoneIndex(INDEX_NONE))) continue;
     FTransform Upper=Pose.GetComponentSpaceTransform(Indices[0]),Lower=Pose.GetComponentSpaceTransform(Indices[1]),End=Pose.GetComponentSpaceTransform(Indices[2]);
     const FVector OldDirection=(End.GetLocation()-Lower.GetLocation()).GetSafeNormal();
     const float Length=(Lower.GetLocation()-Upper.GetLocation()).Size()+(End.GetLocation()-Lower.GetLocation()).Size();
     const FVector Direction=(HangingDown+(Chain==0 ? Right*.04f:Forward*-.035f)).GetSafeNormal();
     const FVector Goal=Upper.GetLocation()+Direction*(Length*LimbExtension);
     const FVector Pole=Upper.GetLocation()+(Chain==0 ? -Forward:Forward)*Length;
     AnimationCore::SolveTwoBoneIK(Upper,Lower,End,Pole,Goal,false,1.0,1.0);
     End.SetRotation((FQuat::FindBetweenNormals(OldDirection,(End.GetLocation()-Lower.GetLocation()).GetSafeNormal())*End.GetRotation()).GetNormalized());
     TArray<FBoneTransform> Changes={FBoneTransform(Indices[0],Upper),FBoneTransform(Indices[1],Lower),FBoneTransform(Indices[2],End)};
     Pose.LocalBlendCSBoneTransforms(Changes,HangWeight*(1.f-MotionWeight)*(Chain==0 ? 1.f-GlideAttackWeight:1.f));
    }
   }
  }
  // Rotate the rendered body around its pelvis, never the capsule or camera.
  // Applying the sampled quaternion directly preserves the complete CCW turn.
  const FQuat BodySpin=FQuat(FVector::UpVector,FMath::DegreesToRadians(WeaponBodyTurn))*AirSpin;
  if(!BodySpin.Equals(FQuat::Identity,.00001f))
  {
   const int32 Pelvis=Bones.GetPoseBoneIndexForBoneName(TEXT("pelvis"));
   if(Pelvis!=INDEX_NONE)
   {
    const FCompactPoseBoneIndex Index=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Pelvis));
    if(Index.GetInt()!=INDEX_NONE)
    {
     FTransform Transform=Pose.GetComponentSpaceTransform(Index);Transform.SetRotation((BodySpin*Transform.GetRotation()).GetNormalized());
     const FBoneTransform Change(Index,Transform);Pose.LocalBlendCSBoneTransforms(MakeArrayView(&Change,1),1.f);
    }
   }
  }
  if(WireWeight>.0001f && !WireTilt.Equals(FQuat::Identity,.00001f))
  {
   const int32 Pelvis=Bones.GetPoseBoneIndexForBoneName(TEXT("pelvis"));
   if(Pelvis!=INDEX_NONE)
   {
    const FCompactPoseBoneIndex Index=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Pelvis));
    if(Index.GetInt()!=INDEX_NONE)
    {
     FTransform Transform=Pose.GetComponentSpaceTransform(Index);Transform.SetRotation((WireTilt*Transform.GetRotation()).GetNormalized());
     const FBoneTransform Change(Index,Transform);Pose.LocalBlendCSBoneTransforms(MakeArrayView(&Change,1),WireWeight);
    }
   }
  }
  const FName Names[]={TEXT("neck_01"),TEXT("neck_02"),TEXT("head")};
  const float Shares[]={.2f,.25f,.55f};
  for (int32 I=0;I<3;++I)
  {
   const int32 MeshIndex=Bones.GetPoseBoneIndexForBoneName(Names[I]);
   if (MeshIndex==INDEX_NONE) continue;
   const FCompactPoseBoneIndex Index=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
   if (Index.GetInt()==INDEX_NONE) continue;
   FTransform Transform=Pose.GetComponentSpaceTransform(Index);
   Transform.SetRotation((FQuat::Slerp(FQuat::Identity,Offset,Shares[I])*Transform.GetRotation()).GetNormalized());
   const FBoneTransform Change(Index,Transform);
   Pose.LocalBlendCSBoneTransforms(MakeArrayView(&Change,1),(1.f-MotionWeight*.9f)*(1.f-DefenseWeight*.9f));
  }
  if(WeaponArmWeight>.0001f)
  {
   const int32 S=Bones.GetPoseBoneIndexForBoneName(TEXT("upperarm_r")),E=Bones.GetPoseBoneIndexForBoneName(TEXT("lowerarm_r")),H=Bones.GetPoseBoneIndexForBoneName(TEXT("hand_r"));
   if(S!=INDEX_NONE && E!=INDEX_NONE && H!=INDEX_NONE)
   {
    const auto Shoulder=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(S)),Elbow=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(E)),Hand=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(H));
    if(Shoulder.GetInt()!=INDEX_NONE && Elbow.GetInt()!=INDEX_NONE && Hand.GetInt()!=INDEX_NONE)
    {
     FTransform Upper=Pose.GetComponentSpaceTransform(Shoulder),Lower=Pose.GetComponentSpaceTransform(Elbow),End=Pose.GetComponentSpaceTransform(Hand);
     const FVector OldDirection=(End.GetLocation()-Lower.GetLocation()).GetSafeNormal();
     AnimationCore::SolveTwoBoneIK(Upper,Lower,End,WeaponElbow,WeaponHand,false,1.0,1.0);
     End.SetRotation((FQuat::FindBetweenNormals(OldDirection,(End.GetLocation()-Lower.GetLocation()).GetSafeNormal())*End.GetRotation()).GetNormalized());
     const TArray<FBoneTransform> Changes={FBoneTransform(Shoulder,Upper),FBoneTransform(Elbow,Lower),FBoneTransform(Hand,End)};
     Pose.LocalBlendCSBoneTransforms(Changes,WeaponArmWeight);
    }
   }
  }
  if (bGlideHands && HangWeight>.0001f)
  {
   const FName ArmNames[2][3]={{TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l")},{TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")}};
   for (int32 Side=0;Side<1;++Side)
   {
    TArray<FCompactPoseBoneIndex> Indices;
    for (const FName Name:ArmNames[Side])
    {
     const int32 Index=Bones.GetPoseBoneIndexForBoneName(Name);
     if (Index!=INDEX_NONE) Indices.Add(Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index)));
    }
    if (Indices.Num()!=3 || Indices.Contains(FCompactPoseBoneIndex(INDEX_NONE))) continue;
    FTransform Upper=Pose.GetComponentSpaceTransform(Indices[0]),Lower=Pose.GetComponentSpaceTransform(Indices[1]),Hand=Pose.GetComponentSpaceTransform(Indices[2]);
    AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,Elbows[Side],Grips[Side],false,1.0,1.0);
    TArray<FBoneTransform> Changes={FBoneTransform(Indices[0],Upper),FBoneTransform(Indices[1],Lower),FBoneTransform(Indices[2],Hand)};
    Pose.LocalBlendCSBoneTransforms(Changes,HangWeight);
   }
  }
  if(EdgeHandWeight>.0001f)
  {
   const FName Chains[2][3]={{TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l")},{TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")}};
   for(int32 Side=0;Side<2;++Side)
   {
    TArray<FCompactPoseBoneIndex> Indices;
    for(FName Name:Chains[Side])
    {
     const int32 Index=Bones.GetPoseBoneIndexForBoneName(Name);
     if(Index!=INDEX_NONE) Indices.Add(Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index)));
    }
    if(Indices.Num()!=3 || Indices.Contains(FCompactPoseBoneIndex(INDEX_NONE))) continue;
    FTransform A=Pose.GetComponentSpaceTransform(Indices[0]),B=Pose.GetComponentSpaceTransform(Indices[1]),C=Pose.GetComponentSpaceTransform(Indices[2]);
    AnimationCore::SolveTwoBoneIK(A,B,C,EdgeElbows[Side],EdgeHands[Side],false,1.,1.);
    TArray<FBoneTransform> Changes={FBoneTransform(Indices[0],A),FBoneTransform(Indices[1],B),FBoneTransform(Indices[2],C)};
    Pose.LocalBlendCSBoneTransforms(Changes,EdgeHandWeight);
   }
  }
  if(FootPlantWeight>.0001f)
  {
   TArray<FCompactPoseBoneIndex> Indices;
   for(FName Name:{FName(TEXT("thigh_r")),FName(TEXT("calf_r")),FName(TEXT("foot_r"))})
   {
    const int32 Index=Bones.GetPoseBoneIndexForBoneName(Name);
    if(Index!=INDEX_NONE) Indices.Add(Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index)));
   }
   if(Indices.Num()==3 && !Indices.Contains(FCompactPoseBoneIndex(INDEX_NONE)))
   {
    FTransform A=Pose.GetComponentSpaceTransform(Indices[0]),B=Pose.GetComponentSpaceTransform(Indices[1]),C=Pose.GetComponentSpaceTransform(Indices[2]);
    AnimationCore::SolveTwoBoneIK(A,B,C,ClimbKnee,ClimbFoot,false,1.,1.);
    FQuat Flat=FQuat::Identity;
    for(FCompactPoseBoneIndex Bone=Indices[2];Bone.GetInt()!=INDEX_NONE;Bone=Bones.GetParentBoneIndex(Bone)) Flat=Bones.GetRefPoseTransform(Bone).GetRotation()*Flat;
    C.SetRotation(Flat.GetNormalized());
    TArray<FBoneTransform> Changes={FBoneTransform(Indices[0],A),FBoneTransform(Indices[1],B),FBoneTransform(Indices[2],C)};
    Pose.LocalBlendCSBoneTransforms(Changes,FootPlantWeight);
   }
  }
  FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(Pose),Output.Pose);
  return true;
 }
};
}
FAnimInstanceProxy* UBBSBHeadGazeAnimInstance::CreateAnimInstanceProxy() { return new FHeadGazeProxy(this); }
void UBBSBHeadGazeAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif
bool UBBSBGazeAuthoringTools::ConfigurePostProcessInput(UObject* BlueprintAsset)
{
#if WITH_EDITOR
 UAnimBlueprint* Blueprint=Cast<UAnimBlueprint>(BlueprintAsset);
 if (!Blueprint || Blueprint->ParentClass!=UBBSBHeadGazeAnimInstance::StaticClass()) return false;
 TArray<UEdGraph*> Graphs;Blueprint->GetAllGraphs(Graphs);
 for (UEdGraph* Graph:Graphs)
 {
  if (Graph->GetFName()!=TEXT("AnimGraph")) continue;
  TArray<UAnimGraphNode_Root*> Roots;Graph->GetNodesOfClass(Roots);
  if (Roots.Num()!=1) return false;
  TArray<UAnimGraphNode_LinkedInputPose*> Inputs;Graph->GetNodesOfClass(Inputs);
  UAnimGraphNode_LinkedInputPose* Input=Inputs.Num() ? Inputs[0]:nullptr;
  if (!Input)
  {
   FGraphNodeCreator<UAnimGraphNode_LinkedInputPose> Creator(*Graph);
   Input=Creator.CreateNode();Input->NodePosX=-300;Input->Node.Name=FAnimNode_LinkedInputPose::DefaultInputPoseName;Creator.Finalize();
  }
  UEdGraphPin* Source=nullptr;UEdGraphPin* Target=nullptr;
  for (UEdGraphPin* Pin:Input->Pins) if (Pin->Direction==EGPD_Output && Pin->PinType.PinSubCategoryObject.Get()==FPoseLink::StaticStruct()) Source=Pin;
  for (UEdGraphPin* Pin:Roots[0]->Pins) if (Pin->Direction==EGPD_Input && Pin->PinType.PinSubCategoryObject.Get()==FPoseLink::StaticStruct()) Target=Pin;
  if (!Source || !Target || !Graph->GetSchema()->TryCreateConnection(Source,Target)) return false;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);return true;
 }
#endif
 return false;
}
