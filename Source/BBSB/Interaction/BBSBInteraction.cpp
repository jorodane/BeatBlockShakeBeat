#include "Interaction/BBSBInteraction.h"
#include "BBSBCharacter.h"
#include "Character/BBSBTraversal.h"
#include "Caravan/BBSBCaravan.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

UBBSBInteractionComponent::UBBSBInteractionComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
}
void UBBSBInteractionComponent::BeginPlay()
{
 Super::BeginPlay();Player=Cast<ABBSBCharacter>(GetOwner());
 BindControls();
}
void UBBSBInteractionComponent::BindControls()
{
 if(Controls) return;
 if(auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr)
 {
  Controls=NewObject<UInputComponent>(this,TEXT("BunniarDialogueControls"));Controls->RegisterComponent();Controls->Priority=40;
  // Dynamic bindings below do not consume keys outside a conversation.
  auto& Next=Controls->BindKey(EKeys::Enter,IE_Pressed,this,&UBBSBInteractionComponent::AdvanceDialogue);Next.bConsumeInput=false;
  auto& Close=Controls->BindKey(EKeys::BackSpace,IE_Pressed,this,&UBBSBInteractionComponent::EndDialogue);Close.bConsumeInput=false;
  PC->PushInputComponent(Controls);
 }
}
void UBBSBInteractionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 EndDialogue();
 if(auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr) if(Controls) PC->PopInputComponent(Controls);
 Super::EndPlay(Reason);
}
bool UBBSBInteractionComponent::BeginDialogue(ABBSBInteractionNPC* NPC)
{
 if(Player && Player->Traversal && Player->Traversal->IsWireTraversalActive()) return false;
 auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr;
 if(!PC || !IsValid(NPC) || Player->Combat->bDead || Player->Combat->IsStageLocked()) return false;
 if(bDialogueActive && ActiveNPC==NPC) return true;
 if(bDialogueActive) EndDialogue();
 ActiveNPC=NPC;LineIndex=0;PreviousViewTarget=PC->GetViewTarget();bDialogueActive=true;
 PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bOwnMoveIgnore=bOwnLookIgnore=true;
 Player->GetCharacterMovement()->StopMovementImmediately();
 FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 DialogueCamera=GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),FTransform::Identity,P);
 if(!DialogueCamera) { EndDialogue();return false; }
 UpdateCamera();PC->SetViewTargetWithBlend(DialogueCamera,CameraBlendSeconds);
 if(Controls) for(auto& Binding:Controls->KeyBindings) Binding.bConsumeInput=true;
 return true;
}
void UBBSBInteractionComponent::AdvanceDialogue()
{
 if(!bDialogueActive || !IsValid(ActiveNPC)) return;
 if(++LineIndex>=ActiveNPC->Lines.Num()) EndDialogue();
}
void UBBSBInteractionComponent::EndDialogue()
{
 if(!bDialogueActive && !bOwnMoveIgnore && !bOwnLookIgnore) return;
 bDialogueActive=false;ActiveNPC=nullptr;
 if(auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr)
 {
  if(bOwnMoveIgnore) PC->SetIgnoreMoveInput(false);
  if(bOwnLookIgnore) PC->SetIgnoreLookInput(false);
  if(PC->GetViewTarget()==DialogueCamera) PC->SetViewTargetWithBlend(PreviousViewTarget.IsValid() ? PreviousViewTarget.Get():Player.Get(),CameraBlendSeconds);
 }
 bOwnMoveIgnore=bOwnLookIgnore=false;PreviousViewTarget.Reset();
 // Keep the camera alive through the outgoing blend.
 if(DialogueCamera) { DialogueCamera->SetLifeSpan(FMath::Max(.1f,CameraBlendSeconds+.1f));DialogueCamera=nullptr; }
 if(Controls) for(auto& Binding:Controls->KeyBindings) Binding.bConsumeInput=false;
}
void UBBSBInteractionComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DT,Type,Function);
 BindControls();
 if(!bDialogueActive) return;
 if(!IsValid(ActiveNPC) || !Player || Player->Combat->bDead || Player->Combat->IsStageLocked() || FVector::Dist(Player->GetActorLocation(),ActiveNPC->GetActorLocation())>1000) { EndDialogue();return; }
 UpdateCamera();
}
void UBBSBInteractionComponent::UpdateCamera()
{
 if(!DialogueCamera || !ActiveNPC) return;
 const FVector Focus=ActiveNPC->HeadPoint()-FVector(0,0,12);
 FVector Desired=Focus+ActiveNPC->GetActorForwardVector()*CameraDistance+ActiveNPC->GetActorRightVector()*CameraSideOffset+FVector(0,0,20);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BunniarDialogueCamera),false,ActiveNPC);Q.AddIgnoredActor(Player);Q.AddIgnoredActor(DialogueCamera);Q.AddIgnoredActor(Player->FieldActions->BunniarEquipment);
 FHitResult Wall;
 if(GetWorld()->SweepSingleByChannel(Wall,Focus,Desired,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(12),Q)) Desired=Wall.Location+Wall.Normal*2;
 DialogueCamera->SetActorLocationAndRotation(Desired,(Focus-Desired).Rotation());DialogueCamera->GetCameraComponent()->SetFieldOfView(CameraFOV);
}
FString UBBSBInteractionComponent::DisplayedLine() const
{
 if(!bDialogueActive || !ActiveNPC || !ActiveNPC->Lines.IsValidIndex(LineIndex)) return FString();
 const FString Line=ActiveNPC->Lines[LineIndex].ToString();
 if(!ActiveNPC->bFaceCovered) return Line;
 FString Mask;
 for(int32 I=0;I<Line.Len();++I)
 {
  const TCHAR C=Line[I];
  if(C=='\n' || C=='\r') Mask.AppendChar(C);
  else if(C>=0xDC00 && C<=0xDFFF && I>0 && Line[I-1]>=0xD800 && Line[I-1]<=0xDBFF) continue;
  else Mask.AppendChar(TEXT('.'));
 }
 return Mask;
}
void UBBSBInteractionComponent::DrawDialogue(AHUD* HUD,UCanvas* Canvas) const
{
 if(!bDialogueActive || !ActiveNPC || !HUD || !Canvas) return;
 const float S=FMath::Clamp(Canvas->ClipX/1440.f,.65f,1.4f),Width=FMath::Min(1000*S,Canvas->ClipX-40*S),X=(Canvas->ClipX-Width)*.5f,Y=Canvas->ClipY-190*S;
 HUD->DrawRect(FLinearColor(.13f,.11f,.16f,.94f),X,Y,Width,146*S);
 HUD->DrawText(ActiveNPC->DisplayName.ToString(),FLinearColor(.98f,.8f,.85f),X+22*S,Y+14*S,GEngine->GetMediumFont(),S);
 FString Row;float RowY=Y+47*S;
 auto DrawRow=[&]() { HUD->DrawText(Row,FLinearColor(.97f,.92f,.85f),X+22*S,RowY,GEngine->GetMediumFont(),S);RowY+=24*S;Row.Reset(); };
 for(TCHAR C:DisplayedLine()) { if(C=='\n') { DrawRow();continue; }float TW,TH;Canvas->StrLen(GEngine->GetMediumFont(),Row+FString::Chr(C),TW,TH);if(TW*S>Width-44*S) DrawRow();Row.AppendChar(C); }
 if(!Row.IsEmpty()) DrawRow();
 HUD->DrawText(TEXT("Enter: 다음  |  Backspace: 대화 종료  |  좌클릭 해제: 버니아만 회수"),FLinearColor(.75f,.75f,.8f),X+22*S,Y+119*S,GEngine->GetSmallFont(),S);
}

ABBSBInteractionNPC::ABBSBInteractionNPC()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;
 BodyTarget=CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyTarget"));SetRootComponent(BodyTarget);BodyTarget->InitCapsuleSize(30,90);
 BodyTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);BodyTarget->SetCollisionResponseToAllChannels(ECR_Ignore);BodyTarget->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mannequin"));Body->SetupAttachment(BodyTarget);Body->SetRelativeLocation(FVector(0,0,-90));Body->SetRelativeRotation(FRotator(0,-90,0));Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));if(Manny.Succeeded()) Body->SetSkeletalMeshAsset(Manny.Object);
 static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));IdleAnimation=Idle.Object;
 HeadTarget=CreateDefaultSubobject<USphereComponent>(TEXT("HeadTarget"));HeadTarget->SetupAttachment(BodyTarget);HeadTarget->InitSphereRadius(18);HeadTarget->SetRelativeLocation(FVector(0,0,75));
 FaceTarget=CreateDefaultSubobject<USphereComponent>(TEXT("FaceTarget"));FaceTarget->SetupAttachment(BodyTarget);FaceTarget->InitSphereRadius(10);FaceTarget->SetRelativeLocation(FVector(14,0,76));
 for(auto* Target:{HeadTarget.Get(),FaceTarget.Get()}) { Target->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Target->SetCollisionResponseToAllChannels(ECR_Ignore);Target->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block); }
 DisplayName=FText::FromString(TEXT("상단원 (시제품)"));Lines={FText::FromString(TEXT("응, 불렀어? 버니아는 편하게 돌려보내도 돼.")),FText::FromString(TEXT("다음 길을 살펴보는 중이었어. 준비되면 같이 가자."))};Tags.Add(TEXT("BBSBInteractable"));
}
void ABBSBInteractionNPC::BeginPlay() { Super::BeginPlay();Body->SetAnimInstanceClass(UBBSBNPCAnimInstance::StaticClass()); }
FVector ABBSBInteractionNPC::HeadPoint() const { return Body->DoesSocketExist(TEXT("head")) ? Body->GetSocketLocation(TEXT("head")):GetActorLocation()+FVector(0,0,75); }
void ABBSBInteractionNPC::ResolveAttachment(UPrimitiveComponent*& Component,FVector& Point,FVector& Normal) const
{
 const bool Face=Component==FaceTarget;Component=Face ? static_cast<UPrimitiveComponent*>(FaceTarget.Get()):static_cast<UPrimitiveComponent*>(HeadTarget.Get());
 Normal=Face ? FaceTarget->GetForwardVector():HeadTarget->GetUpVector();
 // Selection spheres are deliberately generous; use the fitted mesh contact, not their outer skin.
 Point=Face ? FaceTarget->GetComponentLocation()+Normal*(FaceAttachmentForward-15):HeadTarget->GetComponentLocation()+Normal*HeadAttachmentHeight;
}
void ABBSBInteractionNPC::BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector Point,FVector Normal) { AttachedBuddy=Buddy; }
void ABBSBInteractionNPC::BunniarArrived_Implementation(ABBSBBunniarCompanion* Buddy)
{
 if(AttachedBuddy.Get()!=Buddy || !Buddy || !Buddy->bSurfaceAttached) return;
 bBunniarAttached=true;bFaceCovered=Buddy->SurfaceComponent==FaceTarget;++AttachReactions;ReactionAt=GetWorld()->GetTimeSeconds();
 if(auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0))) if(auto* Interaction=Player->FindComponentByClass<UBBSBInteractionComponent>()) Interaction->BeginDialogue(this);
}
void ABBSBInteractionNPC::BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy)
{
 if(AttachedBuddy.Get()!=Buddy) return;
 if(bBunniarAttached) { ++ReleaseReactions;ReactionAt=RecallAt=GetWorld()->GetTimeSeconds(); }
 bBunniarAttached=bFaceCovered=false;
 // Keep the returning companion as a temporary gaze target; dialogue owns its own lifetime.
}
void ABBSBInteractionNPC::Tick(float DT)
{
 Super::Tick(DT);const double Now=GetWorld()->GetTimeSeconds();
 const FQuat HeadWorld=Body->GetSocketTransform(TEXT("head")).GetRotation();
 if(!bHeadFrameReady) { NeutralHeadRelative=GetActorQuat().Inverse()*HeadWorld;bHeadFrameReady=true; }
 const FQuat HeadDelta=(HeadWorld*(GetActorQuat()*NeutralHeadRelative).Inverse()).GetNormalized();
 const FQuat Frame=(HeadDelta*GetActorQuat()).GetNormalized();
 HeadTarget->SetWorldLocationAndRotation(HeadPoint(),Frame);
 FaceTarget->SetWorldLocationAndRotation(HeadPoint()+Frame.GetForwardVector()*15+Frame.GetUpVector(),Frame);
 const float Age=Now-ReactionAt,Amplitude=bCaravanMember ? FamiliarRecoilDegrees:StrangerRecoilDegrees;
 HeadReaction=FRotator(Age<.8f ? FMath::Sin(Age*24)*FMath::Exp(-Age*6)*Amplitude:0,0,0);
 if(!bCaravanMember && (bBunniarAttached || Now-RecallAt<2.0))
 {
  auto* Player=UGameplayStatics::GetPlayerCharacter(this,0);
  FVector Target=Player ? Player->GetActorLocation()+FVector(0,0,60):GetActorLocation()+GetActorForwardVector()*100;
  if(Now-RecallAt<.5 && AttachedBuddy.IsValid()) Target=AttachedBuddy->GetActorLocation();
  FRotator Look=((Target-HeadPoint()).Rotation()-GetActorRotation()).GetNormalized();
  HeadReaction.Yaw=FMath::Clamp(Look.Yaw,-55.f,55.f);HeadReaction.Pitch+=FMath::Clamp(Look.Pitch,-20.f,20.f)*.5f;
 }
}
void ABBSBInteractionNPC::EndPlay(const EEndPlayReason::Type Reason)
{
 if(AttachedBuddy.IsValid() && AttachedBuddy->bSurfaceAttached && AttachedBuddy->SurfaceComponent && AttachedBuddy->SurfaceComponent->GetOwner()==this) AttachedBuddy->ReleaseSurface();
 Super::EndPlay(Reason);
}

ABBSBCarryObject::ABBSBCarryObject()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarryBody"));SetRootComponent(Body);Body->SetMobility(EComponentMobility::Movable);Body->SetCollisionProfileName(TEXT("PhysicsActor"));Body->SetNotifyRigidBodyCollision(true);Body->BodyInstance.bUseCCD=true;
 HeldMovement=CreateDefaultSubobject<UBBSBCarryMovementComponent>(TEXT("CarryMovement"));HeldMovement->PrimaryComponentTick.bCanEverTick=false;HeldMovement->SetUpdatedComponent(Body);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));Body->SetStaticMesh(Cube.Object);
 Tags.Add(TEXT("BBSBInteractable"));Tags.Add(TEXT("BBSBAttackable"));
}
void ABBSBCarryObject::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);Body->SetRelativeScale3D(Size/100.f);BBSBCombatVisuals::Tint(Body,Color);
}
void ABBSBCarryObject::BeginPlay()
{
 Super::BeginPlay();Home=GetActorTransform();Health=MaximumHealth;NormalPawnResponse=Body->GetCollisionResponseToChannel(ECC_Pawn);NormalCameraResponse=Body->GetCollisionResponseToChannel(ECC_Camera);Body->SetMassOverrideInKg(NAME_None,MassKg,true);Body->SetSimulatePhysics(true);Body->OnComponentHit.AddDynamic(this,&ABBSBCarryObject::OnBodyHit);
}
float ABBSBCarryObject::DamageForSpeed(float Speed) const { return Speed<MinimumImpactSpeed ? 0.f:FMath::Max(0.f,Speed)*.01f*ImpactDamagePerMeterPerSecond; }
void ABBSBCarryObject::BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector Point,FVector Normal)
{
 if(bBroken) return;
 AttachedBuddy=Buddy;CaptureHoldAnchor(false);
}
void ABBSBCarryObject::BunniarArrived_Implementation(ABBSBBunniarCompanion* Buddy)
{
 if(bBroken || AttachedBuddy.Get()!=Buddy) return;
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(!Player) return;
 bCarried=true;bManipulating=bCarryBlocked=false;BlockedTravel=0;CarryOwner=Player;
 Body->SetSimulatePhysics(false);Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);Body->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);PreviousPlayerPosition=Player->GetActorLocation();
 bOwnBodyMoveIgnore=!Body->GetMoveIgnoreActors().Contains(Player);bOwnPlayerMoveIgnore=!Player->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(this);
 if(bOwnBodyMoveIgnore) Body->IgnoreActorWhenMoving(Player,true);
 if(bOwnPlayerMoveIgnore) Player->GetCapsuleComponent()->IgnoreActorWhenMoving(this,true);
 // The click already captured the distance. Camera movement during Bunniar's flight must not replace it.
 CarryReachLimit=FMath::Max(FVector::Dist(Player->GetActorLocation(),GetActorLocation()),FMath::Max(100.f,Player->FieldActions->WeaponReach-45));
 AddTickPrerequisiteActor(Player);AddTickPrerequisiteComponent(Player->GetCameraBoom());Buddy->AddTickPrerequisiteActor(this);
 Motion.Reset();CarryVelocity=FVector::ZeroVector;SampleMotion();
}
bool ABBSBCarryObject::GetCarryView(FVector& Location,FRotator& Rotation) const
{
 auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!PC) return false;
 // The boom has already ticked; use its current view rather than the camera manager's previous-frame cache.
 if(auto* Player=Cast<ABBSBCharacter>(PC->GetPawn());Player && PC->GetViewTarget()==Player && Player->GetFollowCamera()->IsActive())
 {
  Location=Player->GetFollowCamera()->GetComponentLocation();Rotation=Player->GetFollowCamera()->GetComponentRotation();
 }
 else PC->GetPlayerViewPoint(Location,Rotation);
 return true;
}
bool ABBSBCarryObject::GetCarryAim(FVector& Location,FRotator& Rotation,FVector& Direction) const
{
 if(!GetCarryView(Location,Rotation)) return false;
 Direction=Rotation.Vector();
 if(auto* PC=UGameplayStatics::GetPlayerController(this,0);PC && PC->bShowMouseCursor)
 {
  FVector MouseOrigin,MouseDirection;
  if(PC->DeprojectMousePositionToWorld(MouseOrigin,MouseDirection)) Direction=MouseDirection.GetSafeNormal();
 }
 return true;
}
void ABBSBCarryObject::CaptureHoldAnchor(bool bKeepPosition)
{
 FVector Location,Direction;FRotator Rotation;
 if(!GetCarryAim(Location,Rotation,Direction)) return;
 HoldDistance=FVector::Dist(GetActorLocation(),Location);
 CameraLocalHoldOffset=bKeepPosition ? Rotation.UnrotateVector(GetActorLocation()-Location-Direction*HoldDistance):FVector::ZeroVector;
 if(!bKeepPosition) OrbitAxis=FVector::UpVector;
}
bool ABBSBCarryObject::BeginManipulation()
{
 if(!bCarried || bBroken) return false;
 auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!PC) return false;
 FixedView=PC->GetControlRotation();FVector ViewLocation;FRotator ViewRotation;
 if(!GetCarryView(ViewLocation,ViewRotation)) return false;
 CameraForward=ViewRotation.Vector();CameraRight=FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
 ManipulationGoal=GetActorLocation();bManipulating=true;Motion.Reset();CarryVelocity=FVector::ZeroVector;SampleMotion();return true;
}
void ABBSBCarryObject::EndManipulation(bool bFlick)
{
 if(!bManipulating) return;
 if(bFlick)
 {
  Drop(true);
  if(AttachedBuddy.IsValid()) AttachedBuddy->ReleaseSurface();
 }
 else { CaptureHoldAnchor(true);bManipulating=false;Motion.Reset();CarryVelocity=FVector::ZeroVector;SampleMotion(); }
}
void ABBSBCarryObject::MoveFromPointer(FVector2D Delta)
{
 if(bCarried && bManipulating && !Delta.IsNearlyZero())
 {
  ManipulationGoal+=(CameraRight*Delta.X-CameraForward*Delta.Y)*DragCentimetersPerPixel;
  if(CarryOwner.IsValid()) ManipulationGoal=CarryOwner->GetActorLocation()+(ManipulationGoal-CarryOwner->GetActorLocation()).GetClampedToMaxSize(CarryReachLimit);
  // Apply movement once in Tick, after the camera updates, so extra pointer samples cannot multiply the speed budget.
 }
}
FVector ABBSBCarryObject::OrbitStep(FVector Target,FVector Center,float MaximumDistance)
{
 const FVector Current=GetActorLocation(),From=Current-Center,To=Target-Center;
 const double FromRadius=From.Size2D(),ToRadius=To.Size2D();
 if(FromRadius<1 || ToRadius<1) return Current+(Target-Current).GetClampedToMaxSize(MaximumDistance);
 const FVector FromDirection(From.X/FromRadius,From.Y/FromRadius,0),ToDirection(To.X/ToRadius,To.Y/ToRadius,0);
 double Angle=FMath::Atan2(FromDirection.X*ToDirection.Y-FromDirection.Y*ToDirection.X,FVector::DotProduct(FromDirection,ToDirection));
 if(FMath::Abs(Angle)<1.e-6) return Current+(Target-Current).GetClampedToMaxSize(MaximumDistance);
 // atan2 always selects the shortest yaw arc. At an exact half-turn either side is equally short.
 if(PI-FMath::Abs(Angle)<1.e-6) Angle=FMath::Sign(OrbitAxis.Z)*PI;
 OrbitAxis=Angle>=0 ? FVector::UpVector:-FVector::UpVector;
 // Interpolate height separately, so a floor-height half-turn cannot choose an arc through the floor or the player's legs.
 const double PathBound=FMath::Sqrt(FMath::Square(Angle*FMath::Max(FromRadius,ToRadius))+FMath::Square(ToRadius-FromRadius)+FMath::Square(To.Z-From.Z));
 const double Alpha=FMath::Min(1.,MaximumDistance/FMath::Max(PathBound,1.e-6));
 if(Alpha>=1) return Target;
 const FVector Direction=FQuat(FVector::UpVector,Angle*Alpha).RotateVector(FromDirection);
 return Center+Direction*FMath::Lerp(FromRadius,ToRadius,Alpha)+FVector::UpVector*FMath::Lerp(From.Z,To.Z,Alpha);
}
void ABBSBCarryObject::MoveHeld(FVector Desired,float DT)
{
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(!Player) return;
 const FVector Center=Player->GetActorLocation();const float Reach=FMath::Max(100.f,CarryReachLimit);
 Desired=Center+(Desired-Center).GetClampedToMaxSize(Reach);
 // Keep the original aim/depth/offset goal. Only the object's actual swept path may constrain it.
 bool bBlocked=false;const FVector Target=Desired;
 float Remaining=FMath::Max(0.f,FollowSpeed)*FMath::Max(0.f,DT);
 for(int32 Segment=0;Segment<64 && Remaining>.001f && !GetActorLocation().Equals(Target,.001f);++Segment)
 {
  // Sweep small chords of the shortest orbit arc, including on long frames, rather than cutting through its centre.
  const float Budget=bManipulating ? Remaining:FMath::Min(Remaining,FMath::Max(1.f,float(FVector::Dist2D(GetActorLocation(),Center))*FMath::DegreesToRadians(5.f)));
  const FVector Before=GetActorLocation();
  const FVector Next=bManipulating ? Before+(Target-Before).GetClampedToMaxSize(Budget):OrbitStep(Target,Center,Budget);
  const FVector Delta=Next-Before;FHitResult Hit;
  HeldMovement->SafeMoveUpdatedComponent(Delta,GetActorQuat(),true,Hit);
  if(Hit.IsValidBlockingHit()) { bBlocked=true;HeldMovement->SlideAlongSurface(Delta,1.f-Hit.Time,Hit.Normal,Hit,false); }
  Remaining-=Budget;
  if(GetActorLocation().Equals(Before,.0001f)) break;
 }
 // Sliding can shorten the hold distance, but must still respect the player's interaction reach.
 if(FVector::DistSquared(GetActorLocation(),Center)>FMath::Square(Reach+.1f))
 {
  const FVector Limited=Center+(GetActorLocation()-Center).GetClampedToMaxSize(Reach);FHitResult ReachHit;
  HeldMovement->SafeMoveUpdatedComponent((Limited-GetActorLocation()).GetClampedToMaxSize(Remaining),GetActorQuat(),true,ReachHit);
 }
 SampleMotion();UpdateCarryObstruction(bBlocked,Player);
}
void ABBSBCarryObject::UpdateCarryObstruction(bool bBlocked,ABBSBCharacter* Player)
{
 if(!bCarried) return;
 if(bBlocked)
 {
  if(!bCarryBlocked) { BlockedPlayerAnchor=Player->GetActorLocation();BlockedObjectAnchor=GetActorLocation(); }
  const FVector PlayerStep=Player->GetActorLocation()-PreviousPlayerPosition;
  if(!PlayerStep.IsNearlyZero())
  {
   FComponentQueryParams Query;Query.AddIgnoredActor(this);Query.AddIgnoredActor(Player);Query.AddIgnoredActor(AttachedBuddy.Get());
   TArray<FHitResult> Hits;GetWorld()->ComponentSweepMulti(Hits,Body,GetActorLocation(),GetActorLocation()+PlayerStep,GetActorQuat(),Query);
   bool bTranslationBlocked=false;
   for(const FHitResult& Hit:Hits) if(Hit.bBlockingHit && FVector::DotProduct(PlayerStep,Hit.Normal)<-UE_KINDA_SMALL_NUMBER) { bTranslationBlocked=true;break; }
   // Aim alignment may still be catching up. Only a surface that prevents following the player's step spends the leash.
   if(!bTranslationBlocked) { BlockedPlayerAnchor=Player->GetActorLocation();BlockedObjectAnchor=GetActorLocation(); }
  }
  const FVector PlayerTravel=Player->GetActorLocation()-BlockedPlayerAnchor;
  const FVector ObjectTravel=GetActorLocation()-BlockedObjectAnchor;
  // Count unfollowed player displacement. Camera-only motion and following a floor/wall do not consume the leash.
  BlockedTravel=FMath::Min(PlayerTravel.Size(),(PlayerTravel-ObjectTravel).Size());
 }
 else BlockedTravel=0;
 bCarryBlocked=bBlocked;
 const float Maximum=Player->FieldActions->WeaponReach;
 const bool bTooFar=FVector::Dist(Player->GetActorLocation(),GetActorLocation())>FMath::Max(CarryReachLimit,Maximum+float(Body->Bounds.SphereRadius));
 if(bTooFar || (bCarryBlocked && BlockedTravel>=Maximum*BlockedReleaseFraction))
 {
  if(AttachedBuddy.IsValid()) AttachedBuddy->ReleaseSurface();else Drop(false);
 }
}
void ABBSBCarryObject::ReleaseCarryIgnores()
{
 if(CarryOwner.IsValid())
 {
  if(bOwnBodyMoveIgnore) Body->IgnoreActorWhenMoving(CarryOwner.Get(),false);
  if(bOwnPlayerMoveIgnore) CarryOwner->GetCapsuleComponent()->IgnoreActorWhenMoving(this,false);
  RemoveTickPrerequisiteActor(CarryOwner.Get());RemoveTickPrerequisiteComponent(CarryOwner->GetCameraBoom());
 }
 if(AttachedBuddy.IsValid()) AttachedBuddy->RemoveTickPrerequisiteActor(this);
 bOwnBodyMoveIgnore=bOwnPlayerMoveIgnore=false;CarryOwner.Reset();
}
void ABBSBCarryObject::RotateFromWheel(float Steps)
{
 if(!bCarried || bBroken || FMath::IsNearlyZero(Steps)) return;
 FVector ViewLocation;FRotator ViewRotation;if(!GetCarryView(ViewLocation,ViewRotation)) return;
 const FVector Axis=bManipulating ? CameraRight:FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
 const float Degrees=Steps*WheelRotationDegrees;
 const int32 Count=FMath::Max(1,FMath::CeilToInt(FMath::Abs(Degrees)/5.f));
 const FQuat Increment(Axis.GetSafeNormal(),FMath::DegreesToRadians(Degrees/Count));
 FComponentQueryParams Query;Query.AddIgnoredActor(this);Query.AddIgnoredActor(UGameplayStatics::GetPlayerCharacter(this,0));Query.AddIgnoredActor(AttachedBuddy.Get());
 for(int32 I=0;I<Count;++I)
 {
  const FQuat Rotation=(Increment*GetActorQuat()).GetNormalized();TArray<FOverlapResult> Overlaps;
  // Rotation sweeps do not test the full angular path, so validate small angular steps explicitly.
  if(GetWorld()->ComponentOverlapMultiByChannel(Overlaps,Body,GetActorLocation(),Rotation,Body->GetCollisionObjectType(),Query)) break;
  SetActorRotation(Rotation,ETeleportType::None);
 }
}
void ABBSBCarryObject::SampleMotion()
{
 const double Now=GetWorld()->GetTimeSeconds();
 if(Motion.Num() && FMath::IsNearlyEqual(Motion.Last().Time,Now,1.e-7)) Motion.Last().Position=GetActorLocation();
 else Motion.Add({Now,GetActorLocation()});
 while(Motion.Num()>2 && Motion[1].Time<Now-FMath::Max(.02f,VelocitySampleSeconds)) Motion.RemoveAt(0);
 if(Motion.Num()>1) CarryVelocity=(Motion.Last().Position-Motion[0].Position)/FMath::Max(.001,Now-Motion[0].Time);
}
FVector ABBSBCarryObject::ReleaseVelocity() { SampleMotion();return CarryVelocity; }
void ABBSBCarryObject::Tick(float DT)
{
 Super::Tick(DT);if(bBroken) return;
 auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
 if(bCarried)
 {
  if(!AttachedBuddy.IsValid() || !AttachedBuddy->bSurfaceAttached || !Player) { Drop(false);return; }
  if(bManipulating)
  {
   if(auto* PC=Cast<APlayerController>(Player->GetController())) { PC->SetControlRotation(FixedView);PC->RotationInput=FRotator::ZeroRotator; }
   ManipulationGoal+=Player->GetActorLocation()-PreviousPlayerPosition;MoveHeld(ManipulationGoal,DT);
  }
  else
  {
   FVector Location,Direction;FRotator Rotation;
   if(GetCarryAim(Location,Rotation,Direction)) MoveHeld(Location+Direction*HoldDistance+Rotation.RotateVector(CameraLocalHoldOffset),DT);
  }
  PreviousPlayerPosition=Player->GetActorLocation();SampleMotion();
 }
 else
 {
  PrePhysicsVelocity=Body->GetPhysicsLinearVelocity();
  const float Clearance=Player ? FMath::Max(100.f,Player->GetCapsuleComponent()->GetScaledCapsuleRadius()+float(Body->Bounds.SphereRadius)+15.f):0;
  if(PawnIgnoreUntil>=0 && GetWorld()->GetTimeSeconds()>=PawnIgnoreUntil && (!Player || FVector::Dist(Player->GetActorLocation(),GetActorLocation())>Clearance)) { Body->SetCollisionResponseToChannel(ECC_Pawn,NormalPawnResponse);Body->SetCollisionResponseToChannel(ECC_Camera,NormalCameraResponse);PawnIgnoreUntil=-1; }
 }
}
void ABBSBCarryObject::Drop(bool bThrow)
{
 if(!bCarried) return;
 const FVector Velocity=ReleaseVelocity();bCarried=bManipulating=bCarryBlocked=false;ReleaseCarryIgnores();
 if(bThrow) LastThrowVelocity=Velocity;
 Body->SetSimulatePhysics(true);Body->SetPhysicsLinearVelocity(Velocity);PrePhysicsVelocity=Velocity;PawnIgnoreUntil=GetWorld()->GetTimeSeconds()+.25;
}
void ABBSBCarryObject::BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy)
{
 if(AttachedBuddy.Get()!=Buddy) return;Drop(false);AttachedBuddy.Reset();
}
void ABBSBCarryObject::OnBodyHit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit)
{
 if(bBroken || bCarried || !Other || !OtherComponent) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(const double* Last=Contacts.Find(OtherComponent);Last && Now-*Last<.12) return;
 FVector OtherVelocity=OtherComponent->GetComponentVelocity();
 if(auto* Object=Cast<ABBSBCarryObject>(Other)) OtherVelocity=Object->PrePhysicsVelocity;
 LastImpactSpeed=FMath::Max(0.f,float(-FVector::DotProduct(PrePhysicsVelocity-OtherVelocity,Hit.ImpactNormal)));
 LastImpactDamage=DamageForSpeed(LastImpactSpeed);if(LastImpactDamage<=0) return;
 Contacts.Add(OtherComponent,Now);
 TakeDamage(LastImpactDamage,FDamageEvent(),nullptr,Other);
 if(!Cast<ABBSBCarryObject>(Other) && (Cast<ABBSBBreakableProp>(Other) || Cast<ABBSBInkEnemy>(Other))) UGameplayStatics::ApplyDamage(Other,LastImpactDamage,nullptr,this,nullptr);
}
float ABBSBCarryObject::TakeDamage(float Amount,const FDamageEvent& Event,AController* Controller,AActor* Causer)
{
 if(bBroken || Amount<=0) return 0;
 const float Applied=FMath::Min(Health,Amount);Health-=Applied;if(Health<=0) BreakObject();return Applied;
}
void ABBSBCarryObject::BreakObject()
{
 bBroken=true;if(AttachedBuddy.IsValid()) AttachedBuddy->ReleaseSurface();bCarried=bManipulating=false;
 Body->SetSimulatePhysics(false);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);Body->SetVisibility(false);Body->SetRenderCustomDepth(false);
 for(int32 I=0;I<4;++I)
 {
  auto* Piece=GetWorld()->SpawnActor<AStaticMeshActor>(GetActorLocation()+FVector((I&1 ? 1:-1)*Size.X*.2f,(I&2 ? 1:-1)*Size.Y*.2f,5),GetActorRotation());
  if(!Piece) continue;auto* Mesh=Piece->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetStaticMesh(Body->GetStaticMesh());Mesh->SetWorldScale3D(Size/100.f*.4f);Mesh->SetMaterial(0,Body->GetMaterial(0));Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));Mesh->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);Mesh->SetSimulatePhysics(true);Mesh->SetPhysicsLinearVelocity(PrePhysicsVelocity*.25f+FVector((I&1 ? 1:-1)*40,(I&2 ? 1:-1)*40,80));Piece->SetLifeSpan(4);
 }
}
void ABBSBCarryObject::ResetObject()
{
 if(AttachedBuddy.IsValid()) AttachedBuddy->ReleaseSurface();Body->SetSimulatePhysics(false);SetActorTransform(Home);bBroken=bCarried=bManipulating=false;Health=MaximumHealth;Motion.Reset();Contacts.Reset();CarryVelocity=LastThrowVelocity=PrePhysicsVelocity=FVector::ZeroVector;LastImpactDamage=LastImpactSpeed=0;Body->SetVisibility(true);Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Body->SetCollisionResponseToChannel(ECC_Pawn,NormalPawnResponse);Body->SetCollisionResponseToChannel(ECC_Camera,NormalCameraResponse);PawnIgnoreUntil=-1;BlockedTravel=0;bCarryBlocked=false;Body->SetSimulatePhysics(true);
}
void ABBSBCarryObject::EndPlay(const EEndPlayReason::Type Reason) { if(AttachedBuddy.IsValid()) AttachedBuddy->ReleaseSurface();ReleaseCarryIgnores();Super::EndPlay(Reason); }
