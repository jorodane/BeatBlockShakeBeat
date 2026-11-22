#include "Interaction/BBSBDraggable.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/BBSBConditions.h"
#include "Interaction/BBSBDragMotion.h"

UBBSBDraggable::UBBSBDraggable()
{
 InteractType = TEXT("Drag");
 bUseBunniarAttachment = true;
 MovedComponent.ComponentProperty = TEXT("MotionBody");
 Spline.ComponentProperty = TEXT("DragPath");
 DragHintText = NSLOCTEXT("BBSBInteraction", "DragHint", "[화살표 방향으로 드래그하여 이동]");
}

void UBBSBDraggable::BeginPlay()
{
 if (!bExternalMotion)
 {
  auto* Component = Cast<USceneComponent>(MovedComponent.GetComponent(GetOwner()));
  if (IsValid(Component) && Component->GetOwner() == GetOwner() && Component != GetOwner()->GetRootComponent())
  {
   Moving = Component;
   Path = Cast<USplineComponent>(Spline.GetComponent(GetOwner()));
   if (Path.IsValid() && Path->IsAttachedTo(Component)) Path.Reset();
   Home = GetOwner()->GetActorTransform().InverseTransformPosition(Component->GetComponentLocation());
   TargetProgress = FMath::Clamp(InitialProgress, 0.f, 1.f);
  }
 }
 Super::BeginPlay();
}

FVector UBBSBDraggable::RouteOffset(float Progress) const
{
 if (Constraint == EBBSBDragConstraint::Axis) return LocalTravel * Progress;
 if (Constraint == EBBSBDragConstraint::Spline && Path.IsValid())
 {
  const FVector Start = Path->GetLocationAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::World);
  const FVector At = Path->GetLocationAtDistanceAlongSpline(Progress * Path->GetSplineLength(), ESplineCoordinateSpace::World);
  return GetOwner()->GetActorTransform().InverseTransformVector(At - Start);
 }
 return FVector::ZeroVector;
}

float UBBSBDraggable::RouteLength() const
{
 if (Constraint == EBBSBDragConstraint::Spline) return Path.IsValid() ? Path->GetSplineLength() : 0.f;
 return GetOwner()->GetActorTransform().TransformVector(LocalTravel).Size();
}

bool UBBSBDraggable::CanInteract_Implementation(ACharacter* Character) const
{
 if (bExternalMotion) return true;
 if (!Moving.IsValid() || !DriveCondition.IsNone() || Moving->Mobility != EComponentMobility::Movable) return false;
 if (const auto* Part = Cast<UPrimitiveComponent>(Moving.Get()); Part && Part->IsSimulatingPhysics()) return false;
 return Constraint == EBBSBDragConstraint::Area || RouteLength() > UE_KINDA_SMALL_NUMBER;
}

float UBBSBDraggable::ProjectLinearDrag(const APlayerController* Controller, const FVector& Start, const FVector& Travel,
 float Progress, FVector2D Delta, float PixelRange, bool bFallbackHorizontal)
{
 FRotator View = FRotator::ZeroRotator;
 if (Controller && !Travel.IsNearlyZero())
 {
  FVector Eye;
  Controller->GetPlayerViewPoint(Eye, View);
  const FVector End = Start + Travel;
  const double Depth0 = FVector::DotProduct(Start - Eye, View.Vector());
  const double Depth1 = FVector::DotProduct(End - Eye, View.Vector());
  FVector2D A, B, Current;
  if (Depth0 > 1. && Depth1 > 1. && Controller->ProjectWorldLocationToScreen(Start, A, true)
      && Controller->ProjectWorldLocationToScreen(End, B, true)
      && Controller->ProjectWorldLocationToScreen(Start + Travel * Progress, Current, true))
  {
   const FVector2D Stroke = B - A;
   if (Stroke.SizeSquared() > FMath::Square(24.))
   {
    int32 Width = 0, Height = 0;
    Controller->GetViewportSize(Width, Height);
    const double MaximumStroke = FMath::Max(double(PixelRange), FMath::Min(Width, Height) * .55);
    if (Stroke.Size() > MaximumStroke)
    {
     const double Increment = FVector2D::DotProduct(Delta, Stroke.GetSafeNormal()) / FMath::Max(20., MaximumStroke);
     return FMath::Clamp(Progress + Increment, 0., 1.);
    }
    const double Along = FMath::Clamp(FVector2D::DotProduct(Current + Delta - A, Stroke) / Stroke.SizeSquared(), 0., 1.);
    return FMath::Clamp(Along * Depth0 / FMath::Max(1., Depth1 * (1. - Along) + Along * Depth0), 0., 1.);
   }
  }
 }
 const FVector ScreenAxis = View.RotateVector(bFallbackHorizontal ? FVector::RightVector : -FVector::UpVector);
 const float Sign = FVector::DotProduct(Travel, ScreenAxis) < 0. ? -1.f : 1.f;
 const double Pixels = bFallbackHorizontal ? Delta.X : Delta.Y;
 return FMath::Clamp(Progress + Sign * Pixels / FMath::Max(20.f, PixelRange), 0., 1.);
}

void UBBSBDraggable::ApplyPointerDelta(FVector2D Delta)
{
 if (!bDragging || bExternalMotion || Delta.ContainsNaN() || !Moving.IsValid() || !IsValid(ActiveContext.Character)) return;
 const auto* Controller = Cast<APlayerController>(ActiveContext.Character->GetController());
 if (!Controller) return;
 const FTransform Frame = GetOwner()->GetActorTransform();
 FVector Eye;
 FRotator View;
 Controller->GetPlayerViewPoint(Eye, View);
 if (Constraint == EBBSBDragConstraint::Area)
 {
  const FVector Step = View.RotateVector(FVector(0., Delta.X, -Delta.Y) * UnitsPerPixel);
  const FVector Desired = TargetOffset + Frame.InverseTransformVector(Step);
  const FBox Bounds(AreaMin.ComponentMin(AreaMax), AreaMin.ComponentMax(AreaMax));
  TargetOffset = Bounds.GetClosestPointTo(Desired);
 }
 else if (Constraint == EBBSBDragConstraint::Spline)
 {
  const float Length = RouteLength();
  if (Length <= UE_KINDA_SMALL_NUMBER) return;
  const float Distance = DragState.Progress * Length;
  const float A = FMath::Max(0.f, Distance - 20.f);
  const float B = FMath::Min(Length, Distance + 20.f);
  const FVector Offset = ActiveContext.Point - Moving->GetComponentLocation();
  FVector2D Start, End;
  const bool bProjected = Controller->ProjectWorldLocationToScreen(Frame.TransformPosition(Home + RouteOffset(A / Length)) + Offset, Start, true)
      && Controller->ProjectWorldLocationToScreen(Frame.TransformPosition(Home + RouteOffset(B / Length)) + Offset, End, true);
  FVector2D Stroke = bProjected ? End - Start : FVector2D::ZeroVector;
  if (Stroke.SizeSquared() < 4.)
  {
   const FVector Local = View.UnrotateVector(Frame.TransformVector(RouteOffset(B / Length) - RouteOffset(A / Length)));
   Stroke = FVector2D(Local.Y, -Local.Z);
  }
  const double Increment = FVector2D::DotProduct(Delta, Stroke.GetSafeNormal()) * UnitsPerPixel / Length;
  TargetProgress = FMath::Clamp(TargetProgress + Increment, 0., 1.);
 }
 else
 {
  const FVector Travel = Frame.TransformVector(LocalTravel);
  const FVector Local = View.UnrotateVector(Travel);
  TargetProgress = ProjectLinearDrag(Controller, ActiveContext.Point - Travel * DragState.Progress, Travel,
    TargetProgress, Delta, RouteLength() / FMath::Max(UnitsPerPixel, .05f), FMath::Abs(Local.Y) >= FMath::Abs(Local.Z));
 }
}

void UBBSBDraggable::SetDragProgress(float Progress)
{
 if (!bExternalMotion && Constraint != EBBSBDragConstraint::Area && FMath::IsFinite(Progress))
 {
  TargetProgress = FMath::Clamp(Progress, 0.f, 1.f);
  UpdateInteractionTick();
 }
}

bool UBBSBDraggable::NeedsIdleTick() const
{
 return Moving.IsValid() && (!DriveCondition.IsNone() || !FMath::IsNearlyEqual(TargetProgress, DragState.Progress, .0001f)
  || (Constraint == EBBSBDragConstraint::Area && !TargetOffset.Equals(DragState.LocalOffset, .01f)));
}

void UBBSBDraggable::MovePart(float DeltaSeconds)
{
 auto* Component = Moving.Get();
 if (!Component || Component->Mobility != EComponentMobility::Movable) return;
 const FTransform Frame = GetOwner()->GetActorTransform();
 const float Budget = FMath::Max(1.f, Speed) * DeltaSeconds;
 DragState.bBlocked = false;
 if (Constraint == EBBSBDragConstraint::Area)
 {
  const FVector Goal = Frame.TransformPosition(Home + TargetOffset);
  const FVector Start = Component->GetComponentLocation();
  if (!Goal.Equals(Start, .01f))
  {
   const FVector Next = Start + (Goal - Start).GetClampedToMaxSize(Budget);
   DragState.bBlocked = BBSBDragMotion::Move(Component, Next) < .9999f;
   DragState.LocalOffset = Frame.InverseTransformPosition(Component->GetComponentLocation()) - Home;
   if (DragState.bBlocked) TargetOffset = DragState.LocalOffset;
  }
  return;
 }
 const float Length = RouteLength();
 if (Length < .01f || FMath::IsNearlyEqual(TargetProgress, DragState.Progress, .0001f)) return;
 const float Next = FMath::FInterpConstantTo(DragState.Progress, TargetProgress, Budget / Length, 1.f);
 // Advance through short segments on a curve so collision follows the path around corners.
 const float Start = DragState.Progress;
 const int32 Steps = FMath::Clamp(FMath::CeilToInt(FMath::Abs(Next - Start) * Length / 10.f), 1, 64);
 for (int32 Index = 1; Index <= Steps; ++Index)
 {
  const float Step = FMath::Lerp(Start, Next, float(Index) / Steps);
  const FVector Goal = Frame.TransformPosition(Home + RouteOffset(Step));
  const float Reached = BBSBDragMotion::Move(Component, Goal);
  DragState.Progress = FMath::Lerp(DragState.Progress, Step, Reached);
  if (Reached < .9999f)
  {
   DragState.bBlocked = true;
   if (DriveCondition.IsNone()) TargetProgress = DragState.Progress;
   break;
  }
 }
 DragState.LocalOffset = Frame.InverseTransformPosition(Component->GetComponentLocation()) - Home;
}

void UBBSBDraggable::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
 if (!bExternalMotion)
 {
  if (!DriveCondition.IsNone())
  {
   if (const auto* Receiver = GetOwner()->FindComponentByClass<UBBSBConditionReceiver>()) TargetProgress = Receiver->GetConditionValue(DriveCondition);
  }
  MovePart(DeltaSeconds);
 }
 Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
 UpdateInteractionTick();
}

void UBBSBDraggable::SetExternalDrag(bool bAvailable, float Progress, const FVector& LocalOffset, bool bBlocked)
{
 bExternalDragAvailable = bAvailable;
 DragState.Progress = Progress;
 DragState.LocalOffset = LocalOffset;
 DragState.bBlocked = bBlocked;
}

void UBBSBDraggable::InteractionStarted()
{
 if (!ActiveContext.bHeld || (bExternalMotion && !bExternalDragAvailable)) return;
 bDragging = true;
 const auto Context = ActiveContext;
 const auto State = DragState;
 OnDragEnter.Broadcast(Context, State);
}

void UBBSBDraggable::InteractionTick(float DeltaSeconds)
{
 if (!bDragging) return;
 const auto Context = ActiveContext;
 const auto State = DragState;
 OnDragStay.Broadcast(Context, State);
}

void UBBSBDraggable::InteractionEnded(const FBBSBInteractContext& Context)
{
 if (!bDragging) return;
 bDragging = false;
 const auto State = DragState;
 OnDragEnd.Broadcast(Context, State);
}

bool UBBSBDraggable::GetDragHint(FVector2D& ScreenAxis, FText& Text) const
{
 if (!bDragging || bExternalMotion || !IsValid(ActiveContext.Character)) return false;
 const auto* PC = Cast<APlayerController>(ActiveContext.Character->GetController());
 if (!PC) return false;
 Text = DragHintText;
 ScreenAxis = FVector2D(1., 0.);
 if (Constraint == EBBSBDragConstraint::Area) return false;
 const auto Frame = GetOwner()->GetActorTransform();
 const FVector Direction = Frame.TransformVector(RouteOffset(FMath::Min(1.f, DragState.Progress + .05f)) - RouteOffset(FMath::Max(0.f, DragState.Progress - .05f))).GetSafeNormal();
 FVector2D A, B;
 if (PC->ProjectWorldLocationToScreen(ActiveContext.Point, A, true) && PC->ProjectWorldLocationToScreen(ActiveContext.Point + Direction * 70., B, true) && !(B - A).IsNearlyZero())
 {
  ScreenAxis = (B - A).GetSafeNormal();
 }
 return !Text.IsEmpty();
}
