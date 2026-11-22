#include "Interaction/BBSBInteractionActors.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/BBSBClimbable.h"
#include "Interaction/BBSBConditions.h"
#include "Interaction/BBSBDraggable.h"
#include "UObject/ConstructorHelpers.h"

ABBSBInteractableActor::ABBSBInteractableActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Origin")));
 MotionBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MotionBody"));
 MotionBody->SetupAttachment(GetRootComponent());
 MotionBody->SetMobility(EComponentMobility::Movable);
 MotionBody->SetCollisionProfileName(TEXT("BlockAllDynamic"));
 MotionBody->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 MotionBody->SetStaticMesh(Cube.Object);
 MotionBody->SetRelativeLocation(FVector(0, 0, 35));
 MotionBody->SetRelativeScale3D(FVector(.7f));
 Interaction = CreateDefaultSubobject<UBBSBInteractable>(TEXT("Interaction"));
 Conditions = CreateDefaultSubobject<UBBSBConditionReceiver>(TEXT("Conditions"));
 Tags.Add(TEXT("BBSBPhysicalInteraction"));
}

ABBSBDraggableActor::ABBSBDraggableActor(const FObjectInitializer& ObjectInitializer)
 : Super(ObjectInitializer.SetDefaultSubobjectClass<UBBSBDraggable>(TEXT("Interaction")))
{
 MotionBody->SetRelativeScale3D(FVector(.22f, 1.8f, 2.1f));
 MotionBody->SetRelativeLocation(FVector(0, 0, 105));
 DragPath = CreateDefaultSubobject<USplineComponent>(TEXT("DragPath"));
 DragPath->SetupAttachment(GetRootComponent());
 DragPath->SetHiddenInGame(true);
 DragPath->SetSplinePoints({FVector::ZeroVector, FVector(0, 140, 0), FVector(0, 240, 120)}, ESplineCoordinateSpace::Local);
}

ABBSBClimbableActor::ABBSBClimbableActor(const FObjectInitializer& ObjectInitializer)
 : Super(ObjectInitializer.SetDefaultSubobjectClass<UBBSBClimbable>(TEXT("Interaction")))
{
 Rungs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rungs"));
 Rungs->SetupAttachment(GetRootComponent());
 Rungs->SetStaticMesh(MotionBody->GetStaticMesh());
 Rungs->SetCastShadow(false);
 Rungs->SetCanEverAffectNavigation(false);
 Rungs->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Rungs->SetCollisionResponseToAllChannels(ECR_Ignore);
 Rungs->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
 MotionBody->SetStaticMesh(nullptr);
 MotionBody->SetRelativeTransform(FTransform::Identity);
 MotionBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 MotionBody->SetCollisionResponseToAllChannels(ECR_Ignore);
 MotionBody->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ABBSBClimbableActor::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 Rungs->ClearInstances();
 if (!bGenerateRungs) return;
 const auto* Climbable = CastChecked<UBBSBClimbable>(Interaction);
 const auto Frame = Climbable->WorldFrame();
 UBBSBClimbable::AddRungs(Rungs, Frame, Climbable->RungSpacing, -96.f, FVector::Distance(Frame.Bottom, Frame.Top) - 20.);
}

ABBSBConditionSwitchActor::ABBSBConditionSwitchActor(const FObjectInitializer& ObjectInitializer)
 : Super(ObjectInitializer.SetDefaultSubobjectClass<UBBSBConditionSwitch>(TEXT("Interaction")))
{
 MotionBody->SetRelativeScale3D(FVector(.94f, .94f, .12f));
 MotionBody->SetRelativeLocation(FVector(0, 0, 6));
 PressureArea = CreateDefaultSubobject<UBoxComponent>(TEXT("PressureArea"));
 PressureArea->SetupAttachment(GetRootComponent());
 PressureArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 PressureArea->SetGenerateOverlapEvents(false);
 PressureArea->SetRelativeLocation(FVector(0, 0, 22));
 PressureArea->InitBoxExtent(FVector(45, 45, 12));
}
