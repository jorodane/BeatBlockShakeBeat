#include "Character/BBSBAimGaze.h"
#include "BBSBCharacter.h"
#include "Combat/BBSBCombat.h"
#include "Field/BBSBFieldActions.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"

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
 AimPoint=Player->FieldActions->VisualAimPoint();
 const FRotator Relative=(Player->GetActorQuat().UnrotateVector(AimPoint-Player->GetMesh()->GetSocketLocation(TEXT("head")))).Rotation();
 const bool Enabled=bEnableHeadGaze && !(Player->Combat && Player->Combat->bDead) && !Player->FieldActions->IsDodging();
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
 virtual void PreUpdate(UAnimInstance* Instance,float DT) override
 {
  FAnimInstanceProxy::PreUpdate(Instance,DT);
  Input=Instance->GetLinkedInputPoseNode();
  const auto* Player=Cast<ABBSBCharacter>(Instance->GetOwningActor());
  Offset=Player && Player->AimGaze ? Player->AimGaze->ComponentAimRotation:FQuat::Identity;
 }
 virtual bool Evaluate(FPoseContext& Output) override
 {
  if (!Input) return false;
  Input->Evaluate_AnyThread(Output);
  if (Offset.Equals(FQuat::Identity,.00001f)) return true;
  FCSPose<FCompactPose> Pose;Pose.InitPose(Output.Pose);
  const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
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
   Pose.LocalBlendCSBoneTransforms(MakeArrayView(&Change,1),1.f);
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
