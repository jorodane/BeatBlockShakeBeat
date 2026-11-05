#include "Character/BBSBRespawn.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "Character/BBSBTraversal.h"
#include "Character/BBSBWorldMap.h"
#include "EngineUtils.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Field/BBSBFieldActions.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Interaction/BBSBInteraction.h"
#include "Animation/AnimSequence.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "Components/SizeBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/TextRenderComponent.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

ABBSBRespawnPoint::ABBSBRespawnPoint()
{
 PrimaryActorTick.bCanEverTick=true;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RespawnOrigin")));
 Base=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pedestal"));Base->SetupAttachment(RootComponent);
 Base->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
 Base->SetRelativeScale3D(FVector(.85f,.85f,.16f));Base->SetRelativeLocation(FVector(0,0,8));Base->SetCollisionProfileName(TEXT("BlockAll"));
 Beacon=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));Beacon->SetupAttachment(RootComponent);
 Beacon->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
 Beacon->SetRelativeLocation(FVector(0,0,65));Beacon->SetRelativeScale3D(FVector(.38f));Beacon->SetCollisionProfileName(TEXT("BlockAll"));
 Base->SetCanEverAffectNavigation(false);Beacon->SetCanEverAffectNavigation(false);
 LandingPoint=CreateDefaultSubobject<USceneComponent>(TEXT("LandingPoint"));LandingPoint->SetupAttachment(RootComponent);LandingPoint->SetRelativeLocation(FVector(130,0,0));
 Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("PointName"));Label->SetupAttachment(RootComponent);Label->SetRelativeLocation(FVector(0,0,105));Label->SetWorldSize(16);Label->SetHorizontalAlignment(EHTA_Center);Label->SetTextRenderColor(FColor(244,221,189));
}
void ABBSBRespawnPoint::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);Label->SetText(PointName);
 BBSBCombatVisuals::Tint(Base,FLinearColor(.25f,.22f,.32f));BBSBCombatVisuals::Tint(Beacon,FLinearColor(.78f,.65f,.48f));
}
void ABBSBRespawnPoint::BeginPlay()
{
 Super::BeginPlay();
}
bool ABBSBRespawnPoint::IsActivePoint() const
{
 const auto* P=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));return P && P->Respawn && P->Respawn->ActivePoint==this;
}
void ABBSBRespawnPoint::Tick(float DT)
{
 Super::Tick(DT);const bool Active=IsActivePoint();
 if(Active!=bWasActive) { bWasActive=Active;BBSBCombatVisuals::Tint(Beacon,Active ? FLinearColor(.40f,.92f,.76f):FLinearColor(.78f,.65f,.48f)); }
 Beacon->SetRelativeLocation(FVector(0,0,65+FMath::Sin(GetWorld()->GetTimeSeconds()*2)*5));
 if(auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))Label->SetWorldRotation((Camera->GetCameraLocation()-Label->GetComponentLocation()).Rotation());
}
FTransform ABBSBRespawnPoint::LandingTransform(const ABBSBCharacter* Player) const
{
 FVector Position=LandingPoint->GetComponentLocation();FHitResult Hit;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBCheckpointFloor),false,this);Query.AddIgnoredActor(Player);
 if(GetWorld()->LineTraceSingleByObjectType(Hit,Position+FVector::UpVector*180,Position-FVector::UpVector*900,BBSBCombatVisuals::GeometryTypes(),Query))Position=Hit.ImpactPoint;
 const float HalfHeight=Player ? Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():96.f;
 return FTransform(FRotator(0,LandingPoint->GetComponentRotation().Yaw,0),Position+FVector::UpVector*(HalfHeight+3));
}
bool ABBSBRespawnPoint::ActivateFor(ABBSBCharacter* Player)
{ return Player && Player->Respawn && Player->Respawn->SetCheckpoint(this); }
FText ABBSBRespawnPoint::BunniarInteractionText_Implementation() const
{ return FText::FromString(IsActivePoint() ? TEXT("현재 리스폰 지점"):TEXT("리스폰 지점으로 설정")); }
FString ABBSBRespawnPoint::TravelId() const
{ return UGameplayStatics::GetCurrentLevelName(this,true)+TEXT(":")+(PointId.IsNone() ? GetName():PointId.ToString()); }
bool ABBSBRespawnPoint::BunniarQuickUse_Implementation(ABBSBBunniarCompanion* Bunniar)
{
 ActivateFor(Cast<ABBSBCharacter>(Bunniar ? Bunniar->GetOwner():nullptr));
 return true; // A beacon consumes F even when it cannot be registered.
}
void ABBSBRespawnPoint::BunniarAttached_Implementation(ABBSBBunniarCompanion* Bunniar,FVector Point,FVector Normal)
{
 if(auto* Player=Cast<ABBSBCharacter>(Bunniar ? Bunniar->GetOwner():nullptr))ActivateFor(Player);
 // Explicit placement follows the normal attachment lifecycle; F never attaches here.
}

TSharedRef<SWidget> UBBSBRespawnWidget::RebuildWidget()
{
 SetIsFocusable(true);
 if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this,TEXT("RespawnTree"));
 if(!WidgetTree->RootWidget)
 {
  auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("Screen"));WidgetTree->RootWidget=Root;
  auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(FLinearColor(.025f,.02f,.04f,.78f));
  auto* ShadeSlot=Root->AddChildToCanvas(Shade);ShadeSlot->SetAnchors(FAnchors(0,0,1,1));ShadeSlot->SetOffsets(FMargin(0));
  auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(440);
  auto* PanelSlot=Root->AddChildToCanvas(Size);PanelSlot->SetAnchors(FAnchors(.5f));PanelSlot->SetAlignment(FVector2D(.5f));PanelSlot->SetAutoSize(true);
  auto* Panel=WidgetTree->ConstructWidget<UBorder>();Panel->SetBrushColor(FLinearColor(.14f,.11f,.19f,.97f));Panel->SetPadding(FMargin(32));Size->AddChild(Panel);
  auto* Rows=WidgetTree->ConstructWidget<UVerticalBox>();Panel->AddChild(Rows);
  auto* Title=WidgetTree->ConstructWidget<UTextBlock>();Title->SetText(FText::FromString(TEXT("쓰러졌어요")));Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold",30));Title->SetJustification(ETextJustify::Center);Title->SetColorAndOpacity(FLinearColor(.98f,.89f,.78f));Rows->AddChildToVerticalBox(Title)->SetPadding(FMargin(0,0,0,20));
  Message=WidgetTree->ConstructWidget<UTextBlock>();Message->SetText(FText::FromString(TEXT("버니아가 곁에서 기다리고 있어요.\n지정한 지점에서 다시 시작할까요?")));Message->SetFont(FCoreStyle::GetDefaultFontStyle("Regular",16));Message->SetJustification(ETextJustify::Center);Message->SetAutoWrapText(true);Message->SetColorAndOpacity(FLinearColor(.89f,.84f,.91f));Rows->AddChildToVerticalBox(Message)->SetPadding(FMargin(0,0,0,24));
  auto MakeButton=[&](const TCHAR* Text,const FLinearColor& Color)
  {
   auto* Button=WidgetTree->ConstructWidget<UButton>();FButtonStyle Style;
   Style.SetNormal(FSlateColorBrush(Color));Style.SetHovered(FSlateColorBrush(Color*1.2f));Style.SetPressed(FSlateColorBrush(Color*.8f));Button->SetStyle(Style);
   auto* Caption=WidgetTree->ConstructWidget<UTextBlock>();Caption->SetText(FText::FromString(Text));Caption->SetJustification(ETextJustify::Center);Caption->SetFont(FCoreStyle::GetDefaultFontStyle("Bold",18));Caption->SetColorAndOpacity(FLinearColor(.98f,.95f,.9f));
   auto* ButtonSlot=Cast<UButtonSlot>(Button->AddChild(Caption));ButtonSlot->SetPadding(FMargin(18,13));Rows->AddChildToVerticalBox(Button)->SetPadding(FMargin(0,4));return Button;
  };
  ConfirmButton=MakeButton(TEXT("리스폰하기"),FLinearColor(.31f,.48f,.44f));ConfirmButton->OnClicked.AddDynamic(this,&UBBSBRespawnWidget::Confirm);
 }
 return Super::RebuildWidget();
}
void UBBSBRespawnWidget::FocusConfirm() { if(ConfirmButton)ConfirmButton->SetKeyboardFocus(); }
void UBBSBRespawnWidget::Confirm() { if(Respawn)Respawn->RequestRespawn(); }

FReply UBBSBRespawnWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
 const auto Key=Event.GetKey();
 if(Key==EKeys::Enter || Key==EKeys::SpaceBar || Key==EKeys::Gamepad_FaceButton_Bottom) { Confirm();return FReply::Handled(); }
 if(Key==EKeys::Escape || Key==EKeys::Gamepad_FaceButton_Right)return FReply::Handled();
 return Super::NativeOnKeyDown(Geometry,Event);
}

UBBSBRespawnComponent::UBBSBRespawnComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
 DeathAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BBSB/AnimationAuthoring/Character/A_Player_Death.A_Player_Death")));
 CarryAnimation=TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BBSB/AnimationAuthoring/Character/A_Player_Fall_Loop.A_Player_Fall_Loop")));
 PromptClass=UBBSBRespawnWidget::StaticClass();
}
void UBBSBRespawnComponent::BeginPlay() { Super::BeginPlay();Player=Cast<ABBSBCharacter>(GetOwner()); }
bool UBBSBRespawnComponent::SetCheckpoint(ABBSBRespawnPoint* Point)
{
 if(!Player || !Player->IsLocallyControlled() || IsSequenceActive() || !IsValid(Point) || Player->Combat->bDead || FVector::DistSquared(Player->GetActorLocation(),Point->GetActorLocation())>FMath::Square(Point->ActivationRange))return false;
 ActivePoint=Point;PendingPointId=Point->TravelId();DiscoveredPoints.Add(PendingPointId);Player->Hunting->NotifyGrowthChanged();Player->Combat->SetFeedback(TEXT("리스폰 지점 설정 · ")+Point->PointName.ToString());return true;
}
bool UBBSBRespawnComponent::TryActivateFromAim()
{
 if(!Player || IsSequenceActive() || Player->Combat->bDead || Player->Hunting->bInventoryOpen || Player->FieldActions->bEquipmentInputBlocked || Player->FieldActions->bEquipmentWheelOpen)return false;
 FHitResult Hit;if(!Player->FieldActions->TraceEquipmentAim(Hit))return false;
 if(auto* Point=Cast<ABBSBRespawnPoint>(Hit.GetActor())) { Point->ActivateFor(Player);return true; }
 return false;
}
FTransform UBBSBRespawnComponent::RespawnTransform() const
{ return IsValid(ActivePoint) ? ActivePoint->LandingTransform(Player):InitialSpawn; }
bool UBBSBRespawnComponent::IsDamageProtected() const
{ return IsSequenceActive() || (GetWorld() && GetWorld()->GetTimeSeconds()<ProtectedUntil); }
void UBBSBRespawnComponent::SavePresentation()
{
 auto* Mesh=Player->GetMesh();SavedAnimationMode=uint8(Mesh->GetAnimationMode());SavedAnimClass=Mesh->GetAnimClass();bOriginalPostProcess=Mesh->GetDisablePostProcessBlueprint();
 bOriginalEquipmentBlock=Player->FieldActions->bEquipmentInputBlocked;SavedGravityScale=Player->GetCharacterMovement()->GravityScale;
 if(auto* Boom=Player->GetCameraBoom()) { bSavedCameraLag=true;bOriginalCameraLag=Boom->bEnableCameraLag;bOriginalCameraRotationLag=Boom->bEnableCameraRotationLag;Boom->bEnableCameraLag=Boom->bEnableCameraRotationLag=false; }
 if(auto* PC=Cast<APlayerController>(Player->GetController()))
 { bOriginalCursor=PC->bShowMouseCursor;PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bOwnMoveLock=bOwnLookLock=true;PC->RotationInput=FRotator::ZeroRotator;
   if(auto* HUD=PC->GetHUD()) { bOriginalHUD=HUD->bShowHUD;HUD->bShowHUD=false; } }
}
void UBBSBRespawnComponent::BeginDeath()
{
 if(!Player || IsSequenceActive())return;
 if(!bInitialSpawnSaved) { InitialSpawn=Player->GetActorTransform();bInitialSpawnSaved=true; }
 Player->WorldMap->CloseMap();Player->Hunting->StopCollection();Player->Hunting->CloseInventory();Player->FieldActions->CancelEquipmentSelection();
 SavePresentation();Phase=EBBSBRespawnPhase::Downed;StartedAt=GetWorld()->GetTimeSeconds();bTravelFromDeath=true;
 auto* Actions=Player->FieldActions.Get();Player->EquipmentGrowth->CancelConcerto();Player->Combat->ReleaseTargetLock();
 Player->Traversal->CancelWireRecall();Player->Traversal->DropParkour();Player->Traversal->EndGlide();Player->Traversal->EndAirWeaponHang();Player->StopJumping();
 TArray<AActor*> Visuals;Actions->FreezeDeathEquipment(Visuals);Drops.Reset();
 for(auto* Visual:Visuals)Drops.Add({Visual,Player->GetVelocity()*.2f,false});
 Actions->InterruptAction(EBBSBActionState::Dead,100000.f);Actions->bEquipmentInputBlocked=true;
 Buddy=Cast<ABBSBBunniarCompanion>(Actions->BunniarEquipment);if(Buddy) { Buddy->ReleaseSurface();Buddy->bRescueFlying=true;Buddy->SetActorHiddenInGame(false); }
 auto* Move=Player->GetCharacterMovement();Move->StopMovementImmediately();Player->ConsumeMovementInputVector();if(!Move->IsFalling())Move->DisableMovement();
 auto* Mesh=Player->GetMesh();Mesh->SetDisablePostProcessBlueprint(true);
 DeathClip=DeathAnimation.LoadSynchronous();if(!DeathClip)DeathClip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01.MM_Death_Front_01"));
 if(DeathClip)Mesh->PlayAnimation(DeathClip,false);
 PromptDelay=FMath::Max(DeathPresentationSeconds,DeathClip ? DeathClip->GetPlayLength()+.2f:1.8f);
}
void UBBSBRespawnComponent::ShowPrompt()
{
 auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr;if(!PC)return;
 Prompt=CreateWidget<UBBSBRespawnWidget>(PC,PromptClass ? PromptClass.Get():UBBSBRespawnWidget::StaticClass());if(!Prompt)return;
 Prompt->Respawn=this;Prompt->AddToViewport(2000);Phase=EBBSBRespawnPhase::AwaitingChoice;PC->bShowMouseCursor=true;
 PC->SetInputMode(FInputModeUIOnly().SetWidgetToFocus(Prompt->TakeWidget()).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));Prompt->FocusConfirm();
}
void UBBSBRespawnComponent::TickDrops(float DT)
{
 for(auto& Drop:Drops)
 {
  auto* Actor=Drop.Actor.Get();if(!Actor || Drop.Landed)continue;
  FVector Center,Extent;Actor->GetActorBounds(false,Center,Extent);
  Drop.Velocity.Z-=980.f*DT;const FVector Step=Drop.Velocity*DT;
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBDeathEquipment),false,Player);Query.AddIgnoredActor(Buddy);for(const auto& Other:Drops)Query.AddIgnoredActor(Other.Actor.Get());
  FHitResult Hit;const float Radius=FMath::Clamp(float(Extent.GetMin()),3.f,10.f);
  if(GetWorld()->SweepSingleByObjectType(Hit,Center,Center+Step,FQuat::Identity,BBSBCombatVisuals::GeometryTypes(),FCollisionShape::MakeSphere(Radius),Query))
  {
   const FVector Tangent=FVector::VectorPlaneProject(Actor->GetActorUpVector(),Hit.ImpactNormal).GetSafeNormal();
   Actor->SetActorRotation(FRotationMatrix::MakeFromXZ(Hit.ImpactNormal,Tangent.IsNearlyZero() ? FVector::ForwardVector:Tangent).Rotator());
   Actor->GetActorBounds(false,Center,Extent);FVector Location=Actor->GetActorLocation();Location.Z+=Hit.ImpactPoint.Z+Extent.Z+1-Center.Z;Actor->SetActorLocation(Location);Drop.Landed=true;
  }
  else { Actor->SetActorLocation(Actor->GetActorLocation()+Step);Actor->AddActorWorldRotation(FRotator(24*DT,15*DT,0)); }
 }
}
void UBBSBRespawnComponent::TickBuddy(float Age,bool Returning)
{
 if(!Buddy || !Player)return;
 FVector Position,Direction;
 if(Returning)
 {
  const FVector Held=Player->GetActorTransform().TransformPosition(Player->Traversal->BunniarOffset);
  const float Pickup=FMath::SmoothStep(0.f,.8f,Age);Position=FMath::Lerp(BuddyPickupFrom,Held,Pickup);Direction=Player->GetActorForwardVector();
 }
 else
 { const float Angle=Age*1.65f;Position=Player->GetActorLocation()+FVector(FMath::Cos(Angle)*115,FMath::Sin(Angle)*115,95+FMath::Sin(Age*3)*12);Direction=FVector(-FMath::Sin(Angle),FMath::Cos(Angle),0); }
 const FVector Face=Buddy->AnimationSet ? Buddy->AnimationSet->FaceDirection.GetSafeNormal():FVector::RightVector;
 const FTransform Relative=Buddy->Visual->GetRelativeTransform();
 const FQuat Model=FRotationMatrix::MakeFromXZ(Relative.TransformVectorNoScale(Face),Relative.TransformVectorNoScale(FVector::UpVector)).ToQuat();
 Buddy->SetActorLocation(Position);Buddy->SetActorRotation(FRotationMatrix::MakeFromXZ(Direction,FVector::UpVector).ToQuat()*Model.Inverse());Buddy->SetActorScale3D(FVector(Returning ? .24f:.16f));
}
bool UBBSBRespawnComponent::ResolveReturnTransform(FTransform& Out) const
{
 if(!Player)return false;const FTransform Desired=RespawnTransform();
 for(const FVector Offset:{FVector::ZeroVector,FVector(100,0,0),FVector(-100,0,0),FVector(0,100,0),FVector(0,-100,0)})
 {
  FVector Position=Desired.GetLocation()+Offset;FRotator Rotation=Desired.Rotator();
  if(GetWorld()->FindTeleportSpot(Player,Position,Rotation)) { Out=FTransform(Rotation,Position);return true; }
 }
 return false;
}
bool UBBSBRespawnComponent::IsPointDiscovered(const ABBSBRespawnPoint* Point) const
{ return IsValid(Point) && Point->GetWorld()==GetWorld() && DiscoveredPoints.Contains(Point->TravelId()); }
void UBBSBRespawnComponent::LoadTravel(const UBBSBHuntingSave* Save)
{
 if(!Save)return;DiscoveredPoints=Save->DiscoveredRespawnPoints;PendingPointId=Save->ActiveRespawnPoint;ActivePoint=nullptr;
 for(TActorIterator<ABBSBRespawnPoint> It(GetWorld());It;++It)if(It->TravelId()==PendingPointId && IsPointDiscovered(*It)) { ActivePoint=*It;break; }
}
void UBBSBRespawnComponent::StoreTravel(UBBSBHuntingSave* Save) const
{ if(Save) { Save->DiscoveredRespawnPoints=DiscoveredPoints;Save->ActiveRespawnPoint=IsValid(ActivePoint) ? ActivePoint->TravelId():PendingPointId; } }
FVector UBBSBRespawnComponent::CarryGrip(bool Left) const
{ return Player ? Player->GetActorTransform().TransformPosition(Left ? Player->Traversal->LeftGripOffset:Player->Traversal->RightGripOffset):FVector::ZeroVector; }
void UBBSBRespawnComponent::GetCarryPose(UAnimSequence*& Carry,UAnimSequence*& Fallen,float& Alpha) const
{ Carry=CarryClip;Fallen=bTravelFromDeath && GetWorld()->GetTimeSeconds()-StartedAt<.8 ? DeathClip.Get():nullptr;Alpha=CarryBlend; }
void UBBSBRespawnComponent::BeginCarryPose()
{
 if(bCarryPoseStarted)return;bCarryPoseStarted=true;bCarryingPlayer=true;CarryBlend=0;CarryClip=CarryAnimation.LoadSynchronous();
 auto* Mesh=Player->GetMesh();Mesh->SetAnimationMode(EAnimationMode::Type(SavedAnimationMode));
 if(SavedAnimationMode==uint8(EAnimationMode::AnimationBlueprint) && SavedAnimClass)Mesh->SetAnimInstanceClass(SavedAnimClass);
 Mesh->SetDisablePostProcessBlueprint(false);
}
bool UBBSBRespawnComponent::RequestFastTravel(ABBSBRespawnPoint* Point)
{
 if(!Player || !Player->IsLocallyControlled() || IsSequenceActive() || Player->Combat->bDead || !IsPointDiscovered(Point) ||
    Player->Combat->IsStageLocked() || Player->Interaction->bDialogueActive || Player->EquipmentGrowth->bConcertoActive ||
    Player->Traversal->IsWireTraversalActive() || Player->Traversal->IsParkourActive() || Player->FieldActions->IsGestureCaptured() || Player->FieldActions->IsDodging() ||
    (Player->FieldActions->bEquipmentInputBlocked && !Player->WorldMap->bMapOpen))return false;
 FTransform Target=Point->LandingTransform(Player);FVector Location=Target.GetLocation();FRotator Rotation=Target.Rotator();
 if(!GetWorld()->FindTeleportSpot(Player,Location,Rotation))return false;Target=FTransform(Rotation,Location);
 if(!PrepareTravel(Target,false))return false;
 ActivePoint=Point;PendingPointId=Point->TravelId();return true;
}
bool UBBSBRespawnComponent::PrepareTravel(const FTransform& Target,bool FromDeath)
{
 if(!Player)return false;
 Player->WorldMap->CloseMap();Player->Hunting->StopCollection();Player->Hunting->CloseInventory();Player->FieldActions->CancelEquipmentSelection();
 if(!FromDeath)SavePresentation();
 ReturnTarget=Target;TravelFrom=Player->GetActorTransform();bTravelFromDeath=FromDeath;CarryBlend=0;bCarryPoseStarted=false;
 Player->Combat->ReleaseTargetLock();Player->Traversal->CancelWireRecall();Player->Traversal->DropParkour();Player->Traversal->EndGlide();Player->Traversal->EndAirWeaponHang();Player->StopJumping();
 auto* Actions=Player->FieldActions.Get();
 if(!FromDeath) { TArray<AActor*> Gear;Actions->FreezeDeathEquipment(Gear);for(auto* Visual:Gear) { HiddenTravelWeapons.Add(Visual);Visual->SetActorHiddenInGame(true); } }
 Actions->InterruptAction(EBBSBActionState::Dead,100000.f);Actions->bEquipmentInputBlocked=true;
 Buddy=Cast<ABBSBBunniarCompanion>(Actions->BunniarEquipment);if(Buddy) { Buddy->ReleaseSurface();Buddy->bRescueFlying=true;Buddy->SetActorHiddenInGame(false);BuddyPickupFrom=Buddy->GetActorLocation(); }
 Player->GetCharacterMovement()->StopMovementImmediately();Player->GetCharacterMovement()->DisableMovement();Player->ConsumeMovementInputVector();Player->SetActorEnableCollision(false);
 // Lift above the complete route before crossing it. Sample geometry, not enemies or effects.
 const FVector From=TravelFrom.GetLocation(),To=Target.GetLocation();CruiseHeight=FMath::Max(From.Z,To.Z)+FMath::Max(100.f,DeliveryHeight);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBRescueFlight),false,Player);Query.AddIgnoredActor(Buddy);
 for(TActorIterator<ABBSBRespawnPoint> It(GetWorld());It;++It)Query.AddIgnoredActor(*It);
 const int32 Samples=FMath::Clamp(FMath::CeilToInt(FVector::Dist2D(From,To)/150.f),1,140);
 for(int32 I=0;I<=Samples;++I)
 {
  const FVector XY=FMath::Lerp(From,To,float(I)/Samples);FHitResult Hit;
  if(GetWorld()->SweepSingleByObjectType(Hit,FVector(XY.X,XY.Y,12000),FVector(XY.X,XY.Y,FMath::Min(From.Z,To.Z)-500),FQuat::Identity,BBSBCombatVisuals::GeometryTypes(),FCollisionShape::MakeSphere(55.f),Query))
   CruiseHeight=FMath::Max(CruiseHeight,float(Hit.ImpactPoint.Z+Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+DeliveryHeight));
 }
 FlightSeconds=FMath::Max(1.f,FVector::Dist2D(From,To)/FMath::Max(300.f,CarrySpeed));
 if(Prompt) { Prompt->RemoveFromParent();Prompt=nullptr; }
 Phase=EBBSBRespawnPhase::Returning;StartedAt=GetWorld()->GetTimeSeconds();BeginCarryPose();
 if(auto* PC=Cast<APlayerController>(Player->GetController())) { PC->bShowMouseCursor=false;PC->SetInputMode(FInputModeGameOnly()); }
 return true;
}
bool UBBSBRespawnComponent::RequestRespawn()
{
 FTransform Target;if(!Player || Phase!=EBBSBRespawnPhase::AwaitingChoice || !ResolveReturnTransform(Target))return false;
 return PrepareTravel(Target,true);
}
void UBBSBRespawnComponent::RestoreAnimation()
{
 if(!Player)return;auto* Mesh=Player->GetMesh();Mesh->SetAnimationMode(EAnimationMode::Type(SavedAnimationMode));
 if(SavedAnimationMode==uint8(EAnimationMode::AnimationBlueprint) && SavedAnimClass)Mesh->SetAnimInstanceClass(SavedAnimClass);
 Mesh->SetDisablePostProcessBlueprint(bOriginalPostProcess);DeathClip=CarryClip=nullptr;
}
void UBBSBRespawnComponent::ReleaseInput()
{
 if(bSavedCameraLag && Player)if(auto* Boom=Player->GetCameraBoom()) { Boom->bEnableCameraLag=bOriginalCameraLag;Boom->bEnableCameraRotationLag=bOriginalCameraRotationLag; }
 bSavedCameraLag=false;
 if(auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr)
 {
  if(bOwnMoveLock)PC->SetIgnoreMoveInput(false);if(bOwnLookLock)PC->SetIgnoreLookInput(false);PC->RotationInput=FRotator::ZeroRotator;PC->bShowMouseCursor=bOriginalCursor;
  if(auto* HUD=PC->GetHUD())HUD->bShowHUD=bOriginalHUD;
  if(bOriginalCursor)PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));else PC->SetInputMode(FInputModeGameOnly().SetConsumeCaptureMouseDown(false));
 }
 bOwnMoveLock=bOwnLookLock=false;
}
void UBBSBRespawnComponent::FinishRespawn()
{
 Player->SetActorLocationAndRotation(ReturnTarget.GetLocation(),ReturnTarget.Rotator(),false,nullptr,ETeleportType::TeleportPhysics);Player->SetActorEnableCollision(true);
 bCarryingPlayer=false;CarryBlend=0;RestoreAnimation();if(bTravelFromDeath)Player->Combat->RestoreAfterRespawn();
 for(AActor* Visual:HiddenTravelWeapons)if(IsValid(Visual))Visual->SetActorHiddenInGame(false);HiddenTravelWeapons.Reset();
 Player->FieldActions->RestoreDeathEquipment();Player->FieldActions->bEquipmentInputBlocked=bOriginalEquipmentBlock;
 auto* Move=Player->GetCharacterMovement();Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Walking);Move->GravityScale=SavedGravityScale;
 if(bTravelFromDeath) { Player->Traversal->WireCharges=2;Player->Traversal->WireRechargeProgress=0; }
 if(Buddy) { Buddy->bRescueFlying=false;Buddy->bFollowing=true; }
 Drops.Reset();Phase=EBBSBRespawnPhase::Alive;ProtectedUntil=GetWorld()->GetTimeSeconds()+FMath::Max(0.f,ProtectionSeconds);ReleaseInput();
 Player->Hunting->NotifyGrowthChanged();Player->Combat->SetFeedback(TEXT("버니아가 표식에 내려주었어요"));
}
void UBBSBRespawnComponent::ResetPresentation()
{
 if(!IsSequenceActive())return;
 if(Prompt) { Prompt->RemoveFromParent();Prompt=nullptr; }
 bCarryingPlayer=false;CarryBlend=0;RestoreAnimation();if(Buddy)Buddy->bRescueFlying=false;
 for(AActor* Visual:HiddenTravelWeapons)if(IsValid(Visual))Visual->SetActorHiddenInGame(false);HiddenTravelWeapons.Reset();
 if(Player) { Player->SetActorEnableCollision(true);Player->FieldActions->RestoreDeathEquipment();Player->FieldActions->bEquipmentInputBlocked=bOriginalEquipmentBlock;Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
 Drops.Reset();Phase=EBBSBRespawnPhase::Alive;ReleaseInput();
}
void UBBSBRespawnComponent::ApplyCamera(float DT,FMinimalViewInfo& View) const
{
 if(!IsSequenceActive() || !Player)return;
 const bool Returning=Phase==EBBSBRespawnPhase::Returning;
 const FVector Focus=Player->GetActorLocation()+FVector::UpVector*40.f;
 const FQuat Facing=(Focus-View.Location).Rotation().Quaternion();
 // Frame both the fallen body and the complete flight circle, including wide PIE views.
 // The bounds stay around the player instead of chasing the circling companion.
 auto* PC=Cast<APlayerController>(Player->GetController());int32 W=1,H=1;if(PC)PC->GetViewportSize(W,H);
 FSceneViewProjectionData Projection;const FIntRect Rect(0,0,FMath::Max(1,W),FMath::Max(1,H));Projection.SetViewRectangle(Rect);
 FMinimalViewInfo ProjectionView=View;const ULocalPlayer* Local=PC ? PC->GetLocalPlayer():nullptr;
 FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(ProjectionView,Local ? Local->AspectRatioAxisConstraint:GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint,Rect,Projection);
 const FBox Bounds(Player->GetActorLocation()+FVector(-165,-165,-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Player->GetActorLocation()+FVector(165,165,200));
 float Retreat=0;
 for(int32 I=0;I<8;++I)
 {
  const FVector Corner((I&1) ? Bounds.Max.X:Bounds.Min.X,(I&2) ? Bounds.Max.Y:Bounds.Min.Y,(I&4) ? Bounds.Max.Z:Bounds.Min.Z);
  const FVector P=Facing.UnrotateVector(Corner-View.Location);
  Retreat=FMath::Max(Retreat,float(FMath::Max(FMath::Abs(P.Y)*Projection.ProjectionMatrix.M[0][0]/.88-P.X,FMath::Abs(P.Z)*Projection.ProjectionMatrix.M[1][1]/.84-P.X)));
 }
 FVector Eye=View.Location-Facing.GetForwardVector()*FMath::Clamp(Retreat,0.f,700.f);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBRespawnCamera),false,Player);Query.AddIgnoredActor(Buddy);FHitResult Hit;
 if(GetWorld()->SweepSingleByChannel(Hit,View.Location,Eye,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(12.f),Query))Eye=Hit.bStartPenetrating ? View.Location:Hit.Location;
 const float Blend=Returning ? 1.f:FMath::SmoothStep(0.f,.45f,float(GetWorld()->GetTimeSeconds()-StartedAt));
 View.Location=FMath::Lerp(View.Location,Eye,Blend);
 View.Rotation=FQuat::Slerp(View.Rotation.Quaternion(),(Focus-View.Location).Rotation().Quaternion(),Blend).GetNormalized().Rotator();
}
void UBBSBRespawnComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DT,Type,Function);if(!Player || !Player->IsLocallyControlled())return;
 if(!bInitialSpawnSaved) { InitialSpawn=Player->GetActorTransform();bInitialSpawnSaved=true; }
 if(Phase==EBBSBRespawnPhase::Alive)
 {
  if(!IsValid(ActivePoint) && !PendingPointId.IsEmpty())for(TActorIterator<ABBSBRespawnPoint> It(GetWorld());It;++It)if(It->TravelId()==PendingPointId && IsPointDiscovered(*It)) { ActivePoint=*It;break; }
  return;
 }
 const float Age=GetWorld()->GetTimeSeconds()-StartedAt;
 if(Phase==EBBSBRespawnPhase::Returning)
 {
  constexpr float Pickup=.8f,Lift=1.f,Land=1.2f;
  const float Arrival=Pickup+Lift+FlightSeconds,End=Arrival+Land;
  CarryBlend=FMath::SmoothStep(0.f,Pickup,Age)*(1.f-FMath::SmoothStep(End-.35f,End,Age));
  const FVector From=TravelFrom.GetLocation(),To=ReturnTarget.GetLocation();FVector Position=From;
  if(Age>=Arrival)Position=FMath::Lerp(FVector(To.X,To.Y,CruiseHeight),To,FMath::SmoothStep(Arrival,End,Age));
  else if(Age>=Pickup+Lift)Position=FMath::Lerp(FVector(From.X,From.Y,CruiseHeight),FVector(To.X,To.Y,CruiseHeight),FMath::SmoothStep(Pickup+Lift,Arrival,Age));
  else Position.Z=FMath::Lerp(float(From.Z),CruiseHeight,FMath::SmoothStep(Pickup,Pickup+Lift,Age));
  const FVector Direction=(To-From).GetSafeNormal2D();const FQuat Facing=(Direction.IsNearlyZero() ? ReturnTarget.Rotator():Direction.Rotation()).Quaternion();
  FQuat Rotation=FQuat::Slerp(TravelFrom.GetRotation(),Facing,FMath::SmoothStep(0.f,Pickup+Lift,Age));
  if(Age>Arrival)Rotation=FQuat::Slerp(Facing,ReturnTarget.GetRotation(),FMath::SmoothStep(Arrival,End,Age));
  Player->SetActorLocationAndRotation(Position,Rotation,false,nullptr,ETeleportType::TeleportPhysics);TickBuddy(Age,true);
  if(Age>=End)FinishRespawn();return;
 }

 TickDrops(FMath::Min(DT,.05f));TickBuddy(Age,false);
 if(Phase==EBBSBRespawnPhase::Downed && Age>=PromptDelay)ShowPrompt();
}
void UBBSBRespawnComponent::EndPlay(const EEndPlayReason::Type Reason)
{ ResetPresentation();Super::EndPlay(Reason); }
