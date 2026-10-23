#include "Interaction/BBSBInteraction.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
 struct FNPCPoseProxy final : FAnimInstanceProxy
 {
  FNPCPoseProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance) {}
  FAnimNode_Base Root;
  UAnimSequence* Idle=nullptr;
  double Time=0;
  FQuat HeadOffset=FQuat::Identity;
  virtual FAnimNode_Base* GetCustomRootNode() override { return &Root; }
  virtual void PreUpdate(UAnimInstance* Instance,float DT) override
  {
   FAnimInstanceProxy::PreUpdate(Instance,DT);
   if(const auto* NPC=Cast<ABBSBInteractionNPC>(Instance->GetOwningActor()))
   {
    Idle=NPC->IdleAnimation;Time=Idle && Idle->GetPlayLength()>0 ? FMath::Fmod(Time+DT,Idle->GetPlayLength()):0;
    // Mesh has the standard mannequin -90-degree relative yaw.
    const FQuat Mesh=NPC->Body->GetRelativeRotation().Quaternion();
    HeadOffset=(Mesh.Inverse()*NPC->HeadReaction.Quaternion()*Mesh).GetNormalized();
   }
  }
  virtual bool Evaluate(FPoseContext& Output) override
  {
   Output.ResetToRefPose();
   if(Idle) { FAnimationPoseData Data(Output);Idle->GetAnimationPose(Data,FAnimExtractContext(Time,false)); }
   const FBoneContainer& Bones=Output.Pose.GetBoneContainer();const int32 SkeletonIndex=Bones.GetReferenceSkeleton().FindBoneIndex(TEXT("head"));
   if(SkeletonIndex==INDEX_NONE) return true;
   const FCompactPoseBoneIndex Index=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SkeletonIndex));
   if(Index.GetInt()<0) return true;
   FCSPose<FCompactPose> CS;CS.InitPose(Output.Pose);FTransform Head=CS.GetComponentSpaceTransform(Index);Head.SetRotation((HeadOffset*Head.GetRotation()).GetNormalized());
   TArray<FBoneTransform> Modified;Modified.Emplace(Index,Head);CS.LocalBlendCSBoneTransforms(Modified,1.f);FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS),Output.Pose);Output.Pose.NormalizeRotations();return true;
  }
 };
}
FAnimInstanceProxy* UBBSBNPCAnimInstance::CreateAnimInstanceProxy() { return new FNPCPoseProxy(this); }
void UBBSBNPCAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
