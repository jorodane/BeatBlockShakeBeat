#include "Stage01/BBSBStage01.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBBunniarAnimation.h"
#include "Character/BBSBRespawn.h"
#include "Character/BBSBTraversal.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
ABBSBStage01Singer::ABBSBStage01Singer()
{
 PrimaryActorTick.TickGroup=TG_PostPhysics;
 bFieldBulletHell=true;bEnableFieldBeatFire=false;bEnableFieldSecondaryPattern=false;bFieldIdleWander=false;
 MaximumHealth=60;FieldShotSpeed=450;ShotDamage=4;bGrantsHuntingRewards=true;
 SingerMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SongBunniar"));SingerMesh->SetupAttachment(GetRootComponent());
 SingerMesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Meshes/Characters/Bunniar/Bunniar_Normal.Bunniar_Normal")));
 SingerMesh->SetAnimInstanceClass(UBBSBBunniarAnimInstance::StaticClass());SingerMesh->SetRelativeScale3D(FVector(.68));SingerMesh->SetRelativeLocation(FVector(0,0,-45));SingerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ABBSBStage01Singer::BeginPlay()
{
 Super::BeginPlay();MeshRest=SingerMesh->GetRelativeTransform();GetCharacterMovement()->DisableMovement();GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
 if(Carrier)
 {
  Carrier->Singer=this;Carrier->SetOwner(this);AimOccluder=Carrier;
  GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
  // The capsule and carry body represent one Bunniar, not two pushable objects.
  Carrier->Body->IgnoreActorWhenMoving(this,true);GetCapsuleComponent()->IgnoreActorWhenMoving(Carrier,true);
  AddTickPrerequisiteActor(Carrier);
 }
 auto* Visual=Carrier ? Carrier->BunniarMesh.Get():SingerMesh.Get();
 // Physics may move the hidden body later in the frame. It must not move an
 // already positioned song visual a second time through the attachment tree.
 Visual->SetAbsolute(true,true,false);
 Visual->PrimaryComponentTick.TickGroup=TG_PostUpdateWork;Visual->AddTickPrerequisiteActor(this);
 Visual->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 Visual->bEnableUpdateRateOptimizations=false;
 TArray<UStaticMeshComponent*> Parts;GetComponents(Parts);for(auto* C:Parts)C->SetVisibility(false);
}
bool ABBSBStage01Singer::IsSongEnabled() const
{
 if(!Director || bDefeated || bConcertoSuspended || bSilenced || !Director->PlayerCharacter())return false;
 const auto* P=Director->PlayerCharacter();if(P->Combat->bDead || P->Respawn->IsSequenceActive() || P->Interaction->bDialogueActive || Director->bGrabActive)return false;
 const auto Phase=Director->Phase;
 switch(Voice)
 {
 case EBBSBStage01Voice::Ambient:return Phase>=EBBSBStage01Phase::Outside;
 case EBBSBStage01Voice::Guard:return Phase>=EBBSBStage01Phase::FindTools;
 case EBBSBStage01Voice::Utility:return Phase==EBBSBStage01Phase::ShopBattle;
 case EBBSBStage01Voice::Roof:return (Phase>=EBBSBStage01Phase::Rooftops && Phase<=EBBSBStage01Phase::Trio) || Phase==EBBSBStage01Phase::ApproachCaptain;
 case EBBSBStage01Voice::Trio:return Phase==EBBSBStage01Phase::Trio;
 case EBBSBStage01Voice::Captain:return Phase==EBBSBStage01Phase::Captain;
 }
 return false;
}
void ABBSBStage01Singer::Tick(float DT)
{
 // One presentation writer, after both the carried movement and physics settle.
 if(Carrier)SetActorLocationAndRotation(Carrier->GetActorLocation()+FVector(0,0,25),Carrier->GetActorRotation(),false,nullptr,ETeleportType::TeleportPhysics);
 // Keep the shared HP, hit feedback and rewards, while this stage owns song scheduling.
 bFieldBulletHell=false;Super::Tick(DT);bFieldBulletHell=true;
 TArray<UStaticMeshComponent*> Parts;GetComponents(Parts);for(auto* C:Parts)C->SetVisibility(false);
 const double Now=GetWorld()->GetTimeSeconds();bSilenced=Now<QuietUntil || (Carrier && Carrier->bCarried);
 const bool CaptainVisible=Voice!=EBBSBStage01Voice::Captain || (Director && Director->Phase>=EBBSBStage01Phase::ReturnForReady);
 if(Voice==EBBSBStage01Voice::Captain) { SetActorHiddenInGame(!CaptainVisible);SetActorEnableCollision(CaptainVisible && !bDefeated); }
 SingerMesh->SetVisibility(!Carrier && CaptainVisible);
 if(bDefeated) { UpdateSongPresentation();SetActorHiddenInGame(false);if(!bReportedQuiet && Director) { bReportedQuiet=true;if(!Director->Quieted.Contains(SingerId))Director->ReportQuiet(this); }return; }
 UpdateSongPresentation();
 if(!bSongAttached)return;auto* P=Director->PlayerCharacter();if(FVector::DistSquared(GetActorLocation(),P->GetActorLocation())>FMath::Square(HearingRadius))return;
 AlertFieldPlayer(P);
 const float BPM=Voice==EBBSBStage01Voice::Trio ? Director->TrioBPM:Voice==EBBSBStage01Voice::Captain ? Director->CaptainBPM:SongBPM;
 const int64 Beat=FMath::FloorToInt64(Now*BPM/60.);if(Beat==LastBeat)return;LastBeat=Beat;
 const FVector Aim=P->GetActorTransform().TransformPosition(P->Combat->FieldProjectileHurtOffset);
 switch(Voice)
 {
 case EBBSBStage01Voice::Ambient:if((Beat+VoiceIndex*3)%8==0)FireAt(Aim,SongSpeed,SongDamage,1);break;
 case EBBSBStage01Voice::Guard:FireAt(Aim,SongSpeed,SongDamage,1);break;
 case EBBSBStage01Voice::Utility:if(Beat%2==0)FireAt(Aim,SongSpeed,SongDamage,1);break;
 case EBBSBStage01Voice::Roof:if((Beat+VoiceIndex)%4==0)FireAt(Aim,SongSpeed,SongDamage,1);break;
 case EBBSBStage01Voice::Trio:
 {
  // One aimed note per singer; the captain retains its separate boss patterns.
  if((Beat+VoiceIndex)%2==0)FireAt(Aim,SongSpeed,SongDamage,1);break;
 }
 case EBBSBStage01Voice::Captain:
  if(Beat%4==0)FireAt(Aim,SongSpeed,SongDamage,7,12);
  if(Beat%8==4)for(int32 I=0;I<12;++I)FireAt(GetActorLocation()+FVector(1800,0,0).RotateAngleAxis(I*30+Beat*5,FVector::UpVector),SongSpeed*.8f,SongDamage,1);
  break;
 }
}
void ABBSBStage01Singer::FireAt(FVector Point,float Speed,float Damage,int32 Count,float Spread)
{
 if(!GetWorld())return;const FVector Origin=(Carrier ? Carrier->BunniarMesh.Get():SingerMesh.Get())->Bounds.Origin+SongSurfaceNormal*12.f;const FRotator Base=(Point-Origin).Rotation();
 for(int32 I=0;I<Count;++I)
 {
  FActorSpawnParameters P;P.Owner=this;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Shot=GetWorld()->SpawnActor<ABBSBInkProjectile>(ABBSBInkProjectile::StaticClass(),Origin,FRotator::ZeroRotator,P);if(!Shot)continue;
  FRotator R=Base;R.Yaw+=(I-(Count-1)*.5f)*Spread;ShotDamage=Damage;
  const float Travel=4.0f;Shot->Initialize(nullptr,this,Origin+R.Vector()*Speed*Travel,EBBSBInkPattern::Tap,Travel,INDEX_NONE);Shot->Damage=Damage;Shot->CollisionRadius=10;Shot->bFieldProjectile=true;++SongShots;
 }
}
float ABBSBStage01Singer::TakeDamage(float Amount,const FDamageEvent& Event,AController* C,AActor* Causer)
{
 if(!Director || !Director->HasTool())return 0;
 if((Voice==EBBSBStage01Voice::Utility && Director->Phase<EBBSBStage01Phase::ShopBattle) || (Voice==EBBSBStage01Voice::Trio && Director->Phase!=EBBSBStage01Phase::Trio) || (Voice==EBBSBStage01Voice::Captain && Director->Phase!=EBBSBStage01Phase::Captain))return 0;
 const float Applied=Super::TakeDamage(Amount,Event,C,Causer);
 if(bDefeated && !bReportedQuiet) { bReportedQuiet=true;Director->ReportQuiet(this); }return Applied;
}
void ABBSBStage01Singer::SilenceFor(float Seconds) { QuietUntil=GetWorld()->GetTimeSeconds()+FMath::Max(0.f,Seconds);bSilenced=true; }
void ABBSBStage01Singer::Rearm() { bDefeated=bReportedQuiet=false;bSilenced=false;QuietUntil=-100;Health=MaximumHealth;ResetHealthFeedback();GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);LastBeat=MIN_int64; }
void ABBSBStage01Director::TickCombat(float DT)
{
 if(Phase!=EBBSBStage01Phase::Trio && Phase!=EBBSBStage01Phase::Captain) { bLaneWarning=false;if(LaneWarningMesh)LaneWarningMesh->SetVisibility(false);return; }
 if(Player->EquipmentGrowth->bConcertoActive)
 { bLaneWarning=bGrabWarning=false;if(LaneWarningMesh)LaneWarningMesh->SetVisibility(false);for(UStaticMeshComponent* C:WarningRing)if(C)C->SetVisibility(false);NextGrabAt=GetWorld()->GetTimeSeconds()+8;NextLaneAt=GetWorld()->GetTimeSeconds()+5;return; }
 const double Now=GetWorld()->GetTimeSeconds();if(bGrabActive || bLadderMoving)return;
 if(Phase==EBBSBStage01Phase::Trio)
 {
  if(!LaneWarningMesh)
  {
   LaneWarningMesh=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(LaneWarningMesh);LaneWarningMesh->SetupAttachment(RootComponent);LaneWarningMesh->SetAbsolute(true,true,true);LaneWarningMesh->SetMobility(EComponentMobility::Movable);LaneWarningMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));LaneWarningMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);LaneWarningMesh->SetCastShadow(false);LaneWarningMesh->RegisterComponent();BBSBCombatVisuals::Tint(LaneWarningMesh,FLinearColor(.70f,.27f,.39f));
  }
  if(!bLaneWarning && Now>=NextLaneAt)
  { WarningLane=FMath::FloorToInt(Now*TrioBPM/480.f)%3;bLaneWarning=true;LaneImpactAt=Now+1.1;NextLaneAt=Now+5.45; }
  LaneWarningMesh->SetWorldLocationAndRotation(TrioCenter+TrioFrame.RotateVector(FVector((WarningLane-1)*250,-150,-96)),TrioFrame);
  LaneWarningMesh->SetWorldScale3D(FVector(2.05f,22.f,.025f));LaneWarningMesh->SetVisibility(bLaneWarning);
  if(bLaneWarning && Now>=LaneImpactAt)
  {
   const FVector Local=TrioFrame.UnrotateVector(Player->GetActorLocation()-TrioCenter);
   if(FMath::Abs(Local.X-(WarningLane-1)*250)<115 && FMath::Abs(Local.Y+150)<1100 && FMath::Abs(Local.Z)<180 && !Player->FieldActions->IsDodgeInvulnerable())
    for(ABBSBStage01Singer* S:Singers)if(S && S->Voice==EBBSBStage01Voice::Trio && !S->bDefeated) { Player->Combat->TakeCombatDamage(8,S,false);break; }
   bLaneWarning=false;LaneWarningMesh->SetVisibility(false);
  }
 }
 if(!bGrabWarning && Now>=NextGrabAt)
 {
  bGrabWarning=true;GrabWarningAt=Now+1.35;GrabPoint=Player->GetActorLocation();GrabWarningRadius=Phase==EBBSBStage01Phase::Captain ? 270:230;
  Say(TEXT("분홍 원 밖으로 피하세요 · 붙잡히면 W A S D 리듬 패링"),1.35f);
 }
 if(bGrabWarning && Now>=GrabWarningAt)
 {
  bGrabWarning=false;NextGrabAt=Now+8;
  if(FVector::Dist2D(Player->GetActorLocation(),GrabPoint)<GrabWarningRadius && FMath::Abs(Player->GetActorLocation().Z-GrabPoint.Z)<180 && !Player->FieldActions->IsDodgeInvulnerable())
   for(ABBSBStage01Singer* S:Singers)if(S && !S->bDefeated && S->Voice==(Phase==EBBSBStage01Phase::Trio ? EBBSBStage01Voice::Trio:EBBSBStage01Voice::Captain)) { BeginGrab(S);break; }
 }
 if(WarningRing.IsEmpty())
 {
  for(int32 I=0;I<32;++I)
  {
   auto* C=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(C);C->SetupAttachment(RootComponent);C->SetAbsolute(true,true,true);C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->RegisterComponent();BBSBCombatVisuals::Tint(C,FLinearColor(.8,.24,.42));WarningRing.Add(C);
  }
 }
 for(int32 I=0;I<WarningRing.Num();++I)
 {
  auto* C=WarningRing[I].Get();C->SetVisibility(bGrabWarning);if(!bGrabWarning)continue;
  const float A=I*360.f/32;C->SetWorldLocationAndRotation(GrabPoint+FVector(GrabWarningRadius,0,-78).RotateAngleAxis(A,FVector::UpVector),FRotator(0,A+90,0));C->SetWorldScale3D(FVector(GrabWarningRadius*.0025f,.065,.035));
 }
}
bool ABBSBStage01Director::BeginGrab(ABBSBStage01Singer* Source)
{
 if(!Player || !Source || bGrabActive || Player->Combat->bDead || Player->Respawn->IsSequenceActive())return false;
 Player->Hunting->CloseInventory();Player->Interaction->EndDialogue();Player->Traversal->CancelWireRecall();Player->Traversal->DropParkour();Player->Traversal->EndGlide();
 Player->FieldActions->ResetActionState();Player->Combat->SetExternalRhythmGrab(true);
 bGrabActive=true;bGrabWarning=bLaneWarning=false;if(LaneWarningMesh)LaneWarningMesh->SetVisibility(false);GrabSource=Source;GrabEpoch=GetWorld()->GetTimeSeconds()+.85;GrabCount=Source->Voice==EBBSBStage01Voice::Captain ? 8:6;GrabNoteIndex=GrabSuccesses=0;GrabResolved.Init(false,GrabCount);GrabLocation=Player->GetActorLocation();
 Player->GetCharacterMovement()->StopMovementImmediately();if(auto* PC=Cast<APlayerController>(Player->GetController())) { PC->SetIgnoreMoveInput(true);bMoveIgnored=true; }
 Say(TEXT("붙잡혔다! 화살표가 닿는 박자에 W A S D · 카메라는 자유롭게"),6);return true;
}
uint8 ABBSBStage01Director::GrabMask(int32 I) const { static const uint8 A[]={1,2,4,8,3,12},B[]={1,2,4,8,3,7,12,15};return I>=0 && I<GrabCount ? (GrabCount==8 ? B[I]:A[I]):0; }
float ABBSBStage01Director::GrabNoteSeconds(int32 I) const { const float BPM=GrabCount==8 ? CaptainBPM:TrioBPM;return float(GrabEpoch+I*60.f/BPM-GetWorld()->GetTimeSeconds()); }
void ABBSBStage01Director::TickGrab(float DT)
{
 if(!bGrabActive || !Player)return;
 if(Player->Combat->bDead || Player->Respawn->IsSequenceActive() || !GrabSource.IsValid() || GrabSource->bDefeated || (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive())) { EndGrab();return; }
 Player->GetCharacterMovement()->StopMovementImmediately();
 for(int32 I=0;I<GrabCount;++I)
 {
  if(GrabResolved[I])continue;const float Seconds=GrabNoteSeconds(I);if(Seconds>GrabWindow)break;
  if(Seconds>=-GrabWindow && Player->Combat->ResolveGrabParry(GrabMask(I),float(GetWorld()->GetTimeSeconds()+Seconds),GrabWindow))
  { GrabResolved[I]=true;++GrabSuccesses;Player->Hunting->CollectEcstatic(6,true);Say(TEXT("리듬 패링! · 엑스태틱 +6"),.4f); }
  else if(Seconds<-GrabWindow) { GrabResolved[I]=true;Player->Combat->TakeCombatDamage(GrabDamage,GrabSource.Get(),false); }
  else break;
  GrabNoteIndex=I+1;
 }
 if(GrabNoteIndex>=GrabCount && GrabNoteSeconds(GrabCount-1)<-.35f) { EndGrab();NextGrabAt=GetWorld()->GetTimeSeconds()+8; }
}
void ABBSBStage01Director::EndGrab()
{
 const bool WasActive=bGrabActive;bGrabActive=bGrabWarning=bLaneWarning=false;if(LaneWarningMesh)LaneWarningMesh->SetVisibility(false);
 if(Player && WasActive)
 {
  Player->Combat->SetExternalRhythmGrab(false);
  if(bMoveIgnored)if(auto* PC=Cast<APlayerController>(Player->GetController()))PC->SetIgnoreMoveInput(false);
 }
 if(WasActive)bMoveIgnored=false;GrabSource.Reset();for(UStaticMeshComponent* C:WarningRing)if(C)C->SetVisibility(false);
}
void ABBSBStage01Director::ClearEncounterShots()
{
 for(TActorIterator<ABBSBInkProjectile> It(GetWorld());It;++It)
  if(auto* S=Cast<ABBSBStage01Singer>(It->Source.Get());S && S->Director==this && AllQuiet(S->Voice))It->Destroy();
}

void ABBSBStage01Singer::UpdateSongPresentation()
{
 auto* SongVisual=Carrier ? Carrier->BunniarMesh.Get():SingerMesh.Get();if(!SongVisual || !SongVisual->GetSkeletalMeshAsset())return;
 const bool Held=Carrier && Carrier->bCarried;const FVector Origin=Carrier ? Carrier->GetActorLocation():GetActorLocation();
 const double Now=GetWorld()->GetTimeSeconds();
 const float DT=LastSongPoseAt>=0 ? FMath::Max(0.f,float(Now-LastSongPoseAt)):0.f;
 if(LastSongPoseAt==Now)return;
 // Measure drift relative to moving support, so a travelling platform stays attached.
 const FVector SupportNow=SongSurface.IsValid() ? SongSurface->GetComponentTransform().TransformPosition(SongLocalPoint):SongSurfacePoint;
 const FVector SupportStep=SongSurface.IsValid() && PreviousSongSurface==SongSurface ? SupportNow-PreviousSupportPoint:FVector::ZeroVector;
 const bool Moving=DT>0 && (Origin-PreviousSongOrigin-SupportStep).Size()>FMath::Max(.05f,6.f*DT);
 const bool Initialized=LastSongPoseAt>=0;
 LastSongPoseAt=Now;PreviousSongOrigin=Origin;
 if(!Held && Now>=NextSurfaceProbe && (!SongSurface.IsValid() || Moving || FVector::DistSquared(Origin,SupportNow)>FMath::Square(110.f)))
 {
  NextSurfaceProbe=Now+.25;FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBSongAttachment),false,this);Query.AddIgnoredActor(Carrier);
  for(TActorIterator<ABBSBStage01Singer> It(GetWorld());It;++It) { Query.AddIgnoredActor(*It);Query.AddIgnoredActor(It->Carrier); }
  if(Director)Query.AddIgnoredActor(Director->PlayerCharacter());FHitResult Hit;
  if(GetWorld()->LineTraceSingleByObjectType(Hit,Origin+FVector::UpVector*80.f,Origin-FVector::UpVector*400.f,BBSBCombatVisuals::GeometryTypes(),Query))
  {
   SongSurface=Hit.GetComponent();SongSurfacePoint=Hit.ImpactPoint;SongSurfaceNormal=Hit.ImpactNormal;
   SongLocalPoint=SongSurface->GetComponentTransform().InverseTransformPosition(SongSurfacePoint);SongLocalNormal=SongSurface->GetComponentTransform().InverseTransformVectorNoScale(SongSurfaceNormal);
  }
 }
 if(SongSurface.IsValid()) { SongSurfacePoint=SongSurface->GetComponentTransform().TransformPosition(SongLocalPoint);SongSurfaceNormal=SongSurface->GetComponentTransform().TransformVectorNoScale(SongLocalNormal).GetSafeNormal(); }
 PreviousSongSurface=SongSurface;PreviousSupportPoint=SongSurfacePoint;
 const bool NearSupport=SongSurface.IsValid() && FVector::DistSquared(Origin,SongSurfacePoint)<FMath::Square(120.f);
 bSongAttached=!Held && !Moving && NearSupport && IsSongEnabled();
 if(Held)
 {
  const FTransform BodyFrame=Carrier->GetActorTransform();
  SongVisual->SetWorldLocationAndRotation(BodyFrame.TransformPosition(FVector(0,0,-40)),BodyFrame.GetRotation());return;
 }
 const FVector N=SongSurfaceNormal.IsNearlyZero() ? FVector::UpVector:SongSurfaceNormal;
 // A loose moving Bunniar follows its actual body every frame. The throttled
 // ground probe only selects support; it must not quantize visible movement.
 const float RestHeight=Carrier ? Carrier->Body->Bounds.BoxExtent.Z:60.f;
 // Project onto the cached support plane at the body's current location.
 // Keeping the old probe's XY until its next refresh creates a lateral catch-up
 // even when the floor is perfectly flat and the object only moved a little.
 const FVector Point=!Moving && NearSupport ? FVector::PointPlaneProject(Origin,SongSurfacePoint,N):Origin-N*RestHeight;
 const FBoxSphereBounds Bounds=SongVisual->GetSkeletalMeshAsset()->GetBounds();const FVector Scale=SongVisual->GetComponentScale();
 const FVector Face=FVector::RightVector;
 const FQuat Model=FRotationMatrix::MakeFromXZ(Face,FVector::UpVector).ToQuat();
 const FVector Tangent=FMath::Abs(N.Z)>.9f ? FVector::ForwardVector:FVector::UpVector;
 const FQuat Frame=FRotationMatrix::MakeFromXZ(N,Tangent).ToQuat();
 const FQuat Q=Frame*Model.Inverse();
 const FVector Center=Bounds.Origin*Scale;const float Back=FVector::DotProduct(Center,Face)-Bounds.BoxExtent.Y*FMath::Abs(Scale.Y);
 const FVector Tangential=Center-Face*FVector::DotProduct(Center,Face);
 FVector Location;FQuat Rotation;
 if(bSongAttached) { Location=Point+N*(2.f-Back)-Q.RotateVector(Tangential);Rotation=Q; }
 else
 {
  const float A=Now*2.6f+VoiceIndex*1.3f;const FVector Side=FVector::CrossProduct(Tangent,N).GetSafeNormal();
  const FVector Hover=Point+N*(72.f+FMath::Sin(Now*3.f)*5.f)+(Tangent*FMath::Cos(A)+Side*FMath::Sin(A))*18.f;
  Rotation=FRotator(0,FMath::RadiansToDegrees(A),0).Quaternion();Location=Hover-Rotation.RotateVector(Center);
 }
 const float Blend=Initialized ? 1.f-FMath::Exp(-24.f*DT):1.f;
 SongVisual->SetWorldLocationAndRotation(FMath::Lerp(SongVisual->GetComponentLocation(),Location,Blend),FQuat::Slerp(SongVisual->GetComponentQuat(),Rotation,Blend).GetNormalized());
}
