#include "Interaction/BBSBInteractionDoor.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
ABBSBInteractionDoor::ABBSBInteractionDoor()
{
 PrimaryActorTick.bCanEverTick=true;Hinge=CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));SetRootComponent(Root);Hinge->SetupAttachment(Root);
 Leaf=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door"));Leaf->SetupAttachment(Hinge);static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));Leaf->SetStaticMesh(Cube.Object);Leaf->SetRelativeScale3D(FVector(.16f,1.1f,2.2f));Leaf->SetRelativeLocation(FVector(0,55,110));Leaf->SetCollisionProfileName(TEXT("BlockAllDynamic"));Leaf->SetMobility(EComponentMobility::Movable);DoorName=FText::FromString(TEXT("문"));
}
bool ABBSBInteractionDoor::ToggleDoor() { if(bLocked)return false;bOpen=!bOpen;return true; }
void ABBSBInteractionDoor::Tick(float DT)
{ Super::Tick(DT);Hinge->SetRelativeRotation(FMath::RInterpTo(Hinge->GetRelativeRotation(),FRotator(0,bOpen ? OpenDegrees:0,0),DT,6)); }
FText ABBSBInteractionDoor::BunniarInteractionText_Implementation() const
{ return FText::FromString(DoorName.ToString()+(bLocked ? TEXT(" · 잠김"):bOpen ? TEXT(" 닫기"):TEXT(" 열기"))); }
