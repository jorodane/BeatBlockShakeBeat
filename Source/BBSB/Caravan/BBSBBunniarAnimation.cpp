#include "Caravan/BBSBBunniarAnimation.h"
#include "Caravan/BBSBCaravan.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UAnimSequence* UBBSBBunniarAnimationSet::Clip(int32 Index) const
{
 UAnimSequence* Clips[]={Idle,Travel,Equipped,Attached}; return Clips[FMath::Clamp(Index,0,3)];
}

namespace
{
 struct FBunniarPoseProxy final : FAnimInstanceProxy
 {
  FBunniarPoseProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance) {}
  FAnimNode_Base Root;
  UAnimSequence* Clips[4]={};
  double Times[4]={};
  float Weights[4]={1,0,0,0};
  EBBSBBunniarMotion PreviousMotion=EBBSBBunniarMotion::Idle;
  virtual FAnimNode_Base* GetCustomRootNode() override { return &Root; }
  virtual void PreUpdate(UAnimInstance* Instance,float DT) override
  {
   FAnimInstanceProxy::PreUpdate(Instance,DT);
   const auto* Anim=CastChecked<UBBSBBunniarAnimInstance>(Instance);
   const auto* Set=Anim->AnimationSet.Get();
   const bool EnteringAttached=Anim->Motion==EBBSBBunniarMotion::Attached && PreviousMotion!=EBBSBBunniarMotion::Attached;
   PreviousMotion=Anim->Motion;
   const float Step=1-FMath::Exp(-FMath::Max(0.f,DT)*5.f/FMath::Max(Set ? Set->BlendSeconds:.16f,.01f));
   for (int32 I=0;I<4;++I)
   {
    UAnimSequence* Next=Set ? Set->Clip(I):nullptr;
    if (Next!=Clips[I]) { Clips[I]=Next; Times[I]=0; }
    if (I==int32(EBBSBBunniarMotion::Attached))
    {
     // Fold once per attachment, then retain the authored end pose until release.
     if (EnteringAttached) Times[I]=0;
     if (Anim->Motion==EBBSBBunniarMotion::Attached)
      Times[I]=Next ? FMath::Clamp(Times[I]+DT,0.0,double(Next->GetPlayLength())):0;
    }
    else Times[I]=Next && Next->GetPlayLength()>0 ? FMath::Fmod(Times[I]+DT,Next->GetPlayLength()):0;
    Weights[I]=FMath::Lerp(Weights[I],int32(Anim->Motion)==I ? 1.f:0.f,Step);
   }
  }
  virtual bool Evaluate(FPoseContext& Output) override
  {
   Output.ResetToRefPose(); float Sum=0;
   FAnimationPoseData Result(Output);
   for (int32 I=0;I<4;++I)
   {
    if (!Clips[I] || Weights[I]<.0001f) continue;
    FPoseContext Pose(this); Pose.ResetToRefPose(); FAnimationPoseData Data(Pose);
    Clips[I]->GetAnimationPose(Data,FAnimExtractContext(Times[I],false));
    if (Sum==0) { Output.Pose=Pose.Pose; Output.Curve=Pose.Curve; Output.CustomAttributes=Pose.CustomAttributes; }
    else FAnimationRuntime::BlendTwoPosesTogetherInPlace(Result,Data,Sum/(Sum+Weights[I]));
    Sum+=Weights[I];
   }
   Output.Pose.NormalizeRotations(); return true;
  }
 };
}

void UBBSBBunniarAnimInstance::NativeInitializeAnimation()
{
 Super::NativeInitializeAnimation();
 if (auto* Buddy=Cast<ABBSBBunniarCompanion>(GetOwningActor())) AnimationSet=Buddy->AnimationSet;
 if (!AnimationSet) AnimationSet=LoadObject<UBBSBBunniarAnimationSet>(nullptr,TEXT("/Game/BBSB/AnimationAuthoring/Bunniar/DA_BunniarAnimation.DA_BunniarAnimation"));
}
void UBBSBBunniarAnimInstance::NativeUpdateAnimation(float DT)
{
 Super::NativeUpdateAnimation(DT);
 Motion=EBBSBBunniarMotion::Idle;
 if (const auto* Buddy=Cast<ABBSBBunniarCompanion>(GetOwningActor()))
 {
  AnimationSet=Buddy->AnimationSet;
  const auto* Player=UGameplayStatics::GetPlayerCharacter(Buddy,0);
  Motion=Buddy->bSurfaceAttached ? EBBSBBunniarMotion::Attached:Buddy->bEquipped ? EBBSBBunniarMotion::Equipped:
   Player && Player->GetVelocity().Size2D()>(AnimationSet ? AnimationSet->TravelSpeedThreshold:60) ? EBBSBBunniarMotion::Travel:EBBSBBunniarMotion::Idle;
 }
 else if (const auto* Performer=Cast<ABBSBCaravanPerformer>(GetOwningActor())) Motion=Performer->bDefeated ? EBBSBBunniarMotion::Idle:EBBSBBunniarMotion::Equipped;
}
FAnimInstanceProxy* UBBSBBunniarAnimInstance::CreateAnimInstanceProxy() { return new FBunniarPoseProxy(this); }
void UBBSBBunniarAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
