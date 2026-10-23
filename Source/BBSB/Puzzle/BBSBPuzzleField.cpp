#include "Puzzle/BBSBPuzzleField.h"
#include "BBSBCharacter.h"
#include "Interaction/BBSBInteraction.h"
#include "Caravan/BBSBCaravan.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/InputComponent.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"

namespace Meadow
{
 const FLinearColor Wood(.26f,.16f,.10f),Brass(.65f,.44f,.19f),Cream(.80f,.73f,.57f),Rose(.61f,.27f,.39f),Mint(.28f,.56f,.42f),Blue(.32f,.45f,.60f);
 UStaticMesh* Mesh(const TCHAR* Shape) { return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape)); }
 ABBSBPuzzleField* Field(UWorld* World) { for (TActorIterator<ABBSBPuzzleField> It(World);It;++It) return *It; return nullptr; }
 bool IsCarry(EBBSBPuzzleKind Kind) { return Kind==EBBSBPuzzleKind::CarrySocket || Kind==EBBSBPuzzleKind::Balance; }
 const FVector2D WirePoints[]={FVector2D(80,0),FVector2D(80,-70),FVector2D(150,-70)};
}

ABBSBPuzzleStation::ABBSBPuzzleStation()
{
 PrimaryActorTick.bCanEverTick=true;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StationOrigin")));
 auto Create=[&](const TCHAR* Name)
 {
  auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name); C->SetupAttachment(GetRootComponent());
  C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(Meadow::Mesh(TEXT("Cube"))); return C;
 };
 Receiver=Create(TEXT("TouchPanel"));Payload=Create(TEXT("MovingPiece"));Socket=Create(TEXT("Destination"));Output=Create(TEXT("Mechanism"));Indicator=Create(TEXT("TimingLamp"));
 Receiver->SetCollisionProfileName(TEXT("BlockAll"));Payload->SetCollisionProfileName(TEXT("BlockAll"));
 Receiver->SetCollisionObjectType(ECC_WorldDynamic);Payload->SetCollisionObjectType(ECC_WorldDynamic);
 for (auto* C:{Socket.Get(),Indicator.Get()}) C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Output->SetCollisionProfileName(TEXT("BlockAll"));
 Sign=CreateDefaultSubobject<UTextRenderComponent>(TEXT("StationSign"));Sign->SetupAttachment(GetRootComponent());
 Sign->SetHorizontalAlignment(EHTA_Center);Sign->SetWorldSize(25);Sign->SetTextRenderColor(FColor(235,223,188));
 Tags.Add(TEXT("BBSBInteractable"));
}
UStaticMeshComponent* ABBSBPuzzleStation::Part(const FString& Name,FVector Location,FVector Size,FLinearColor Color,const TCHAR* Shape,bool Solid)
{
 auto* C=NewObject<UStaticMeshComponent>(this,*Name);AddInstanceComponent(C);C->SetupAttachment(GetRootComponent());
 C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(Meadow::Mesh(Shape));
 C->SetRelativeLocation(Location);C->SetRelativeScale3D(Size/100.f);C->SetCollisionEnabled(Solid ? ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
 if (Solid) C->SetCollisionProfileName(TEXT("BlockAll"));
 C->ComponentTags.Add(TEXT("BBSBGeneratedPuzzlePart"));C->RegisterComponent();BBSBCombatVisuals::Tint(C,Color);Parts.Add(C);return C;
}
void ABBSBPuzzleStation::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform);Build(); }
void ABBSBPuzzleStation::Build()
{
 TArray<UStaticMeshComponent*> Existing;GetComponents(Existing);
 for(UStaticMeshComponent* C:Existing)
 {
  const FString Name=C->GetName();
  const bool LegacyPart=Name.StartsWith(TEXT("StoneFooting")) || Name.StartsWith(TEXT("GatePost")) || Name.StartsWith(TEXT("BoardLeg")) || Name.StartsWith(TEXT("Pad")) || Name.StartsWith(TEXT("LadderRail")) || Name.StartsWith(TEXT("LadderLanding")) || Name.StartsWith(TEXT("Rung")) || Name.StartsWith(TEXT("Wire"));
  if(C->ComponentHasTag(TEXT("BBSBGeneratedPuzzlePart")) || LegacyPart) C->DestroyComponent();
 }
 Parts.Empty();
 auto Shape=[&](UStaticMeshComponent* C,FVector Pos,FVector Size,FLinearColor Color,const TCHAR* MeshName=TEXT("Cube"))
 { C->SetStaticMesh(Meadow::Mesh(MeshName));C->SetRelativeLocation(Pos);C->SetRelativeScale3D(Size/100.f);C->SetRelativeRotation(FRotator::ZeroRotator);BBSBCombatVisuals::Tint(C,Color); };
 Shape(Receiver,FVector(0,0,100),FVector(65,230,140),Meadow::Brass);
 Shape(Payload,FVector(-105,-150,140),FVector(60),Meadow::Rose,TEXT("Sphere"));
 Shape(Socket,FVector(-105,220,140),FVector(85,85,35),Meadow::Mint,TEXT("Cylinder"));Socket->SetRelativeRotation(FRotator(90,0,0));
 Shape(Output,FVector(210,0,145),FVector(45,210,260),Meadow::Wood);
 Shape(Indicator,FVector(-40,0,200),FVector(30),Meadow::Blue,TEXT("Sphere"));
 Payload->SetVisibility(Meadow::IsCarry(Kind) || Kind==EBBSBPuzzleKind::ShakeLoose || Kind==EBBSBPuzzleKind::FlickRight || Kind==EBBSBPuzzleKind::FlickUp || Kind==EBBSBPuzzleKind::ChargeFlick || Kind==EBBSBPuzzleKind::ShakeFlick || Kind==EBBSBPuzzleKind::Relay);
 Payload->SetCollisionEnabled(Payload->IsVisible() ? ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
 Socket->SetVisibility(Payload->IsVisible());
 if (Kind==EBBSBPuzzleKind::FlickUp || Kind==EBBSBPuzzleKind::ChargeFlick) Socket->SetRelativeLocation(FVector(-105,-150,420));
 Part(TEXT("StoneFooting"),FVector(80,0,6),FVector(650,650,16),Meadow::Cream,TEXT("Cylinder"),true);
 for (int Side:{-1,1})
 {
  Part(FString::Printf(TEXT("GatePost%d"),Side),FVector(210,Side*135,150),FVector(25,25,300),Meadow::Brass,TEXT("Cube"),true);
  Part(FString::Printf(TEXT("BoardLeg%d"),Side),FVector(0,Side*90,36),FVector(18,18,72),Meadow::Wood,TEXT("Cube"),true);
 }
 if (Kind==EBBSBPuzzleKind::BeatOrder)
  for (int I=0;I<3;++I) Part(FString::Printf(TEXT("Pad%d"),I),FVector(-36,(I-1)*70,115),FVector(8,54,70),I==0 ? Meadow::Rose:I==1 ? Meadow::Mint:Meadow::Blue);
 if (Kind==EBBSBPuzzleKind::HoldLift) { Shape(Output,FVector(235,0,35),FVector(160,200,35),Meadow::Blue); }
 if (Kind==EBBSBPuzzleKind::Ladder)
 {
  Shape(Output,FVector(210,-65,370),FVector(18,18,300),Meadow::Wood);
  auto* Rail=Part(TEXT("LadderRailR"),FVector(210,65,370),FVector(18,18,300),Meadow::Wood,TEXT("Cube"),true);Rail->AttachToComponent(Output,FAttachmentTransformRules::KeepWorldTransform);
  for (int I=0;I<9;++I) { auto* Rung=Part(FString::Printf(TEXT("Rung%d"),I),FVector(210,0,240+I*32),FVector(22,130,10),Meadow::Cream,TEXT("Cube"),true);Rung->AttachToComponent(Output,FAttachmentTransformRules::KeepWorldTransform); }
  Part(TEXT("LadderLanding"),FVector(280,0,330),FVector(140,260,30),Meadow::Wood,TEXT("Cube"),true);
 }
 if (Kind==EBBSBPuzzleKind::Lever) Shape(Output,FVector(210,0,130),FVector(20,20,190),Meadow::Rose);
 if (Kind==EBBSBPuzzleKind::Wire)
 {
  Shape(Output,FVector(205,0,160),FVector(40,250,210),Meadow::Wood);
  for (int I=0;I<3;++I) { auto* Wire=Part(FString::Printf(TEXT("Wire%d"),I),FVector(178,-75+I*75,170),FVector(8,70,8),Meadow::Mint);Wire->SetVisibility(false); }
 }
 if (Kind==EBBSBPuzzleKind::DivePendulum) Shape(Output,FVector(205,0,160),FVector(35,40,80),Meadow::Rose);
 if (Kind==EBBSBPuzzleKind::Balance) { Shape(Socket,FVector(-105,150,140),FVector(100,100,25),Meadow::Mint,TEXT("Cylinder")); }
 PayloadHome=Payload->GetRelativeLocation();OutputHome=Output->GetRelativeLocation();
 Sign->SetRelativeLocation(FVector(0,0,270));Sign->SetText(FText::FromString(FString::Printf(TEXT("%02d  %s"),Number,*Title())));
}
void ABBSBPuzzleStation::BeginPlay() { Super::BeginPlay();PayloadHome=Payload->GetRelativeLocation();OutputHome=Output->GetRelativeLocation();ResetPuzzle(); }
FString ABBSBPuzzleStation::Title() const
{
 static const TCHAR* Names[]={TEXT("Tap shutter"),TEXT("Three-beat chime"),TEXT("Ordered keys"),TEXT("Held gate"),TEXT("Hold elevator"),TEXT("Shake loose"),TEXT("Shake burst"),TEXT("Flick right"),TEXT("Flick upward"),TEXT("Dive on green"),TEXT("Stop the pendulum"),TEXT("Lower ladder"),TEXT("Pull lever"),TEXT("Connect the wire"),TEXT("Gesture phrase"),TEXT("Charge and lob"),TEXT("Shake and launch"),TEXT("Carry and mount"),TEXT("Balance delivery"),TEXT("Five-action relay")};
 return Names[FMath::Clamp(int32(Kind),0,19)];
}
FString ABBSBPuzzleStation::Hint() const
{
 static const TCHAR* Hints[]={
  TEXT("Beat: tap the brass panel once."),TEXT("Beat: three taps on consecutive green pulses. Early/late taps restart."),TEXT("Beat: tap the LEFT, CENTER, RIGHT pads in that order."),
  TEXT("Hold: keep pressed 1.25s. The gate closes when released."),TEXT("Hold: keep the lift powered to the top. Releasing pauses it."),TEXT("Shake: two recognized shakes free the stuck piece."),
  TEXT("Shake: three shakes within 2.4s to open the chest."),TEXT("Flick RIGHT: drag quickly right and release. Other directions miss."),TEXT("Flick UP: sweep upward and release to lob over the barrier."),
  TEXT("Dive: press, wait for GREEN, release gently. Do not Flick."),TEXT("Dive: release gently when the moving block crosses the center."),
  TEXT("Hold + drag DOWN 160px to lower the ladder."),TEXT("Hold + drag DOWN 80px to pull the lever."),TEXT("Hold + drag RIGHT 80px, UP 70px, RIGHT 70px. Follow the dots."),
  TEXT("One hold: Beat > Hold > Shake > gentle release (Dive)."),TEXT("Hold 1.0-1.8s, then Flick UP. Too little or too much power misses."),
  TEXT("Shake twice, then Flick RIGHT before letting go."),TEXT("Shake twice to free it. Release. Hold the pink piece, drag to the green socket, release gently."),
  TEXT("Hold the pink piece. Drag onto the green plate, steady for 0.8s, release gently."),TEXT("Beat > Hold > Shake > Flick RIGHT. Then press again and Dive on GREEN.")};
 return Hints[FMath::Clamp(int32(Kind),0,19)];
}
void ABBSBPuzzleStation::ResetPuzzle()
{
 if (AttachedBuddy) AttachedBuddy->ReleaseSurface();
 bSolved=bHeld=bLoose=bCarrying=bFlight=bHoldSeen=bShakeSeen=false;Step=0;Progress=OutputAlpha=0;Drag=FVector2D::ZeroVector;
 HeldAt=GetWorld() ? GetWorld()->GetTimeSeconds():0;LastBeatAt=BurstAt=-100;BalanceAt=-1;AttachedBuddy=nullptr;
 Payload->SetRelativeLocation(PayloadHome);Output->SetRelativeLocation(OutputHome);Output->SetRelativeRotation(FRotator::ZeroRotator);Feedback=TEXT("Ready");
 BBSBCombatVisuals::Tint(Receiver,Meadow::Brass);BBSBCombatVisuals::Tint(Payload,Meadow::Rose);SetWireVisual(0);
 Sign->SetText(FText::FromString(FString::Printf(TEXT("%02d  %s"),Number,*Title())));
}
void ABBSBPuzzleStation::Solve(const FString& Message)
{
 bSolved=true;Progress=1;Feedback=Message;BBSBCombatVisuals::Tint(Receiver,Meadow::Mint);
 Sign->SetText(FText::FromString(FString::Printf(TEXT("%02d  %s  [OK]"),Number,*Title())));
}
void ABBSBPuzzleStation::Fail(const FString& Message,bool ResetStep)
{
 Feedback=Message;if (ResetStep) { Step=0;Progress=0; }BalanceAt=-1;
}
float ABBSBPuzzleStation::CuePhase() const
{
 const double Now=GetWorld()->GetTimeSeconds();
 if (Kind==EBBSBPuzzleKind::BeatRhythm) return FMath::Frac(Now*BPM/60.0);
 return FMath::Fmod(FMath::Max(0.0,Now-HeldAt),1.8)/1.8;
}
bool ABBSBPuzzleStation::IsTimingOpen() const
{
 const double Now=GetWorld()->GetTimeSeconds();
 if (Kind==EBBSBPuzzleKind::BeatRhythm) { const double Beat=Now*BPM/60.0;return FMath::Abs(Beat-FMath::RoundToDouble(Beat))*60.0/BPM<=TimingWindow; }
 if (!bHeld) return false;
 if (Kind==EBBSBPuzzleKind::DivePendulum) return Now-HeldAt>.55 && FMath::Abs(FMath::Sin((Now-HeldAt)*PI))<.22;
 return FMath::Abs(FMath::Fmod(Now-HeldAt,1.8)-1.2)<=TimingWindow;
}
FVector ABBSBPuzzleStation::SocketPoint() const { return Socket->GetComponentLocation(); }
void ABBSBPuzzleStation::BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector Point,FVector Normal)
{
 bHeld=true;AttachedBuddy=Buddy;HeldAt=GetWorld()->GetTimeSeconds();bHoldSeen=bShakeSeen=false;Drag=FVector2D::ZeroVector;BalanceAt=-1;++Attempts;
 Pad=FMath::Clamp(FMath::RoundToInt((GetActorTransform().InverseTransformPosition(Point).Y+70)/70),0,2);
 auto* PC=UGameplayStatics::GetPlayerController(this,0);
 if (PC) { FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);CarryRight=FRotationMatrix(View).GetUnitAxis(EAxis::Y);CarryUp=FRotationMatrix(View).GetUnitAxis(EAxis::Z);float X=0,Y=0;PC->GetMousePosition(X,Y);PointerOrigin=FVector2D(X,Y); }
 if(Meadow::IsCarry(Kind))
 {
  CarryRight=FVector::VectorPlaneProject(CarryRight,GetActorForwardVector()).GetSafeNormal();
  CarryUp=FVector::VectorPlaneProject(CarryUp,GetActorForwardVector()).GetSafeNormal();
 }
 bCarrying=!bSolved && Meadow::IsCarry(Kind) && Buddy && Buddy->SurfaceComponent==Payload && (bLoose || Kind==EBBSBPuzzleKind::Balance);
 if (Kind==EBBSBPuzzleKind::Sequence && !bSolved) Step=0;
 if (Kind==EBBSBPuzzleKind::ShakeFlick && !bSolved) Step=0;
 if (Kind==EBBSBPuzzleKind::Wire && !bSolved) { Step=0;SetWireVisual(0); }
 Feedback=bCarrying ? TEXT("Carry: drag toward the green socket, then Dive."):TEXT("Attached: keep holding to operate.");
}
void ABBSBPuzzleStation::BunniarDragged_Implementation(ABBSBBunniarCompanion* Buddy,FVector2D Delta,FVector2D Total)
{
 Drag=Total;if (bSolved || !bHeld) return;
 if (bCarrying)
 {
  FVector Position=Payload->GetComponentLocation()+(CarryRight*Delta.X-CarryUp*Delta.Y)*1.2f;
  FVector Local=GetActorTransform().InverseTransformPosition(Position);Local.X=FMath::Clamp(Local.X,-150.,300.);Local.Y=FMath::Clamp(Local.Y,-300.,350.);Local.Z=FMath::Clamp(Local.Z,55.,360.);
  Payload->SetWorldLocation(GetActorTransform().TransformPosition(Local),true);
  return;
 }
 if (!bHoldSeen) return;
 if (Kind==EBBSBPuzzleKind::Ladder || Kind==EBBSBPuzzleKind::Lever)
 {
  const float Required=Kind==EBBSBPuzzleKind::Lever ? DragDistance*.5f:DragDistance;
  Progress=FMath::Clamp(float(Total.Y)/Required,0.f,1.f);
  if (Progress>=1) Solve(Kind==EBBSBPuzzleKind::Ladder ? TEXT("Ladder lowered."):TEXT("Lever pulled; the shutter is open."));
 }
 if (Kind==EBBSBPuzzleKind::Wire && Step<3)
 {
  const FVector2D Target=Meadow::WirePoints[Step];
  if ((Total-Target).Size()<27) { ++Step;Progress=Step/3.f;SetWireVisual(Step);Feedback=FString::Printf(TEXT("Wire %d / 3"),Step);if(Step==3) Solve(TEXT("Circuit connected.")); }
  else
  {
   const FVector2D Start=Step==0 ? FVector2D::ZeroVector:Meadow::WirePoints[Step-1];
   const FVector2D Segment=Target-Start;const float T=FMath::Clamp(float(FVector2D::DotProduct(Total-Start,Segment)/Segment.SizeSquared()),0.f,1.f);
   if ((Total-(Start+Segment*T)).Size()>55) { Step=0;Progress=0;SetWireVisual(0);Feedback=TEXT("Wire left its guide. Return to the first dot."); }
  }
 }
}
void ABBSBPuzzleStation::SetWireVisual(int32 Segments)
{
 if (Kind!=EBBSBPuzzleKind::Wire) return;
 for (UStaticMeshComponent* C:Parts) if (C && C->GetName().StartsWith(TEXT("Wire")))
 { const int32 Index=FCString::Atoi(*C->GetName().Mid(4));C->SetVisibility(Index<Segments); }
}
void ABBSBPuzzleStation::Launch(FVector2D Direction,bool Success)
{
 bFlight=true;bFlightSuccess=Success;FlightAt=GetWorld()->GetTimeSeconds();FlightFrom=Payload->GetRelativeLocation();
 const FVector WorldDirection=CarryRight*Direction.X-CarryUp*Direction.Y;
 FlightTo=Success ? Socket->GetRelativeLocation():FlightFrom+GetActorTransform().InverseTransformVectorNoScale(WorldDirection)*300;
}
void ABBSBPuzzleStation::BunniarGesture_Implementation(ABBSBBunniarCompanion* Buddy,EBBSBFieldGesture Gesture,FVector2D Direction,float HeldSeconds)
{
 if (!bHeld || bSolved) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if (Gesture==EBBSBFieldGesture::Beat)
 {
  if (Kind==EBBSBPuzzleKind::BeatLatch) Solve(TEXT("Beat opened the shutter."));
  else if (Kind==EBBSBPuzzleKind::BeatRhythm)
  {
   const double Interval=60.0/BPM;
   if (!IsTimingOpen()) Fail(TEXT("Early or late. Tap on the next green pulse."));
   else if (Step>0 && FMath::Abs(Now-LastBeatAt-Interval)>TimingWindow*2) { Step=1;Progress=1.f/3;Feedback=TEXT("New rhythm: 1 / 3"); }
   else { ++Step;Progress=Step/3.f;Feedback=FString::Printf(TEXT("On beat: %d / 3"),Step);if(Step>=3) Solve(TEXT("Three consecutive beats.")); }
   LastBeatAt=Now;
  }
  else if (Kind==EBBSBPuzzleKind::BeatOrder)
  {
   if (Pad==Step) { ++Step;Progress=Step/3.f;Feedback=FString::Printf(TEXT("Key %d / 3"),Step);if(Step==3) Solve(TEXT("Left > center > right.")); }
   else Fail(TEXT("Wrong key. Start with the left pad."));
  }
  else if (Kind==EBBSBPuzzleKind::Sequence) { Step=1;Progress=.25f;Feedback=TEXT("Beat done. Keep holding."); }
  else if (Kind==EBBSBPuzzleKind::Relay && Step!=4) { Step=1;Progress=.2f;Feedback=TEXT("Beat done. Hold, then Shake."); }
  return;
 }
 if (Gesture==EBBSBFieldGesture::Hold)
 {
  bHoldSeen=true;
  if (Kind==EBBSBPuzzleKind::Sequence && Step==1) { Step=2;Progress=.5f;Feedback=TEXT("Hold done. Shake next."); }
  if (Kind==EBBSBPuzzleKind::Relay && Step==1) { Step=2;Progress=.4f;Feedback=TEXT("Hold done. Shake next."); }
  return;
 }
 if (Gesture==EBBSBFieldGesture::Shake)
 {
  bShakeSeen=true;
  if (Kind==EBBSBPuzzleKind::ShakeLoose || (Kind==EBBSBPuzzleKind::CarrySocket && !bLoose))
  {
   ++Step;Progress=Step*.5f;
   if(Step>=2) { bLoose=true;Payload->SetRelativeLocation(PayloadHome-FVector(0,0,35));BBSBCombatVisuals::Tint(Payload,Meadow::Cream);if(Kind==EBBSBPuzzleKind::ShakeLoose) Solve(TEXT("Piece detached."));else { Progress=.35f;Feedback=TEXT("Loose. Release, then hold the detached piece."); } }
  }
  if (Kind==EBBSBPuzzleKind::ShakeBurst)
  {
   if (Now-BurstAt>2.4) { Step=0;BurstAt=Now; }
   ++Step;Progress=Step/3.f;Feedback=FString::Printf(TEXT("Shake burst %d / 3"),Step);if(Step>=3) Solve(TEXT("Chest unlocked."));
  }
  if (Kind==EBBSBPuzzleKind::Sequence && Step==2) { Step=3;Progress=.75f;Feedback=TEXT("Shake done. Stop moving and release gently."); }
  if (Kind==EBBSBPuzzleKind::ShakeFlick) { Step=FMath::Min(2,Step+1);Progress=Step*.35f;Feedback=Step>=2 ? TEXT("Ready: Flick RIGHT."):TEXT("One more Shake."); }
  if (Kind==EBBSBPuzzleKind::Relay && Step==2) { Step=3;Progress=.6f;Feedback=TEXT("Shake done. Flick RIGHT."); }
  return;
 }
 const bool Dive=Gesture==EBBSBFieldGesture::Dive;
 const bool Right=Direction.X>.75f;
 const bool Up=Direction.Y<-.75f;
 if (Kind==EBBSBPuzzleKind::FlickRight || Kind==EBBSBPuzzleKind::FlickUp || Kind==EBBSBPuzzleKind::ChargeFlick || Kind==EBBSBPuzzleKind::ShakeFlick)
 {
  const bool Valid=!Dive && (Kind==EBBSBPuzzleKind::FlickRight || Kind==EBBSBPuzzleKind::ShakeFlick ? Right:Up)
   && (Kind!=EBBSBPuzzleKind::ChargeFlick || (HeldSeconds>=1.f && HeldSeconds<=1.8f)) && (Kind!=EBBSBPuzzleKind::ShakeFlick || Step>=2);
  Launch(Direction,Valid);
  if(!Valid) Fail(Dive ? TEXT("Gentle release was Dive. Sweep quickly to Flick."):Kind==EBBSBPuzzleKind::ChargeFlick ? TEXT("Miss: hold 1.0-1.8s and Flick UP."):TEXT("Miss: check direction and preparation."));
 }
 else if (Kind==EBBSBPuzzleKind::DiveCue || Kind==EBBSBPuzzleKind::DivePendulum)
 { if (Dive && IsTimingOpen()) Solve(TEXT("Dive landed inside the timing window."));else Fail(Dive ? TEXT("Dive missed the green window. Press again to retry."):TEXT("Flick detected. Release gently for Dive.")); }
 else if (Kind==EBBSBPuzzleKind::Sequence)
 { if (Dive && Step==3) Solve(TEXT("Beat > Hold > Shake > Dive complete."));else Fail(TEXT("Phrase broken. Start again: Beat > Hold > Shake > Dive.")); }
 else if (Meadow::IsCarry(Kind) && bCarrying)
 {
  if (Dive && FVector::Dist(Payload->GetComponentLocation(),SocketPoint())<=75 && (Kind!=EBBSBPuzzleKind::Balance || Progress>=.99f))
  { Payload->SetWorldLocation(SocketPoint());Solve(TEXT("Piece mounted on the destination.")); }
  else Fail(Kind==EBBSBPuzzleKind::Balance ? TEXT("Keep it on the plate for 0.8s, then Dive."):TEXT("Release gently while the piece is inside the socket."),false);
 }
 else if (Kind==EBBSBPuzzleKind::Relay)
 {
  if (Step==3 && !Dive && Right) { Step=4;Progress=.8f;Launch(Direction,false);Feedback=TEXT("Launch complete. Press again, then Dive on GREEN."); }
  else if (Step==4 && Dive && IsTimingOpen()) Solve(TEXT("All five actions linked."));
  else Fail(Step==4 ? TEXT("Final Dive missed. Press again and wait for GREEN."):TEXT("Relay reset. Beat > Hold > Shake > Flick RIGHT."),Step!=4);
 }
}
void ABBSBPuzzleStation::BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy)
{
 bHeld=bCarrying=false;AttachedBuddy=nullptr;
 if (!bSolved && (Kind==EBBSBPuzzleKind::Wire || Kind==EBBSBPuzzleKind::Sequence)) { Step=0;Progress=0;SetWireVisual(0); }
 if (Kind==EBBSBPuzzleKind::HoldGate && !bSolved) Progress=0;
}
void ABBSBPuzzleStation::Tick(float DT)
{
 Super::Tick(DT);const double Now=GetWorld()->GetTimeSeconds();
 if (Kind==EBBSBPuzzleKind::HoldGate && bHeld && bHoldSeen && !bSolved) { Progress=FMath::Clamp(float(Now-HeldAt)/1.25f,0.f,1.f);if(Progress>=1) Solve(TEXT("Gate held open. Releasing closes it.")); }
 if (Kind==EBBSBPuzzleKind::HoldLift && bHeld && bHoldSeen && !bSolved) { Progress=FMath::Min(1.f,Progress+DT/2.f);if(Progress>=1) Solve(TEXT("Lift reached the landing.")); }
 if (Kind==EBBSBPuzzleKind::Balance && bCarrying && !bSolved)
 {
  if (FVector::Dist(Payload->GetComponentLocation(),SocketPoint())<=65) { if(BalanceAt<0) BalanceAt=Now;Progress=FMath::Clamp(float((Now-BalanceAt)/.8),0.f,1.f);Feedback=Progress>=1 ? TEXT("Stable. Release gently now."):TEXT("Steady on the plate..."); }
  else { BalanceAt=-1;Progress=0; }
 }
 if (Kind==EBBSBPuzzleKind::ShakeBurst && !bSolved && Step>0 && Now-BurstAt>2.4) Fail(TEXT("Burst expired. Three Shakes in 2.4s."));
 float Target=bSolved ? 1.f:0.f;
 if (Kind==EBBSBPuzzleKind::HoldGate) Target=bHeld && bHoldSeen ? FMath::Clamp(float(Now-HeldAt)/1.25f,0.f,1.f):0.f;
 if (Kind==EBBSBPuzzleKind::HoldLift || Kind==EBBSBPuzzleKind::Ladder || Kind==EBBSBPuzzleKind::Lever) Target=Progress;
 OutputAlpha=FMath::FInterpTo(OutputAlpha,Target,DT,4.f);
 if (Kind==EBBSBPuzzleKind::Ladder) Output->SetRelativeLocation(OutputHome-FVector(0,0,200*OutputAlpha));
 else if (Kind==EBBSBPuzzleKind::Lever) Output->SetRelativeRotation(FRotator(OutputAlpha*65,0,0));
 else if (Kind==EBBSBPuzzleKind::HoldLift) Output->SetRelativeLocation(OutputHome+FVector(0,0,250*OutputAlpha));
 else if (Kind==EBBSBPuzzleKind::DivePendulum && !bSolved) Output->SetRelativeLocation(OutputHome+FVector(0,140*FMath::Sin((Now-HeldAt)*PI),0));
 else Output->SetRelativeLocation(OutputHome+FVector(0,0,280*OutputAlpha));
 if (bFlight)
 {
  const float T=FMath::Clamp(float((Now-FlightAt)/1.0),0.f,1.f);FVector Position=FMath::Lerp(FlightFrom,FlightTo,T);Position.Z+=FMath::Sin(T*PI)*180;
  Payload->SetRelativeLocation(Position);
  if(T>=1) { bFlight=false;if(bFlightSuccess) Solve(TEXT("Flick reached the destination."));else Payload->SetRelativeLocation(PayloadHome); }
 }
 const bool Timed=Kind==EBBSBPuzzleKind::BeatRhythm || Kind==EBBSBPuzzleKind::DiveCue || Kind==EBBSBPuzzleKind::DivePendulum || (Kind==EBBSBPuzzleKind::Relay && Step==4);
 BBSBCombatVisuals::Tint(Indicator,bSolved || (Timed && IsTimingOpen()) ? Meadow::Mint:Timed ? Meadow::Rose:Meadow::Blue);
 if (auto* Cam=UGameplayStatics::GetPlayerCameraManager(this,0)) Sign->SetWorldRotation((Cam->GetCameraLocation()-Sign->GetComponentLocation()).Rotation());
}

ABBSBPuzzleField::ABBSBPuzzleField() { PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FieldOrigin"))); }
void ABBSBPuzzleField::BeginPlay()
{
 Super::BeginPlay();
 for (TActorIterator<ABBSBPuzzleStation> It(GetWorld());It;++It) Stations.Add(*It);
 Stations.Sort([](const ABBSBPuzzleStation& A,const ABBSBPuzzleStation& B){ return A.Number<B.Number; });
 BindPlayer();
}
void ABBSBPuzzleField::BindPlayer()
{
 if(Controls) return;
 Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if (Player)
 {
  auto* PC=Cast<APlayerController>(Player->GetController());if(!PC || !Player->InputComponent) return;
  SafeStart=Player->GetActorLocation();Player->FieldActions->bShowPracticeHUD=false;
  Controls=NewObject<UInputComponent>(this,TEXT("FieldPuzzleKeys"));Controls->RegisterComponent();Controls->Priority=5;
  Controls->BindKey(EKeys::R,IE_Pressed,this,&ABBSBPuzzleField::ResetNearest);Controls->BindKey(EKeys::BackSpace,IE_Pressed,this,&ABBSBPuzzleField::ResetAll);
  if(PC) { PC->PushInputComponent(Controls);PC->ClientSetHUD(ABBSBPuzzleFieldHUD::StaticClass()); }
 }
}
void ABBSBPuzzleField::ReleaseBoundary()
{
 if (bOwnsMoveIgnore && Player) if (auto* PC=Cast<APlayerController>(Player->GetController())) PC->SetIgnoreMoveInput(false);
 bOwnsMoveIgnore=bReturning=false;
}
void ABBSBPuzzleField::EndPlay(const EEndPlayReason::Type Reason)
{
 ReleaseBoundary();if (Player) if(auto* PC=Cast<APlayerController>(Player->GetController())) if(Controls) PC->PopInputComponent(Controls);
 if(RhythmAudio) RhythmAudio->DestroyComponent();
 Super::EndPlay(Reason);
}
void ABBSBPuzzleField::ResetNearest() { if(Focus) Focus->ResetPuzzle(); }
void ABBSBPuzzleField::ResetAll() { for(ABBSBPuzzleStation* Station:Stations) if(Station) Station->ResetPuzzle(); }
void ABBSBPuzzleField::Tick(float DT)
{
 Super::Tick(DT);if(!Controls) BindPlayer();if (!Player) return;
 SolvedCount=0;Focus=nullptr;float Nearest=850.f;
 for (ABBSBPuzzleStation* Station:Stations) if(Station)
 {
  if(Station->bSolved) ++SolvedCount;
  const float Distance=FVector::Dist2D(Player->GetActorLocation(),Station->GetActorLocation());if(Distance<Nearest) { Nearest=Distance;Focus=Station; }
 }
 if (auto* Buddy=Cast<ABBSBBunniarCompanion>(Player->FieldActions->FieldEquipment))
  if (Buddy->bSurfaceAttached && Buddy->SurfaceComponent) if(auto* Station=Cast<ABBSBPuzzleStation>(Buddy->SurfaceComponent->GetOwner())) Focus=Station;
 if(Focus && Focus->Kind==EBBSBPuzzleKind::BeatRhythm && Player->FieldActions->FieldEquipment && !Player->FieldActions->bEquipmentWheelOpen)
 {
  const int32 Beat=FMath::FloorToInt(GetWorld()->GetTimeSeconds()*Focus->BPM/60.f);
  if(RhythmFocus.Get()!=Focus) { RhythmFocus=Focus;LastRhythmBeat=Beat; }
  else if(Beat!=LastRhythmBeat)
  {
   LastRhythmBeat=Beat;
   if(!RhythmAudio)
   {
    RhythmAudio=NewObject<UAudioComponent>(this,TEXT("PuzzleRhythmCue"));RhythmAudio->bAutoActivate=false;RhythmAudio->bIsUISound=true;RhythmAudio->RegisterComponent();
    RhythmClick=NewObject<USoundWaveProcedural>(this);RhythmClick->SetSampleRate(24000);RhythmClick->NumChannels=1;RhythmClick->Duration=INDEFINITELY_LOOPING_DURATION;RhythmAudio->SetSound(RhythmClick);
   }
   TArray<int16> Samples;Samples.SetNumUninitialized(1200);
   for(int32 I=0;I<Samples.Num();++I) Samples[I]=int16(1200.f*FMath::Sin(2*PI*(Beat%4==0 ? 880.f:660.f)*I/24000.f)*FMath::Exp(-I/230.f));
   RhythmClick->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Samples.Num()*sizeof(int16));if(!RhythmAudio->IsPlaying()) RhythmAudio->Play();
  }
 }
 else { RhythmFocus.Reset();if(RhythmAudio) RhythmAudio->Stop();if(RhythmClick) RhythmClick->ResetAudio(); }
 FVector Position=Player->GetActorLocation()-GetActorLocation();
 const double Radius=FMath::Sqrt(FMath::Square(Position.X/FMath::Max(100.,BoundaryRadii.X))+FMath::Square(Position.Y/FMath::Max(100.,BoundaryRadii.Y)));
 if (Radius>1 && !bReturning)
 {
  bReturning=true;++ReturnCount;
  if (auto* Buddy=Cast<ABBSBBunniarCompanion>(Player->FieldActions->FieldEquipment)) Buddy->ReleaseSurface();
  Player->FieldActions->CancelEquipmentSelection();
  if(auto* PC=Cast<APlayerController>(Player->GetController())) { PC->SetIgnoreMoveInput(true);bOwnsMoveIgnore=true; }
 }
 if (bReturning)
 {
  if(Radius<ReturnHysteresis || Player->Combat->bDead) ReleaseBoundary();
  else
  {
   FVector Direction(-Position.X/FMath::Square(BoundaryRadii.X),-Position.Y/FMath::Square(BoundaryRadii.Y),0);Direction.Normalize();
   Player->ConsumeMovementInputVector();Player->AddMovementInput(Direction,1.f,true);
  }
 }
 if (Position.Z<-1400)
 {
  ReleaseBoundary();Player->GetCharacterMovement()->StopMovementImmediately();Player->SetActorLocation(SafeStart,false,nullptr,ETeleportType::TeleportPhysics);
 }
}

void ABBSBPuzzleFieldHUD::DrawHUD()
{
 Super::DrawHUD();auto* Player=Cast<ABBSBCharacter>(GetOwningPawn());auto* Field=Meadow::Field(GetWorld());
 if(Player && Player->Interaction && Player->Interaction->bDialogueActive) { Player->Interaction->DrawDialogue(this,Canvas);return; }
 if(!Canvas || !Player || !Field) return;auto* Actions=Player->FieldActions.Get();
 if(Actions->bEquipmentWheelOpen) { Actions->DrawEquipmentWheel(this,Canvas);return; }
 const float S=FMath::Clamp(Canvas->ClipX/1440.f,.65f,1.3f),W=Canvas->ClipX,H=Canvas->ClipY,CX=W*.5f,CY=H*.5f;
 const FLinearColor Ink(.94f,.90f,.78f),Green(.57f,.85f,.66f),Rose(.95f,.58f,.65f),Paper(.13f,.17f,.16f,.85f);
 DrawLine(CX-7*S,CY,CX-2*S,CY,Ink,1.5f);DrawLine(CX+2*S,CY,CX+7*S,CY,Ink,1.5f);DrawLine(CX,CY-7*S,CX,CY-2*S,Ink,1.5f);DrawLine(CX,CY+2*S,CX,CY+7*S,Ink,1.5f);
 DrawText(FString::Printf(TEXT("BUNNIAR MEADOW   %d / 20"),Field->SolvedCount),Ink,26*S,22*S,GEngine->GetMediumFont(),S);
 DrawText(Actions->FieldEquipment ? TEXT("Bunniar  |  Tab: sword"):TEXT("Sword  |  Tab: Bunniar"),Ink,W-270*S,22*S,GEngine->GetSmallFont(),S);
 DrawText(TEXT("WASD Walk   Shift Run   Space Dodge   Tab Switch / Hold for wheel"),Ink,26*S,H-34*S,GEngine->GetSmallFont(),S);
 if(Field->bReturning) { DrawRect(Paper,CX-230*S,105*S,460*S,42*S);DrawText(TEXT("The trail ends here. Returning to the meadow..."),Ink,CX-215*S,116*S,GEngine->GetSmallFont(),S); }
 if(auto* Station=Field->Focus.Get())
 {
  const float X=CX-360*S,Y=58*S;
  DrawRect(Paper,X,Y,720*S,114*S);
  DrawText(FString::Printf(TEXT("%02d  %s   %s   [R] retry"),Station->Number,*Station->Title(),Station->bSolved ? TEXT("COMPLETE"):TEXT("")),Station->bSolved ? Green:Ink,X+16*S,Y+10*S,GEngine->GetMediumFont(),S);
  DrawText(Station->Hint(),Ink,X+16*S,Y+40*S,GEngine->GetSmallFont(),.94f*S);
  DrawText(Station->Feedback,Station->IsTimingOpen() ? Green:Rose,X+16*S,Y+64*S,GEngine->GetSmallFont(),S);
  DrawRect(FLinearColor(.31f,.34f,.28f),X+16*S,Y+93*S,688*S,7*S);DrawRect(Green,X+16*S,Y+93*S,688*S*FMath::Clamp(Station->Progress,0.f,1.f),7*S);
  const bool Timed=Station->Kind==EBBSBPuzzleKind::BeatRhythm || Station->Kind==EBBSBPuzzleKind::DiveCue || Station->Kind==EBBSBPuzzleKind::DivePendulum || (Station->Kind==EBBSBPuzzleKind::Relay && Station->Step==4);
  if(Timed)
  {
   DrawRect(Paper,CX-130*S,CY+62*S,260*S,35*S);
   DrawText(Station->IsTimingOpen() ? TEXT("GREEN  -  NOW"):TEXT("Wait for green..."),Station->IsTimingOpen() ? Green:Ink,CX-100*S,CY+70*S,GEngine->GetSmallFont(),S);
   DrawRect(Ink,CX-120*S+240*S*Station->CuePhase(),CY+94*S,3*S,12*S);
  }
  if(Station->Kind==EBBSBPuzzleKind::Wire && Station->bHeld)
  {
   FVector2D Previous=Station->PointerOrigin;
   for(int I=0;I<3;++I) { const FVector2D Point=Station->PointerOrigin+Meadow::WirePoints[I];DrawLine(Previous.X,Previous.Y,Point.X,Point.Y,I<Station->Step ? Green:Ink,3*S);DrawRect(I<Station->Step ? Green:Rose,Point.X-5*S,Point.Y-5*S,10*S,10*S);Previous=Point; }
  }
 }
 else DrawText(TEXT("Follow the numbered boards. Hold Bunniar against a surface; release to detach."),Ink,CX-310*S,62*S,GEngine->GetSmallFont(),S);
}
