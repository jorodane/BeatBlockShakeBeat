#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Interaction/BBSBInteraction.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBCarryRiderTest,"BBSB.Interaction.Carry.RiderAndPenetrationGuard",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBCarryRiderTest::RunTest(const FString&)
{
 UWorld* W=GEditor ? GEditor->GetEditorWorldContext().World():nullptr;if(!W)return false;
 FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 const FVector At(120000,120000,120000);
 auto* Prop=W->SpawnActor<ABBSBCarryObject>(ABBSBCarryObject::StaticClass(),At,FRotator::ZeroRotator,P);
 auto* Rider=W->SpawnActor<ACharacter>(ACharacter::StaticClass(),At+FVector(220,0,0),FRotator::ZeroRotator,P);
 if(!Prop || !Rider)return false;
 Prop->SetActorScale3D(FVector(1.5f));Rider->GetCapsuleComponent()->SetCapsuleSize(35,90);
 TestTrue(TEXT("A nearby player can pick up an ordinary crate"),Prop->CanBeCarriedBy(Rider));
 Rider->SetActorLocation(At+FVector(0,0,167));Rider->SetBase(Prop->Body);
 TestFalse(TEXT("A rider cannot remove their own moving base"),Prop->CanBeCarriedBy(Rider));
 Rider->SetBase(static_cast<UPrimitiveComponent*>(nullptr));
 TestFalse(TEXT("The landing frame is protected before the base is assigned"),Prop->CanBeCarriedBy(Rider));
 Rider->SetActorLocation(At);
 Prop->Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
 TestFalse(TEXT("A cached Pawn ignore cannot hide penetration from pickup validation"),Prop->CanBeCarriedBy(Rider));
 Rider->SetActorLocation(At+FVector(0,0,250));
 TestTrue(TEXT("An airborne player above the crate can still pick it up"),Prop->CanBeCarriedBy(Rider));
 W->DestroyActor(Rider,false,false);W->DestroyActor(Prop,false,false);return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBStepSupportTest,"BBSB.Interaction.Carry.StableSupportArea",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBStepSupportTest::RunTest(const FString&)
{
 UWorld* W=GEditor ? GEditor->GetEditorWorldContext().World():nullptr;if(!W)return false;
 FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 const FVector At(125000,125000,125000);
 auto* Floor=W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),At-FVector(0,0,50),FRotator::ZeroRotator,P);
 auto* Prop=W->SpawnActor<ABBSBCarryObject>(ABBSBCarryObject::StaticClass(),At+FVector(0,0,75),FRotator::ZeroRotator,P);
 if(!Floor || !Prop)return false;
 auto* Surface=Floor->GetStaticMeshComponent();Surface->SetMobility(EComponentMobility::Movable);
 Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Surface->SetCollisionProfileName(TEXT("BlockAll"));Floor->SetActorScale3D(FVector(10,10,1));
 Prop->SetActorScale3D(FVector(1.5f));
 TestTrue(TEXT("A flat face is supported"),Prop->HasStableStepSupport());
 const double A=FMath::DegreesToRadians(4.);
 Prop->SetActorRotation(FRotator(0,0,4));Prop->SetActorLocation(At+FVector(0,0,75*(FMath::Cos(A)+FMath::Sin(A))));
 TestFalse(TEXT("A low-speed tilted edge cannot freeze with its far side ten centimetres in the air"),Prop->HasStableStepSupport());
 Prop->SetActorRotation(FRotator::ZeroRotator);Prop->SetActorLocation(At+FVector(0,0,80));
 TestFalse(TEXT("A face five centimetres above the floor is not seated"),Prop->HasStableStepSupport());
 Prop->SetActorLocation(At+FVector(0,0,75));Floor->SetActorScale3D(FVector(1,1,1));
 TestTrue(TEXT("A smaller plinth beneath the centre still provides stable support"),Prop->HasStableStepSupport());
 Floor->SetActorLocation(At+FVector(80,0,-50));
 TestFalse(TEXT("Contact on one side does not support the centre of mass"),Prop->HasStableStepSupport());
 const FQuat Slope=FRotator(0,0,12).Quaternion();const FVector Normal=Slope.GetAxisZ();
 Floor->SetActorScale3D(FVector(10,10,1));Floor->SetActorRotation(Slope);Floor->SetActorLocation(At-Normal*50);
 Prop->SetActorRotation(Slope);Prop->SetActorLocation(At+Normal*75);
 TestTrue(TEXT("A face aligned to a sloping ramp is stable"),Prop->HasStableStepSupport());
 Floor->SetActorRotation(FRotator::ZeroRotator);Floor->SetActorLocation(At-FVector(0,0,50));
 Prop->SetActorRotation(FRotator(0,0,90));Prop->SetActorLocation(At+FVector(0,0,75));
 TestTrue(TEXT("A cube resting on another face can settle without being forced upright"),Prop->HasStableStepSupport());
 Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 TestFalse(TEXT("Removed support is detected"),Prop->HasStableStepSupport());
 W->DestroyActor(Prop,false,false);W->DestroyActor(Floor,false,false);return !HasAnyErrors();
}
#endif
