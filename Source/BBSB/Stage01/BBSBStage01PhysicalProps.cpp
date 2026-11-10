#include "Stage01/BBSBStage01.h"
#include "Interaction/BBSBDragMotion.h"
#include "BBSBCharacter.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void ABBSBStage01Device::RegisterPhysicalSurfaces()
{
 if(!GetWorld() || !GetWorld()->IsGameWorld())return;
 const auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(!Player || !Player->Interaction)return;
 if(Kind!=EBBSBStage01Node::CabinDoor && Kind!=EBBSBStage01Node::LowerLadder && Kind!=EBBSBStage01Node::ClimbLadder && Kind!=EBBSBStage01Node::CargoButton)
  for(AActor* Target:Targets)if(IsValid(Target))BBSBBunniarSurface::Bind(Target,this);
 Player->Interaction->RegisterInteractionCandidate(this);bSurfacesRegistered=true;
}
void ABBSBStage01Device::RebuildPhysicalPresentation()
{
 Tags.AddUnique(TEXT("BBSBPhysicalInteraction"));bSurfacesRegistered=false;Label->SetHiddenInGame(true);
 auto Empty=[](UStaticMeshComponent* C)
 { C->SetStaticMesh(nullptr);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetVisibility(false); };
 Empty(Body);Empty(DoorLeft);Empty(DoorRight);for(UStaticMeshComponent* C:DeviceDetails)Empty(C);
 Body->SetRelativeTransform(FTransform::Identity);Body->SetHiddenInGame(false);
 for(UStaticMeshComponent* C:DeviceDetails) { C->SetRelativeTransform(FTransform::Identity);C->SetHiddenInGame(false); }
 auto Part=[&](UStaticMeshComponent* C,const TCHAR* Shape,FVector Position,FVector Scale,FRotator Rotation,FLinearColor Color)
 {
  C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Shape));C->SetRelativeLocationAndRotation(Position,Rotation);C->SetRelativeScale3D(Scale);C->SetVisibility(true);C->SetHiddenInGame(false);
  C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);C->SetCollisionResponseToAllChannels(ECR_Ignore);C->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
  C->SetCanEverAffectNavigation(false);BBSBCombatVisuals::Tint(C,Color);
 };
 const TCHAR* Cylinder=TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),*Sphere=TEXT("/Engine/BasicShapes/Sphere.Sphere"),*Cone=TEXT("/Engine/BasicShapes/Cone.Cone"),*Plane=TEXT("/Engine/BasicShapes/Plane.Plane");
 const FLinearColor Brass(.55f,.39f,.23f),Wood(.22f,.16f,.12f),Paper(.86f,.81f,.71f);
 if(Kind==EBBSBStage01Node::CabinDoor && Targets.Num()>0)
 {
  auto* Template=Cast<AStaticMeshActor>(Targets[0]);auto* Source=Template ? Template->GetStaticMeshComponent():nullptr;
  if(Source && Source->GetStaticMesh())
  {
   DoorFrame=Closed.Num()>0 ? Closed[0]:Template->GetActorTransform();DoorAxis=DoorFrame.GetUnitAxis(EAxis::Y);
   const float Width=Source->GetStaticMesh()->GetBoundingBox().GetSize().Y*FMath::Abs(DoorFrame.GetScale3D().Y);DoorTravel=Width*.5f+30.f;
   for(auto* Leaf:{DoorLeft.Get(),DoorRight.Get()})
   {
    Leaf->SetStaticMesh(Source->GetStaticMesh());for(int32 I=0;I<Source->GetNumMaterials();++I)Leaf->SetMaterial(I,Source->GetMaterial(I));
    Leaf->SetVisibility(true);Leaf->SetHiddenInGame(false);Leaf->SetCollisionProfileName(TEXT("BlockAllDynamic"));Leaf->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);Leaf->SetCanEverAffectNavigation(false);
   }
   // The old solid block remains only as an authoring reference for mesh, size and materials.
   Source->SetVisibility(false);Source->SetHiddenInGame(true);Source->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  }
 }
 else if(Kind==EBBSBStage01Node::CargoLever)
 {
  Part(Body,Cylinder,FVector::ZeroVector,FVector(.64f,.64f,.30f),FRotator(0,0,90),Brass);
  Part(DeviceDetails[0],Cylinder,FVector(0,-19,0),FVector(.84f,.84f,.06f),FRotator(0,0,90),Wood);
  Part(DeviceDetails[1],Cylinder,FVector(0,19,0),FVector(.84f,.84f,.06f),FRotator(0,0,90),Wood);
  Part(DeviceDetails[2],Cylinder,FVector(26,0,-45),FVector(.035f,.035f,.9f),FRotator::ZeroRotator,Paper);
 }
 else if(Kind==EBBSBStage01Node::LightLever)
 {
  Part(Body,Cone,FVector(0,0,34),FVector(.65f,.65f,.6f),FRotator(135,0,0),Brass);
  Part(DeviceDetails[0],Cylinder,FVector(0,0,-44),FVector(.55f,.55f,.08f),FRotator::ZeroRotator,Wood);
  Part(DeviceDetails[1],Cylinder,FVector(0,0,-5),FVector(.06f,.06f,.8f),FRotator::ZeroRotator,Brass);
  Part(DeviceDetails[2],Sphere,FVector(0,0,34),FVector(.24f),FRotator::ZeroRotator,Paper);
 }
 else if(Kind==EBBSBStage01Node::CargoButton)
 {
  Part(Body,Cylinder,FVector::ZeroVector,FVector(.5f,.5f,.13f),FRotator::ZeroRotator,Brass);
  Part(DeviceDetails[0],Cylinder,FVector(0,0,9),FVector(.35f,.35f,.10f),FRotator::ZeroRotator,FLinearColor(.45f,.25f,.3f));
 }
 else if(Kind==EBBSBStage01Node::BuyTea)
 {
  Part(Body,Cylinder,FVector::ZeroVector,FVector(.25f,.25f,.3f),FRotator::ZeroRotator,Paper);
  Part(DeviceDetails[0],Cylinder,FVector(0,0,16),FVector(.23f,.23f,.025f),FRotator::ZeroRotator,Wood);
 }
 else if(Kind==EBBSBStage01Node::BuyParts)
 {
  Part(Body,Cylinder,FVector::ZeroVector,FVector(.4f,.4f,.09f),FRotator::ZeroRotator,Brass);
  Part(DeviceDetails[0],Cylinder,FVector(0,0,10),FVector(.13f,.13f,.19f),FRotator::ZeroRotator,Wood);
 }
 else if(Kind==EBBSBStage01Node::TuneReward)Part(Body,Sphere,FVector::ZeroVector,FVector(.32f),FRotator::ZeroRotator,FLinearColor(.72f,.63f,.82f));
 else if(Targets.IsEmpty() && Kind!=EBBSBStage01Node::LowerLadder && Kind!=EBBSBStage01Node::ClimbLadder)
 {
  // Notes and departure tickets are the actual readable paper, not a separate control handle.
  Part(Body,Plane,FVector::ZeroVector,FVector(.55f,.4f,1),FRotator(20,0,0),Paper);
 }
 RebuildLadderGeometry();UpdatePhysicalPresentation();RegisterPhysicalSurfaces();
}
void ABBSBStage01Device::UpdatePhysicalPresentation(bool bSweepMotion,float PreviousPainted)
{
 TArray<BBSBDragMotion::FMove> Moves;
 auto Queue=[&](USceneComponent* Frame,const FVector& Goal)
 { if(Frame)Moves.Add({Frame,Goal}); };
 // These authored props are render-only meshes. The real board and mallet,
 // rather than an invisible device handle, must receive cursor and F traces.
 if(Kind==EBBSBStage01Node::ToolCache)
 {
  FCollisionResponseContainer SelectionOnly(ECR_Ignore);SelectionOnly.SetResponse(ECC_Visibility,ECR_Block);
  for(AActor* Target:Targets)if(IsValid(Target))
  {
   Target->SetActorHiddenInGame(bSolved);Target->SetActorEnableCollision(!bSolved);
   TInlineComponentArray<UStaticMeshComponent*> Parts(Target);
   for(auto* Part:Parts)if(Part->GetStaticMesh())
   {
    const auto Collision=bSolved ? ECollisionEnabled::NoCollision:ECollisionEnabled::QueryOnly;
    if(Part->GetCollisionEnabled()!=Collision)Part->SetCollisionEnabled(Collision);
    if(!bSolved && Part->GetCollisionResponseToChannels()!=SelectionOnly)Part->SetCollisionResponseToChannels(SelectionOnly);
   }
  }
 }
 if(Kind==EBBSBStage01Node::LowerLadder)
 {
  const FVector Axis=(LadderTopPoint()-LadderBottomPoint()).GetSafeNormal();
  Queue(LadderExtension,GetActorLocation()+Axis*LadderRetractDistance()*(1.f-Painted));
 }
 else if(Kind==EBBSBStage01Node::CabinDoor && DoorLeft->GetStaticMesh())
 {
  const float Width=DoorLeft->GetStaticMesh()->GetBoundingBox().GetSize().Y*FMath::Abs(DoorFrame.GetScale3D().Y);
  for(int32 I=0;I<2;++I)
  {
   UStaticMeshComponent* Leaf=I==0 ? DoorLeft.Get():DoorRight.Get();const float Sign=I==0 ? -1.f:1.f;
   FVector Scale=DoorFrame.GetScale3D();Scale.Y*=.5f;
   Leaf->SetWorldRotation(DoorFrame.GetRotation());Leaf->SetWorldScale3D(Scale);
   Queue(Leaf,DoorFrame.GetLocation()+DoorAxis*(Sign*(Width*.25f+DoorTravel*Painted)));
  }
 }
 else
 {
  const float T=IsDirectDrag() ? Painted:FMath::SmoothStep(0.f,1.f,Painted);
  for(int32 I=0;I<Targets.Num();++I)if(IsValid(Targets[I]) && Closed.IsValidIndex(I))Queue(Targets[I]->GetRootComponent(),Closed[I].GetLocation()+OpenOffset*T);
 }
 if(bSweepMotion && GetWorld() && GetWorld()->IsGameWorld())
 {
  TArray<UPrimitiveComponent*> Housing;
  if(Kind==EBBSBStage01Node::CabinDoor)
   for(AActor* Structure:DoorTrackHousing)if(IsValid(Structure))
   {
    TInlineComponentArray<UPrimitiveComponent*> Parts(Structure);
    for(auto* Part:Parts)if(Part->GetMobility()==EComponentMobility::Static && !Part->IsSimulatingPhysics())Housing.AddUnique(Part);
   }
  const float Allowed=BBSBDragMotion::Move(Moves,Housing);
  if(Allowed<.99999f)
  {
   if(IsDirectDrag())Painted=FMath::Lerp(PreviousPainted,Painted,Allowed);
   else
   {
    const float T=FMath::Lerp(FMath::SmoothStep(0.f,1.f,PreviousPainted),FMath::SmoothStep(0.f,1.f,Painted),Allowed);
    float Low=0,High=1;for(int32 I=0;I<20;++I) { const float Mid=(Low+High)*.5f;if(FMath::SmoothStep(0.f,1.f,Mid)<T)Low=Mid;else High=Mid; }Painted=(Low+High)*.5f;
   }
   if(IsDirectDrag())Progress=Painted;
  }
 }
 else for(const auto& Move:Moves)Move.Frame->SetWorldLocation(Move.Destination,false,nullptr,ETeleportType::TeleportPhysics);
 if(Kind==EBBSBStage01Node::CargoLever)
 {
  Body->SetRelativeRotation(FRotator(0,Painted*300.f,90));
  if(DeviceDetails.Num()>2) { DeviceDetails[2]->SetRelativeScale3D(FVector(.035f,.035f,.9f+Painted*1.1f));DeviceDetails[2]->SetRelativeLocation(FVector(26,0,-45-Painted*55)); }
 }
 if(Kind==EBBSBStage01Node::LightLever)Body->SetRelativeRotation(FRotator(135-Painted*65.f,0,0));
 if(Kind==EBBSBStage01Node::CargoButton && DeviceDetails.Num()>0)DeviceDetails[0]->SetRelativeLocation(FVector(0,0,9-Painted*8));
}

void ABBSBStage01Device::RestoreSessionState(float Value,bool Solved)
{
 bHeld=false;Progress=Painted=FMath::Clamp(Value,0.f,1.f);bSolved=Solved;UpdatePhysicalPresentation(false);
}
