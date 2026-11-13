#include "Interaction/BBSBInteraction.h"
#include "BBSBCharacter.h"
#include "Field/BBSBFieldActions.h"
#include "Caravan/BBSBCaravan.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

bool ABBSBCarryObject::CanBeCarriedBy(const ACharacter* Player) const
{
 if(!Player || bBroken || !Body)return false;
 if(const auto* Base=Cast<UPrimitiveComponent>(Player->GetMovementBaseObject());Base && Base->GetOwner()==this)return false;
 const auto* Capsule=Player->GetCapsuleComponent();if(!Capsule)return false;
 // A tiny shrink allows normal resting side contact, without accepting a pawn
 // embedded in the prop. This geometric test ignores the temporary Pawn response.
 if(Body->OverlapComponent(Capsule->GetComponentLocation(),Capsule->GetComponentQuat(),Capsule->GetCollisionShape(-1.f)))return false;
 const FVector Feet=Capsule->GetComponentLocation()-FVector::UpVector*Capsule->GetScaledCapsuleHalfHeight();
 const FBox Bounds=Body->Bounds.GetBox();
 const float Margin=Capsule->GetScaledCapsuleRadius()*.55f;
 return !(Feet.Z>=Bounds.Max.Z-4.f && Feet.Z<=Bounds.Max.Z+8.f &&
  Feet.X>Bounds.Min.X-Margin && Feet.X<Bounds.Max.X+Margin && Feet.Y>Bounds.Min.Y-Margin && Feet.Y<Bounds.Max.Y+Margin);
}
bool ABBSBCarryObject::IsPuzzleRecoveryEnabled() const
{ return bEnablePuzzleRecovery && (bRecoverIfLost || ActorHasTag(TEXT("BBSBStepCrate"))); }
void ABBSBCarryObject::InitializeCarrySafety()
{ PuzzleRecoveryAnchor=Home;NextRecoveryCheck=0;PuzzleLostSeconds=0;PrepareStepCrateSimulation(); }

bool ABBSBCarryObject::PuzzlePlacementClear(const FTransform& Pose,bool IncludePlayer) const
{
 if(!Body || !Body->GetStaticMesh() || !GetWorld())return false;
 const FBox Box=Body->GetStaticMesh()->GetBoundingBox();
 const FVector Extent=(Box.GetExtent()*Pose.GetScale3D().GetAbs()-FVector(1.5f)).ComponentMax(FVector(1));
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBRecoverySpace),false,this);
 const auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if(Player)
 {
  if(!IncludePlayer)Q.AddIgnoredActor(Player);
  if(Player->FieldActions)Q.AddIgnoredActor(Player->FieldActions->BunniarEquipment);
 }
 return !GetWorld()->OverlapBlockingTestByChannel(Pose.TransformPosition(Box.GetCenter()),Pose.GetRotation(),ECC_WorldStatic,FCollisionShape::MakeBox(Extent),Q);
}
bool ABBSBCarryObject::PuzzleHasApproach(const FTransform& Pose) const
{
 const auto* Player=UGameplayStatics::GetPlayerCharacter(this,0);const auto* Capsule=Player ? Player->GetCapsuleComponent():nullptr;
 const float Radius=Capsule ? Capsule->GetScaledCapsuleRadius():35.f,HalfHeight=Capsule ? Capsule->GetScaledCapsuleHalfHeight():90.f;
 const FBoxSphereBounds Bounds=Body->CalcBounds(Pose);const FVector At=Bounds.Origin;
 const float StandOff=FVector2D(Bounds.BoxExtent.X,Bounds.BoxExtent.Y).Size()+Radius+12.f;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBRecoveryApproach),false,this);Q.AddIgnoredActor(Player);
 if(const auto* P=Cast<ABBSBCharacter>(Player);P && P->FieldActions)Q.AddIgnoredActor(P->FieldActions->BunniarEquipment);
 for(int32 I=0;I<8;++I)
 {
  const float Angle=I*PI/4;const FVector Side=At+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*StandOff;
  FHitResult Floor;
  if(!GetWorld()->LineTraceSingleByObjectType(Floor,Side+FVector::UpVector*(Bounds.BoxExtent.Z+55.f),Side-FVector::UpVector*(Bounds.BoxExtent.Z+90.f),BBSBCombatVisuals::GeometryTypes(),Q) || Floor.ImpactNormal.Z<.65f)continue;
  const FVector Stand=Floor.ImpactPoint+FVector::UpVector*(HalfHeight+2.f);
  if(GetWorld()->OverlapBlockingTestByChannel(Stand,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,HalfHeight),Q))continue;
  // Free space on the far side of a train wall is not an approach to this crate.
  FHitResult Wall;
  if(GetWorld()->LineTraceSingleByChannel(Wall,Stand,At,ECC_WorldStatic,Q))continue;
  return true;
 }
 return false;
}
bool ABBSBCarryObject::PuzzleVisibleToPlayer(const ABBSBCharacter* Player) const
{
 if(!Player || !Player->FieldActions)return false;
 if(FVector::Dist(Player->GetActorLocation(),GetActorLocation())>Player->FieldActions->WeaponReach)return false;
 FVector Eye;FRotator R;Player->GetActorEyesViewPoint(Eye,R);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBRecoveryVisibility),false,Player);
 Q.AddIgnoredActor(Player->FieldActions->BunniarEquipment);
 for(const FVector& Offset:{FVector::ZeroVector,FVector(0,0,Body->Bounds.BoxExtent.Z*.7f)})
 {
  FHitResult Hit;
  if(!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Body->Bounds.Origin+Offset,ECC_Visibility,Q) || Hit.GetActor()==this)return true;
 }
 return false;
}
bool ABBSBCarryObject::TryRecoverPuzzleObject()
{
 if(!IsPuzzleRecoveryEnabled() || bCarried || AttachedBuddy.IsValid())return false;
 // Never materialise a returned crate in the player or in another solid prop.
 FTransform Goal;bool Found=false;
 for(const FTransform& Anchor:{PuzzleRecoveryAnchor,Home})
 {
  if(PuzzlePlacementClear(Anchor,true) && PuzzleHasApproach(Anchor)) { Goal=Anchor;Found=true;break; }
 }
 if(!Found)return false;
 Body->SetSimulatePhysics(false);ReleaseCarryBindings();SetActorTransform(Goal,false,nullptr,ETeleportType::TeleportPhysics);
 bBroken=bCarried=bManipulating=bCarryBlocked=false;Health=MaximumHealth;BlockedTravel=0;PuzzleLostSeconds=0;
 Body->SetVisibility(true);Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);SetActorHiddenInGame(false);SetActorEnableCollision(true);
 Body->SetCollisionResponseToChannel(ECC_Pawn,NormalPawnResponse);Body->SetCollisionResponseToChannel(ECC_Camera,NormalCameraResponse);PawnIgnoreUntil=-1;
 Motion.Reset();Contacts.Reset();CarryVelocity=LastThrowVelocity=PrePhysicsVelocity=FVector::ZeroVector;
 PuzzleRecoveryAnchor=Goal;++PuzzleRecoveryCount;return true;
}
void ABBSBCarryObject::TickCarrySafety(float,const ABBSBCharacter* Player)
{
 if(!IsPuzzleRecoveryEnabled() || bCarried || AttachedBuddy.IsValid()) { PuzzleLostSeconds=0;return; }
 const double Now=GetWorld()->GetTimeSeconds();if(Now<NextRecoveryCheck)return;
 const float Elapsed=LastRecoveryCheck>0 ? float(FMath::Min(1.,Now-LastRecoveryCheck)):0.f;
 LastRecoveryCheck=Now;NextRecoveryCheck=Now+.4;
 const bool Escaped=GetActorLocation().Z<Home.GetLocation().Z-PuzzleMaximumFall || FVector::DistSquared(GetActorLocation(),Home.GetLocation())>FMath::Square(PuzzleMaximumTravel);
 const bool Clear=!bBroken && PuzzlePlacementClear(GetActorTransform(),false);
 const bool Settled=!Body->IsSimulatingPhysics() || (Body->GetPhysicsLinearVelocity().Size()<60 && Body->GetPhysicsAngularVelocityInDegrees().Size()<20);
 FCollisionQueryParams SupportQuery(SCENE_QUERY_STAT(BBSBRecoverySupport),false,this);SupportQuery.AddIgnoredActor(Player);
 const FVector Bottom=Body->Bounds.Origin-FVector::UpVector*Body->Bounds.BoxExtent.Z;FHitResult Support;
 const bool Supported=ActorHasTag(TEXT("BBSBStepCrate")) ? HasStableStepSupport():
  GetWorld()->LineTraceSingleByObjectType(Support,Bottom+FVector::UpVector*4,Bottom-FVector::UpVector*12,BBSBCombatVisuals::GeometryTypes(),SupportQuery) && Support.ImpactNormal.Z>.65f;
 const bool Approach=Clear && Settled && Supported && PuzzleHasApproach(GetActorTransform());
 const bool Lost=bBroken || Escaped || (!Clear && Settled) || (Settled && !Approach && !PuzzleVisibleToPlayer(Player));
 if(!Lost)
 {
  PuzzleLostSeconds=0;
  if(Approach)PuzzleRecoveryAnchor=GetActorTransform();
  return;
 }
 PuzzleLostSeconds+=Elapsed;
 if(PuzzleLostSeconds>=FMath::Max(.4f,PuzzleRecoveryDelay))TryRecoverPuzzleObject();
}

namespace
{
 double Cross(const FVector2D& A,const FVector2D& B) { return A.X*B.Y-A.Y*B.X; }
 bool SupportedCenter(TArray<FVector2D>& Points,const FVector2D& Center)
 {
  if(Points.Num()<3)return false;
  Points.Sort([](const FVector2D& A,const FVector2D& B){return A.X==B.X ? A.Y<B.Y:A.X<B.X;});
  TArray<FVector2D> Hull;Hull.Reserve(Points.Num()*2);
  for(const auto& P:Points)
  {
   while(Hull.Num()>1 && Cross(Hull.Last()-Hull[Hull.Num()-2],P-Hull.Last())<=.001)Hull.Pop(EAllowShrinking::No);
   Hull.Add(P);
  }
  const int32 Lower=Hull.Num();
  for(int32 I=Points.Num()-2;I>=0;--I)
  {
   const FVector2D P=Points[I];
   while(Hull.Num()>Lower && Cross(Hull.Last()-Hull[Hull.Num()-2],P-Hull.Last())<=.001)Hull.Pop(EAllowShrinking::No);
   Hull.Add(P);
  }
  Hull.Pop(EAllowShrinking::No);if(Hull.Num()<3)return false;
  // A line/edge contact, or a centre of mass outside the contacts, can still tip.
  for(int32 I=0;I<Hull.Num();++I)
  {
   const FVector2D Edge=Hull[(I+1)%Hull.Num()]-Hull[I];
   if(Cross(Edge,Center-Hull[I])<Edge.Size())return false; // 1 cm inside every edge.
  }
  return true;
 }
}

bool ABBSBCarryObject::HasStableStepSupport() const
{
 if(!Body || !Body->GetStaticMesh() || !GetWorld())return false;
 const FTransform Frame=Body->GetComponentTransform();const FBox Box=Body->GetStaticMesh()->GetBoundingBox();
 const FVector Center=Frame.TransformPosition(Box.GetCenter()),Extent=Box.GetExtent()*Frame.GetScale3D().GetAbs();
 const FVector Axes[]={Frame.GetUnitAxis(EAxis::X),Frame.GetUnitAxis(EAxis::Y),Frame.GetUnitAxis(EAxis::Z)};
 int32 Down=0;for(int32 I=1;I<3;++I)if(FMath::Abs(Axes[I].Z)>FMath::Abs(Axes[Down].Z))Down=I;
 const FVector Up=Axes[Down]*FMath::Sign(Axes[Down].Z);if(Up.Z<.65f)return false;
 const int32 U=(Down+1)%3,V=(Down+2)%3;const FVector Face=Center-Up*Extent[Down];
 const FVector GravityFoot=Center-FVector::UpVector*(Extent[Down]/Up.Z);
 const FVector2D Balance(FVector::DotProduct(GravityFoot-Face,Axes[U]),FVector::DotProduct(GravityFoot-Face,Axes[V]));
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBStepContactArea),false,this);
 const auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));Q.AddIgnoredActor(Player);
 if(Player && Player->FieldActions)Q.AddIgnoredActor(Player->FieldActions->BunniarEquipment);
 TArray<FVector2D> SupportPoints;SupportPoints.Reserve(25);
 // Include inner contacts, so a box can rest on a smaller plinth or overhang a ledge.
 for(int32 Y=-2;Y<=2;++Y)for(int32 X=-2;X<=2;++X)
 {
  const FVector Sample=Face+Axes[U]*(Extent[U]*X*.45f)+Axes[V]*(Extent[V]*Y*.45f);FHitResult Hit;
  if(!GetWorld()->LineTraceSingleByObjectType(Hit,Sample+FVector::UpVector*4,Sample-FVector::UpVector*.6f,BBSBCombatVisuals::GeometryTypes(),Q))continue;
  const float Gap=Sample.Z-Hit.ImpactPoint.Z;
  if(Hit.bStartPenetrating || Gap<-1.f || Gap>.6f || Hit.ImpactNormal.Z<.65f || FVector::DotProduct(Up,Hit.ImpactNormal)<.98f)continue;
  if(const auto* C=Hit.GetComponent();C && C->IsSimulatingPhysics() && (C->GetPhysicsLinearVelocity().Size()>5 || C->GetPhysicsAngularVelocityInDegrees().Size()>3))continue;
  SupportPoints.Add(FVector2D(FVector::DotProduct(Hit.ImpactPoint-Face,Axes[U]),FVector::DotProduct(Hit.ImpactPoint-Face,Axes[V])));
 }
 return SupportedCenter(SupportPoints,Balance);
}

void ABBSBCarryObject::PrepareStepCrateSimulation()
{
 if(!ActorHasTag(TEXT("BBSBStepCrate")))return;
 StepSupportedSeconds=0;NextStepSupportCheck=0;
 // Solid contact stays enabled during settling. The character movement component
 // limits additional push impulses instead of making the falling crate intangible.
 Body->SetAngularDamping(FMath::Max(1.f,Body->GetAngularDamping()));
 Body->SetMaxDepenetrationVelocity(NAME_None,150.f);
}

void ABBSBCarryObject::TickStepCratePhysics(float DT,const ABBSBCharacter*)
{
 if(!ActorHasTag(TEXT("BBSBStepCrate")) || AttachedBuddy.IsValid())return;
 if(!Body->IsSimulatingPhysics())
 {
  const double Now=GetWorld()->GetTimeSeconds();if(Now<NextStepSupportCheck)return;NextStepSupportCheck=Now+.15;
  if(HasStableStepSupport())return;
  // Also repair old saves which contain a prematurely frozen, tilted crate.
  PrepareStepCrateSimulation();Body->SetSimulatePhysics(true);Body->WakeAllRigidBodies();
 }
 const bool Slow=Body->GetPhysicsLinearVelocity().Size()<5.f && Body->GetPhysicsAngularVelocityInDegrees().Size()<3.f;
 StepSupportedSeconds=Slow && HasStableStepSupport() ? StepSupportedSeconds+FMath::Max(0.f,DT):0;
 if(StepSupportedSeconds<FMath::Max(.1f,StepSettleSeconds))return;
 Body->SetPhysicsLinearVelocity(FVector::ZeroVector);Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
 Body->SetSimulatePhysics(false);NextStepSupportCheck=GetWorld()->GetTimeSeconds()+.15;
}

void ABBSBCarryObject::RestoreReleasedPawnCollision()
{
 if(PawnIgnoreUntil<0 || GetWorld()->GetTimeSeconds()<PawnIgnoreUntil)return;
 const auto* Player=UGameplayStatics::GetPlayerCharacter(this,0);const auto* Capsule=Player ? Player->GetCapsuleComponent():nullptr;
 // A held prop can be released inside the player. Only this existing overlap
 // delays restoration; free simulation is not a reason to turn collision off.
 if(Capsule && Body->OverlapComponent(Capsule->GetComponentLocation(),Capsule->GetComponentQuat(),Capsule->GetCollisionShape(-.5f)))
 {
  Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);Body->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);return;
 }
 Body->SetCollisionResponseToChannel(ECC_Pawn,NormalPawnResponse);Body->SetCollisionResponseToChannel(ECC_Camera,NormalCameraResponse);PawnIgnoreUntil=-1;
}
