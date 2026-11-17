#include "Interaction/BBSBDragMotion.h"
#include "BBSBCharacter.h"
#include "Character/BBSBTraversal.h"
#include "Interaction/BBSBInteraction.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
 constexpr float Skin=.1f,StepDistance=12.f;
 struct FShape { FVector Center;FQuat Rotation;FCollisionShape Shape; };
 struct FPath { USceneComponent* Frame;FVector From,Delta;TArray<UPrimitiveComponent*> Parts; };

 void MeshShapes(UStaticMeshComponent* Part,const FTransform& Frame,TArray<FShape>& Out)
 {
  const UStaticMesh* Mesh=Part->GetStaticMesh();if(!Mesh)return;
  const UBodySetup* Setup=Mesh->GetBodySetup();const FVector Scale=Frame.GetScale3D().GetAbs();
  if(Setup)
  {
   for(const auto& Box:Setup->AggGeom.BoxElems)
   {
    const FTransform T=Box.GetTransform()*Frame;
    Out.Add({T.GetLocation(),T.GetRotation(),FCollisionShape::MakeBox(FVector(Box.X,Box.Y,Box.Z)*Scale*.5f)});
   }
   for(const auto& Sphere:Setup->AggGeom.SphereElems)
    Out.Add({Frame.TransformPosition(Sphere.Center),FQuat::Identity,FCollisionShape::MakeSphere(Sphere.Radius*Scale.GetMax())});
   for(const auto& Capsule:Setup->AggGeom.SphylElems)
   {
    const FTransform T=Capsule.GetTransform()*Frame;const double Radius=Capsule.Radius*FMath::Max(Scale.X,Scale.Y);
    Out.Add({T.GetLocation(),T.GetRotation(),FCollisionShape::MakeCapsule(Radius,Capsule.Length*Scale.Z*.5f+Radius)});
   }
  }
  // Convex/complex-only props get a conservative solid envelope. Never allow
  // an unsupported collision shape to make a dragged object pass through a pawn.
  if(!Setup || Setup->AggGeom.ConvexElems.Num()>0 || (Setup->AggGeom.BoxElems.IsEmpty() && Setup->AggGeom.SphereElems.IsEmpty() && Setup->AggGeom.SphylElems.IsEmpty()))
  {
   const FBox Box=Mesh->GetBoundingBox();
   Out.Add({Frame.TransformPosition(Box.GetCenter()),Frame.GetRotation(),FCollisionShape::MakeBox(Box.GetExtent()*Scale)});
  }
 }
 void Shapes(UPrimitiveComponent* Part,TArray<FShape>& Out)
 {
  if(auto* Instances=Cast<UInstancedStaticMeshComponent>(Part))
  {
   for(int32 I=0;I<Instances->GetInstanceCount();++I)
   { FTransform Frame;if(Instances->GetInstanceTransform(I,Frame,true))MeshShapes(Instances,Frame,Out); }
  }
  else if(auto* Mesh=Cast<UStaticMeshComponent>(Part))MeshShapes(Mesh,Mesh->GetComponentTransform(),Out);
  else if(Part->IsA<UShapeComponent>())Out.Add({Part->GetComponentLocation(),Part->GetComponentQuat(),Part->GetCollisionShape()});
 }
 void Collect(FPath& Path)
 {
  TArray<USceneComponent*> Components;Components.Add(Path.Frame);Path.Frame->GetChildrenComponents(true,Components);
  // GetChildrenComponents may replace the output array on some component types.
  Components.AddUnique(Path.Frame);
  for(auto* Component:Components)if(auto* Part=Cast<UPrimitiveComponent>(Component);Part && Part->IsRegistered() && Part->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
  {
   // Pointer selectors are not extra solid pieces of a carried object. In
   // particular, Bunniar's absolute visual/query sphere may overlap its own
   // following character capsule without blocking the physical carrier.
   if(Part->GetCollisionEnabled()==ECollisionEnabled::QueryOnly && Part->GetCollisionResponseToChannel(ECC_WorldStatic)==ECR_Ignore && Part->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Ignore)continue;
   Path.Parts.AddUnique(Part);
  }
 }
 USceneComponent* PushFrame(UPrimitiveComponent* Part)
 {
  auto* Owner=Part ? Part->GetOwner():nullptr;if(!IsValid(Owner) || Part->GetMobility()!=EComponentMobility::Movable)return nullptr;
  if(Part->IsSimulatingPhysics())return Part;
  if(Part->IsA<UInstancedStaticMeshComponent>())return nullptr;
  if(const auto* Player=Cast<ABBSBCharacter>(Owner);Player && Player->Traversal && (Player->Traversal->IsParkourActive() || Player->Traversal->IsWireTraversalActive()))return nullptr;
  if(const auto* Pawn=Cast<ACharacter>(Owner))
  {
   if(!Pawn->GetCharacterMovement() || Pawn->GetCharacterMovement()->MovementMode==MOVE_None)return nullptr;
   if(const auto* PC=Cast<APlayerController>(Pawn->GetController());PC && PC->IsMoveInputIgnored())return nullptr;
  }
  if(const auto* Carry=Cast<ABBSBCarryObject>(Owner);Carry && Carry->bCarried)return nullptr;
  // Authored mechanisms are constrained by their own tracks, rather than loose props.
  if(Owner->ActorHasTag(TEXT("BBSBPhysicalInteraction")))return nullptr;
  auto* Root=Owner->GetRootComponent();
  return Root && Root->GetMobility()==EComponentMobility::Movable && !Root->GetAttachParent() &&
   (Root->IsA<UPrimitiveComponent>() || Owner->ActorHasTag(TEXT("BBSBPushable"))) ? Root:nullptr;
 }
 bool FirstHit(const TArray<FPath>& Paths,FHitResult& Best,int32& PathIndex,TConstArrayView<UPrimitiveComponent*> TrackHousing,const FVector& ProbeOffset=FVector::ZeroVector)
 {
  Best=FHitResult();Best.Time=1.f;PathIndex=INDEX_NONE;
  for(int32 Index=0;Index<Paths.Num();++Index)
  {
   const auto& Path=Paths[Index];if(Path.Delta.IsNearlyZero(.0001f))continue;
   for(auto* Part:Path.Parts)
   {
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBDragMotion),false);Query.bFindInitialOverlaps=true;
    for(const auto& Other:Paths)Query.AddIgnoredActor(Other.Frame->GetOwner());
    for(auto* Housing:TrackHousing)
     if(IsValid(Housing) && Housing->GetMobility()==EComponentMobility::Static && !Housing->IsSimulatingPhysics())Query.AddIgnoredComponent(Housing);
    for(auto* Actor:Part->GetMoveIgnoreActors())if(IsValid(Actor))Query.AddIgnoredActor(Actor);
    for(auto* Component:Part->GetMoveIgnoreComponents())if(IsValid(Component))Query.AddIgnoredComponent(Component);
    const auto* Carry=Cast<ABBSBCarryObject>(Part->GetOwner());const bool ExactCarry=Carry && Part==Carry->Body;
    TArray<FShape> Geometry;
    if(ExactCarry)Geometry.Add({Part->GetComponentLocation(),Part->GetComponentQuat(),FCollisionShape()});else Shapes(Part,Geometry);
    for(const auto& Shape:Geometry)
    {
     FCollisionQueryParams ShapeQuery=Query;
     for(int32 Attempt=0;Attempt<12;++Attempt)
     {
      TArray<FHitResult> Hits;bool Skipped=false;FHitResult LastSkipped;
      if(ExactCarry)
      {
       // Sweep the actual cooked collision body, just like native component
       // movement. A raw bounds box can snag on terrain the real convex body
       // clears, particularly after a physics object has come to rest.
       FComponentQueryParams ComponentQuery;ComponentQuery.ShapeCollisionMask=0;static_cast<FCollisionQueryParams&>(ComponentQuery)=ShapeQuery;
       Part->GetWorld()->ComponentSweepMulti(Hits,Part,Shape.Center+ProbeOffset,Shape.Center+ProbeOffset+Path.Delta,Shape.Rotation,ComponentQuery);
      }
      else Part->GetWorld()->SweepMultiByChannel(Hits,Shape.Center+ProbeOffset,Shape.Center+ProbeOffset+Path.Delta,Shape.Rotation,ECC_WorldStatic,Shape.Shape,ShapeQuery);
      for(const auto& Hit:Hits)
      {
       if(!Hit.bBlockingHit || !IsValid(Hit.GetComponent()))continue;
       // Re-query beyond tangent/outward initial contact, otherwise a touching
       // floor's zero-time hit can hide a real wall further along this sweep.
       if((Hit.bStartPenetrating || Hit.Time<.00001f) && FVector::DotProduct(Path.Delta,Hit.Normal)>=-.0001f)
       { ShapeQuery.AddIgnoredComponent(Hit.GetComponent());Skipped=true;LastSkipped=Hit;continue; }
       if(PathIndex==INDEX_NONE || Hit.Time<Best.Time) { Best=Hit;PathIndex=Index; }
      }
      if(!Skipped)break;
      if(Attempt==11) { Best=LastSkipped;Best.Time=0;PathIndex=Index; }
     }
    }
   }
  }
  return PathIndex!=INDEX_NONE;
 }
 float Solve(TConstArrayView<BBSBDragMotion::FMove> Requests,TSet<USceneComponent*>& Stack,int32 Depth,FHitResult* BlockingHit=nullptr,TConstArrayView<UPrimitiveComponent*> TrackHousing={})
 {
  if(Depth>6)return 0;
  TArray<FPath> Paths;float Maximum=0;
  for(const auto& Request:Requests)
  {
   auto* Frame=Request.Frame;if(!IsValid(Frame) || Request.Destination.ContainsNaN() || Stack.Contains(Frame))return 0;
   FPath& Path=Paths.AddDefaulted_GetRef();Path.Frame=Frame;Path.From=Frame->GetComponentLocation();Path.Delta=Request.Destination-Path.From;Collect(Path);
   Maximum=FMath::Max(Maximum,float(Path.Delta.Size()));
  }
  if(Paths.IsEmpty() || Maximum<.0001f)return 1;
  for(const auto& Path:Paths)Stack.Add(Path.Frame);
  const int32 Steps=FMath::Clamp(FMath::CeilToInt(Maximum/StepDistance),1,256);float Reached=0;
  for(int32 Step=1;Step<=Steps;++Step)
  {
   const float Goal=float(Step)/Steps;
   bool Finished=false;
   for(int32 Contact=0;Contact<16;++Contact)
   {
    TArray<FPath> Remainder=Paths;
    for(auto& Path:Remainder)Path.Delta=Path.From+Path.Delta*Goal-Path.Frame->GetComponentLocation();
    FHitResult Hit;int32 Index;const bool Blocked=FirstHit(Remainder,Hit,Index,TrackHousing);
    if(Blocked && BlockingHit)*BlockingHit=Hit;
    const float Safe=Blocked ? FMath::Clamp(Hit.Time-Skin/FMath::Max(.001f,float(Remainder[Index].Delta.Size())),0.f,1.f):1.f;
    for(const auto& Path:Remainder)Path.Frame->SetWorldLocation(Path.Frame->GetComponentLocation()+Path.Delta*Safe,false,nullptr,ETeleportType::TeleportPhysics);
    Reached=FMath::Lerp(Reached,Goal,Safe);
    if(!Blocked) { Reached=Goal;Finished=true;break; }
    // A held object's carrier is an obstacle, never a target of its push chain.
    const auto* Held=Cast<ABBSBCarryObject>(Remainder[Index].Frame->GetOwner());
    if(Held && Held->bCarried && Held->CarryingCharacter()==Hit.GetActor())break;
    auto* Pushed=PushFrame(Hit.GetComponent());if(!Pushed || Stack.Contains(Pushed))break;
    FVector Delta=Remainder[Index].Delta*(1.f-Safe);
    if(Hit.bStartPenetrating)Delta-=Hit.Normal*(Hit.PenetrationDepth+Skin);
    const FVector Before=Pushed->GetComponentLocation();
    const BBSBDragMotion::FMove Push{Pushed,Before+Delta};Solve(MakeArrayView(&Push,1),Stack,Depth+1);
    if(FVector::DotProduct(Pushed->GetComponentLocation()-Before,Delta.GetSafeNormal())<.001f)break;
    if(auto* Body=Cast<UPrimitiveComponent>(Pushed);Body && Body->IsSimulatingPhysics())Body->WakeAllRigidBodies();
   }
   if(!Finished)break;
  }
  for(const auto& Path:Paths)Stack.Remove(Path.Frame);
  return FMath::Clamp(Reached,0.f,1.f);
 }
}
float BBSBDragMotion::Move(TConstArrayView<FMove> Moves,TConstArrayView<UPrimitiveComponent*> TrackHousing)
{ TSet<USceneComponent*> Stack;return Solve(Moves,Stack,0,nullptr,TrackHousing); }
float BBSBDragMotion::Move(USceneComponent* Frame,const FVector& Destination,FHitResult* BlockingHit)
{ const FMove Request{Frame,Destination};TSet<USceneComponent*> Stack;return Solve(MakeArrayView(&Request,1),Stack,0,BlockingHit); }
bool BBSBDragMotion::ClearPath(USceneComponent* Frame,const FVector& From,const FVector& To)
{
 if(!IsValid(Frame) || From.ContainsNaN() || To.ContainsNaN())return false;
 FPath Path{Frame,From,To-From,{}};Collect(Path);if(Path.Parts.IsEmpty())return false;
 const FVector Offset=To-Frame->GetComponentLocation();
 for(auto* Part:Path.Parts)
 {
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBFollowPose),false,Frame->GetOwner());
  for(auto* Actor:Part->GetMoveIgnoreActors())if(IsValid(Actor))Query.AddIgnoredActor(Actor);
  for(auto* Component:Part->GetMoveIgnoreComponents())if(IsValid(Component))Query.AddIgnoredComponent(Component);
  if(const auto* Carry=Cast<ABBSBCarryObject>(Part->GetOwner());Carry && Part==Carry->Body)
  {
   FComponentQueryParams ComponentQuery;ComponentQuery.ShapeCollisionMask=0;static_cast<FCollisionQueryParams&>(ComponentQuery)=Query;TArray<FOverlapResult> Overlaps;
   if(Part->GetWorld()->ComponentOverlapMultiByChannel(Overlaps,Part,Part->GetComponentLocation()+Offset,Part->GetComponentQuat(),Part->GetCollisionObjectType(),ComponentQuery))return false;
  }
  else
  {
   TArray<FShape> Geometry;Shapes(Part,Geometry);
   for(const auto& Shape:Geometry)
    if(Part->GetWorld()->OverlapBlockingTestByChannel(Shape.Center+Offset,Shape.Rotation,ECC_WorldStatic,Shape.Shape,Query))return false;
  }
 }
 TArray<FPath> Paths;Paths.Add(MoveTemp(Path));FHitResult Hit;int32 Index;
 return !FirstHit(Paths,Hit,Index,{},From-Frame->GetComponentLocation());
}
bool BBSBDragMotion::FindFollowRoute(const FVector& From,const FVector& Goal,const FVector& Normal,const FVector& ObstacleCenter,
 const FVector& FallbackSide,float MaximumOffset,TFunctionRef<bool(FVector,FVector)> Clear,TArray<FVector>& Route)
{
 Route.Reset();if(MaximumOffset<=0 || !Clear(Goal,Goal))return false;
 FVector Side=FVector::CrossProduct(Normal,FVector::UpVector).GetSafeNormal();if(Side.IsNearlyZero())Side=FallbackSide.GetSafeNormal();
 if(Side.IsNearlyZero())Side=FVector::RightVector;
 if(FVector::DotProduct(From-ObstacleCenter,Side)<0)Side=-Side;
 FVector Across=FVector::CrossProduct(FVector::UpVector,FallbackSide).GetSafeNormal();if(Across.IsNearlyZero())Across=FVector::ForwardVector;
 if(FVector::DotProduct(From-ObstacleCenter,Across)<0)Across=-Across;
 double Best=MAX_dbl;
 for(float Fraction:{.25f,.5f,.75f,1.f})
  for(const FVector& Axis:{Side,-Side,FVector::UpVector,-FVector::UpVector,Across,-Across})
  {
   const FVector Offset=Axis*MaximumOffset*Fraction,A=From+Offset,B=Goal+Offset;
   const double Length=2.*Offset.Size()+FVector::Distance(A,B);
   if(Length>=Best || !Clear(From,A) || !Clear(A,B) || !Clear(B,Goal))continue;
   Route={A,B,Goal};Best=Length;
  }
 return !Route.IsEmpty();
}
