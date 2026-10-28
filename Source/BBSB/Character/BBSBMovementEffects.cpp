#include "BBSBTraversal.h"
#include "BBSBCharacter.h"
#include "Field/BBSBFieldActions.h"
#include "Combat/BBSBCombat.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

void UBBSBTraversalComponent::TickMovementEffects(float DT)
{
 if(!Character || !Presentation) return;
 const auto* Move=Character->GetCharacterMovement();
 const bool Alive=!(Character->Combat && Character->Combat->bDead);
 const double Now=GetWorld()->GetTimeSeconds();
 const FVector Position=Character->GetActorLocation();
 const float HalfHeight=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 const bool Dashing=Alive && Character->FieldActions && Character->FieldActions->IsDodging();
 UpdateDashSmear(DT,Dashing);
 if(Dashing)
 {
  const FVector Direction=Move->Velocity.GetSafeNormal2D().IsNearlyZero() ? Character->GetActorForwardVector():Move->Velocity.GetSafeNormal();
  if(!bEffectWasDashing && Move->IsMovingOnGround()) SpawnSmoke(Presentation->DashBurst,Position-FVector::UpVector*(HalfHeight-3.f),FVector::UpVector,1.f);
  if((!Presentation->bEnableDashSmear || !Presentation->DashSmearMaterial) && (!bEffectWasDashing || Now-LastDashWakeAt>=Presentation->DashWakeInterval))
  { SpawnSmoke(Presentation->DashWake,Position-FVector::UpVector*20.f,Direction,1.f);LastDashWakeAt=Now; }
 }
 bEffectWasDashing=Dashing;

 const bool Running=Alive && Move->IsMovingOnGround() && !Dashing && !IsParkourActive() && Move->Velocity.Size2D()>=Presentation->RunSmokeMinimumSpeed;
 if(!Running) { bFootContact[0]=bFootContact[1]=false;FootstepTravel=0; }
 else
 {
  FootstepTravel+=Move->Velocity.Size2D()*DT;
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBFootstepEffect),false,Character);
  for(int32 Foot=0;Foot<2;++Foot)
  {
   const FName Socket=Foot==0 ? Presentation->LeftFootSocket:Presentation->RightFootSocket;
   const FVector FootPosition=Character->GetMesh()->DoesSocketExist(Socket) ? Character->GetMesh()->GetSocketLocation(Socket)
    : Position+Character->GetActorRightVector()*(Foot==0 ? -12.f:12.f)-FVector::UpVector*HalfHeight;
   FHitResult Ground;
   const bool Contact=GetWorld()->LineTraceSingleByChannel(Ground,FootPosition+FVector::UpVector*16.f,FootPosition-FVector::UpVector*26.f,ECC_Visibility,Query)
    && Ground.ImpactNormal.Z>.55f && FootPosition.Z-Ground.ImpactPoint.Z<8.f;
   const bool StrideFallback=Foot==NextFootstep && FootstepTravel>=Presentation->FootstepSpacing;
   if(Contact && (!bFootContact[Foot] || StrideFallback) && Now-LastFootstepEffectAt>=.14)
   {
    SpawnSmoke(Presentation->RunFootstepSmoke,Ground.ImpactPoint+Ground.ImpactNormal*2.f,Ground.ImpactNormal,.8f);
    LastFootstepEffectAt=Now;FootstepTravel=0;NextFootstep=1-Foot;
   }
   bFootContact[Foot]=Contact;
  }
 }

 const bool ParkourWall=ParkourMode==EBBSBParkourMode::LedgeHang || ParkourMode==EBBSBParkourMode::WallSlide;
 const bool WireWall=bWireArrival && WireArrival==EBBSBWireArrival::Wall;
 const bool OnWall=Alive && (ParkourWall || WireWall);
 if(OnWall)
 {
  const FVector Normal=(ParkourWall ? LedgeNormal:WireNormal).GetSafeNormal();
  const FVector Contact=ParkourMode==EBBSBParkourMode::LedgeHang ? LedgePoint:Position-Normal*Character->GetCapsuleComponent()->GetScaledCapsuleRadius()+FVector::UpVector*15.f;
  if(!bEffectWasOnWall)
  {
   const FVector Across=FVector::CrossProduct(Normal,FVector::UpVector).GetSafeNormal()*12.f;
   SpawnSmoke(Presentation->WallGrabBurst,Contact+Normal*3.f+Across,Normal,.85f);
   SpawnSmoke(Presentation->WallGrabBurst,Contact+Normal*3.f-Across,Normal,.85f);
  }
  const bool Sliding=Move->Velocity.Z<-5.f && ((ParkourMode==EBBSBParkourMode::WallSlide && ParkourAge>=WireWallSlideStartsAt) || (WireWall && WireMotionAge>=WireWallSlideStartsAt));
  if(Sliding && Now-LastWallSlideEffectAt>=Presentation->WallSlideSmokeInterval)
  {
   const FVector Scrape=Position-Normal*Character->GetCapsuleComponent()->GetScaledCapsuleRadius()-FVector::UpVector*(HalfHeight*.35f);
   SpawnSmoke(Presentation->WallSlideSmoke,Scrape+Normal*4.f,Normal,.8f);LastWallSlideEffectAt=Now;
  }
 }
 bEffectWasOnWall=OnWall;
}

void UBBSBTraversalComponent::UpdateDashSmear(float DT,bool bDashing)
{
 auto* Mesh=Character ? Character->GetMesh():nullptr;
 if(!Mesh || !Presentation || !Presentation->bEnableDashSmear || !Presentation->DashSmearMaterial || Presentation->EffectScale<=0 || (Character->Combat && Character->Combat->bDead))
 { ResetDashSmear();return; }
 const float Age=Character->FieldActions ? Character->FieldActions->GetDodgeAge():0.f;
 const float Duration=Character->FieldActions ? Character->FieldActions->DodgeDuration:.3f;
 const float Envelope=bDashing ? FMath::SmoothStep(0.f,.045f,Age)*FMath::SmoothStep(0.f,.075f,Duration-Age):0.f;
 DashSmearAlpha=bDashing ? Envelope:FMath::Max(0.f,DashSmearAlpha-DT/.065f);
 if(bDashing)
 {
  const FVector Motion=Character->GetCharacterMovement()->Velocity.GetSafeNormal();
  if(!Motion.IsNearlyZero()) DashSmearDirection=Motion;
 }
 if(DashSmearAlpha<=KINDA_SMALL_NUMBER) { ResetDashSmear();return; }
 if(!DashSmearMID || DashSmearMID->Parent!=Presentation->DashSmearMaterial)
  DashSmearMID=UMaterialInstanceDynamic::Create(Presentation->DashSmearMaterial,this);
 if(!bDashSmearOverlaySaved)
 {
  SavedDashOverlay=Mesh->GetOverlayMaterial();DashSmearSavedBounds=Mesh->BoundsScale;bDashSmearOverlaySaved=true;
  Mesh->SetBoundsScale(FMath::Max(DashSmearSavedBounds,1.5f));
 }
 Mesh->SetOverlayMaterial(DashSmearMID);
 DashSmearMID->SetVectorParameterValue(TEXT("DashDirection"),FLinearColor(DashSmearDirection.X,DashSmearDirection.Y,DashSmearDirection.Z,0));
 const FVector Center=Character->GetActorLocation();
 DashSmearMID->SetVectorParameterValue(TEXT("DashCenter"),FLinearColor(Center.X,Center.Y,Center.Z,0));
 DashSmearMID->SetScalarParameterValue(TEXT("SmearAlpha"),DashSmearAlpha);
 DashSmearMID->SetScalarParameterValue(TEXT("SmearLength"),Presentation->DashSmearLength);
 DashSmearMID->SetScalarParameterValue(TEXT("Opacity"),Presentation->DashSmearOpacity*FMath::Min(Presentation->EffectScale,1.f));
}

void UBBSBTraversalComponent::ResetDashSmear()
{
 if(bDashSmearOverlaySaved && Character && Character->GetMesh())
 {
  auto* Mesh=Character->GetMesh();
  if(Mesh->GetOverlayMaterial()==DashSmearMID) Mesh->SetOverlayMaterial(SavedDashOverlay);
  Mesh->SetBoundsScale(DashSmearSavedBounds);
 }
 bDashSmearOverlaySaved=false;SavedDashOverlay=nullptr;DashSmearAlpha=0;
}
