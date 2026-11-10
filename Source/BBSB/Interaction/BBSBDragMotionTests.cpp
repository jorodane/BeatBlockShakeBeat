#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Interaction/BBSBDragMotion.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
 struct FDragFixture
 {
  UWorld* World=GEditor->GetEditorWorldContext().World();
  UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
  TArray<AActor*> Actors;FVector Base;
  explicit FDragFixture(int32 Row):Base(100000,100000+Row*1500,100000) {}
  ~FDragFixture() { for(auto* Actor:Actors)if(IsValid(Actor))World->DestroyActor(Actor,false,false); }
  FActorSpawnParameters Params() const
  { FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;return P; }
  UStaticMeshComponent* Box(FVector Offset,FVector Scale,bool Movable=true,bool Physics=false)
  {
   auto* Actor=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),Base+Offset,FRotator::ZeroRotator,Params());Actors.Add(Actor);
   auto* Body=Actor->GetStaticMeshComponent();Body->SetMobility(EComponentMobility::Movable);Body->SetStaticMesh(Cube);Body->SetCollisionProfileName(TEXT("BlockAll"));Body->SetWorldScale3D(Scale);
   Body->SetMobility(Movable ? EComponentMobility::Movable:EComponentMobility::Static);if(Physics)Body->SetSimulatePhysics(true);return Body;
  }
  ACharacter* Pawn(FVector Offset)
  {
   auto* Actor=World->SpawnActor<ACharacter>(ACharacter::StaticClass(),Base+Offset,FRotator::ZeroRotator,Params());Actors.Add(Actor);
   Actor->GetCharacterMovement()->SetMovementMode(MOVE_Walking);return Actor;
  }
 };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBDragCollisionTest,"BBSB.Interaction.Drag.SweptPushAndStop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBDragCollisionTest::RunTest(const FString&)
{
 if(!GEditor || !GEditor->GetEditorWorldContext().World()) { AddError(TEXT("Editor world required"));return false; }
 {
  FDragFixture F(0);auto* M=F.Box(FVector::ZeroVector,FVector(1));
  TestTrue(TEXT("Free drag reaches its requested position"),BBSBDragMotion::Move(M,F.Base+FVector(200,0,0))>.999f);
 }
 {
  FDragFixture F(1);auto* M=F.Box(FVector::ZeroVector,FVector(1));F.Box(FVector(0,0,-60),FVector(10,10,.2),false);F.Box(FVector(150,0,0),FVector(1),false);
  const float Allowed=BBSBDragMotion::Move(M,F.Base+FVector(200,0,0));
  TestTrue(TEXT("Tangent floor contact does not hide a blocking wall"),Allowed>.2f && Allowed<.26f);
  TestTrue(TEXT("A blocked prop reverses immediately"),BBSBDragMotion::Move(M,F.Base-FVector(100,0,0))>.999f);
 }
 {
  FDragFixture F(2);auto* M=F.Box(FVector::ZeroVector,FVector(1));auto* P=F.Box(FVector(130,0,0),FVector(1));
  TestTrue(TEXT("A loose prop is pushed all the way when space is available"),BBSBDragMotion::Move(M,F.Base+FVector(100,0,0))>.999f);
  TestTrue(TEXT("Pushed prop keeps solid separation"),P->GetComponentLocation().X-M->GetComponentLocation().X>=99.95);
 }
 {
  FDragFixture F(3);auto* M=F.Box(FVector::ZeroVector,FVector(1));auto* P=F.Box(FVector(130,0,0),FVector(1),true,true);F.Box(FVector(270,0,0),FVector(1),false);
  const float Allowed=BBSBDragMotion::Move(M,F.Base+FVector(200,0,0));
  TestTrue(TEXT("Physics prop is pushed up to its own obstruction"),P->GetComponentLocation().X-F.Base.X>169 && P->GetComponentLocation().X-F.Base.X<170.05);
  TestTrue(TEXT("The dragging source stops rather than crushing a pinned physics prop"),Allowed>.3f && Allowed<.36f && P->GetComponentLocation().X-M->GetComponentLocation().X>=99.95);
 }
 {
  FDragFixture F(4);auto* M=F.Box(FVector::ZeroVector,FVector(1));auto* A=F.Box(FVector(130,0,0),FVector(1));auto* B=F.Box(FVector(240,0,0),FVector(1));F.Box(FVector(380,0,0),FVector(1),false);
  const float Allowed=BBSBDragMotion::Move(M,F.Base+FVector(300,0,0));
  TestTrue(TEXT("A push chain stops at the first immovable obstruction"),Allowed>.2f && Allowed<.28f);
  TestTrue(TEXT("Every link in the push chain stays outside its neighbours"),A->GetComponentLocation().X-M->GetComponentLocation().X>=99.9 && B->GetComponentLocation().X-A->GetComponentLocation().X>=99.9);
 }
 {
  FDragFixture F(5);auto* Left=F.Box(FVector(0,-100,0),FVector(.2,.4,2));auto* Right=F.Box(FVector(0,100,0),FVector(.2,.4,2));auto* Pawn=F.Pawn(FVector::ZeroVector);
  const BBSBDragMotion::FMove Pair[]={{Left,F.Base},{Right,F.Base}};
  const float Allowed=BBSBDragMotion::Move(MakeArrayView(Pair));const float Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
  TestTrue(TEXT("Opposing door leaves stop together before trapping a pawn"),Allowed>.3f && Allowed<.6f);
  TestTrue(TEXT("The resulting doorway retains room for the full pawn capsule"),Right->GetComponentLocation().Y-Left->GetComponentLocation().Y-40.f>=Radius*2.f-.1f);
 }
 {
  FDragFixture F(6);auto* M=F.Box(FVector::ZeroVector,FVector(.2,2,2));auto* Pawn=F.Pawn(FVector(80,0,0));
  TestTrue(TEXT("A movable character is pushed without cancelling its movement"),BBSBDragMotion::Move(M,F.Base+FVector(150,0,0))>.999f && Pawn->GetActorLocation().X-F.Base.X>190);
 }
 {
  FDragFixture F(7);auto* M=F.Box(FVector::ZeroVector,FVector(1));auto* Ignored=F.Box(FVector(100,0,0),FVector(1),false);M->IgnoreActorWhenMoving(Ignored->GetOwner(),true);
  TestTrue(TEXT("Existing carried-object move ignores remain respected"),BBSBDragMotion::Move(M,F.Base+FVector(200,0,0))>.999f);
  TestTrue(TEXT("Collision helper preserves the original move-ignore policy"),M->GetMoveIgnoreActors().Contains(Ignored->GetOwner()));
 }
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBDoorHousingTest,"BBSB.Interaction.Drag.DoorHousingIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBDoorHousingTest::RunTest(const FString&)
{
 if(!GEditor || !GEditor->GetEditorWorldContext().World())return false;
 {
  FDragFixture F(8);auto* Door=F.Box(FVector::ZeroVector,FVector(.2,1,2));auto* Housing=F.Box(FVector(100,0,0),FVector(.6,2,2),false);
  UPrimitiveComponent* Pocket[]={Housing};const BBSBDragMotion::FMove Open{Door,F.Base+FVector(200,0,0)};
  TestTrue(TEXT("The door leaf can slide into its explicitly authored static pocket"),BBSBDragMotion::Move(MakeArrayView(&Open,1),MakeArrayView(Pocket))>.999f);
  Door->SetWorldLocation(F.Base);F.Box(FVector(300,0,0),FVector(.2,3,3),false);
  const BBSBDragMotion::FMove Further{Door,F.Base+FVector(400,0,0)};
  const float Reached=BBSBDragMotion::Move(MakeArrayView(&Further,1),MakeArrayView(Pocket));
  TestTrue(TEXT("Unrelated static obstacles remain solid even when the door has a pocket"),Reached>.6f && Reached<.71f);
 }
 {
  FDragFixture F(9);auto* Door=F.Box(FVector::ZeroVector,FVector(.2,2,2));auto* Housing=F.Box(FVector(220,0,0),FVector(1,3,3),false);auto* Pawn=F.Pawn(FVector(80,0,0));
  UPrimitiveComponent* Pocket[]={Housing};const BBSBDragMotion::FMove Open{Door,F.Base+FVector(200,0,0)};
  const float Reached=BBSBDragMotion::Move(MakeArrayView(&Open,1),MakeArrayView(Pocket));
  TestTrue(TEXT("The housing exception is never inherited by a pushed character"),Reached<.5f && Pawn->GetActorLocation().X-F.Base.X<137);
 }
 {
  FDragFixture F(10);auto* Door=F.Box(FVector::ZeroVector,FVector(.2,1,2));auto* Loose=F.Box(FVector(130,0,0),FVector(1));
  UPrimitiveComponent* WrongHousing[]={Loose};const BBSBDragMotion::FMove Open{Door,F.Base+FVector(200,0,0)};
  BBSBDragMotion::Move(MakeArrayView(&Open,1),MakeArrayView(WrongHousing));
  TestTrue(TEXT("A movable prop cannot be excluded by the static-housing setting"),Loose->GetComponentLocation().X-F.Base.X>259);
 }
 return !HasAnyErrors();
}
#endif
