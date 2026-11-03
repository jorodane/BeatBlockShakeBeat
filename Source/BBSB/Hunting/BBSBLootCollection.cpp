#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBLootPickup.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "Character/BBSBTraversal.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Field/BBSBFieldActions.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

void UBBSBHuntingComponent::RegisterLoot(ABBSBLootPickup* Loot)
{ if(IsValid(Loot))LootActors.Add(Loot); }
void UBBSBHuntingComponent::UnregisterLoot(ABBSBLootPickup* Loot)
{ LootActors.Remove(Loot); }
void UBBSBHuntingComponent::IgnoreLoot(FCollisionQueryParams& Query) const
{ for(const auto& Weak:LootActors)if(Weak.IsValid())Query.AddIgnoredActor(Weak.Get()); }
bool UBBSBHuntingComponent::TryBeginCollection()
{
 if(!Player || !Player->FieldActions || Player->FieldActions->FieldEquipment!=Player->FieldActions->BunniarEquipment)return false;
 FHitResult Hit;if(!Player->FieldActions->TraceEquipmentAim(Hit))return false;
 auto* Loot=Cast<ABBSBLootPickup>(Hit.GetActor());if(!Loot || !Loot->IsAvailable())return false;
 if(Player->Combat->bDead || Player->Combat->IsStageLocked() || Player->FieldActions->IsEquipmentActionBusy())return true;
 bCollectionHeld=true;bVacuuming=false;bBuddyLootReturning=false;
 CollectionStartedAt=GetWorld()->GetTimeSeconds();NextVacuumScanAt=CollectionStartedAt;
 VacuumPoint=Hit.ImpactPoint;VacuumNormal=Hit.ImpactNormal.IsNearlyZero() ? FVector::UpVector:Hit.ImpactNormal;
 Collect(Loot);return true;
}
void UBBSBHuntingComponent::StopCollection()
{
 const bool WasActive=bCollectionHeld || bVacuuming,WasQuick=bQuickCollection;
 if((bVacuuming || WasQuick) && Player)
  if(auto* Buddy=Cast<ABBSBBunniarCompanion>(Player->FieldActions->BunniarEquipment);IsValid(Buddy))
  { bBuddyLootReturning=true;BuddyLootReturnFrom=Buddy->GetActorTransform();BuddyLootReturnAt=GetWorld()->GetTimeSeconds(); }
 bCollectionHeld=bVacuuming=bQuickCollection=bQuickInputHeld=false;QuickPendingLoot.Reset();SetVacuumVisible(false);
 if(WasActive && !WasQuick && Player && Player->FieldActions)Player->FieldActions->CancelLootGesture();
}
bool UBBSBHuntingComponent::TraceCollectionHit(FHitResult& Hit) const
{
 auto* A=Player ? Player->FieldActions.Get():nullptr;FVector Eye,Direction;if(!A || !A->CursorAimRay(Eye,Direction))return false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBQuickLootAim),true,Player);Q.AddIgnoredActor(A->BunniarEquipment);
 for(TActorIterator<ABBSBInkProjectile> It(GetWorld());It;++It)Q.AddIgnoredActor(*It);
 if(!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+Direction*FMath::Max(2500.f,A->WeaponReach+1000.f),ECC_Visibility,Q))return false;
 const auto* Loot=Cast<ABBSBLootPickup>(Hit.GetActor());return Loot && Loot->IsAvailable() && FVector::DistSquared(Player->GetActorLocation(),Loot->GetActorLocation())<=FMath::Square(A->WeaponReach);
}
void UBBSBHuntingComponent::QuickCollectDown()
{
 auto* A=Player ? Player->FieldActions.Get():nullptr;auto* Buddy=A ? Cast<ABBSBBunniarCompanion>(A->BunniarEquipment):nullptr;
 if(!A || !Buddy || bCollectionHeld || Player->Combat->bDead || A->bEquipmentInputBlocked || A->bEquipmentWheelOpen || Buddy->bSurfaceAttached || Buddy->bAirHovering || Buddy->bGliding || Player->Traversal->IsWireTraversalActive() || Player->Traversal->IsParkourActive() || (A->FieldEquipment==Buddy && A->IsGestureCaptured()))return;
 bQuickCollection=bQuickInputHeld=bCollectionHeld=true;bVacuuming=bBuddyLootReturning=false;
 CollectionStartedAt=NextVacuumScanAt=GetWorld()->GetTimeSeconds();QuickTravelFrom=Buddy->GetActorTransform();
 QuickTravelSeconds=Buddy->AnimationSet ? FMath::Max(.08f,Buddy->AnimationSet->AttachTravelSeconds):.24f;
 ResolveVacuumAim(VacuumPoint,VacuumNormal);FHitResult Hit;
 if(TraceCollectionHit(Hit)) { QuickPendingLoot=Cast<ABBSBLootPickup>(Hit.GetActor());VacuumPoint=QuickPendingLoot->GetActorLocation();VacuumNormal=FVector::UpVector; }
 else QuickTravelSeconds=0;
}
void UBBSBHuntingComponent::QuickCollectUp()
{ if(!bQuickCollection)return;bQuickInputHeld=false;if(!QuickPendingLoot.IsValid())StopCollection(); }
void UBBSBHuntingComponent::TickQuickCollection(float DT)
{
 auto* Buddy=Cast<ABBSBBunniarCompanion>(Player->FieldActions->BunniarEquipment);
 if(!Buddy || Buddy->bGliding || Buddy->bSurfaceAttached || Buddy->bAirHovering || Player->Traversal->IsWireTraversalActive() || Player->Traversal->IsParkourActive()) { StopCollection();return; }
 if(auto* Loot=QuickPendingLoot.Get())
 {
  VacuumPoint=Loot->GetActorLocation();
  if(GetWorld()->GetTimeSeconds()-CollectionStartedAt<QuickTravelSeconds)return;
  if(FVector::DistSquared(Loot->GetActorLocation(),Player->GetActorLocation())<=FMath::Square(Player->FieldActions->WeaponReach) && LootVisible(Loot,Buddy->GetActorLocation()))Collect(Loot);
  QuickPendingLoot.Reset();
 }
 if(!bQuickInputHeld) { StopCollection();return; }
 UpdateVacuum(DT);
}
int32 UBBSBHuntingComponent::Collect(ABBSBLootPickup* Loot)
{
 if(!IsValid(Loot) || !Loot->IsAvailable())return 0;
 const int32 Accepted=AddItem(Loot->Item,Loot->Quantity);
 if(Accepted>0)
 {
  Loot->TakeQuantity(Accepted);if(Loot->Quantity<=0)Loot->BeginCollectionFlight(this);
 }
 if(Loot->Quantity>0 && GetWorld()->GetTimeSeconds()-LastFullNoticeAt>1.)
 { LastFullNoticeAt=GetWorld()->GetTimeSeconds();StatusNotice=TEXT("가방이 가득 찼어요");StatusUntil=LastFullNoticeAt+2.; }
 return Accepted;
}
bool UBBSBHuntingComponent::ResolveVacuumAim(FVector& Point,FVector& Normal) const
{
 auto* Actions=Player ? Player->FieldActions.Get():nullptr;if(!Actions)return false;
 FVector Eye,Direction;if(!Actions->CursorAimRay(Eye,Direction))return false;
 const double Reach=FMath::Max(50.f,Actions->WeaponReach);const FVector Offset=Eye-Player->GetActorLocation();
 const double B=FVector::DotProduct(Offset,Direction),D=B*B-Offset.SizeSquared()+Reach*Reach;
 const double Distance=FMath::Max(0.,-B+(D>=0 ? FMath::Sqrt(D):0.));
 Point=Eye+Direction*Distance;Normal=-Direction;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBLootVacuumAim),true,Player);Query.AddIgnoredActor(Actions->BunniarEquipment);
 IgnoreLoot(Query);
 for(TActorIterator<ABBSBInkProjectile> It(GetWorld());It;++It)Query.AddIgnoredActor(*It);
 FHitResult Hit;
 if(GetWorld()->LineTraceSingleByChannel(Hit,Eye,Point,ECC_Visibility,Query))
 { Point=Hit.ImpactPoint;Normal=Hit.ImpactNormal; }
 return true;
}
bool UBBSBHuntingComponent::LootVisible(const ABBSBLootPickup* Loot,const FVector& From) const
{
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBLootVacuumCover),true,Player);Query.AddIgnoredActor(Loot);
 FHitResult Hit;
 return !GetWorld()->LineTraceSingleByObjectType(Hit,From,Loot->GetActorLocation(),BBSBCombatVisuals::GeometryTypes(),Query);
}
void UBBSBHuntingComponent::UpdateVacuum(float DT)
{
 const double Now=GetWorld()->GetTimeSeconds();
 if(!bVacuuming && Now-CollectionStartedAt<VacuumHoldSeconds)return;
 bVacuuming=true;ResolveVacuumAim(VacuumPoint,VacuumNormal);UpdateVacuumVisual();
 if(Now<NextVacuumScanAt)return;NextVacuumScanAt=Now+FMath::Max(.04f,VacuumScanSeconds);
 const float Radius=FMath::Clamp(VacuumRadius,50.f,600.f),Reach=FMath::Max(50.f,Player->FieldActions->WeaponReach);
 const FVector Origin=VacuumPoint+VacuumNormal*24.f;
 for(auto It=LootActors.CreateIterator();It;++It)
 {
  auto* Loot=It->Get();if(!IsValid(Loot)) { It.RemoveCurrent();continue; }
  if(!Loot->IsAvailable() || FVector::DistSquared(Loot->GetActorLocation(),VacuumPoint)>FMath::Square(Radius) ||
    FVector::DistSquared(Loot->GetActorLocation(),Player->GetActorLocation())>FMath::Square(Reach))continue;
  if(LootVisible(Loot,Origin))Collect(Loot);
 }
}
FVector UBBSBHuntingComponent::CollectionMouth() const
{
 if((bVacuuming || bQuickCollection) && Player)
  if(auto* Buddy=Player->FieldActions->BunniarEquipment.Get();IsValid(Buddy))return Buddy->GetActorLocation();
 return Player ? Player->GetActorLocation()+FVector::UpVector*45:VacuumPoint;
}
void UBBSBHuntingComponent::SetVacuumVisible(bool Visible)
{ for(auto Ribbon:VacuumRibbons)if(Ribbon)Ribbon->SetVisibility(Visible); }
void UBBSBHuntingComponent::UpdateVacuumVisual()
{
 if(!Player)return;
 if(!VacuumRoot)
 {
  VacuumRoot=NewObject<USceneComponent>(Player,TEXT("LootVacuumOrigin"));Player->AddInstanceComponent(VacuumRoot);
  VacuumRoot->SetupAttachment(Player->GetRootComponent());VacuumRoot->RegisterComponent();
  auto* Material=VacuumMaterial.LoadSynchronous();if(!Material)Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BBSB/VFX/Traversal/M_TraversalWind.M_TraversalWind"));
  if(Material)VacuumMID=UMaterialInstanceDynamic::Create(Material,this);
  if(VacuumMID) { VacuumMID->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.61f,.91f,.78f));VacuumMID->SetScalarParameterValue(TEXT("Density"),.6f); }
  auto* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
  for(int32 I=0;I<16;++I)
  {
   auto* Ribbon=NewObject<USplineMeshComponent>(Player,*FString::Printf(TEXT("LootVacuumRibbon_%d"),I));Player->AddInstanceComponent(Ribbon);
   Ribbon->SetupAttachment(VacuumRoot);Ribbon->SetMobility(EComponentMobility::Movable);Ribbon->SetStaticMesh(Plane);
   Ribbon->SetForwardAxis(ESplineMeshAxis::X,false);Ribbon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Ribbon->SetCastShadow(false);Ribbon->SetCanEverAffectNavigation(false);Ribbon->SetRenderCustomDepth(false);
   if(VacuumMID)Ribbon->SetMaterial(0,VacuumMID);Ribbon->RegisterComponent();VacuumRibbons.Add(Ribbon);
  }
 }
 VacuumRoot->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(VacuumNormal).ToQuat(),VacuumPoint,FVector::OneVector));
 const float Phase=float(GetWorld()->GetTimeSeconds()*7.),Radius=FMath::Clamp(VacuumRadius*.72f,36.f,432.f);
 for(int32 Strand=0;Strand<2;++Strand)for(int32 Segment=0;Segment<8;++Segment)
 {
  auto Point=[&](float U)
  { const float A=Phase+Strand*PI+U*PI*3,R=FMath::Lerp(Radius,12.f,U);return FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,U*72.f); };
  auto Tangent=[&](float U)
  {
   const float A=Phase+Strand*PI+U*PI*3,R=FMath::Lerp(Radius,12.f,U),DR=12.f-Radius;
   return FVector(DR*FMath::Cos(A)-R*PI*3*FMath::Sin(A),DR*FMath::Sin(A)+R*PI*3*FMath::Cos(A),72.f)/8.f;
  };
  const float A=Segment/8.f,B=(Segment+1)/8.f;auto* Ribbon=VacuumRibbons[Strand*8+Segment].Get();
  Ribbon->SetStartAndEnd(Point(A),Tangent(A),Point(B),Tangent(B),false);
  Ribbon->SetStartScale(FVector2D(FMath::Lerp(.08f,.035f,A),1),false);Ribbon->SetEndScale(FVector2D(FMath::Lerp(.08f,.035f,B),1),true);
  Ribbon->SetVisibility(true);
 }
}
bool UBBSBHuntingComponent::TickBuddyPresentation(float DT)
{
 if(!Player || !Player->FieldActions || Player->Combat->bDead) { bBuddyLootReturning=false;return false; }
 auto* Buddy=Cast<ABBSBBunniarCompanion>(Player->FieldActions->BunniarEquipment);
 if(!IsValid(Buddy))return false;
 if(Player->Traversal->IsWireTraversalActive() || Player->Traversal->IsBunniarTraversalActive()) { bBuddyLootReturning=false;return false; }
 if(!bVacuuming && !bQuickCollection && (!bBuddyLootReturning || Buddy->bSurfaceAttached || Buddy->bAirHovering || Buddy->bGliding))
 { bBuddyLootReturning=false;return false; }
 const FVector Face=Buddy->AnimationSet ? Buddy->AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
 const FQuat Model=FRotationMatrix::MakeFromXZ(Buddy->Visual->GetRelativeTransform().TransformVectorNoScale(Face),Buddy->Visual->GetRelativeTransform().TransformVectorNoScale(FVector::UpVector)).ToQuat();
 const float Scale=Buddy->AnimationSet ? (Buddy->bEquipped ? Buddy->AnimationSet->EquippedScale:Buddy->AnimationSet->CompanionScale):.12f;
 FVector Eye;FRotator View;Cast<APlayerController>(Player->GetController())->GetPlayerViewPoint(Eye,View);
 if(bQuickCollection && !bVacuuming)
 {
  if(QuickTravelSeconds<=0)return false;
  const float T=FMath::Clamp(float((GetWorld()->GetTimeSeconds()-CollectionStartedAt)/QuickTravelSeconds),0.f,1.f),Alpha=1-FMath::Pow(1-T,3.f);
  const FVector Goal=VacuumPoint+FVector::UpVector*55.f;const FQuat Facing=FRotationMatrix::MakeFromXZ((Eye-Goal).GetSafeNormal(),FVector::UpVector).ToQuat()*Model.Inverse();
  Buddy->SetActorLocation(FMath::Lerp(QuickTravelFrom.GetLocation(),Goal,Alpha));Buddy->SetActorRotation(FQuat::Slerp(QuickTravelFrom.GetRotation(),Facing,Alpha));Buddy->SetActorScale3D(FMath::Lerp(QuickTravelFrom.GetScale3D(),FVector(Scale),Alpha));return true;
 }
 if(bVacuuming)
 {
  const FVector Goal=VacuumPoint+VacuumNormal*78.f;
  const FQuat Facing=FRotationMatrix::MakeFromXZ((Eye-Goal).GetSafeNormal(),FVector::UpVector).ToQuat();
  const FQuat Spin(FVector::ForwardVector,float(GetWorld()->GetTimeSeconds()*5.));
  Buddy->SetActorLocation(FMath::VInterpTo(Buddy->GetActorLocation(),Goal,DT,14.f));
  Buddy->SetActorRotation((Facing*Spin*Model.Inverse()).GetNormalized());Buddy->SetActorScale3D(FVector(Scale));
 }
 else
 {
  const float T=FMath::Clamp(float((GetWorld()->GetTimeSeconds()-BuddyLootReturnAt)/FMath::Max(.1f,Buddy->ReturnTravelSeconds)),0.f,1.f),A=T*T*(3-2*T);
  const FRotator Yaw(0,Player->GetControlRotation().Yaw,0);
  const FVector Home=Buddy->bEquipped ? Player->FieldActions->FieldEquipmentRestTransform().GetLocation():Player->GetActorLocation()+Yaw.RotateVector(FVector(-40,-65,25));
  const auto* PC=Cast<APlayerController>(Player->GetController());const FVector Aim=Player->FieldActions->VisualAimPoint();const FQuat Facing=FRotationMatrix::MakeFromXZ((bInventoryOpen || (PC && PC->bShowMouseCursor)) ? Player->GetActorForwardVector():(Aim-Home).GetSafeNormal(),FVector::UpVector).ToQuat()*Model.Inverse();
  Buddy->SetActorLocation(FMath::Lerp(BuddyLootReturnFrom.GetLocation(),Home,A));
  Buddy->SetActorRotation(FQuat::Slerp(BuddyLootReturnFrom.GetRotation(),Facing,A));Buddy->SetActorScale3D(FMath::Lerp(BuddyLootReturnFrom.GetScale3D(),FVector(Scale),A));
  if(T>=1)bBuddyLootReturning=false;
 }
 return true;
}
