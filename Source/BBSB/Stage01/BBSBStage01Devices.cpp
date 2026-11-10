#include "Stage01/BBSBStage01.h"
#include "BBSBCharacter.h"
#include "Character/BBSBTraversal.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
ABBSBStage01Device::ABBSBStage01Device()
{
 PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DeviceOrigin")));
 // Preserve the subobject name for existing maps; it no longer represents an abstract cube handle.
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BrassHandle"));Body->SetupAttachment(RootComponent);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("ActionLabel"));Label->SetupAttachment(RootComponent);Label->SetRelativeLocation(FVector(0,0,65));Label->SetWorldSize(18);Label->SetHorizontalAlignment(EHTA_Center);Label->SetTextRenderColor(FColor(239,225,200));Label->SetHiddenInGame(true);
 LadderGeometry=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LadderRailsAndRungs"));LadderGeometry->SetupAttachment(RootComponent);
 LadderExtension=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SlidingLadderSection"));LadderExtension->SetupAttachment(RootComponent);
 for(auto* C:{LadderGeometry.Get(),LadderExtension.Get()})
 {
  C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));C->SetCastShadow(false);C->SetCanEverAffectNavigation(false);
  C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);C->SetCollisionResponseToAllChannels(ECR_Ignore);C->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 }
 DoorLeft=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinDoorLeftLeaf"));DoorLeft->SetupAttachment(RootComponent);
 DoorRight=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinDoorRightLeaf"));DoorRight->SetupAttachment(RootComponent);
 for(const FName Name:{FName(TEXT("DeviceDetailA")),FName(TEXT("DeviceDetailB")),FName(TEXT("DeviceDetailC"))})
 {
  auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(RootComponent);DeviceDetails.Add(C);
 }
 Tags.Add(TEXT("BBSBInteractable"));Tags.Add(TEXT("BBSBPhysicalInteraction"));
}
void ABBSBStage01Device::OnConstruction(const FTransform& T)
{ Super::OnConstruction(T);Label->SetText(Title);RebuildPhysicalPresentation(); }
void ABBSBStage01Device::BeginPlay()
{
 Super::BeginPlay();Closed.Reset();
 for(AActor* Target:Targets)
 {
  Closed.Add(Target ? Target->GetActorTransform():FTransform::Identity);
  if(Target)if(auto* C=Target->GetRootComponent())C->SetMobility(EComponentMobility::Movable);
 }
 RebuildPhysicalPresentation();RegisterPhysicalSurfaces();Label->SetText(Title);
}
bool ABBSBStage01Device::IsDirectDrag() const
{
 return Kind==EBBSBStage01Node::CabinDoor || Kind==EBBSBStage01Node::LowerLadder || Kind==EBBSBStage01Node::Trapdoor ||
  Kind==EBBSBStage01Node::CargoLever || Kind==EBBSBStage01Node::LightLever || Kind==EBBSBStage01Node::ReadyGate;
}
void ABBSBStage01Device::SetSolved(bool Value) { bSolved=Value;Progress=Value ? 1.f:0.f; }
void ABBSBStage01Device::Tick(float DT)
{
 Super::Tick(DT);const float PreviousPainted=Painted;
 const float Goal=Kind==EBBSBStage01Node::CabinDoor ? Progress:bSolved ? 1.f:Progress;
 Painted=IsDirectDrag() ? FMath::FInterpTo(Painted,Goal,DT,24.f):FMath::FInterpConstantTo(Painted,Goal,DT,1.f/FMath::Max(.05f,MotionSeconds));
 if(FMath::Abs(Painted-Goal)<.0001f)Painted=Goal;
 UpdatePhysicalPresentation(true,PreviousPainted);if(!bSurfacesRegistered)RegisterPhysicalSurfaces();
 // Record completion only after the actual geometry reaches the end of its track.
 if(IsDirectDrag() && !bSolved && Progress>=.9999f && Painted>=.9999f && !Use()) { Progress=0;bHeld=false; }
 if(auto* Cam=UGameplayStatics::GetPlayerCameraManager(this,0))Label->SetWorldRotation((Cam->GetCameraLocation()-Label->GetComponentLocation()).Rotation());
}
FText ABBSBStage01Device::BunniarInteractionText_Implementation() const
{
 FString Help;
 switch(Kind)
 {
 case EBBSBStage01Node::CabinDoor:Help=TEXT("움직이기");break;
 case EBBSBStage01Node::LowerLadder:Help=bSolved ? TEXT("사다리 잡기"):TEXT("겹친 사다리를 붙잡고 아래로 드래그");break;
 case EBBSBStage01Node::ClimbLadder:Help=TEXT("사다리 잡기");break;
 case EBBSBStage01Node::CargoLever:Help=bSolved ? TEXT("고정 해제됨"):TEXT("윈치에 붙여 아래로 풀기");break;
 case EBBSBStage01Node::LightLever:Help=bSolved ? TEXT("켜짐"):TEXT("조명 본체를 아래로 드래그");break;
 case EBBSBStage01Node::ReadyGate:Help=bSolved ? TEXT("열림"):TEXT("문에 붙여 옆으로 드래그");break;
 case EBBSBStage01Node::Trapdoor:Help=bSolved ? TEXT("지붕으로 올라가기"):TEXT("덮개에 붙여 옆으로 드래그");break;
 default:Help=TEXT("F 또는 버니아 클릭");break;
 }
 return FText::FromString(Title.ToString()+TEXT(" · ")+Help);
}
bool ABBSBStage01Device::BunniarQuickUse_Implementation(ABBSBBunniarCompanion* Buddy)
{
 switch(Kind)
 {
 case EBBSBStage01Node::CabinDoor:return false;
 case EBBSBStage01Node::CargoLever:case EBBSBStage01Node::LightLever:case EBBSBStage01Node::ReadyGate:return bSolved ? (Use(),true):false;
 case EBBSBStage01Node::LowerLadder:case EBBSBStage01Node::Trapdoor:return bSolved ? (Use(),true):false;
 default:Use();return true;
 }
}
void ABBSBStage01Device::BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector P,FVector N)
{
 if(Kind==EBBSBStage01Node::ClimbLadder || (Kind==EBBSBStage01Node::LowerLadder && bSolved))return;
 bHeld=true;HeldAt=GetWorld()->GetTimeSeconds();CapturedProgress=Progress;
 if(Kind==EBBSBStage01Node::LowerLadder)
 {
  DragMotion=-(LadderTopPoint()-LadderBottomPoint()).GetSafeNormal()*LadderRetractDistance();
  if(Buddy && Buddy->SurfaceComponent && Buddy->SurfaceComponent!=LadderExtension)
  {
   Buddy->RetargetAttachedSurface(LadderExtension);
  }
 }
 else if(Kind==EBBSBStage01Node::CabinDoor)
 {
  const float Sign=Buddy && Buddy->SurfaceComponent==DoorLeft ? -1.f:1.f;DragMotion=DoorAxis*DoorTravel*Sign;
 }
 else if(Kind==EBBSBStage01Node::CargoLever || Kind==EBBSBStage01Node::LightLever)DragMotion=-FVector::UpVector*90.f;
 else DragMotion=OpenOffset;
 switch(Kind)
 {
 case EBBSBStage01Node::ToolCache:case EBBSBStage01Node::UtilityDoor:case EBBSBStage01Node::CargoButton:case EBBSBStage01Node::CargoLift:case EBBSBStage01Node::TrioExit:case EBBSBStage01Node::CaptainNote:case EBBSBStage01Node::BuyTea:case EBBSBStage01Node::BuyParts:case EBBSBStage01Node::TuneReward:case EBBSBStage01Node::Departure:Use();break;
 default:if(bSolved && Kind!=EBBSBStage01Node::CabinDoor)Use();break;
 }
}
float ABBSBStage01Device::ProjectedDragProgress(ABBSBBunniarCompanion* Buddy,FVector2D Delta,FVector2D Total) const
{
 const auto* PC=Cast<APlayerController>(UGameplayStatics::GetPlayerController(this,0));
 if(Buddy && PC && !DragMotion.IsNearlyZero())
 {
  const FVector P0=Buddy->SurfacePoint-DragMotion*Painted,P1=P0+DragMotion,Goal=P0+DragMotion*Progress;
  FVector2D A,B,G;FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
  const float ZA=FVector::DotProduct(P0-Eye,View.Vector()),ZB=FVector::DotProduct(P1-Eye,View.Vector());
  if(ZA>1.f && ZB>1.f && PC->ProjectWorldLocationToScreen(P0,A,true) && PC->ProjectWorldLocationToScreen(P1,B,true) && PC->ProjectWorldLocationToScreen(Goal,G,true) && (B-A).SizeSquared()>24.f*24.f)
  {
   int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
   const float MaximumStroke=FMath::Max(RequiredDrag,FMath::Min(Width,Height)*.55f);
   const FVector2D Stroke=B-A;
   // A ladder lowered toward the camera can project far beyond the viewport.
   // Keep that drag usable without requiring several screen lengths of motion.
   if(Stroke.Size()>MaximumStroke)
    return FMath::Clamp(Progress+float(FVector2D::DotProduct(Delta,Stroke.GetSafeNormal()))/FMath::Max(20.f,MaximumStroke),0.f,1.f);
   const float U=FMath::Clamp(float(FVector2D::DotProduct(G+Delta-A,B-A)/(B-A).SizeSquared()),0.f,1.f);
   // Perspective-correct progress makes the grabbed point follow the pointer on screen.
   return FMath::Clamp(U*ZA/FMath::Max(1.f,ZB*(1.f-U)+U*ZA),0.f,1.f);
  }
 }
 const bool Horizontal=Kind==EBBSBStage01Node::CabinDoor || Kind==EBBSBStage01Node::Trapdoor || Kind==EBBSBStage01Node::ReadyGate;
 FVector Eye;FRotator View;if(PC)PC->GetPlayerViewPoint(Eye,View);
 const float Projection=FVector::DotProduct(DragMotion,View.RotateVector(Horizontal ? FVector::RightVector:-FVector::UpVector));
 const float Sign=Projection<0.f ? -1.f:1.f;
 return FMath::Clamp(Progress+Sign*(Horizontal ? float(Delta.X):float(Delta.Y))/FMath::Max(20.f,RequiredDrag),0.f,1.f);
}
void ABBSBStage01Device::BunniarDragged_Implementation(ABBSBBunniarCompanion* Buddy,FVector2D Delta,FVector2D Total)
{
 if(!bHeld || (bSolved && Kind!=EBBSBStage01Node::CabinDoor) || !IsDirectDrag())return;
 Progress=ProjectedDragProgress(Buddy,Delta,Total);
 // Tick commits the milestone after collision-limited physical movement.
}
void ABBSBStage01Device::BunniarGesture_Implementation(ABBSBBunniarCompanion* Buddy,EBBSBFieldGesture G,FVector2D Direction,float Seconds)
{ if(G==EBBSBFieldGesture::Hold && bHeld && Kind==EBBSBStage01Node::CargoButton)Use(); }
void ABBSBStage01Device::BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy)
{ bHeld=false; /* A partly opened prop stays where it was released; another grab resumes it. */ }
bool ABBSBStage01Device::Use()
{
 if(!Director)return false;
 if(Kind==EBBSBStage01Node::Trapdoor && bSolved && Painted>.98f)return Director->MoveAlongLadder(Destination,1.5f,60,GetActorLocation());
 return Director->UseDevice(this);
}
bool ABBSBStage01NPC::BunniarQuickUse_Implementation(ABBSBBunniarCompanion* Buddy) { if(Director)Director->TalkTo(this);return true; }
void ABBSBStage01NPC::BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector P,FVector N) { if(Director)Director->TalkTo(this);Super::BunniarAttached_Implementation(Buddy,P,N); }
ABBSBStage01CarrySinger::ABBSBStage01CarrySinger()
{
 Size=FVector(75,85,70);MassKg=1.5;MaximumHealth=100000;ImpactDamagePerMeterPerSecond=0;
 BunniarMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CarriedSongBunniar"));BunniarMesh->SetupAttachment(Body);
 BunniarMesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Meshes/Characters/Bunniar/Bunniar_Normal.Bunniar_Normal")));
 BunniarMesh->SetAnimInstanceClass(UBBSBBunniarAnimInstance::StaticClass());BunniarMesh->SetRelativeScale3D(FVector(.55));BunniarMesh->SetRelativeLocation(FVector(0,0,-40));BunniarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Body->SetHiddenInGame(true,false);
 Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
}
void ABBSBStage01CarrySinger::BeginPlay() { Super::BeginPlay();SongHome=GetActorLocation(); }
FText ABBSBStage01CarrySinger::BunniarInteractionText_Implementation() const { return FText::FromString(TEXT("버니아 집기 · 유지: 운반 · 우클릭: 조작/던지기")); }
void ABBSBStage01CarrySinger::BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector P,FVector N) { Super::BunniarAttached_Implementation(Buddy,P,N);if(Singer)Singer->SilenceFor(100000); }
void ABBSBStage01CarrySinger::BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy) { Super::BunniarReleased_Implementation(Buddy);if(Singer)Singer->SilenceFor(Singer->QuietSecondsAfterCarry); }
void ABBSBStage01CarrySinger::Tick(float DT)
{
 Super::Tick(DT);Body->SetHiddenInGame(true,false);
 if(!bCarried && (GetActorLocation().Z<SongHome.Z-600 || FVector::DistSquared2D(GetActorLocation(),SongHome)>FMath::Square(3000.f))) { ResetObject();Body->SetPhysicsLinearVelocity(FVector::ZeroVector);Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector); }
 if(Body->GetCollisionObjectType()!=ECC_Pawn)Body->SetCollisionObjectType(ECC_Pawn);
}

FVector ABBSBStage01Device::LadderNormal() const
{
 const FVector N=LadderOutward.IsNearlyZero() ? (GetActorLocation()-Destination).GetSafeNormal2D():LadderOutward.GetSafeNormal2D();
 return N.IsNearlyZero() ? -GetActorForwardVector():N;
}
FVector ABBSBStage01Device::LadderBottomPoint() const
{
 return LadderBottom.IsNearlyZero() ? GetActorLocation()+LadderNormal()*50.f-FVector::UpVector*55.f:LadderBottom;
}
FVector ABBSBStage01Device::LadderTopPoint() const
{
 return LadderTop.IsNearlyZero() ? Destination+LadderNormal()*120.f:LadderTop;
}
bool ABBSBStage01Device::MountLadder()
{
 if(!Director || (Kind==EBBSBStage01Node::LowerLadder && (!bSolved || Painted<.98f)))return false;
 auto* P=Director->PlayerCharacter();if(!P || !P->Traversal || !P->Traversal->BeginLadder(this,LadderBottomPoint(),LadderTopPoint(),Destination,LadderNormal()))return false;
 P->Traversal->LadderRungSpacing=FMath::Clamp(RungSpacing,20.f,50.f);return true;
}

float ABBSBStage01Device::LadderRetractDistance() const
{
 const float Length=FVector::Distance(LadderBottomPoint(),LadderTopPoint());
 return FMath::Max(0.f,(Length+76.f)*.5f-FMath::Clamp(RungSpacing,20.f,50.f));
}
void ABBSBStage01Device::RebuildLadderGeometry()
{
 if(!LadderGeometry || !LadderExtension)return;
 for(auto* C:{LadderGeometry.Get(),LadderExtension.Get()})
 { C->SetCollisionResponseToAllChannels(ECR_Ignore);C->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); }
 LadderGeometry->SetRelativeLocation(FVector::ZeroVector);LadderExtension->SetRelativeLocation(FVector::ZeroVector);
 LadderGeometry->ClearInstances();LadderExtension->ClearInstances();
 const bool Ladder=Kind==EBBSBStage01Node::LowerLadder || Kind==EBBSBStage01Node::ClimbLadder;
 LadderGeometry->SetVisibility(Ladder);LadderExtension->SetVisibility(Ladder && Kind==EBBSBStage01Node::LowerLadder);
 LadderGeometry->SetCollisionEnabled(Ladder ? ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
 LadderExtension->SetCollisionEnabled(Ladder && Kind==EBBSBStage01Node::LowerLadder ? ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
 if(!Ladder)return;
 const FVector B=LadderBottomPoint(),T=LadderTopPoint(),N=LadderNormal(),Axis=(T-B).GetSafeNormal();
 const FVector Side=FVector::CrossProduct(FVector::UpVector,-N).GetSafeNormal();const float Spacing=FMath::Clamp(RungSpacing,20.f,50.f);
 const FQuat Frame=FRotationMatrix::MakeFromYZ(Side,Axis).ToQuat();const float Length=FVector::Distance(B,T),Start=-96.f,End=Length-20.f,Joint=(Start+End)*.5f;
 const FVector Grip=B-N*32.f+FVector::UpVector*20.f;
 auto Section=[&](UInstancedStaticMeshComponent* C,float From,float To,FVector Layer)
 {
  for(float Sign:{-1.f,1.f})C->AddInstance(FTransform(Frame,Grip+Axis*((From+To)*.5f)+Side*(Sign*26.f)+Layer,FVector(.08f,.08f,(To-From)/100.f)),true);
  for(float D=FMath::CeilToFloat(From/Spacing)*Spacing;D<=To;D+=Spacing)C->AddInstance(FTransform(Frame,Grip+Axis*D+Layer,FVector(.10f,.60f,.08f)),true);
  C->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BBSB/Stage01/Materials/MI_Brass.MI_Brass")));
 };
 if(Kind==EBBSBStage01Node::LowerLadder)
 { Section(LadderGeometry,Joint-Spacing,End,FVector::ZeroVector);Section(LadderExtension,Start,Joint+Spacing,N*3.f); }
 else Section(LadderGeometry,Start,End,FVector::ZeroVector);
 UpdatePhysicalPresentation();
}
