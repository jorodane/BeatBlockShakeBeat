#include "Stage01/BBSBStage01.h"
#include "BBSBCharacter.h"
#include "Character/BBSBTraversal.h"
#include "Interaction/BBSBClimbable.h"
#include "Interaction/BBSBConditions.h"
#include "Interaction/BBSBDraggable.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
ABBSBStage01Device::ABBSBStage01Device()
{
 PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DeviceOrigin")));
 Interaction = CreateDefaultSubobject<UBBSBDraggable>(TEXT("Interaction"));
 Interaction->bExternalMotion = true;
 Interaction->InteractType = NAME_None;
 Climbable = CreateDefaultSubobject<UBBSBClimbable>(TEXT("Climbable"));
 Climbable->bPrimaryInteraction = false;
 Conditions = CreateDefaultSubobject<UBBSBConditionReceiver>(TEXT("Conditions"));
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
#if WITH_EDITOR
void ABBSBStage01Device::PostDuplicate(EDuplicateMode::Type Mode)
{
 Super::PostDuplicate(Mode);if(Mode!=EDuplicateMode::PIE && !IsTemplate()) { bAutomaticNodeId=true;NodeId=GetFName(); }
}
void ABBSBStage01Device::PostEditImport()
{ Super::PostEditImport();if(!IsTemplate()) { bAutomaticNodeId=true;NodeId=GetFName(); } }
#endif
void ABBSBStage01Device::OnConstruction(const FTransform& T)
{ Super::OnConstruction(T);if(bAutomaticNodeId && GetWorld() && !GetWorld()->IsGameWorld())NodeId=GetFName();Label->SetText(Title);RebuildPhysicalPresentation(); }
void ABBSBStage01Device::BeginPlay()
{
 Super::BeginPlay();
 if(NodeId.IsNone())NodeId=GetFName();
 if(!Director)for(TActorIterator<ABBSBStage01Director> It(GetWorld());It;++It) { Director=*It;break; }
 RebuildPhysicalPresentation();CaptureSurfaceHomes();RegisterPhysicalSurfaces();Label->SetText(Title);
 if(Director)Director->RegisterDevice(this);
}
bool ABBSBStage01Device::IsDirectDrag() const
{
 return Kind==EBBSBStage01Node::CabinDoor || Kind==EBBSBStage01Node::LowerLadder || Kind==EBBSBStage01Node::Trapdoor ||
  Kind==EBBSBStage01Node::CargoLever || Kind==EBBSBStage01Node::LightLever || Kind==EBBSBStage01Node::ReadyGate;
}
void ABBSBStage01Device::SetSolved(bool Value)
{
 bSolved = Value;
 Progress = Value ? 1.f : 0.f;
 UpdateClimbAvailability();
}
void ABBSBStage01Device::Tick(float DT)
{
 Super::Tick(DT);const float PreviousPainted=Painted;
 const float Goal=Kind==EBBSBStage01Node::CabinDoor ? Progress:bSolved ? 1.f:Progress;
 Painted=IsDirectDrag() ? FMath::FInterpTo(Painted,Goal,DT,24.f):FMath::FInterpConstantTo(Painted,Goal,DT,1.f/FMath::Max(.05f,MotionSeconds));
 if(FMath::Abs(Painted-Goal)<.0001f)Painted=Goal;
 const float AttemptedProgress = Painted;
 UpdatePhysicalPresentation(true,PreviousPainted);if(!bSurfacesRegistered)RegisterPhysicalSurfaces();
 UpdateClimbAvailability();
 Interaction->SetExternalDrag(IsDirectDrag() && (!bSolved || Kind == EBBSBStage01Node::CabinDoor), Painted,
   GetActorTransform().InverseTransformVector(DragMotion * Painted), !FMath::IsNearlyEqual(AttemptedProgress, Painted));
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
 else DragMotion=MotionOffset();
 Interaction->SetExternalDrag(IsDirectDrag() && (!bSolved || Kind == EBBSBStage01Node::CabinDoor), Painted,
   GetActorTransform().InverseTransformVector(DragMotion * Painted));
 switch(Kind)
 {
 case EBBSBStage01Node::ToolCache:case EBBSBStage01Node::UtilityDoor:case EBBSBStage01Node::CargoButton:case EBBSBStage01Node::CargoLift:case EBBSBStage01Node::TrioExit:case EBBSBStage01Node::CaptainNote:case EBBSBStage01Node::BuyTea:case EBBSBStage01Node::BuyParts:case EBBSBStage01Node::TuneReward:case EBBSBStage01Node::Departure:Use();break;
 default:if(bSolved && Kind!=EBBSBStage01Node::CabinDoor)Use();break;
 }
}
float ABBSBStage01Device::ProjectedDragProgress(ABBSBBunniarCompanion* Buddy,FVector2D Delta,FVector2D Total) const
{
 const auto* PC = UGameplayStatics::GetPlayerController(this, 0);
 const bool Horizontal=Kind==EBBSBStage01Node::CabinDoor || Kind==EBBSBStage01Node::Trapdoor || Kind==EBBSBStage01Node::ReadyGate;
 const FVector PointAtZero = (Buddy ? Buddy->SurfacePoint : GetActorLocation()) - DragMotion * Painted;
 return UBBSBDraggable::ProjectLinearDrag(PC, PointAtZero, DragMotion, Progress, Delta, RequiredDrag, Horizontal);
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
 if(!Director)
 {
  if(Kind==EBBSBStage01Node::ClimbLadder || (Kind==EBBSBStage01Node::LowerLadder && bSolved))return MountLadder();
  SetSolved(true);return true;
 }
 if(Kind==EBBSBStage01Node::Trapdoor && bSolved && Painted>.98f)return Director->MoveAlongLadder(DestinationPoint(),1.5f,60,GetActorLocation());
 return Director->UseDevice(this);
}
void ABBSBStage01NPC::BeginPlay()
{
 Super::BeginPlay();if(!Director)for(TActorIterator<ABBSBStage01Director> It(GetWorld());It;++It) { Director=*It;break; }
}
bool ABBSBStage01NPC::BunniarQuickUse_Implementation(ABBSBBunniarCompanion* Buddy) { if(Director)Director->TalkTo(this);return true; }
void ABBSBStage01NPC::BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector P,FVector N) { if(Director)Director->TalkTo(this);Super::BunniarAttached_Implementation(Buddy,P,N); }
ABBSBStage01CarrySinger::ABBSBStage01CarrySinger()
{
 Size=FVector(75,85,70);MassKg=1.5;MaximumHealth=100000;ImpactDamagePerMeterPerSecond=0;
 BunniarMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CarriedSongBunniar"));BunniarMesh->SetupAttachment(Body);
 BunniarMesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Meshes/Characters/Bunniar/Bunniar_Normal.Bunniar_Normal")));
 BunniarMesh->SetAnimInstanceClass(UBBSBBunniarAnimInstance::StaticClass());BunniarMesh->SetRelativeScale3D(FVector(.55));BunniarMesh->SetRelativeLocation(FVector(0,0,-40));BunniarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GrabSurface=CreateDefaultSubobject<USphereComponent>(TEXT("BunniarGrabSurface"));GrabSurface->SetupAttachment(BunniarMesh);
 GrabSurface->InitSphereRadius(54.f);GrabSurface->SetRelativeLocation(FVector(0,0,75));
 GrabSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);GrabSurface->SetCollisionResponseToAllChannels(ECR_Ignore);GrabSurface->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);GrabSurface->SetGenerateOverlapEvents(false);
 Body->SetHiddenInGame(true,false);
 Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
}
void ABBSBStage01CarrySinger::BeginPlay() { Super::BeginPlay();SongHome=GetActorLocation(); }
UPrimitiveComponent* ABBSBStage01CarrySinger::CarrySurfaceComponent() const { return GrabSurface; }
bool ABBSBStage01CarrySinger::TraceCarryAim(FVector From,FVector To,FHitResult& Hit) const
{
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BunniarRoundAim),false);
 if(!GrabSurface || !GrabSurface->LineTraceComponent(Hit,From,To,Query))return false;
 Hit.bBlockingHit=true;Hit.Distance=FVector::Distance(From,Hit.ImpactPoint);return true;
}
FText ABBSBStage01CarrySinger::BunniarInteractionText_Implementation() const { return FText::FromString(TEXT("버니아 집기 · 좌클릭 유지: 운반 · 우클릭: 던지기")); }
void ABBSBStage01CarrySinger::BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector P,FVector N)
{
 HeldMeshLocalFrame=BunniarMesh->GetComponentTransform().GetRelativeTransform(GetActorTransform());
 Super::BunniarAttached_Implementation(Buddy,P,N);if(Singer)Singer->SilenceFor(100000);
}
void ABBSBStage01CarrySinger::BunniarArrived_Implementation(ABBSBBunniarCompanion* Buddy)
{
 // Keep the visible pose at contact. Resetting it to the hidden body's rest
 // offset would move the grabbed point sideways at the exact pickup frame.
 if(!HasCarryAttachment())HeldMeshLocalFrame=BunniarMesh->GetComponentTransform().GetRelativeTransform(GetActorTransform());
 Super::BunniarArrived_Implementation(Buddy);
}
void ABBSBStage01CarrySinger::BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy)
{
 Super::BunniarReleased_Implementation(Buddy);
 if(!bCarried && !bBroken && Body->IsSimulatingPhysics())BeginThrowGlide();
 if(Singer)Singer->SilenceFor(Singer->QuietSecondsAfterCarry);
}
void ABBSBStage01CarrySinger::BeginThrowGlide()
{
 bThrowGliding=true;Body->SetEnableGravity(false);Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
 if(Singer)Singer->bSongAttached=false;
}
void ABBSBStage01CarrySinger::EndThrowGlide(bool Landed)
{
 bThrowGliding=false;Body->SetEnableGravity(true);
 // A valid landing can be below the old perch. It becomes the new rest point
 // instead of triggering the old six-metre recovery teleport on the next tick.
 if(Landed)SongHome=GetActorLocation();
}
bool ABBSBStage01CarrySinger::HasGlideSupport() const
{
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BunniarGlideLanding),false,this);Query.AddIgnoredActor(Singer);
 if(const auto* Player=UGameplayStatics::GetPlayerCharacter(this,0))Query.AddIgnoredActor(Player);
 const FVector From=Body->Bounds.Origin,To=From-FVector::UpVector*(Body->Bounds.BoxExtent.Z+4.f);
 for(int32 Attempt=0;Attempt<8;++Attempt)
 {
  FHitResult Hit;if(!GetWorld()->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Query))return false;
  const auto* C=Hit.GetComponent();
  if(C && C->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics && C->GetCollisionResponseToChannel(Body->GetCollisionObjectType())==ECR_Block)
   return Hit.ImpactNormal.Z>.45f;
  if(C)Query.AddIgnoredComponent(C);else return false;
 }
 return false;
}
void ABBSBStage01CarrySinger::TickThrowGlide(float DT)
{
 if(bCarried || bBroken || !Body->IsSimulatingPhysics()) { if(bThrowGliding)EndThrowGlide(false);return; }
 FVector Velocity=Body->GetPhysicsLinearVelocity();
 // Resume natural free flight after loading an airborne body as well.
 if(!bThrowGliding && (FMath::Abs(Velocity.Z)>50.f || Velocity.SizeSquared2D()>FMath::Square(150.f)) && !HasGlideSupport())BeginThrowGlide();
 if(!bThrowGliding)return;
 if(Velocity.Z<=0.f && HasGlideSupport()) { EndThrowGlide(true);return; }
 Body->SetPhysicsLinearVelocity(AdvanceThrowVelocity(Velocity, DT));
}
FVector ABBSBStage01CarrySinger::AdvanceThrowVelocity(FVector Velocity, float DT) const
{
 const float Drag=FMath::Exp(-FMath::Max(.1f,ThrowGlideDrag)*FMath::Max(0.f,DT));Velocity.X*=Drag;Velocity.Y*=Drag;
 // An upward throw loses its excess speed just like a horizontal throw.
 // Keep the existing gentle terminal descent instead of damping it to a hover.
 if(Velocity.Z>0.f)Velocity.Z*=Drag;
 Velocity.Z=FMath::FInterpConstantTo(Velocity.Z,-FMath::Max(10.f,ThrowGlideFallSpeed),DT,FMath::Max(10.f,ThrowGlideVerticalAcceleration));
 return Velocity;
}
void ABBSBStage01CarrySinger::Tick(float DT)
{
 TickThrowGlide(DT);Super::Tick(DT);Body->SetHiddenInGame(true,false);
 if(!bCarried && !bThrowGliding && (GetActorLocation().Z<SongHome.Z-600 || FVector::DistSquared2D(GetActorLocation(),SongHome)>FMath::Square(3000.f))) { ResetObject();Body->SetPhysicsLinearVelocity(FVector::ZeroVector);Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector); }
 if(Body->GetCollisionObjectType()!=ECC_Pawn)Body->SetCollisionObjectType(ECC_Pawn);
}

FVector ABBSBStage01Device::LadderNormal() const
{
 const FVector N=LadderOutward.IsNearlyZero() ? (GetActorLocation()-DestinationPoint()).GetSafeNormal2D():(bLocalPlacement ? GetActorTransform().TransformVectorNoScale(LadderOutward):LadderOutward).GetSafeNormal2D();
 return N.IsNearlyZero() ? -GetActorForwardVector():N;
}
FVector ABBSBStage01Device::LadderBottomPoint() const
{
 return LadderBottom.IsNearlyZero() ? GetActorLocation()+LadderNormal()*50.f-FVector::UpVector*55.f:bLocalPlacement ? GetActorTransform().TransformPosition(LadderBottom):LadderBottom;
}
FVector ABBSBStage01Device::LadderTopPoint() const
{
 return LadderTop.IsNearlyZero() ? DestinationPoint()+LadderNormal()*120.f:bLocalPlacement ? GetActorTransform().TransformPosition(LadderTop):LadderTop;
}
bool ABBSBStage01Device::MountLadder()
{
 auto* P=Director ? Director->PlayerCharacter():Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 SyncClimbableFrame();
 return Climbable->TryMount(P);
}

void ABBSBStage01Device::UpdateClimbAvailability()
{
 const bool bReady = Kind == EBBSBStage01Node::ClimbLadder || (Kind == EBBSBStage01Node::LowerLadder && bSolved && Painted >= .98f);
 Climbable->SetRouteReady(bReady);
}

void ABBSBStage01Device::SyncClimbableFrame()
{
 UpdateClimbAvailability();
 if (Kind != EBBSBStage01Node::ClimbLadder && Kind != EBBSBStage01Node::LowerLadder) return;
 const auto Frame = GetActorTransform();
 Climbable->LocalFrame.Bottom = Frame.InverseTransformPosition(LadderBottomPoint());
 Climbable->LocalFrame.Top = Frame.InverseTransformPosition(LadderTopPoint());
 Climbable->LocalFrame.Exit = Frame.InverseTransformPosition(DestinationPoint());
 Climbable->LocalFrame.Outward = Frame.InverseTransformVectorNoScale(LadderNormal());
 Climbable->RungSpacing = RungSpacing;
 Climbable->GripDepth = Kind == EBBSBStage01Node::LowerLadder ? 29.f : 32.f;
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
 SyncClimbableFrame();
 if(!Ladder)return;
 const auto Frame = Climbable->WorldFrame();
 const float Spacing = FMath::Clamp(RungSpacing, 20.f, 50.f);
 const float Start = -96.f, End = FVector::Distance(Frame.Bottom, Frame.Top) - 20.f, Joint = (Start + End) * .5f;
 auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BBSB/Stage01/Materials/MI_Brass.MI_Brass"));
 LadderGeometry->SetMaterial(0, Material);
 if(Kind==EBBSBStage01Node::LowerLadder)
 {
  UBBSBClimbable::AddRungs(LadderGeometry, Frame, Spacing, Joint - Spacing, End);
  UBBSBClimbable::AddRungs(LadderExtension, Frame, Spacing, Start, Joint + Spacing, Frame.Outward * 3.f);
  LadderExtension->SetMaterial(0, Material);
 }
 else UBBSBClimbable::AddRungs(LadderGeometry, Frame, Spacing, Start, End);
 UpdatePhysicalPresentation();
}

bool ABBSBStage01Device::DragHint(ABBSBBunniarCompanion* Buddy,FVector2D& ScreenAxis,bool& BothDirections) const
{
 ScreenAxis=FVector2D::ZeroVector;BothDirections=Kind==EBBSBStage01Node::CabinDoor;
 if(!bHeld || !IsDirectDrag() || (bSolved && !BothDirections) || DragMotion.IsNearlyZero())return false;
 if(const auto* PC=UGameplayStatics::GetPlayerController(this,0))
 {
  FVector2D A,B;const FVector Start=Buddy ? Buddy->SurfacePoint-DragMotion*Painted:GetActorLocation();
  if(PC->ProjectWorldLocationToScreen(Start,A,true) && PC->ProjectWorldLocationToScreen(Start+DragMotion,B,true))ScreenAxis=(B-A).GetSafeNormal();
 }
 if(ScreenAxis.IsNearlyZero())ScreenAxis=Kind==EBBSBStage01Node::LowerLadder || Kind==EBBSBStage01Node::CargoLever || Kind==EBBSBStage01Node::LightLever ? FVector2D(0,1):FVector2D(1,0);
 return true;
}
