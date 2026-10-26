#include "Character/BBSBMovementAuthoring.h"
#include "Animation/AnimInstance.h"
#include "Animation/BlendSpace.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchFeatureChannel_Trajectory.h"
#include "PoseSearch/PoseSearchFeatureChannel_Pose.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_MotionMatching.h"
#include "AnimGraphNode_PoseSearchHistoryCollector.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_StateMachine.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

namespace
{
 const FName HistoryTag(TEXT("BBSB_MoveHistory"));
 FAnimNode_MotionMatching* FindMatchNode(UAnimInstance* Instance)
 {
  if(Instance) for(TFieldIterator<FStructProperty> It(Instance->GetClass());It;++It)
   if(It->Struct==FAnimNode_MotionMatching::StaticStruct()) return It->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(Instance);
  return nullptr;
 }
#if WITH_EDITOR
 UEdGraphPin* PosePin(UEdGraphNode* Node,EEdGraphPinDirection Direction)
 {
  for(auto* Pin:Node->Pins) if(Pin->Direction==Direction && Pin->PinType.PinSubCategoryObject==FPoseLink::StaticStruct()) return Pin;
  return nullptr;
 }
 template<typename T> T* MakeNode(UEdGraph* Graph,int32 X,int32 Y)
 {
  auto* Node=NewObject<T>(Graph,NAME_None,RF_Transactional);Graph->AddNode(Node,false,false);Node->CreateNewGuid();Node->PostPlacedNewNode();Node->AllocateDefaultPins();Node->NodePosX=X;Node->NodePosY=Y;return Node;
 }
#endif
}

bool UBBSBMovementAuthoring::RefreshBlendSpace(UBlendSpace* BlendSpace)
{
#if WITH_EDITOR
 if(BlendSpace) { BlendSpace->ResampleData();BlendSpace->MarkPackageDirty();return true; }
#endif
 return false;
}
TArray<float> UBBSBMovementAuthoring::BlendWeights(UBlendSpace* BlendSpace,FVector Input)
{
 TArray<float> Weights;if(!BlendSpace) return Weights;
 Weights.Init(0.f,BlendSpace->GetBlendSamples().Num());TArray<FBlendSampleData> Samples;int32 Index=INDEX_NONE;
 if(BlendSpace->GetSamplesFromBlendInput(Input,Samples,Index,true)) for(const auto& Sample:Samples) if(Weights.IsValidIndex(Sample.SampleDataIndex)) Weights[Sample.SampleDataIndex]=Sample.TotalWeight;
 return Weights;
}
bool UBBSBMovementAuthoring::ConfigureMotionDatabase(UPoseSearchSchema* Schema,UPoseSearchDatabase* Database,UBlendSpace* BlendSpace,bool bDash)
{
#if WITH_EDITOR
 if(!Schema || !Database || !BlendSpace) return false;
 Schema->Modify();Database->Modify();
 for(const FName Property:{FName(TEXT("Channels")),FName(TEXT("Skeletons"))})
 {
  auto* Array=FindFProperty<FArrayProperty>(Schema->GetClass(),Property);if(!Array) return false;
  FScriptArrayHelper(Array,Array->ContainerPtrToValuePtr<void>(Schema)).EmptyValues();
 }
 Schema->AddSkeleton(BlendSpace->GetSkeleton());Schema->SampleRate=bDash ? 60:30;
 auto* Trajectory=NewObject<UPoseSearchFeatureChannel_Trajectory>(Schema);Trajectory->Weight=bDash ? 5.f:3.f;Trajectory->Samples.Reset();
 for(const float Offset: {0.1f,0.2f,0.3f})
 {
  auto& Sample=Trajectory->Samples.AddDefaulted_GetRef();Sample.Offset=Offset;Sample.Flags=int32(EPoseSearchTrajectoryFlags::PositionXY)|int32(EPoseSearchTrajectoryFlags::VelocityXY);Sample.Weight=1.f;
 }
 Schema->AddChannel(Trajectory);
 auto* Pose=NewObject<UPoseSearchFeatureChannel_Pose>(Schema);Pose->Weight=bDash ? .15f:1.f;Pose->InputQueryPose=EInputQueryPose::UseCharacterPose;Pose->SampledBones.Reset();
 for(const FName Bone: {FName(TEXT("pelvis")),FName(TEXT("foot_l")),FName(TEXT("foot_r"))})
 {
  auto& Sample=Pose->SampledBones.AddDefaulted_GetRef();Sample.Reference.BoneName=Bone;Sample.Flags=int32(EPoseSearchBoneFlags::Position)|int32(EPoseSearchBoneFlags::Velocity);Sample.Weight=Bone==TEXT("pelvis") ? .5f:1.f;
 }
 Schema->AddChannel(Pose);Schema->PostEditChange();Schema->MarkPackageDirty();Database->Schema=Schema;
 while(Database->GetNumAnimationAssets()>0) Database->RemoveAnimationAssetAt(Database->GetNumAnimationAssets()-1);
 FPoseSearchDatabaseAnimationAsset Entry;Entry.AnimAsset=BlendSpace;Entry.bUseGridForSampling=false;Entry.NumberOfHorizontalSamples=16;Entry.NumberOfVerticalSamples=bDash ? 1:3;
 if(bDash) Entry.SamplingRange=FFloatInterval(0.f,.085f);
 Database->AddAnimationAsset(Entry);Database->PoseSearchMode=EPoseSearchMode::BruteForce;Database->ContinuingPoseCostBias=-.03f;Database->MarkPackageDirty();
 using namespace UE::PoseSearch;
 return FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database,ERequestAsyncBuildFlag::NewRequest|ERequestAsyncBuildFlag::WaitForCompletion)==EAsyncBuildIndexResult::Success && Database->GetSearchIndex().GetNumPoses()>0;
#else
 return false;
#endif
}
bool UBBSBMovementAuthoring::InstallMotionMatching(UObject* BlueprintAsset,UPoseSearchDatabase* Database)
{
#if WITH_EDITOR
 auto* Blueprint=Cast<UAnimBlueprint>(BlueprintAsset);if(!Blueprint || !Database) return false;
 TArray<UEdGraph*> Graphs;Blueprint->GetAllGraphs(Graphs);UEdGraph* RunGraph=nullptr;UEdGraph* MainGraph=nullptr;
 for(auto* Graph:Graphs) { if(Graph->GetName()==TEXT("Walk / Run")) RunGraph=Graph;if(Graph->GetName()==TEXT("AnimGraph")) MainGraph=Graph; }
 if(!RunGraph || !MainGraph) return false;Blueprint->Modify();RunGraph->Modify();MainGraph->Modify();
 UAnimGraphNode_MotionMatching* Match=nullptr;UAnimGraphNode_StateResult* Result=nullptr;
 for(const auto& Node:RunGraph->Nodes) { if(auto* Found=Cast<UAnimGraphNode_MotionMatching>(Node)) Match=Found;if(auto* Found=Cast<UAnimGraphNode_StateResult>(Node)) Result=Found; }
 if(!Result) return false;
 if(!Match) Match=MakeNode<UAnimGraphNode_MotionMatching>(RunGraph,-250,0);
 auto* MatchProperty=FindFProperty<FStructProperty>(Match->GetClass(),TEXT("Node"));if(!MatchProperty) return false;void* Data=MatchProperty->ContainerPtrToValuePtr<void>(Match);
 auto* DatabaseProperty=FindFProperty<FObjectPropertyBase>(MatchProperty->Struct,TEXT("Database"));auto* BlendTime=FindFProperty<FFloatProperty>(MatchProperty->Struct,TEXT("BlendTime"));auto* Reselect=FindFProperty<FFloatProperty>(MatchProperty->Struct,TEXT("PoseReselectHistory"));auto* MaxBlends=FindFProperty<FIntProperty>(MatchProperty->Struct,TEXT("MaxActiveBlends"));
 if(!DatabaseProperty || !BlendTime || !Reselect || !MaxBlends) return false;
 DatabaseProperty->SetObjectPropertyValue_InContainer(Data,Database);BlendTime->SetPropertyValue_InContainer(Data,.09f);Reselect->SetPropertyValue_InContainer(Data,.12f);MaxBlends->SetPropertyValue_InContainer(Data,4);
 if(auto* DatabasePin=Match->FindPin(TEXT("Database"))) DatabasePin->DefaultObject=Database;
 Match->SetTag(TEXT("BBSB_GroundMotionMatching"));
 auto* RunInput=PosePin(Result,EGPD_Input);auto* MatchOutput=PosePin(Match,EGPD_Output);if(!RunInput || !MatchOutput) return false;
 RunGraph->GetSchema()->BreakPinLinks(*RunInput,true);if(!RunGraph->GetSchema()->TryCreateConnection(MatchOutput,RunInput)) return false;
 UAnimGraphNode_PoseSearchHistoryCollector* Collector=nullptr;UAnimGraphNode_StateMachine* Locomotion=nullptr;
 for(const auto& Node:MainGraph->Nodes) { if(auto* Found=Cast<UAnimGraphNode_PoseSearchHistoryCollector>(Node)) Collector=Found;if(auto* Found=Cast<UAnimGraphNode_StateMachine>(Node)) Locomotion=Found; }
 if(!Locomotion) return false;
 if(!Collector)
 {
  auto* Out=PosePin(Locomotion,EGPD_Output);if(!Out) return false;const auto Consumers=Out->LinkedTo;
  Collector=MakeNode<UAnimGraphNode_PoseSearchHistoryCollector>(MainGraph,Locomotion->NodePosX+230,Locomotion->NodePosY);
  auto* In=PosePin(Collector,EGPD_Input);auto* Collected=PosePin(Collector,EGPD_Output);if(!In || !Collected) return false;
  MainGraph->GetSchema()->BreakPinLinks(*Out,true);if(!MainGraph->GetSchema()->TryCreateConnection(Out,In)) return false;
  for(auto* Consumer:Consumers) if(!MainGraph->GetSchema()->TryCreateConnection(Collected,Consumer)) return false;
 }
 auto* CollectorProperty=FindFProperty<FStructProperty>(Collector->GetClass(),TEXT("Node"));if(!CollectorProperty) return false;auto* History=CollectorProperty->ContainerPtrToValuePtr<FAnimNode_PoseSearchHistoryCollector>(Collector);
 History->bGenerateTrajectory=true;History->PoseCount=24;History->SamplingInterval=1.f/60.f;History->TrajectoryPredictionCount=8;History->PredictionSamplingInterval=.1f;
 History->CollectedBones.Reset();for(const FName Bone:{FName(TEXT("root")),FName(TEXT("pelvis")),FName(TEXT("foot_l")),FName(TEXT("foot_r"))}) { FBoneReference Reference;Reference.BoneName=Bone;History->CollectedBones.Add(Reference); }
 Collector->SetTag(HistoryTag);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);FKismetEditorUtilities::CompileBlueprint(Blueprint);
 Blueprint->MarkPackageDirty();return Blueprint->Status!=BS_Error;
#else
 return false;
#endif
}
FBBSBMotionMatchDebug UBBSBMovementAuthoring::MotionMatchingDebug(UAnimInstance* Instance)
{
 FBBSBMotionMatchDebug Debug;
 if(auto* Node=FindMatchNode(Instance))
 {
  const auto& Result=Node->GetMotionMatchingState().SearchResult;Debug.bValid=Result.SelectedAnim!=nullptr;Debug.Animation=GetPathNameSafe(Result.SelectedAnim);Debug.Database=GetPathNameSafe(Result.SelectedDatabase);Debug.BlendParameters=Result.BlendParameters;Debug.Time=Result.SelectedTime;Debug.Cost=Result.SearchCost;
  if(Result.SelectedDatabase) Debug.IndexedPoses=Result.SelectedDatabase->GetSearchIndex().GetNumPoses();
 }
 if(Instance) if(const auto* Collector=UPoseSearchLibrary::FindPoseHistoryNode(HistoryTag,Instance)) if(const auto* History=Collector->GetPoseHistoryPtr()) Debug.HistoryEntries=History->GetNumEntries();
 return Debug;
}
void UBBSBMovementAuthoring::MatchAfterDash(UAnimInstance* Instance)
{
 if(Instance && UPoseSearchLibrary::FindPoseHistoryNode(HistoryTag,Instance))
 {
  UPoseSearchLibrary::OverridePoseHistoryFromOwningMesh(Instance,HistoryTag);
  if(auto* Node=FindMatchNode(Instance)) Node->SetInterruptMode(EPoseSearchInterruptMode::ForceInterruptAndInvalidateContinuingPose);
 }
}
