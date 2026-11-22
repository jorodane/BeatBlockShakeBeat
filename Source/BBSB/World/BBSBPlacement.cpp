#include "World/BBSBPlacement.h"
#include "Stage01/BBSBStage01.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "UObject/UObjectHash.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#endif

UClass* UBBSBPlacementAuthoringTools::ResolvePlacementClass(UClass* Requested)
{
 return ResolvePlacementPreset(Requested ? Requested->GetFName():NAME_None,Requested);
}
UClass* UBBSBPlacementAuthoringTools::ResolvePlacementPreset(FName Key,UClass* Requested)
{
 if(!Requested || Requested->ClassGeneratedBy)return Requested;
 static TWeakObjectPtr<UBBSBPlacementClasses> Cached;
 auto* Library=Cached.Get();if(!Library) { Library=LoadObject<UBBSBPlacementClasses>(nullptr,TEXT("/Game/BBSB/Placeables/DA_PlacementClasses.DA_PlacementClasses"));Cached=Library; }
 if(Library)if(const auto* Entry=Library->Classes.Find(Key))
  if(UClass* Class=Entry->LoadSynchronous();Class && Class->IsChildOf(Requested) && !Class->HasAnyClassFlags(CLASS_Abstract))return Class;
 return Requested;
}
AActor* UBBSBPlacementAuthoringTools::ReplacePlacedActor(AActor* Original,TSubclassOf<AActor> BlueprintClass)
{
#if WITH_EDITOR
 if (!IsValid(Original) || !BlueprintClass || Original->GetWorld()->IsGameWorld() || BlueprintClass == Original->GetClass()) return nullptr;
 const bool bStoryPresetMerge = Original->GetClass()->GetSuperClass() == ABBSBStage01Device::StaticClass()
   && BlueprintClass->GetSuperClass() == ABBSBStage01Device::StaticClass();
 if (!BlueprintClass->IsChildOf(Original->GetClass()) && !bStoryPresetMerge) return nullptr;
 const FScopedTransaction Transaction(NSLOCTEXT("BBSB","ConvertPlaceable","Convert BBSB actor to placeable Blueprint"));
 UClass* OldClass=Original->GetClass();auto* Level=Original->GetLevel();const FName Name=Original->GetFName();
 const FString Label=Original->GetActorLabel();const FName Folder=Original->GetFolderPath();const FGuid Guid=Original->GetActorGuid();
 const auto* Device=Cast<ABBSBStage01Device>(Original);const FName Node=Device ? Device->NodeId:NAME_None;
 const bool Local=Device ? Device->bLocalPlacement:Cast<ABBSBStage01Director>(Original) ? CastChecked<ABBSBStage01Director>(Original)->bLocalPlacement:false;
 const bool AutoId=Device && Device->bAutomaticNodeId;
 const bool SpawnCarrier=Cast<ABBSBStage01Singer>(Original) && CastChecked<ABBSBStage01Singer>(Original)->bSpawnCarrierIfMissing;
 TArray<UObject*> Instances;GetObjectsOfClass(OldClass,Instances,true);TSet<UObject*> Keep;
 for(UObject* Object:Instances)if(Object!=Original)Keep.Add(Object);
 FReplaceInstancesOfClassParameters Params;Params.InstancesThatShouldUseOldClass=&Keep;Params.bPreserveRootComponent=false;
 auto Replace=[&]() { FBlueprintCompileReinstancer::ReplaceInstancesOfClass(OldClass,BlueprintClass,Params); };
 // The engine reinstancer preserves external packages, actor/instance GUIDs,
 // attachments and references. Suppress a new preset's automatic ID while the
 // existing instance's explicit story ID and legacy coordinates are restored.
 if(auto* Defaults=Cast<ABBSBStage01Device>(BlueprintClass->GetDefaultObject()))
 { TGuardValue<bool> IdGuard(Defaults->bAutomaticNodeId,AutoId);TGuardValue<bool> SpaceGuard(Defaults->bLocalPlacement,Local);Replace(); }
 else if(auto* StoryDefaults=Cast<ABBSBStage01Director>(BlueprintClass->GetDefaultObject()))
 { TGuardValue<bool> SpaceGuard(StoryDefaults->bLocalPlacement,Local);Replace(); }
 else Replace();
 AActor* Result=FindObject<AActor>(Level,*Name.ToString());if(!Result || !Result->IsA(BlueprintClass) || Result->GetActorGuid()!=Guid)return nullptr;
 Result->SetActorLabel(Label);Result->SetFolderPath(Folder);
 if(auto* New=Cast<ABBSBStage01Device>(Result)) { New->NodeId=Node;New->bAutomaticNodeId=AutoId;New->bLocalPlacement=Local; }
 if(auto* New=Cast<ABBSBStage01Director>(Result))New->bLocalPlacement=Local;
 if(auto* New=Cast<ABBSBStage01Singer>(Result))New->bSpawnCarrierIfMissing=SpawnCarrier;
 Result->MarkPackageDirty();return Result;
#else
 return nullptr;
#endif
}
int32 UBBSBPlacementAuthoringTools::LoadPlacedActorsForEditing(UWorld* World)
{
#if WITH_EDITOR
 if(World && !World->IsGameWorld())if(auto* Partition=World->GetWorldPartition())
 {
  TArray<FGuid> Guids;FWorldPartitionHelpers::ForEachActorDescInstance(Partition,[&](const FWorldPartitionActorDescInstance* Desc) { Guids.Add(Desc->GetGuid());return true; });
  Partition->PinActors(Guids);return Guids.Num();
 }
#endif
 return 0;
}

TArray<USceneComponent*> ABBSBStage01Device::SurfaceFrames() const
{
 TArray<USceneComponent*> Frames;
 if(!Targets.IsEmpty())for(AActor* Target:Targets) { if(IsValid(Target))Frames.Add(Target->GetRootComponent()); }
 else for(UStaticMeshComponent* Surface:PlacementSurfaces)if(IsValid(Surface) && Surface->GetStaticMesh())Frames.Add(Surface);
 return Frames;
}
void ABBSBStage01Device::BuildPlacementSurfaces()
{
 for(UStaticMeshComponent* Surface:PlacementSurfaces)if(IsValid(Surface))Surface->DestroyComponent();PlacementSurfaces.Reset();
 if(!Targets.IsEmpty())return;
 const bool bGeneratedLadder = Kind == EBBSBStage01Node::LowerLadder || Kind == EBBSBStage01Node::ClimbLadder;
 for(int32 I=0;I<SurfaceParts.Num();++I)
 {
  const auto& Spec=SurfaceParts[I];
  // Migrated ladders may retain a hidden layout mesh. The generated rails and rungs are the usable surface.
  if (!Spec.Mesh || (bGeneratedLadder && !Spec.bVisible)) continue;
  auto* Part=NewObject<UStaticMeshComponent>(this,FName(*FString::Printf(TEXT("PlacementSurface%d"),I)),RF_Transactional);
  Part->CreationMethod=EComponentCreationMethod::UserConstructionScript;Part->SetupAttachment(GetRootComponent());Part->SetMobility(EComponentMobility::Movable);
  Part->SetStaticMesh(Spec.Mesh);Part->SetRelativeTransform(Spec.Transform);Part->SetVisibility(Spec.bVisible);Part->SetHiddenInGame(!Spec.bVisible);
  for(int32 M=0;M<Spec.Materials.Num();++M)Part->SetMaterial(M,Spec.Materials[M]);
  Part->SetCollisionProfileName(Spec.bBlockMovement ? TEXT("BlockAllDynamic"):TEXT("OverlapAllDynamic"));
  if(!Spec.bBlockMovement) { Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Part->SetCollisionResponseToAllChannels(ECR_Ignore);Part->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); }
  Part->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);Part->SetCanEverAffectNavigation(false);Part->RegisterComponent();PlacementSurfaces.Add(Part);
 }
}
FTransform ABBSBStage01Device::SurfaceHome(int32 Index) const
{
 if(!Closed.IsValidIndex(Index))return FTransform::Identity;
 return bLocalPlacement ? Closed[Index]*GetActorTransform():Closed[Index];
}
void ABBSBStage01Device::CaptureSurfaceHomes()
{
 Closed.Reset();for(auto* Frame:SurfaceFrames()) { Frame->SetMobility(EComponentMobility::Movable);Closed.Add(bLocalPlacement ? Frame->GetComponentTransform().GetRelativeTransform(GetActorTransform()):Frame->GetComponentTransform()); }
}
FVector ABBSBStage01Device::DestinationPoint() const { return bLocalPlacement ? GetActorTransform().TransformPosition(Destination):Destination; }
FVector ABBSBStage01Device::MotionOffset() const { return bLocalPlacement ? GetActorTransform().TransformVector(OpenOffset):OpenOffset; }
void ABBSBStage01Device::ConvertToLocalPlacement()
{
 if(bLocalPlacement)return;Modify();const FTransform Frame=GetActorTransform();
 if(!Destination.IsNearlyZero())Destination=Frame.InverseTransformPosition(Destination);
 if(!LadderBottom.IsNearlyZero())LadderBottom=Frame.InverseTransformPosition(LadderBottom);
 if(!LadderTop.IsNearlyZero())LadderTop=Frame.InverseTransformPosition(LadderTop);
 if(!LadderOutward.IsNearlyZero())LadderOutward=Frame.InverseTransformVectorNoScale(LadderOutward);
 OpenOffset=Frame.InverseTransformVector(OpenOffset);
 // Absorb actual authored meshes; references remain as hidden recovery objects.
 // Non-static gameplay actors are never consumed by this migration.
 bool CanAbsorb=!Targets.IsEmpty();for(AActor* Target:Targets)if(!Cast<AStaticMeshActor>(Target))CanAbsorb=false;
 if(CanAbsorb)
 {
  SurfaceParts.Reset();
  for(AActor* Target:Targets)
  {
   auto* Source=CastChecked<AStaticMeshActor>(Target)->GetStaticMeshComponent();if(!Source->GetStaticMesh())continue;
   auto& Spec=SurfaceParts.AddDefaulted_GetRef();Spec.Mesh=Source->GetStaticMesh();Spec.Transform=Source->GetComponentTransform().GetRelativeTransform(Frame);
   for(int32 M=0;M<Source->GetNumMaterials();++M)Spec.Materials.Add(Source->GetMaterial(M));
   Spec.bVisible=Kind==EBBSBStage01Node::CabinDoor || (Source->IsVisible() && !Target->IsHidden());Spec.bBlockMovement=Source->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block;
   Target->Modify();Target->Tags.AddUnique(TEXT("BBSBLegacyPlacementReference"));Target->SetActorEnableCollision(false);Target->SetActorHiddenInGame(true);Source->SetVisibility(false,true);
#if WITH_EDITOR
   Target->SetIsTemporarilyHiddenInEditor(true);
#endif
  }
  Targets.Reset();
 }
 bLocalPlacement=true;RebuildPhysicalPresentation();CaptureSurfaceHomes();MarkPackageDirty();
}
