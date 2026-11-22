#include "Field/BBSBFieldActions.h"
#include "Caravan/BBSBCaravan.h"
#include "BBSBCharacter.h"
#include "Character/BBSBTraversal.h"
#include "CanvasItem.h"
#include "Framework/Application/SlateApplication.h"
#include "RenderUtils.h"
#include "Interaction/BBSBInteraction.h"
#include "Combat/BBSBCombat.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/BBSBOutlinedPaths.h"

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
 const auto* Buddy=Cast<ABBSBBunniarCompanion>(InteractionEquipment());
 return Buddy && Buddy->IsSurfaceFocusActive();
}

bool UBBSBFieldActionComponent::IsBunniarPointerHighlighted() const
{ return HoverBuddy.IsValid(); }

void UBBSBFieldActionComponent::UpdateAimPresentation()
{
 AimPresentation=EBBSBAimPresentation::Hidden;AimPresentationActor=nullptr;
 if(SurfaceAimDecal) SurfaceAimDecal->SetVisibility(false);
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 const auto* Interaction=Character ? Character->FindComponentByClass<UBBSBInteractionComponent>():nullptr;
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 const auto* BuddyAnchor=Cast<ABBSBBunniarCompanion>(InteractionEquipment());
 const bool BuddyFocus=PC && PC->IsLocalController() && !bEquipmentWheelOpen && BuddyAnchor && IsBunniarPointerHighlighted() && !(Interaction && Interaction->bDialogueActive) && !(Combat && (Combat->bDead || Combat->HasTargetLock()));
 if(Camera && (BuddyFocus || BuddyOutlineMID))
 {
  if(!BuddyOutlineMID) if(auto* Material=BuddyOutlineMaterial.LoadSynchronous()) BuddyOutlineMID=UMaterialInstanceDynamic::Create(Material,this);
  if(BuddyOutlineMID) Camera->AddOrUpdateBlendable(BuddyOutlineMID,BuddyFocus ? 1.f:0.f);
 }
 if(!PC || !PC->IsLocalController() || (Combat && Combat->HasTargetLock()) || bEquipmentWheelOpen || IsSurfaceAimLocked() || BuddyFocus || (Interaction && Interaction->bDialogueActive) || (Combat && Combat->bDead)) return;
 AimPresentation=BuddyAnchor ? EBBSBAimPresentation::Diamond:EBBSBAimPresentation::Air;AimPresentationColor=AirAimColor;
 FHitResult Hit;
 if(!TraceEquipmentAim(Hit)) { FVector Point;ResolveEquipmentAim(Point,Hit);AimPresentationPoint=Point;return; }
 AimPresentationPoint=Hit.ImpactPoint;AimPresentationActor=Hit.GetActor();
 if(IsEquipmentTarget(Hit.GetActor()))
 {
  AimPresentation=BuddyAnchor ? EBBSBAimPresentation::Diamond:EBBSBAimPresentation::Interactive;
  AimPresentationColor=Cast<ABBSBInteractionNPC>(Hit.GetActor()) ? NPCAimColor:Cast<ABBSBCarryObject>(Hit.GetActor()) ? CarryAimColor:Cast<ABBSBInkEnemy>(Hit.GetActor()) ? AttackAimColor:FieldEquipment ? DeviceAimColor:AttackAimColor;
  return;
 }
 AimPresentation=BuddyAnchor ? EBBSBAimPresentation::Diamond:EBBSBAimPresentation::Surface;AimPresentationColor=SurfaceAimColor;
 const auto* Buddy=Cast<ABBSBBunniarCompanion>(InteractionEquipment());
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

ABBSBCarryObject* UBBSBFieldActionComponent::HeldBunniarCarry() const
{
 const auto* Buddy = Cast<ABBSBBunniarCompanion>(InteractionEquipment());
 if (!Buddy || !Buddy->bSurfaceAttached || !IsValid(Buddy->SurfaceComponent)) return nullptr;
 auto* Object = Cast<ABBSBCarryObject>(Buddy->SurfaceComponent->GetOwner());
 return IsValid(Object) && Object->bCarried && !Object->bBroken && Object->CarryingCharacter() == Character ? Object : nullptr;
}

ABBSBCarryObject* UBBSBFieldActionComponent::PreparedCarryThrow() const
{
 auto* Object = CarryThrowTarget.Get();
 const auto* Player = Cast<ABBSBCharacter>(Character);
 if (!Object || !Player || !IsGestureChannelHeld(0) || bEquipmentInputBlocked || bEquipmentWheelOpen ||
     bDeathEquipmentFrozen || bStoryPresentationHidden || bDodging || !GetWorld() || GetWorld()->IsPaused() ||
     (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive())) return nullptr;
 if ((Player->Combat && (!Player->Combat->CanAttack() || Player->Combat->IsStageLocked())) ||
     (Player->Traversal && (Player->Traversal->IsWireTraversalActive() || Player->Traversal->IsParkourActive())) ||
     (Player->Interaction && Player->Interaction->bDialogueActive)) return nullptr;
 return HeldBunniarCarry() == Object ? Object : nullptr;
}

void UBBSBFieldActionComponent::CancelCarryThrow(const ABBSBCarryObject* Object)
{
 if (CarryThrowTarget.Get() == Object) CarryThrowTarget.Reset();
}

bool UBBSBFieldActionComponent::IsCarryThrowPreviewActive() const
{
 return PreparedCarryThrow() != nullptr;
}

void UBBSBFieldActionComponent::DrawCarryThrowPreview(AHUD* HUD, UCanvas* Canvas) const
{
 const auto* Object = PreparedCarryThrow();
 const auto* PC = HUD ? HUD->GetOwningPlayerController() : nullptr;
 if (!Object || !PC || !Canvas) return;

 const TArray<FVector> Path = Object->ForwardThrowPreviewPath();
 TArray<FVector2D, TInlineAllocator<16>> Screen;
 for (const FVector& WorldPoint : Path)
 {
  FVector2D Point;
  if (!PC->ProjectWorldLocationToScreen(WorldPoint, Point, true)) break;
  if (Screen.IsEmpty() || FVector2D::DistSquared(Screen.Last(), Point) > .16) Screen.Add(Point);
 }
 if (Screen.Num() < 2) return;

 struct FEdge { FVector2D Left, Right; FLinearColor Color; };
 const float Scale = FMath::Clamp(Canvas->SizeX / 1280.f, .7f, 1.5f);
 const FLinearColor StartColor(1.f, .29f, .46f, .98f);
 const FLinearColor EndColor(1.f, .76f, .70f, 0.f);
 auto Edge = [&](int32 Index)
 {
  const float T = float(Index) / (Screen.Num() - 1);
  const FVector2D Direction = (Screen[FMath::Min(Index + 1, Screen.Num() - 1)] - Screen[FMath::Max(0, Index - 1)]).GetSafeNormal();
  const FVector2D Side(-Direction.Y, Direction.X);
  const float HalfWidth = FMath::Lerp(3.2f, .35f, T) * Scale;
  return FEdge{Screen[Index] - Side * HalfWidth, Screen[Index] + Side * HalfWidth,
               FMath::Lerp(StartColor, EndColor, FMath::SmoothStep(.12f, 1.f, T))};
 };
 TArray<FCanvasUVTri> Triangles;
 Triangles.Reserve((Screen.Num() - 1) * 2);
 auto AddTriangle = [&](FVector2D A, FVector2D B, FVector2D C, FLinearColor ColorA, FLinearColor ColorB, FLinearColor ColorC)
 {
  FCanvasUVTri& Tri = Triangles.AddDefaulted_GetRef();
  Tri.V0_Pos = A; Tri.V1_Pos = B; Tri.V2_Pos = C;
  Tri.V0_UV = Tri.V1_UV = Tri.V2_UV = FVector2D::ZeroVector;
  Tri.V0_Color = ColorA; Tri.V1_Color = ColorB; Tri.V2_Color = ColorC;
 };
 FEdge Previous = Edge(0);
 for (int32 Index = 1; Index < Screen.Num(); ++Index)
 {
  const FEdge Next = Edge(Index);
  AddTriangle(Previous.Left, Previous.Right, Next.Left, Previous.Color, Previous.Color, Next.Color);
  AddTriangle(Next.Left, Previous.Right, Next.Right, Next.Color, Previous.Color, Next.Color);
  Previous = Next;
 }
 FCanvasTriangleItem Ribbon(Triangles, GWhiteTexture);
 Ribbon.BlendMode = SE_BLEND_Translucent;
 Canvas->DrawItem(Ribbon);
}

void UBBSBFieldActionComponent::DrawContextAim(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || AimPresentation==EBBSBAimPresentation::Hidden || IsSurfaceAimLocked()) return;
 if(const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;Combat && Combat->HasTargetLock()) return;
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 if(!PC) return;
 float X=Canvas->SizeX*.5f,Y=Canvas->SizeY*.5f;
 if(PC->bShowMouseCursor) PC->GetMousePosition(X,Y);
 const float S=FMath::Clamp(Canvas->SizeX/1280.f,.7f,1.4f);
 const FLinearColor Shade(.055f,.04f,.075f,.98f);
 TArray<BBSBOutlinedPaths::FPath> Paths;
 auto Path=[&](TArray<FVector2D> Points,bool Closed=false)
 { BBSBOutlinedPaths::FPath P;P.Points=MoveTemp(Points);P.Color=AimPresentationColor;P.Width=2.5f*S;P.Border=2.f*S;P.bClosed=Closed;Paths.Add(MoveTemp(P)); };
 auto Line=[&](FVector2D A,FVector2D B) { Path({A,B}); };
 const FVector2D Center(X,Y);
 if(AimPresentation==EBBSBAimPresentation::Diamond)
 { const float R=8.f*S;Path({Center+FVector2D(0,-R),Center+FVector2D(R,0),Center+FVector2D(0,R),Center+FVector2D(-R,0)},true); }
 else if(AimPresentation==EBBSBAimPresentation::Interactive)
 {
  TArray<FVector2D> Ring;for(int32 I=0;I<48;++I) { const float A=2*PI*I/48.f;Ring.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*8.f*S); }Path(MoveTemp(Ring),true);
 }
 else
 {
  const float Gap=(AimPresentation==EBBSBAimPresentation::Air ? 7.f:2.f)*S;
  const float End=Gap+5.f*S;
  Line(Center+FVector2D(-End,0),Center+FVector2D(-Gap,0));Line(Center+FVector2D(Gap,0),Center+FVector2D(End,0));
  Line(Center+FVector2D(0,-End),Center+FVector2D(0,-Gap));Line(Center+FVector2D(0,Gap),Center+FVector2D(0,End));
 }
 BBSBOutlinedPaths::Draw(Canvas,Paths,Shade);
}
