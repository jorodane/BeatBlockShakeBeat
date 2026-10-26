#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "BBSBCharacter.h"
#include "Character/BBSBTraversal.h"
#include "Interaction/BBSBInteraction.h"
#include "BBSB.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace Caravan
{
 const FLinearColor Cream(.78f,.66f,.47f), Gold(.63f,.37f,.11f), Wine(.29f,.075f,.12f), Wood(.20f,.115f,.075f), Blue(.10f,.17f,.23f), Green(.20f,.40f,.29f);
 const TCHAR* SaveSlot=TEXT("BBSB_Caravan_Prototype_v1");
 UStaticMesh* Mesh(const TCHAR* Name) { return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Name,Name)); }
 ABBSBCaravanStage* Director(UWorld* World) { for (TActorIterator<ABBSBCaravanStage> It(World);It;++It) return *It; return nullptr; }
}

ABBSBBunniarDevice::ABBSBBunniarDevice()
{
 PrimaryActorTick.bCanEverTick=true;
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeviceBody")); SetRootComponent(Body);
 Body->SetStaticMesh(Caravan::Mesh(TEXT("Cube"))); Body->SetMobility(EComponentMobility::Movable);
 Body->SetCollisionProfileName(TEXT("BlockAll"));
 Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("DeviceName")); Label->SetupAttachment(Body);
 Label->SetRelativeLocation(FVector(0,0,100)); Label->SetWorldSize(20); Label->SetHorizontalAlignment(EHTA_Center); Label->SetTextRenderColor(FColor(255,224,165));
 Tags.Add(TEXT("BBSBInteractable"));
}
void ABBSBBunniarDevice::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 Body->SetWorldScale3D(Device==EBBSBBunniarDevice::Lift ? FVector(6.4f,7.f,.6f):FVector(1.0f,.8f,1.1f));
 Label->SetWorldScale3D(FVector(1)); Label->SetText(FText::FromString(DeviceLabel));
 BBSBCombatVisuals::Tint(Body,Caravan::Gold);
}
void ABBSBBunniarDevice::Tick(float DT)
{
 Super::Tick(DT);
 if (bPowered)
 {
  Activation=FMath::Min(1.f,Activation+DT/1.5f);
  if (Device!=EBBSBBunniarDevice::Lift && Activation>=1) bLatched=true;
  if (Device==EBBSBBunniarDevice::Lift)
  {
   // Continuous moving base: detaching pauses in place, reattaching never teleports the rider.
   const double Age=GetWorld()->GetTimeSeconds()-PoweredAt;
   const float Phase=FMath::Fmod(float(Age),12.f);
   const float Height=Phase<2 ? 0 : Phase<5 ? (Phase-2)/3.f : Phase<8 ? 1 : Phase<11 ? 1-(Phase-8)/3.f : 0;
   SetActorLocation(StartPosition+FVector(0,0,700*Height),false,nullptr,ETeleportType::None);
  }
 }
 BBSBCombatVisuals::Tint(Body,bPowered ? FLinearColor(.18f,.55f,.46f):bLatched ? Caravan::Cream:Caravan::Gold);
 if (APlayerCameraManager* Cam=UGameplayStatics::GetPlayerCameraManager(this,0)) Label->SetWorldRotation((Cam->GetCameraLocation()-Label->GetComponentLocation()).Rotation());
}
bool ABBSBBunniarDevice::Use() { return Director && Director->InteractWith(this); }
FVector ABBSBBunniarDevice::AttachmentPoint() const { return GetActorLocation()+(Device==EBBSBBunniarDevice::Lift ? FVector(-210,-230,70):FVector(0,0,85)); }
void ABBSBBunniarDevice::SetFocused(bool bFocused) { Body->SetRenderCustomDepth(bFocused); Body->SetCustomDepthStencilValue(bFocused ? 2:0); }

ABBSBBunniarCompanion::ABBSBBunniarCompanion()
{
 AnimationLibrary=TSoftObjectPtr<UBBSBBunniarAnimationSet>(FSoftObjectPath(TEXT("/Game/BBSB/AnimationAuthoring/Bunniar/DA_BunniarAnimation.DA_BunniarAnimation")));
 PrimaryActorTick.bCanEverTick=true;
 PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BuddyOrigin")));
 Visual=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BunniarModel")); Visual->SetupAttachment(GetRootComponent());
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Model(TEXT("/Game/Meshes/Characters/Bunniar/Bunniar_Normal.Bunniar_Normal"));
 if (Model.Succeeded()) Visual->SetSkeletalMeshAsset(Model.Object);
 Visual->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 Visual->SetRelativeScale3D(FVector(1.65f)); Visual->SetRelativeLocation(FVector(0,0,-25)); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 BandageA=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BandageLeft")); BandageA->SetupAttachment(Visual);
 BandageB=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BandageRight")); BandageB->SetupAttachment(Visual);
 for (UStaticMeshComponent* Band:{BandageA.Get(),BandageB.Get()}) { Band->SetStaticMesh(Caravan::Mesh(TEXT("Cube"))); Band->SetRelativeLocation(FVector(0,58,50)); Band->SetRelativeScale3D(FVector(.20f,.025f,.045f)); Band->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
 BandageA->SetRelativeRotation(FRotator(40,0,0)); BandageB->SetRelativeRotation(FRotator(-40,0,0));
}
void ABBSBBunniarCompanion::Tick(float DT)
{
 Super::Tick(DT);
 const float T=GetWorld()->GetTimeSeconds(); FVector Target=GetActorLocation();
 if(const auto* P=Cast<ABBSBCharacter>(GetOwner());P && P->Traversal) P->Traversal->UpdateWirePresentation();
 if (bGliding)
 {
  const auto* Player=Cast<ABBSBCharacter>(GetOwner());
  if (Player && Player->Traversal && Player->Traversal->IsBunniarTraversalActive())
  {
   const auto* Traversal=Player->Traversal.Get();
   const float Duration=AnimationSet ? AnimationSet->AttachTravelSeconds:.24f;
   const float Alpha=Duration<=0 ? 1.f:1-FMath::Pow(1-FMath::Clamp((T-float(SurfaceTransitAt))/Duration,0.f,1.f),3.f);
   const FVector Face=AnimationSet ? AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
   const FQuat ModelFrame=FRotationMatrix::MakeFromXZ(Visual->GetRelativeTransform().TransformVectorNoScale(Face),FVector::UpVector).ToQuat();
   if (Traversal->bGlideDashing)
   {
    const FQuat Spin(Traversal->GlideDashTangent,Traversal->GlideDashElapsed*Traversal->GlideDashSpinHz*2*PI);
    const FQuat Rotation=Spin*Traversal->GlideDashTangent.Rotation().Quaternion()*ModelFrame.Inverse();
    const FVector Back=AnimationSet ? AnimationSet->BackContactPoint:FVector(0,-54,70);
    const FVector Contact=Visual->GetRelativeTransform().TransformPosition(Back)*Traversal->BunniarScale;
    const FVector NormalPosition=Player->GetActorTransform().TransformPosition(Traversal->BunniarOffset)+Traversal->GlideLeadOffset;
    const float Lead=FMath::SmoothStep(0.f,FMath::Max(.01f,Traversal->GlideDashLeadSeconds),Traversal->GlideDashElapsed);
    // Spin through the hand/back contact. An origin under the mesh makes the
    // whole companion orbit through the player's head instead of drilling ahead.
    SetActorLocation(FMath::Lerp(NormalPosition,Traversal->GlideGrip(true)-Rotation.RotateVector(Contact),Lead));
    SetActorRotation(Rotation);
    // The ordinary glide branch blends from the final drill frame on release.
    SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=T;
   }
   else
   {
    SetActorLocation(FMath::Lerp(SurfaceTransitFrom.GetLocation(),Player->GetActorTransform().TransformPosition(Traversal->BunniarOffset)+Traversal->GlideLeadOffset,Alpha));
    const FQuat Rotation=Player->GetActorQuat()*ModelFrame.Inverse();
    SetActorRotation(Alpha<1.f ? FQuat::Slerp(SurfaceTransitFrom.GetRotation(),Rotation,Alpha):FMath::QInterpTo(GetActorQuat(),Rotation,DT,12.f));
   }
   SetActorScale3D(FMath::Lerp(SurfaceTransitFrom.GetScale3D(),FVector(Traversal->BunniarScale),Alpha));
   return;
  }
  EndGlidePresentation();
 }
 if (auto* P=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0))) bEquipped=P->FieldActions && P->FieldActions->FieldEquipment==this;
 if(bEquipped!=bLastEquipped)
 {
  bLastEquipped=bEquipped;
  if(bFollowing && !bSurfaceAttached && !bAirHovering)
  {
   SurfaceTransitFrom=GetActorTransform();SurfaceTransitAt=T;ReturnTransitSeconds=EquipmentTravelSeconds;bSurfaceReturning=true;
  }
 }
 const float SmallScale=AnimationSet ? AnimationSet->CompanionScale:.12f;
 const float NormalScale=!bFollowing ? 1.f:bEquipped ? (AnimationSet ? AnimationSet->EquippedScale:.175f):SmallScale;
 const float TravelSeconds=bSurfaceReturning ? ReturnTransitSeconds:AnimationSet ? AnimationSet->AttachTravelSeconds:.24f;
 const float Transit=TravelSeconds<=0 ? 1.f:FMath::Clamp(float((T-SurfaceTransitAt)/TravelSeconds),0.f,1.f);
 const float Ease=bSurfaceReturning ? Transit*Transit*(3-2*Transit):1-FMath::Pow(1-Transit,3.f);
 SetActorScale3D(bSurfaceAttached || bAirHovering || bSurfaceReturning ? FMath::Lerp(SurfaceTransitFrom.GetScale3D(),FVector(bSurfaceAttached || bAirHovering ? SmallScale:NormalScale),Ease):FVector(NormalScale));
 if(bAirHovering)
 {
  if(!CanMaintainAirHover()) { ReleaseSurface();return; }
  else
  {
   SurfaceArrivalAlpha=Transit;
   SetActorLocation(FMath::Lerp(SurfaceTransitFrom.GetLocation(),AirHoverPoint,Ease));
   const auto* P=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
   const FVector Face=AnimationSet ? AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
   const FQuat ModelFrame=FRotationMatrix::MakeFromXZ(Visual->GetRelativeTransform().TransformVectorNoScale(Face),Visual->GetRelativeTransform().TransformVectorNoScale(FVector::UpVector)).ToQuat();
   const FVector Aim=P->FieldActions->VisualAimPoint();
   FVector Eye;FRotator View;Cast<APlayerController>(P->GetController())->GetPlayerViewPoint(Eye,View);
   // A ray through the anchor has no look direction. Face the player until the aim moves away.
   const FVector LookDirection=FVector::DistSquared(Aim,AirHoverPoint)>FMath::Square(12.f) ? Aim-AirHoverPoint:Eye-AirHoverPoint;
   const FQuat Look=FRotationMatrix::MakeFromXZ(LookDirection.GetSafeNormal(),FVector::UpVector).ToQuat()*ModelFrame.Inverse();
   AirHoverRotation=FMath::QInterpTo(AirHoverRotation,Look,DT,AirGazeInterpSpeed);
   SetActorRotation(Transit<1.f ? FQuat::Slerp(SurfaceTransitFrom.GetRotation(),AirHoverRotation,Ease):AirHoverRotation);
   return;
  }
 }
 if (bSurfaceAttached)
 {
  const auto* P=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
  if (!CanMaintainSurfaceAttachment()) { ReleaseSurface();return; }
  else
  {
   const FTransform Frame=SurfaceLocalFrame*SurfaceComponent->GetComponentTransform();
   SurfacePoint=Frame.GetLocation(); const FQuat Q=Frame.GetRotation().GetNormalized();
   const FVector Face=AnimationSet ? AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
   const FVector FaceAxis=Visual->GetRelativeTransform().TransformVectorNoScale(Face.IsNearlyZero() ? FVector::RightVector:Face);
   SurfaceNormal=Q.RotateVector(FaceAxis).GetSafeNormal();
   const FVector Back=AnimationSet ? AnimationSet->BackContactPoint:FVector(0,-54,70);
   const FVector Contact=Visual->GetRelativeTransform().TransformPosition(Back)*GetActorScale3D();
   SurfaceArrivalAlpha=Transit;
   if(auto* Object=Cast<ABBSBCarryObject>(SurfaceComponent->GetOwner());Object && Object->bManipulating && !P->FieldActions->IsGestureChannelHeld(1)) { Object->EndManipulation(false);P->FieldActions->SetEquipmentPointerCapture(false); }
   SetActorRotation(FQuat::Slerp(SurfaceTransitFrom.GetRotation(),Q,Ease));
   SetActorLocation(FMath::Lerp(SurfaceTransitFrom.GetLocation(),SurfacePoint-Q.RotateVector(Contact)+SurfaceNormal*(AnimationSet ? AnimationSet->SurfaceGap:2),Ease));
   if(!bSurfaceArrived && Transit>=1)
   {
    bSurfaceArrived=true;
    if(AActor* SurfaceOwner=SurfaceComponent->GetOwner();SurfaceOwner && SurfaceOwner->Implements<UBBSBBunniarSurface>()) IBBSBBunniarSurface::Execute_BunniarArrived(SurfaceOwner,this);
   }
   return;
  }
 }
 if (bFollowing)
 {
  if (auto* P=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))
  {
   const FRotator R(0,P->GetControlRotation().Yaw,0);
   Target=bEquipped ? P->FieldActions->FieldEquipmentRestTransform().GetLocation():P->GetActorLocation()+R.RotateVector(FVector(-40,-65,25+FMath::Sin(T*3)*4));
   const FVector Face=AnimationSet ? AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
   const FVector FaceAxis=Visual->GetRelativeTransform().TransformVectorNoScale(Face.IsNearlyZero() ? FVector::RightVector:Face);
   const FQuat ModelFrame=FRotationMatrix::MakeFromXZ(FaceAxis,Visual->GetRelativeTransform().TransformVectorNoScale(FVector::UpVector)).ToQuat();
   const FVector Aim=P->FieldActions ? P->FieldActions->VisualAimPoint():GetActorLocation()+R.Vector()*1000;
   const FQuat Look=FRotationMatrix::MakeFromXZ((Aim-Target).GetSafeNormal(),FVector::UpVector).ToQuat()*ModelFrame.Inverse();
   SetActorRotation(bSurfaceReturning ? FQuat::Slerp(SurfaceTransitFrom.GetRotation(),Look,Ease):FMath::QInterpTo(GetActorQuat(),Look,DT,10));
  }
 }
 if (bSurfaceReturning)
 {
  SetActorLocation(FMath::Lerp(SurfaceTransitFrom.GetLocation(),Target,Ease));
  if (Transit>=1) bSurfaceReturning=false;
 }
 else SetActorLocation(bEquipped && !AttachedDevice ? Target:FMath::VInterpTo(GetActorLocation(),Target,DT,10));
 if (BandageA->GetMaterial(0)==Caravan::Mesh(TEXT("Cube"))->GetMaterial(0)) { BBSBCombatVisuals::Tint(BandageA,FLinearColor(.92f,.83f,.69f)); BBSBCombatVisuals::Tint(BandageB,FLinearColor(.92f,.83f,.69f)); }
}

ABBSBCaravanPerformer::ABBSBCaravanPerformer()
{
 BunniarVisual=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PerformerBunniar")); BunniarVisual->SetupAttachment(GetRootComponent());
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Model(TEXT("/Game/Meshes/Characters/Bunniar/Bunniar_Normal.Bunniar_Normal"));
 if (Model.Succeeded()) BunniarVisual->SetSkeletalMeshAsset(Model.Object);
 BunniarVisual->SetAnimInstanceClass(UBBSBBunniarAnimInstance::StaticClass());
 BunniarVisual->SetRelativeScale3D(FVector(1.1f)); BunniarVisual->SetRelativeLocation(FVector(0,0,-45));
 BunniarVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ABBSBCaravanPerformer::BeginPlay()
{
 Super::BeginPlay();
 Body->SetVisibility(bCaptain); BunniarVisual->SetVisibility(!bCaptain);
 TArray<UStaticMeshComponent*> Meshes; GetComponents(Meshes);
 for (auto* M:Meshes) if (M->ComponentHasTag(TEXT("InkEye"))) M->SetVisibility(bCaptain);
}
void ABBSBCaravanPerformer::Tick(float DT)
{
 Super::Tick(DT);
 BunniarVisual->SetRelativeRotation(FRotator(0,90,0));
 BunniarVisual->SetRelativeLocation(FVector(0,0,-45+FMath::Sin(GetWorld()->GetTimeSeconds()*3)*8));
 if (bDefeated) { Body->SetVisibility(false); BunniarVisual->SetVisibility(!bCaptain); Label->SetText(FText::FromString(bCaptain ? TEXT("상단장: ...좋아. 원인부터 찾아보자."):TEXT("버니아: ♪ 잠깐 쉬었다가!"))); }
}
float ABBSBCaravanPerformer::TakeDamage(float Amount,const FDamageEvent& Event,AController* EventInstigator,AActor* Causer)
{
 return bEncounterEnabled ? Super::TakeDamage(Amount,Event,EventInstigator,Causer):0.f;
}

ABBSBCaravanStage::ABBSBCaravanStage()
{
 PrimaryActorTick.bCanEverTick=true;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CaravanOrigin")));
}
UStaticMeshComponent* ABBSBCaravanStage::Shape(const FString& Name,FVector Position,FVector Size,FLinearColor Color,const TCHAR* Mesh,FRotator Rotation,bool bSolid)
{
 auto* Part=NewObject<UStaticMeshComponent>(this,MakeUniqueObjectName(this,UStaticMeshComponent::StaticClass(),FName(*Name)));
 AddInstanceComponent(Part); Part->ComponentTags.Add(TEXT("CaravanGenerated")); Part->SetupAttachment(GetRootComponent());
 Part->SetStaticMesh(Caravan::Mesh(Mesh)); Part->SetRelativeLocation(Position); Part->SetRelativeRotation(Rotation); Part->SetRelativeScale3D(Size/100.f);
 Part->SetMobility(EComponentMobility::Movable); Part->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
 Part->SetCollisionObjectType(ECC_WorldStatic); Part->SetCollisionResponseToAllChannels(ECR_Block); Part->RegisterComponent();
 BBSBCombatVisuals::Tint(Part,Color); Parts.Add(Part); return Part;
}
void ABBSBCaravanStage::Text(const FString& Value,FVector Position,float Size,FColor Color,FRotator Rotation)
{
 auto* T=NewObject<UTextRenderComponent>(this); AddInstanceComponent(T); T->ComponentTags.Add(TEXT("CaravanGenerated")); T->SetupAttachment(GetRootComponent());
 T->SetRelativeLocation(Position); T->SetRelativeRotation(Rotation); T->SetWorldSize(Size); T->SetHorizontalAlignment(EHTA_Center); T->SetTextRenderColor(Color); T->SetText(FText::FromString(Value)); T->RegisterComponent();
}
void ABBSBCaravanStage::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); BuildLayout(); }
void ABBSBCaravanStage::BuildLayout()
{
 TArray<UActorComponent*> Old=GetComponentsByTag(UActorComponent::StaticClass(),TEXT("CaravanGenerated"));
 for (UActorComponent* C:Old) C->DestroyComponent(); Parts.Reset();
 using namespace Caravan;
 Shape(TEXT("CliffRoad"),FVector(5000,-1000,-120),FVector(12800,2000,240),FLinearColor(.48f,.40f,.31f));
 Shape(TEXT("RockWall"),FVector(5100,1400,600),FVector(13200,650,2500),FLinearColor(.39f,.39f,.37f));
 Shape(TEXT("CliffFace"),FVector(5000,-700,-1400),FVector(13000,2600,2500),FLinearColor(.37f,.34f,.31f));
 for (int32 I=0;I<15;++I)
 {
  Shape(TEXT("DistantMountain"),FVector(I*1500-5000,-17000-(I%3)*3500,-2500),FVector(3700,4300,5000+(I%4)*1500),FLinearColor(.39f+.015f*I,.46f+.008f*I,.53f+.006f*I),TEXT("Cone"),FRotator(0,I*21,0),false);
  Shape(TEXT("RoadRail"),FVector(I*850-1000,-1800,80),FVector(35,35,160),Gold);
 }
 Shape(TEXT("RoadRailLeft"),FVector(3050,-1800,160),FVector(8900,28,25),Wood);
 Shape(TEXT("RoadRailRight"),FVector(9950,-1800,160),FVector(2900,28,25),Wood);
 for (int32 Car=1;Car<=6;++Car)
 {
  const float X=(6-Car)*2000.f; const bool Art=Car<=3; const FLinearColor Hull=Art ? (Car==1 ? Cream:Wine):Blue;
  Shape(TEXT("CarFloor"),FVector(X,0,160),FVector(1600,1600,80),Wood);
  Shape(TEXT("CarSkirt"),FVector(X,0,70),FVector(1660,1500,90),Hull);
  Shape(TEXT("InsideRug"),FVector(X,0,205),FVector(1100,1050,8),Art ? Wine:FLinearColor(.25f,.26f,.22f));
  Shape(TEXT("FarWall"),FVector(X,800,550),FVector(1640,45,700),Hull);
  // Side doorway is 400cm wide; actual steps make the route walkable with the existing dodge controls.
  for (int32 Side:{-1,1}) Shape(TEXT("DoorSide"),FVector(X+Side*510,-800,525),FVector(580,45,650),Hull);
  Shape(TEXT("DoorLintel"),FVector(X,-800,840),FVector(440,45,80),Gold);
  for (int32 End:{-1,1})
  {
   for (int32 Side:{-1,1}) Shape(TEXT("EndWall"),FVector(X+End*800,Side*530,550),FVector(40,540,700),Hull);
   Shape(TEXT("EndLintel"),FVector(X+End*800,0,800),FVector(40,520,200),Hull);
  }
  if (Car==3)
  {
   for (int32 Side:{-1,1}) Shape(TEXT("OpenStageRoof"),FVector(X,Side*650,900),FVector(1600,300,65),Wine);
   Text(TEXT("3  /  OPEN REHEARSAL"),FVector(X,-850,790),38);
  }
  else if (Car==5)
  {
   Shape(TEXT("LiftRoofRear"),FVector(X-475,0,900),FVector(650,1600,60),Blue);
   Shape(TEXT("LiftRoofFront"),FVector(X+675,0,900),FVector(250,1600,60),Blue);
   for (int32 Side:{-1,1}) Shape(TEXT("LiftRoofSide"),FVector(X+200,Side*625,900),FVector(1000,350,60),Blue);
  }
  else Shape(TEXT("CarRoof"),FVector(X,0,900),FVector(1660,1660,70),Hull);
  for (int32 Side:{-1,1})
  {
   Shape(TEXT("GoldTrim"),FVector(X,Side*825,300),FVector(1630,25,25),Gold,TEXT("Cube"),FRotator::ZeroRotator,false);
   Shape(TEXT("RoofTrim"),FVector(X,Side*825,895),FVector(1670,25,40),Gold);
   Shape(TEXT("RoofRail"),FVector(X,Side*750,980),FVector(1600,22,80),Gold);
   for (int32 W:{-1,1})
   {
    Shape(TEXT("WindowFrame"),FVector(X+W*510,Side*827,570),FVector(400,16,310),Gold,TEXT("Cube"),FRotator::ZeroRotator,false);
    Shape(TEXT("WindowBlue"),FVector(X+W*510,Side*838,575),FVector(345,12,255),FLinearColor(.19f,.31f,.35f),TEXT("Cube"),FRotator::ZeroRotator,false);
    Shape(TEXT("Wheel"),FVector(X+W*500,Side*710,-10),FVector(170,170,80),Wood,TEXT("Cylinder"),FRotator(0,0,90),false);
    Shape(TEXT("WheelHub"),FVector(X+W*500,Side*755,-10),FVector(72,72,22),Gold,TEXT("Cylinder"),FRotator(0,0,90),false);
   }
  }
  for (int32 S=0;S<5;++S) Shape(TEXT("DoorSteps"),FVector(X,-1240+S*95,20+S*20),FVector(380,100,40+S*40),Wood);
  Text(FString::Printf(TEXT("%d  /  %s"),Car,Car<=3 ? TEXT("ART CARAVAN"):TEXT("LIFE CARAVAN")),FVector(X,-850,770),40);
 }
 for (int32 C=1;C<6;++C)
 {
  const float X=(6-C)*2000.f-1000;
  Shape(TEXT("Coupling"),FVector(X,0,100),FVector(440,120,80),Gold);
  if (C!=3) Shape(TEXT("Gangway"),FVector(X,0,180),FVector(450,520,40),Wood);
 }
 Shape(TEXT("RoofGangway54"),FVector(3000,0,910),FVector(450,450,45),Wood);
 RoofBridge=Shape(TEXT("RoofWinchBridge"),FVector(5000,0,1100),FVector(460,460,55),Gold);
 RoofBridge->SetRelativeRotation(FRotator(90,0,0));
 CargoGate=Shape(TEXT("CargoShutter"),FVector(1000,0,460),FVector(65,520,560),Blue);
 ForwardGate=Shape(TEXT("StageEquipmentBlockade"),FVector(5000,-1210,300),FVector(150,1000,650),Wine);
 Shape(TEXT("Car4FrontBulkhead"),FVector(4800,0,500),FVector(45,520,600),Blue);
 Shape(TEXT("CargoSideBulkhead"),FVector(2000,-800,500),FVector(420,45,600),Blue);
 NorthGate=Shape(TEXT("RehearsalFrontDoor"),FVector(7000,0,500),FVector(80,520,600),Wine);
 // 4: desk and book support, sleeping corner, three window plants.
 Shape(TEXT("WritingDesk"),FVector(4200,300,340),FVector(390,170,30),Wood);
 Shape(TEXT("WritingBook"),FVector(4200,300,365),FVector(110,90,12),Cream,TEXT("Cube"),FRotator(8,0,0),false);
 Shape(TEXT("Bed"),FVector(3500,440,255),FVector(480,320,110),Cream);
 for (int32 I=0;I<3;++I)
 {
  Shape(TEXT("PlantPot"),FVector(3800+I*180,-660,360),FVector(70,70,90),Wine,TEXT("Cylinder"));
  Shape(TEXT("PlantLeaves"),FVector(3800+I*180,-660,450),FVector(140,100,160),Green,TEXT("Sphere"),FRotator(0,I*30,15),false);
 }
 // Cargo paths deliberately leave a full-width lane to the elevator.
 for (int32 I=0;I<7;++I) Shape(TEXT("CargoCrate"),FVector(1450+(I%3)*220,500+(I/3)*75,300),FVector(160,150,200),I%2 ? Wood:Gold);
 for (int32 C:{1,2,6})
 {
  const float X=(6-C)*2000.f;
  Shape(TEXT("ShopCounter"),FVector(X+250,440,320),FVector(650,180,240),Wood);
  for (int32 I=0;I<6;++I) Shape(TEXT("ShopGoods"),FVector(X+I*90,440,480),FVector(55,55,80+I*8),I%2 ? Cream:Wine,TEXT("Cylinder"),FRotator::ZeroRotator,false);
 }
 // Instruments sit at the edges, leaving a clear combat formation in car 3.
 for (int32 I=0;I<3;++I)
 {
  Shape(TEXT("RehearsalDrum"),FVector(5650+I*350,570,340),FVector(170,170,190),Wine,TEXT("Cylinder"));
  Shape(TEXT("DrumRim"),FVector(5650+I*350,570,440),FVector(185,185,18),Gold,TEXT("Cylinder"),FRotator::ZeroRotator,false);
 }
 // Outdoor final stage and its approach, kept outside the narrow road.
 Shape(TEXT("OutdoorStage"),FVector(8000,-3100,100),FVector(2500,2200,200),Wood);
 Shape(TEXT("StageCarpet"),FVector(8000,-3100,205),FVector(2200,1950,10),Wine);
 Shape(TEXT("ApproachBridge"),FVector(8000,-1900,0),FVector(850,800,50),Wood);
 for (int32 I=0;I<5;++I) Shape(TEXT("StageSteps"),FVector(8000,-1620-I*85,20+I*20),FVector(800,100,40+I*40),Wood);
 for (int32 Side:{-1,1})
 {
  Shape(TEXT("StageColumn"),FVector(8000+Side*1150,-3900,700),FVector(70,70,1300),Gold);
  Shape(TEXT("StageCurtain"),FVector(8000+Side*950,-4100,650),FVector(420,60,1200),Wine,TEXT("Cube"),FRotator::ZeroRotator,false);
 }
 Shape(TEXT("StageProscenium"),FVector(8000,-4100,1300),FVector(2400,100,90),Gold);
 Text(TEXT("HALF-STEP  /  REHEARSAL"),FVector(8000,-4060,1100),65,FColor(232,194,113),FRotator(0,90,0));
}

void ABBSBCaravanStage::BeginPlay()
{
 Super::BeginPlay(); BuildLayout(); StartedAt=GetWorld()->GetTimeSeconds(); Checkpoint=P(FVector(4000,0,302));
 Inventory=Cast<UBBSBCaravanSave>(bPersistProgress ? UGameplayStatics::LoadGameFromSlot(Caravan::SaveSlot,0):nullptr);
 if (!Inventory) Inventory=NewObject<UBBSBCaravanSave>(this);
 auto Device=[&](EBBSBBunniarDevice Kind,FVector Location,const TCHAR* Name)
 {
  auto* D=GetWorld()->SpawnActor<ABBSBBunniarDevice>(P(Location),FRotator::ZeroRotator);
  D->Device=Kind; D->DeviceLabel=Name; D->Director=this; D->StartPosition=P(Location); D->OnConstruction(D->GetActorTransform()); Devices.Add(D); return D;
 };
 auto* Book=Device(EBBSBBunniarDevice::Bookstand,FVector(4200,180,300),TEXT("책상 기록 / 버니아 부착"));
 Device(EBBSBBunniarDevice::Shutter,FVector(780,-150,300),TEXT("화물문 크랭크 / 버니아 부착"));
 Device(EBBSBBunniarDevice::Lift,FVector(2200,0,200),TEXT("화물 승강기 / 버니아 구동"));
 Device(EBBSBBunniarDevice::Winch,FVector(4480,-260,990),TEXT("지붕 다리 윈치 / 버니아 구동"));
 Device(EBBSBBunniarDevice::ArtShop,FVector(8000,150,300),TEXT("2호차 예술단 상점"));
 Device(EBBSBBunniarDevice::UtilityShop,FVector(0,150,300),TEXT("6호차 현생단 상점"));
 Device(EBBSBBunniarDevice::CaptainDesk,FVector(10000,150,300),TEXT("상단장의 연습 기록 / 버니아 부착"));
 Device(EBBSBBunniarDevice::Departure,FVector(10400,-400,300),TEXT("상단 운행표 / 버니아 부착"));
 Buddy=GetWorld()->SpawnActor<ABBSBBunniarCompanion>(Book->AttachmentPoint(),FRotator::ZeroRotator); Buddy->Director=this;
 Buddy->BandageA->SetVisibility(true); Buddy->BandageB->SetVisibility(true);
 auto Boss=[&](bool Final)
 {
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* B=GetWorld()->SpawnActor<ABBSBCaravanPerformer>(P(Final ? FVector(8000,-3200,300):FVector(6000,0,300)),FRotator::ZeroRotator,Params);
  B->bCaptain=Final; B->MaximumHealth=B->Health=Final ? 210.f:135.f; B->ShotDamage=Final ? 6.f:4.f;
  B->EncounterID=Final ? 4102:4101; B->EnemyName=Final ? FName(TEXT("상단장 · 임시 모델")):FName(TEXT("삑사리를 즐기는 버니아"));
  B->MainPattern=B->AuthoredPattern=Final ? EBBSBInkPattern::Curve:EBBSBInkPattern::Tap;
  B->Tint=Final ? Caravan::Wine:FLinearColor(.77f,.60f,.72f); B->Body->SetVisibility(Final); B->BunniarVisual->SetVisibility(!Final); B->UpdateLabel(); return B;
 };
 Miniboss=Boss(false); Captain=Boss(true);
 Say(TEXT("레파라도: 쉬는 시간에 기록까지 정리하는데.\n잠깐, 방금 그 음... 너였어?"),7);
}
void ABBSBCaravanStage::EndPlay(const EEndPlayReason::Type Reason)
{
 CloseShop();
 if (Player && Player->FieldActions)
 {
  if (Player->FieldActions->FieldEquipment==Buddy) Player->FieldActions->ClearFieldEquipment();
  Player->FieldActions->EquipmentGesture.Unbind();
  if (auto* PC=Cast<APlayerController>(Player->GetController())) if (Controls) PC->PopInputComponent(Controls);
 }
 Super::EndPlay(Reason);
}
void ABBSBCaravanStage::BindPlayer()
{
 Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)); if (!Player) return;
 auto* PC=Cast<APlayerController>(Player->GetController()); if (!PC || !Player->InputComponent) return;
 Player->FieldActions->bShowPracticeHUD=false;
 if (!Cast<ABBSBCaravanHUD>(PC->GetHUD())) PC->ClientSetHUD(ABBSBCaravanHUD::StaticClass());
 Controls=NewObject<UInputComponent>(this,TEXT("CaravanControls")); Controls->RegisterComponent(); Controls->Priority=20;
 Controls->BindKey(EKeys::G,IE_Pressed,this,&ABBSBCaravanStage::RecallBuddy);
 Controls->BindKey(EKeys::H,IE_Pressed,this,&ABBSBCaravanStage::ConsumeTea);
 Controls->BindKey(EKeys::BackSpace,IE_Pressed,this,&ABBSBCaravanStage::RetryCheckpoint);
 Controls->BindKey(EKeys::Up,IE_Pressed,this,&ABBSBCaravanStage::ShopUp);
 Controls->BindKey(EKeys::Down,IE_Pressed,this,&ABBSBCaravanStage::ShopDown);
 Controls->BindKey(EKeys::Enter,IE_Pressed,this,&ABBSBCaravanStage::ShopBuy);
 Player->FieldActions->BunniarEquipment=Buddy; Buddy->bFollowing=true;
 Player->FieldActions->EquipmentGesture.BindUObject(Buddy,&ABBSBBunniarCompanion::HandleEquipmentGesture);
 PC->PushInputComponent(Controls); bBound=true;
}
ABBSBBunniarDevice* ABBSBCaravanStage::FindDevice(EBBSBBunniarDevice Kind) const { for (ABBSBBunniarDevice* D:Devices) if (D && D->Device==Kind) return D; return nullptr; }
void ABBSBCaravanStage::Say(const FString& Text,float Seconds) { Dialogue=Text; DialogueUntil=GetWorld()->GetTimeSeconds()+Seconds; UE_LOG(LogBBSB,Display,TEXT("Caravan: %s"),*Text); }
void ABBSBCaravanStage::Tick(float DT)
{
 Super::Tick(DT); if (!bBound) BindPlayer(); if (!Player) return;
 if (!bIntroCue && GetWorld()->GetTimeSeconds()-StartedAt>7 && Step==EBBSBCaravanStep::Rest) { bIntroCue=true; Say(TEXT("레파라도: 또? 입을 막아도 소리가 나잖아.\n...책상 기록부터 확인하고 단장을 만나러 가자."),7); }
 UpdateFocus(); UpdateProgress();
 if (Buddy && Buddy->AttachedDevice && Player->FieldActions->FieldEquipment!=Buddy)
 { Buddy->ReleaseSurface(); }
 if (Player->Combat->bDead) { if (DeathAt<0) DeathAt=GetWorld()->GetTimeSeconds(); if (GetWorld()->GetTimeSeconds()-DeathAt>1.8) RetryCheckpoint(); }
 else DeathAt=-1;
 if (Player->GetActorLocation().Z<GetActorLocation().Z-350) { ++FallsRecovered; RetryCheckpoint(); Say(TEXT("버니아가 끌어올려 주었다. 마지막 안전 지점에서 다시."),4); }
}
void ABBSBCaravanStage::UpdateFocus()
{
 // Read the common target; never select or write custom depth from a second system.
 Focus=Buddy && Player->FieldActions->FieldEquipment==Buddy ? Cast<ABBSBBunniarDevice>(Player->FieldActions->HoveredActor):nullptr;
 if(Focus && Focus->Director!=this) Focus=nullptr;
}
void ABBSBCaravanStage::Interact()
{
 if(OpenShop) { CloseShop();return; }
 if(Player && Buddy && Player->FieldActions->FieldEquipment==Buddy) Buddy->AttachFromCursor();
}
ABBSBBunniarDevice* ABBSBCaravanStage::AimDevice() const
{
 return Player && Player->FieldActions->FieldEquipment==Buddy ? Cast<ABBSBBunniarDevice>(Player->FieldActions->HoveredActor):nullptr;
}
bool ABBSBCaravanStage::EquipBunniar()
{
 if (!Player || !Buddy || OpenShop) return false;
 return Player->FieldActions->RequestEquipment(0);
}
void ABBSBCaravanStage::EquipBunniarKey()
{
 if (!EquipBunniar()) Say(TEXT("버니아는 Tab으로 선택하고 행동이 끝난 뒤 장착할 수 있다."),3);
}
bool ABBSBCaravanStage::InteractWith(ABBSBBunniarDevice* D)
{
 if (!Player || !IsValid(D) || D->Director!=this) return false;
 if (Player->FieldActions->bEquipmentWheelOpen) return false;
 if(Player->FieldActions->FieldEquipment!=Buddy || !Buddy->bSurfaceAttached || !Buddy->SurfaceComponent || Buddy->SurfaceComponent->GetOwner()!=D) return false;
 const bool Puzzle=D->Device==EBBSBBunniarDevice::Shutter || D->Device==EBBSBBunniarDevice::Lift || D->Device==EBBSBBunniarDevice::Winch;
 if (Puzzle && Player->FieldActions->FieldEquipment!=Buddy) { Say(TEXT("버니아 (Tab)를 장착하고 장치를 조준하자."),3); return false; }
 if (FVector::Dist(Player->GetActorLocation(),Buddy->SurfacePoint)>Player->FieldActions->WeaponReach) return false;
 if (Player->Combat->IsStageLocked() || !Player->FieldActions->CanMove()) return false;
 FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(CaravanUse),false,Player); Q.AddIgnoredActor(Buddy);
 if (GetWorld()->LineTraceSingleByChannel(Hit,Player->GetActorLocation()+FVector(0,0,25),Buddy->SurfacePoint,ECC_Visibility,Q) && Hit.GetActor()!=D) return false;
 switch (D->Device)
 {
 case EBBSBBunniarDevice::Bookstand:
  if (Step!=EBBSBCaravanStep::Rest) return false;
  Step=EBBSBCaravanStep::Detour; Buddy->bFollowing=true; Buddy->BandageA->SetVisibility(true); Buddy->BandageB->SetVisibility(true);
  Say(TEXT("레파라도: 반창고는 효과가 없네. 떼는 건 나중에.\n버니아 (Tab) · 좌클릭을 누른 동안 부착 / 놓으면 회수. 6호차로 돌아가자."),9); return true;
 case EBBSBBunniarDevice::ArtShop: case EBBSBBunniarDevice::UtilityShop:
  OpenShop=D; ShopSelection=0; Player->FieldActions->bEquipmentInputBlocked=true; Player->FieldActions->CancelEquipmentSelection(); Player->FieldActions->InterruptAction(EBBSBActionState::Transition,100000);
  if (auto* PC=Cast<APlayerController>(Player->GetController())) { PC->SetIgnoreLookInput(true); PC->SetIgnoreMoveInput(true); bShopLookOwned=bShopMoveOwned=true; }
  return true;
 case EBBSBBunniarDevice::CaptainDesk:
  if (Step!=EBBSBCaravanStep::SearchCaptain || !bVisitedArtCar) { Say(TEXT("연습 기록을 읽기 전에, 3호차와 2호차 상황부터 확인하자.")); return false; }
  bReadCaptainNote=true; Checkpoint=P(FVector(8000,-1200,110));
  Say(TEXT("레파라도: 삑사리를 연습으로 고치려고? 관객도 없는 절벽에?\n원인이 그대로면 백 번을 해도 같아. 밖의 가설 무대로 가자."),9); return true;
 case EBBSBBunniarDevice::Departure:
  Say(Step==EBBSBCaravanStep::Complete ? TEXT("1지역 초안 완료. 상단은 다음 지역으로 출발할 준비가 됐다.\n2·6호차에 새 상품이 입고됐다. 자유롭게 둘러보거나 PIE를 다시 시작할 수 있다."):TEXT("상단장이 돌아오기 전에는 출발할 수 없다."),9); return Step==EBBSBCaravanStep::Complete;
 default: break;
 }
 if (!Buddy->bFollowing) { Say(TEXT("먼저 4호차의 책받침 버니아를 데려오자.")); return false; }
 if (Buddy->AttachedDevice) { Say(TEXT("버튼을 놓아 회수한 뒤 다른 표면에 붙일 수 있다."),3); return false; }
 if (D->Device==EBBSBBunniarDevice::Lift && !bShutterOpen) return false;
 if (D->Device==EBBSBBunniarDevice::Winch && !bLiftRidden) return false;
 Buddy->AttachedDevice=D; D->bPowered=true;
 if (D->Device==EBBSBBunniarDevice::Lift)
 {
  const float Height=FMath::Clamp((D->GetActorLocation().Z-D->StartPosition.Z)/700.f,0.f,1.f);
  D->PoweredAt=GetWorld()->GetTimeSeconds()-(Height>0.01f ? 2+Height*3:0);
  Say(TEXT("레파라도: 승강기는 들어 올리고, 윈치는 다리를 당기고.\n누른 채로 발판 위에서 기다려 보자. 놓으면 멈춘다."),6);
 }
 else { D->PoweredAt=GetWorld()->GetTimeSeconds(); Say(D->Device==EBBSBBunniarDevice::Shutter ? TEXT("크랭크가 화물문을 연다. 열린 뒤 버튼을 놓아 회수하자."):TEXT("윈치가 지붕 사이의 다리를 내린다. 완료 후 버튼을 놓아 회수하자."),4); }
 return true;
}
void ABBSBCaravanStage::RecallBuddy()
{
 if (!Buddy || !Buddy->bSurfaceAttached || !Player || !Player->FieldActions->CanMove() || Player->Combat->IsStageLocked() || OpenShop) return;
 Buddy->ReleaseSurface(); Say(TEXT("버니아를 회수했다. 누른 동안 표면에 등을 붙이고, 놓으면 돌아온다."),2.5f);
}
void ABBSBCaravanStage::UpdateProgress()
{
 // A dissolved formation is retryable even when the player escaped without dying.
 if (!Player->Combat->bDead && !Player->Combat->IsStageLocked())
 {
  if (Step==EBBSBCaravanStep::Miniboss && !Miniboss->bDefeated) { Miniboss->bEncounterEnabled=false; Step=EBBSBCaravanStep::Rooftops; }
  if (Step==EBBSBCaravanStep::CaptainDuel && !Captain->bDefeated) { Captain->bEncounterEnabled=false; Step=EBBSBCaravanStep::SearchCaptain; }
 }
 auto* Shutter=FindDevice(EBBSBBunniarDevice::Shutter); auto* Winch=FindDevice(EBBSBBunniarDevice::Winch);
 if (Shutter && Shutter->bLatched && !bShutterOpen) { bShutterOpen=true; Step=EBBSBCaravanStep::Cargo; Checkpoint=P(FVector(1400,0,310)); }
 if (bShutterOpen) { CargoGate->SetCollisionEnabled(ECollisionEnabled::NoCollision); CargoGate->SetHiddenInGame(true); }
 const FVector Local=Player->GetActorLocation()-GetActorLocation();
 if (!bLiftRidden && bShutterOpen && Local.Z>990 && FMath::Abs(Local.X-2200)<550 && FMath::Abs(Local.Y)<650)
 { bLiftRidden=true; Step=EBBSBCaravanStep::Rooftops; Checkpoint=P(FVector(2670,0,1040)); Say(TEXT("레파라도: 지붕으로 돌아가는 길이 있었네.\n4호차 지붕의 윈치로 다음 다리를 내리자.")); }
 if (Winch && Winch->bLatched) bBridgeOpen=true;
 if (bBridgeOpen) { RoofBridge->SetRelativeRotation(FRotator::ZeroRotator); RoofBridge->SetRelativeLocation(FVector(5000,0,910)); }
 if (Step==EBBSBCaravanStep::Rooftops && bBridgeOpen && FMath::Abs(Local.X-6000)<700 && FMath::Abs(Local.Y)<650 && Local.Z<650)
 {
  if (!Buddy->AttachedDevice) StartEncounter(false);
 }
 if (Step==EBBSBCaravanStep::Miniboss && Miniboss->bDefeated && !Player->Combat->IsStageLocked())
 {
  Step=EBBSBCaravanStep::SearchCaptain; Checkpoint=P(FVector(6500,0,310));
  Say(TEXT("레파라도: 새 소리가 재미있는 건 알겠는데, 일단 쉬자.\n단장은 1호차인가? 2호차부터 확인해야겠어."),8);
 }
 if (Step>=EBBSBCaravanStep::SearchCaptain)
 {
  NorthGate->SetCollisionEnabled(ECollisionEnabled::NoCollision); NorthGate->SetHiddenInGame(true);
  ForwardGate->SetCollisionEnabled(ECollisionEnabled::NoCollision); ForwardGate->SetHiddenInGame(true);
 }
 if (Step==EBBSBCaravanStep::SearchCaptain && !bVisitedArtCar && FMath::Abs(Local.X-8000)<650 && FMath::Abs(Local.Y)<650)
 { bVisitedArtCar=true; Say(TEXT("상점 메모: 단장님의 연습에서도 삑사리가 났다.\n1호차 책상에 연습 기록을 두고 가설 공연장으로 나가셨다."),7); }
 if (Step==EBBSBCaravanStep::SearchCaptain && bReadCaptainNote && FVector::Dist2D(Player->GetActorLocation(),Captain->GetActorLocation())<850) StartEncounter(true);
 if (Step==EBBSBCaravanStep::CaptainDuel && Captain->bDefeated && !Player->Combat->IsStageLocked())
 {
  Step=EBBSBCaravanStep::Complete; Checkpoint=P(FVector(8000,-2400,310));
  if (!Inventory->bSupplyUnlocked) { Inventory->Coins+=60; Inventory->bSupplyUnlocked=true; SaveProgress(); }
  Say(TEXT("레파라도: 연습은 충분해. 이제 원인을 확인할 차례야.\n1지역 완료 · 새 유통망 확보! 2·6호차에 상품이 해금됐다. 소유하려면 구매해야 한다."),12);
 }
}
bool ABBSBCaravanStage::StartEncounter(bool Final)
{
 if (!Player || Player->Combat->IsStageLocked() || OpenShop || Buddy->bSurfaceAttached) return false;
 if (Final ? Step!=EBBSBCaravanStep::SearchCaptain || !bReadCaptainNote : Step!=EBBSBCaravanStep::Rooftops || !bBridgeOpen) return false;
 ABBSBCaravanPerformer* Boss=Final ? Captain.Get():Miniboss.Get(); if (!Boss || Boss->bDefeated) return false;
 auto* S=GetWorld()->SpawnActor<ABBSBCombatStage>(); S->SetExperiment(Final ? 1:0); S->BPM=StageBPM; S->ParryDistance=Final ? 570.f:430.f; S->RingRadius=160;
 if (!S->Initialize(Player->Combat,Boss)) { S->Destroy(); return false; }
 Encounter=S; Boss->bEncounterEnabled=true; Player->Combat->AttachStage(S);
 Step=Final ? EBBSBCaravanStep::CaptainDuel:EBBSBCaravanStep::Miniboss;
 Checkpoint=P(Final ? FVector(8000,-2420,310):FVector(5500,0,310));
  Say(Final ? TEXT("상단장과 음악 경합 · Tab으로 한손검을 장착하자.\nWASD/X 방향 방어 · Space 회피 · 마우스로 무기 공격"):TEXT("삑사리를 즐기는 버니아 · Tab으로 한손검을 장착하자.\nWASD/X 방향 방어 · Space 회피 · 마우스로 무기 공격"),8);
 return true;
}
void ABBSBCaravanStage::RetryCheckpoint()
{
 if(OpenShop) { CloseShop();return; }
 if (!Player) return; CloseShop();
 Player->FieldActions->CancelEquipmentSelection();
 if (Encounter && Encounter->IsActive()) Encounter->Dissolve(false);
 Player->Combat->LeaveStage(false); Player->Combat->bDead=false; Player->Combat->Health=Player->Combat->MaximumHealth; Player->Combat->Ecstatic=0;
 Player->FieldActions->ResetActionState(); Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Player->GetCharacterMovement()->StopMovementImmediately();
 if (Buddy) Buddy->ReleaseSurface();
 Player->SetActorLocation(Checkpoint,false,nullptr,ETeleportType::TeleportPhysics); DeathAt=-1;
 if (Step==EBBSBCaravanStep::Miniboss) { Miniboss->ResetEnemy(); Step=EBBSBCaravanStep::Rooftops; }
 if (Step==EBBSBCaravanStep::CaptainDuel) { Captain->ResetEnemy(); Step=EBBSBCaravanStep::SearchCaptain; }
 Say(TEXT("마지막 안전 지점에서 다시 시작한다."),3);
}
void ABBSBCaravanStage::SaveProgress() { if (bPersistProgress && Inventory) UGameplayStatics::SaveGameToSlot(Inventory,Caravan::SaveSlot,0); }
void ABBSBCaravanStage::CloseShop()
{
 if (Player && Player->FieldActions) Player->FieldActions->bEquipmentInputBlocked=false;
 if (Player && OpenShop)
 {
  Player->FieldActions->ResetActionState();
  if (auto* PC=Cast<APlayerController>(Player->GetController())) { if (bShopLookOwned) PC->SetIgnoreLookInput(false); if (bShopMoveOwned) PC->SetIgnoreMoveInput(false); }
 }
 bShopLookOwned=bShopMoveOwned=false; OpenShop=nullptr;
}
void ABBSBCaravanStage::ShopUp() { if (OpenShop) ShopSelection=0; }
void ABBSBCaravanStage::ShopDown() { if (OpenShop) ShopSelection=1; }
void ABBSBCaravanStage::ShopBuy() { if (OpenShop) Purchase(ShopSelection); }
bool ABBSBCaravanStage::Purchase(int32 Index)
{
 if (!OpenShop || !Inventory || Index<0 || Index>1) return false;
 if (Index==1 && !Inventory->bSupplyUnlocked) { Say(TEXT("지역 문제를 해결해 유통망을 확보하면 입고된다。"),3); return false; }
 const bool Art=OpenShop->Device==EBBSBBunniarDevice::ArtShop; const int32 Price=Index==0 ? (Art ? 15:20):30;
 if (Art && Index==1 && Inventory->bRibbonOwned) { Say(TEXT("이미 구매한 장식이다."),3); return false; }
 if (Inventory->Coins<Price) { Say(TEXT("소지금이 부족하다."),3); return false; }
 Inventory->Coins-=Price;
 if (Art) { if (Index==0) ++Inventory->Postcards; else Inventory->bRibbonOwned=true; }
 else Inventory->Tea+=Index==0 ? 1:2;
 SaveProgress(); Say(TEXT("직원가로 구매했다. 해금된 상품도 상단의 재고이므로 따로 구매한다."),4); return true;
}
void ABBSBCaravanStage::ConsumeTea()
{
 if (!Player || OpenShop || !Inventory || Inventory->Tea<=0 || !Player->FieldActions->CanMove() || Player->Combat->Health>=Player->Combat->MaximumHealth) return;
 --Inventory->Tea; Player->Combat->Health=FMath::Min(Player->Combat->MaximumHealth,Player->Combat->Health+40); SaveProgress(); Say(TEXT("휴식용 차 · 체력 40 회복"),3);
}
FString ABBSBCaravanStage::Objective() const
{
 switch (Step)
 {
 case EBBSBCaravanStep::Rest: return TEXT("4호차 · 책상 기록 살펴보기 [버니아·좌클릭]\nTab 전환 · 길게 눌러 장비 선택");
 case EBBSBCaravanStep::Detour: return TEXT("버니아 (Tab) · 6호차 화물문 크랭크 조준\n좌클릭 유지 · 열린 뒤 버튼 놓기");
 case EBBSBCaravanStep::Cargo: return TEXT("버니아 (Tab) · 승강기에 좌클릭 유지\n발판에 올라 지붕까지 이동");
 case EBBSBCaravanStep::Rooftops: return !bBridgeOpen ? TEXT("버니아 (Tab) · 4호차 지붕 윈치에 좌클릭 유지"):Buddy && Buddy->bSurfaceAttached ? TEXT("버튼 놓아 회수 · 3호차 열린 지붕으로"):TEXT("다리를 건너 3호차 열린 지붕으로 내려가기");
 case EBBSBCaravanStep::Miniboss: return TEXT("3호차 · 버니아의 시끄러운 연주 멈추기");
 case EBBSBCaravanStep::SearchCaptain: return !bVisitedArtCar ? TEXT("2호차 · 상단장의 행적 확인하기"):!bReadCaptainNote ? TEXT("1호차 · 상단장의 연습 기록 읽기 [버니아·좌클릭]"):TEXT("2호차 밖 · 가설 공연장으로 이동");
 case EBBSBCaravanStep::CaptainDuel: return TEXT("가설 공연장 · 상단장과 음악 경합");
 default: return TEXT("1지역 완료 · 두 상점에 신규 상품 입고\n1호차 운행표에서 출발 준비 확인 [버니아·좌클릭]");
 }
}
FVector ABBSBCaravanStage::ObjectiveLocation() const
{
 switch (Step)
 {
 case EBBSBCaravanStep::Rest: return P(FVector(4200,180,300));
 case EBBSBCaravanStep::Detour: return P(FVector(780,-150,300));
 case EBBSBCaravanStep::Cargo: { auto* D=FindDevice(EBBSBBunniarDevice::Lift); return D ? D->GetActorLocation():P(FVector(2200,0,200)); }
 case EBBSBCaravanStep::Rooftops: return P(bBridgeOpen ? FVector(6000,0,950):FVector(4480,-260,990));
 case EBBSBCaravanStep::Miniboss: return Miniboss ? Miniboss->GetActorLocation():GetActorLocation();
 case EBBSBCaravanStep::SearchCaptain: return P(!bVisitedArtCar ? FVector(8000,0,300):!bReadCaptainNote ? FVector(10000,150,300):FVector(8000,-3000,300));
 case EBBSBCaravanStep::CaptainDuel: return Captain ? Captain->GetActorLocation():GetActorLocation();
 default: return P(FVector(10400,-400,300));
 }
}
FString ABBSBCaravanStage::Status() const { return FString::Printf(TEXT("step=%d shutter=%d lift=%d bridge=%d mini=%d note=%d captain=%d stock=%d coins=%d"),int32(Step),bShutterOpen,bLiftRidden,bBridgeOpen,Miniboss && Miniboss->bDefeated,bReadCaptainNote,Captain && Captain->bDefeated,Inventory && Inventory->bSupplyUnlocked,Inventory ? Inventory->Coins:0); }

void ABBSBCaravanHUD::DrawHUD()
{
 Super::DrawHUD(); if (!Canvas) return; if(auto* P=Cast<ABBSBCharacter>(GetOwningPawn());P && P->Interaction && P->Interaction->bDialogueActive) return; auto* D=Caravan::Director(GetWorld()); if (!D) return;
 if (auto* Pawn=GetOwningPawn()) if (auto* Actions=Pawn->FindComponentByClass<UBBSBFieldActionComponent>(); Actions && Actions->bEquipmentWheelOpen) return;
 const float S=FMath::Clamp(Canvas->SizeX/1280.f,.70f,1.3f),W=Canvas->SizeX,H=Canvas->SizeY;
 const FLinearColor Paper(.96f,.85f,.68f), Panel(.12f,.10f,.13f,.87f), Pink(.97f,.63f,.70f);
 auto WrappedText=[&](const FString& Value,FLinearColor Color,float X,float Y,float Width,float Scale)
 {
  FString Wrapped,Line;
  for (TCHAR C:Value)
  {
   if (C==TEXT('\n')) { Wrapped+=Line+TEXT("\n"); Line.Reset(); continue; }
   FString Candidate=Line; Candidate.AppendChar(C); float TextWidth=0,TextHeight=0;
   GetTextSize(Candidate,TextWidth,TextHeight,GEngine->GetLargeFont(),Scale);
   if (TextWidth>Width && !Line.IsEmpty()) { Wrapped+=Line+TEXT("\n"); Line.Reset(); }
   Line.AppendChar(C);
  }
  DrawText(Wrapped+Line,Color,X,Y,GEngine->GetLargeFont(),Scale);
 };
 DrawRect(Panel,W-425*S,90*S,410*S,106*S); DrawText(TEXT("버니아 반 발자국 상단  /  LO-FI CHILL"),Pink,W-410*S,100*S,GEngine->GetLargeFont(),1.10f*S);
 WrappedText(D->Objective(),Paper,W-410*S,130*S,380*S,1.25f*S);
 if (D->Inventory) DrawText(FString::Printf(TEXT("상단화 %d  ·  차 %d [H]  ·  엽서 %d%s"),D->Inventory->Coins,D->Inventory->Tea,D->Inventory->Postcards,D->Inventory->bRibbonOwned ? TEXT("  ♪"):TEXT("")),Paper,20*S,94*S,GEngine->GetSmallFont(),1.2f*S);
 if (D->Focus && !D->OpenShop)
 {
  const bool Puzzle=D->Focus->Device==EBBSBBunniarDevice::Shutter || D->Focus->Device==EBBSBBunniarDevice::Lift || D->Focus->Device==EBBSBBunniarDevice::Winch;
  DrawRect(Panel,W*.5f-210*S,H-100*S,420*S,47*S); WrappedText((Puzzle ? TEXT("[좌클릭] "):TEXT("[버니아·좌클릭] "))+D->Focus->DeviceLabel,Paper,W*.5f-193*S,H-88*S,390*S,1.3f*S);
 }
 if (D->Buddy && D->Buddy->bFollowing)
  DrawText(D->Buddy->bSurfaceAttached ? TEXT("버니아: 등을 붙인 채 드래그 · 놓으면 회수"):D->Buddy->bEquipped ? TEXT("좌클릭 유지: 표면 부착 · 놓기: 회수"):TEXT("버니아: 작은 동행 장비 · [Tab] 장착"),Paper,W*.5f-180*S,H-40*S,GEngine->GetSmallFont(),1.2f*S);
 if (GetWorld()->GetTimeSeconds()<D->DialogueUntil && !D->OpenShop)
 { DrawRect(Panel,W*.5f-450*S,H-205*S,900*S,94*S); WrappedText(D->Dialogue,Paper,W*.5f-431*S,H-195*S,860*S,1.35f*S); }
 FVector2D Screen; const FVector Goal=D->ObjectiveLocation();
 if (GetOwningPlayerController()->ProjectWorldLocationToScreen(Goal+FVector(0,0,150),Screen) && !D->OpenShop && D->Step!=EBBSBCaravanStep::Miniboss && D->Step!=EBBSBCaravanStep::CaptainDuel)
 { Screen.X=FMath::Clamp(Screen.X,40.f,W-40); Screen.Y=FMath::Clamp(Screen.Y,205*S,H-235*S); DrawText(FString::Printf(TEXT("◇  %.0fm"),FVector::Dist(Goal,GetOwningPawn()->GetActorLocation())/100),Pink,Screen.X,Screen.Y,GEngine->GetSmallFont(),S); }
 if (D->OpenShop)
 {
  const bool Art=D->OpenShop->Device==EBBSBBunniarDevice::ArtShop; const float X=W*.5f-310*S,Y=H*.5f-135*S;
  DrawRect(Panel,X,Y,620*S,270*S); DrawText(Art ? TEXT("2호차 · 예술단 상점"):TEXT("6호차 · 현생단 상점"),Pink,X+25*S,Y+20*S,GEngine->GetLargeFont(),1.5f*S);
  DrawText(TEXT("직원가  /  ↑↓ 선택 · Enter 구매 · Backspace 닫기"),Paper,X+25*S,Y+75*S,GEngine->GetSmallFont(),1.2f*S);
  FString A=Art ? TEXT("절벽길 엽서  ·  15"):TEXT("휴식용 차 1개  ·  20  /  H 체력 회복");
  FString B=D->Inventory->bSupplyUnlocked ? (Art ? TEXT("지역 악보 리본  ·  30"):TEXT("지역 특산 차 2개  ·  30")):TEXT("[미입고] 지역 문제 해결 → 유통망 확보 필요");
  WrappedText((D->ShopSelection==0 ? TEXT("▶ "):TEXT("   "))+A,Paper,X+25*S,Y+115*S,570*S,1.3f*S);
  WrappedText((D->ShopSelection==1 ? TEXT("▶ "):TEXT("   "))+B,Paper,X+25*S,Y+160*S,570*S,1.3f*S);
  DrawText(FString::Printf(TEXT("소지금 %d  ·  상품 해금과 소유는 별개"),D->Inventory->Coins),Pink,X+25*S,Y+215*S,GEngine->GetSmallFont(),1.2f*S);
 }
}
