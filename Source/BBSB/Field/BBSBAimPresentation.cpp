#include "Field/BBSBFieldActions.h"
#include "Caravan/BBSBCaravan.h"
#include "Interaction/BBSBInteraction.h"
#include "Combat/BBSBCombat.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

FVector UBBSBFieldActionComponent::MovementDashDirection() const
{
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 if(!PC) return FVector::ForwardVector;
 FVector2D Input=GetWorld()->GetTimeSeconds()-LastMoveAt<.08 ? LastMove:FVector2D::ZeroVector;
 const float Right=float(PC->IsInputKeyDown(EKeys::D))-float(PC->IsInputKeyDown(EKeys::A));
 const float Forward=float(PC->IsInputKeyDown(EKeys::W))-float(PC->IsInputKeyDown(EKeys::S)||PC->IsInputKeyDown(EKeys::X));
 if(Right!=0 || Forward!=0) Input=FVector2D(Right,Forward);
 FRotator View=PC->GetControlRotation();
 if(IsSurfaceAimLocked()) { FVector Eye;PC->GetPlayerViewPoint(Eye,View); }
 return Input.IsNearlyZero() ? Character->GetActorForwardVector().GetSafeNormal2D():FRotator(0,View.Yaw,0).RotateVector(FVector(Input.Y,Input.X,0)).GetSafeNormal();
}

bool UBBSBFieldActionComponent::IsSurfaceAimLocked() const
{
 const auto* Buddy=Cast<ABBSBBunniarCompanion>(FieldEquipment);
 return Buddy && Buddy->bSurfaceAttached && Buddy->bSurfaceViewLocked && IsValid(Buddy->SurfaceComponent);
}

void UBBSBFieldActionComponent::UpdateAimPresentation()
{
 AimPresentation=EBBSBAimPresentation::Hidden;AimPresentationActor=nullptr;
 if(SurfaceAimDecal) SurfaceAimDecal->SetVisibility(false);
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 const auto* Interaction=Character ? Character->FindComponentByClass<UBBSBInteractionComponent>():nullptr;
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 const bool BuddyFocus=PC && PC->IsLocalController() && !bEquipmentWheelOpen && IsSurfaceAimLocked() && !(Interaction && Interaction->bDialogueActive) && !(Combat && Combat->bDead);
 if(Camera && (BuddyFocus || BuddyOutlineMID))
 {
  if(!BuddyOutlineMID) if(auto* Material=BuddyOutlineMaterial.LoadSynchronous()) BuddyOutlineMID=UMaterialInstanceDynamic::Create(Material,this);
  if(BuddyOutlineMID) Camera->AddOrUpdateBlendable(BuddyOutlineMID,BuddyFocus ? 1.f:0.f);
 }
 if(!PC || !PC->IsLocalController() || bEquipmentWheelOpen || IsSurfaceAimLocked() || (Interaction && Interaction->bDialogueActive) || (Combat && Combat->bDead)) return;
 AimPresentation=EBBSBAimPresentation::Air;AimPresentationColor=AirAimColor;
 FHitResult Hit;
 if(!TraceEquipmentAim(Hit)) { FVector Point;ResolveEquipmentAim(Point,Hit);AimPresentationPoint=Point;return; }
 AimPresentationPoint=Hit.ImpactPoint;AimPresentationActor=Hit.GetActor();
 if(IsEquipmentTarget(Hit.GetActor()))
 {
  AimPresentation=EBBSBAimPresentation::Interactive;
  AimPresentationColor=Cast<ABBSBInteractionNPC>(Hit.GetActor()) ? NPCAimColor:Cast<ABBSBCarryObject>(Hit.GetActor()) ? CarryAimColor:FieldEquipment ? DeviceAimColor:AttackAimColor;
  return;
 }
 AimPresentation=EBBSBAimPresentation::Surface;AimPresentationColor=SurfaceAimColor;
 const auto* Buddy=Cast<ABBSBBunniarCompanion>(FieldEquipment);
 if(!Buddy || Buddy->bSurfaceAttached || Buddy->bAirHovering || Buddy->bGliding) return;
 if(!SurfaceAimDecal)
 {
  auto* Material=SurfaceAimMaterial.LoadSynchronous();
  if(!Material) return;
  SurfaceAimDecal=NewObject<UDecalComponent>(Character,TEXT("BunniarAimDecal"));Character->AddInstanceComponent(SurfaceAimDecal);
  SurfaceAimDecal->SetDecalMaterial(UMaterialInstanceDynamic::Create(Material,SurfaceAimDecal));
  SurfaceAimDecal->SetFadeScreenSize(.0001f);SurfaceAimDecal->SortOrder=5;SurfaceAimDecal->RegisterComponent();
 }
 if(auto* MID=Cast<UMaterialInstanceDynamic>(SurfaceAimDecal->GetDecalMaterial())) MID->SetVectorParameterValue(TEXT("Tint"),SurfaceDecalColor);
 // The material's ring occupies 81% of its projection box; use centimetres for the visible radius.
 const float Radius=FMath::Max(2.f,SurfaceAimRadius)/.81f;
 SurfaceAimDecal->DecalSize=FVector(4.f,Radius,Radius);
 SurfaceAimDecal->SetWorldLocationAndRotation(Hit.ImpactPoint+Hit.ImpactNormal*.3f,Hit.ImpactNormal.Rotation());
 SurfaceAimDecal->SetVisibility(true);
}

void UBBSBFieldActionComponent::DrawContextAim(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || AimPresentation==EBBSBAimPresentation::Hidden || IsSurfaceAimLocked()) return;
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 if(!PC) return;
 float X=Canvas->SizeX*.5f,Y=Canvas->SizeY*.5f;
 if(PC->bShowMouseCursor) PC->GetMousePosition(X,Y);
 const float S=FMath::Clamp(Canvas->SizeX/1280.f,.7f,1.4f);
 const FLinearColor Shade(.10f,.08f,.12f,.55f);
 auto Line=[&](FVector2D A,FVector2D B)
 {
  HUD->DrawLine(A.X,A.Y,B.X,B.Y,Shade,3.f*S);
  HUD->DrawLine(A.X,A.Y,B.X,B.Y,AimPresentationColor,1.5f*S);
 };
 const FVector2D Center(X,Y);
 if(AimPresentation==EBBSBAimPresentation::Interactive)
 {
  for(int32 I=0;I<40;++I)
  {
   const float A=2*PI*I/40.f,B=2*PI*(I+1)/40.f;
   Line(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*8.f*S,Center+FVector2D(FMath::Cos(B),FMath::Sin(B))*8.f*S);
  }
 }
 else
 {
  const float Gap=(AimPresentation==EBBSBAimPresentation::Air ? 7.f:2.f)*S;
  const float End=Gap+5.f*S;
  Line(Center+FVector2D(-End,0),Center+FVector2D(-Gap,0));Line(Center+FVector2D(Gap,0),Center+FVector2D(End,0));
  Line(Center+FVector2D(0,-End),Center+FVector2D(0,-Gap));Line(Center+FVector2D(0,Gap),Center+FVector2D(0,End));
 }
}
