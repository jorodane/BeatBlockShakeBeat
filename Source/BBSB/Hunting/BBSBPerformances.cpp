#include "HAL/PlatformTime.h"
#include "UI/BBSBPaperTheme.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBItemIcon.h"
#include "Hunting/BBSBHUDTypography.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "BBSBCharacter.h"
#include "Field/BBSBFieldActions.h"
#include "Field/BBSBWeaponAnimation.h"
#include "Field/BBSBBladeTrail.h"
#include "Field/BBSBBulletClearFX.h"
#include "CanvasItem.h"
#include "RenderUtils.h"
#include "Character/BBSBTraversal.h"
#include "Interaction/BBSBInteraction.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBDamageEvents.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"

void UBBSBEquipmentGrowthComponent::AddConcertoCharge(float Amount)
{
 if(!FMath::IsFinite(Amount) || Amount<=0 || bConcertoActive)return;
 const float Before=ConcertoCharge;ConcertoCharge=FMath::Clamp(ConcertoCharge+Amount,0.f,100.f);
 if(ConcertoCharge!=Before) { ConcertoGain.Gain(Before,ConcertoCharge,GetWorld()->GetTimeSeconds());Changed(); }
}
float UBBSBEquipmentGrowthComponent::DisplayedConcertoCharge() const
{ return ConcertoGain.Initialized ? FMath::Clamp(float(ConcertoGain.Value(GetWorld()->GetTimeSeconds())),0.f,ConcertoCharge):ConcertoCharge; }

void UBBSBEquipmentGrowthComponent::NotifyWeaponHit(const FBBSBWeaponSnapshot& S,AActor* Target,float Damage,const FVector& Point,bool Support)
{
 if(!Player || !Target || Damage<=0 || !S.InstanceId.IsValid())return;
 EnsureInitialized();if(!bConcertoActive)AddConcertoCharge(Damage*Settings->ChargePerDamage*(Support ? Settings->SupportChargeScale:1.f));
 auto* Enemy=Cast<ABBSBInkEnemy>(Target);
 for(const auto& Effect:S.Effects)
 {
  if(Effect.Effect==EBBSBOrbEffect::Element && Enemy && !Enemy->bDefeated)
  {
   auto& State=Statuses.FindOrAdd(Enemy);if(State.OriginalSpeed<=0)State.OriginalSpeed=Enemy->GetCharacterMovement()->MaxWalkSpeed;
   State.BurnDPS=FMath::Max(State.BurnDPS,Effect.BurnDamagePerSecond);State.Slow=FMath::Min(State.Slow,FMath::Clamp(Effect.SlowMultiplier,.1f,1.f));State.Until=FMath::Max(State.Until,GetWorld()->GetTimeSeconds()+Effect.StatusSeconds);
   if(State.NextBurn<=0)State.NextBurn=GetWorld()->GetTimeSeconds()+.5;
  }
  if(Effect.Effect!=EBBSBOrbEffect::Ensemble || Support || bConcertoActive)continue;
  bool Condition=Effect.Condition==EBBSBOrbCondition::Always;
  if(Effect.Condition==EBBSBOrbCondition::FullHealth)Condition=Player->Combat->Health>=Player->Combat->MaximumHealth;
  if(Effect.Condition==EBBSBOrbCondition::Airborne)Condition=Player->GetCharacterMovement()->IsFalling();
  if(Effect.Condition==EBBSBOrbCondition::TargetLocked)Condition=Player->Combat->HasTargetLock();
  if(Effect.Condition==EBBSBOrbCondition::OnBeat) { const double B=GetWorld()->GetTimeSeconds()*Player->Combat->FieldBPM/60.;Condition=FMath::Abs(B-FMath::RoundToDouble(B))<.15; }
  if(!Condition)continue;
  const int64 Beat=FMath::CeilToDouble(MusicBeat()+.001);
  int32 Seat=0;
  for(const auto& E:Player->Hunting->EquippedItems)if(E.Item && E.Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon && E.GridY<UBBSBFieldActionComponent::EquipmentWheelSlotCount && E.InstanceId!=S.InstanceId)
  {
   const auto Other=WeaponSnapshot(E.InstanceId);
   if(Effect.bPartnerNeedsEnsembleOrb && !Other.Effects.ContainsByPredicate([](const auto& O){return O.Effect==EBBSBOrbEffect::Ensemble;}))continue;
   const bool Queued=PendingSupport.ContainsByPredicate([&](const auto& P){return P.Weapon.InstanceId==Other.InstanceId && P.Beat==Beat;});
   if(!Queued)PendingSupport.Add({Other,Point,Beat});
  }
 }
}

void UBBSBEquipmentGrowthComponent::TickStatuses(float DT)
{
 const double Now=GetWorld()->GetTimeSeconds();
 for(auto It=Statuses.CreateIterator();It;++It)
 {
  auto* E=It.Key().Get();auto& S=It.Value();
  if(!E || E->bDefeated || Now>=S.Until)
  { if(E) { E->GrowthSlowMultiplier=1;E->GetCharacterMovement()->MaxWalkSpeed=S.OriginalSpeed; }It.RemoveCurrent();continue; }
  E->GrowthSlowMultiplier=S.Slow;E->GetCharacterMovement()->MaxWalkSpeed=S.OriginalSpeed*S.Slow;
  if(S.BurnDPS>0 && Now>=S.NextBurn)
  { S.NextBurn=Now+.5;E->TakeDamage(S.BurnDPS*.5f,FBBSBStatusDamageEvent(),Player->GetController(),Player); }
 }
}

bool UBBSBEquipmentGrowthComponent::TryConcerto()
{
 EnsureInitialized();
 if(!Player || !bProfileReady || bConcertoActive || Player->Combat->bDead || Player->Combat->bExternalRhythmGrab || Player->Interaction->bDialogueActive || Player->Traversal->IsWireTraversalActive() || Player->Traversal->IsParkourActive() || Player->FieldActions->bEquipmentInputBlocked)return false;
 if(ConcertoCharge<100.f) { Feedback=FString::Printf(TEXT("협주곡 %.0f%%"),ConcertoCharge);return false; }
 FGuid Main=Player->FieldActions->EquippedWeaponInstance;
 if(!Player->FieldActions->EquipmentWheelInstances.Contains(Main))Main=Player->FieldActions->LastCombatWeaponInstance;
 auto MainSnapshot=WeaponSnapshot(Main);if(!MainSnapshot.InstanceId.IsValid()) { Feedback=TEXT("협주곡을 연주할 무기를 장착해 주세요");return false; }
 ConcertoWeapons.Reset();ConcertoWeapons.Add(MainSnapshot);
 for(const auto& E:Player->Hunting->EquippedItems)if(E.Item && E.Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon && E.GridY>=0 && E.GridY<UBBSBFieldActionComponent::EquipmentWheelSlotCount && E.InstanceId!=Main)ConcertoWeapons.Add(WeaponSnapshot(E.InstanceId));
 Player->FieldActions->ResetActionState();Player->Traversal->EndGlide();
 ConcertoMusicStart=MusicBeat();ConcertoCharge=0;ConcertoGain.Reset(0);bConcertoActive=true;ConcertoMainWeapon=Main;ConcertoStartedAt=GetWorld()->GetTimeSeconds();NextConcertoMainAt=ConcertoStartedAt;NextConcertoSupportBeat=FMath::CeilToDouble(MusicBeat()+.001);++ConcertoActivations;Changed();
 ConcertoDuration=MainSnapshot.Main.Pattern==EBBSBPerformancePattern::Spiral ? FMath::Max(1.f,MainSnapshot.Main.Duration):Settings->Duration;
 ConcertoElapsed=FaceSlowElapsed=0;ConcertoPlayRate=1;ConcertoClearStart=BulletsCleared;
 for(TActorIterator<ABBSBInkEnemy> It(GetWorld());It;++It)SuspendConcertoEnemy(*It);
 ConcertoOrigin=Player->GetActorLocation();ConcertoFrame=FRotator(0,Player->GetActorRotation().Yaw,0).Quaternion();
 CameraPreset=MainSnapshot.Main.CameraPreset;bConcertoCameraActive=CameraPreset && CameraPreset->bEnabled && CameraPreset->Keys.Num()>1;bCameraReturning=bCameraSampled=false;WhirlwindRadius=0;
 Player->ConsumeMovementInputVector();auto* Move=Player->GetCharacterMovement();Move->Velocity.X=Move->Velocity.Y=0;
 if(Move->IsFalling())Player->Traversal->BeginAirWeaponHang(ConcertoDuration+.49f);
 UAnimSequence* Animation=MainSnapshot.Main.ConductorAnimation;
 if(!Animation && Player->FieldActions->ActiveWeaponAnimationSet)
  if(const auto* Clip=Player->FieldActions->ActiveWeaponAnimationSet->FindClip(EBBSBFieldGesture::Beat,0,1))Animation=Clip->CharacterAnimation;
 if(Animation)if(auto* Anim=Player->GetMesh()->GetAnimInstance())ConcertoMontage=Anim->PlaySlotAnimationAsDynamicMontage(Animation,TEXT("DefaultSlot"),.08f,.12f,1.f,FMath::Max(1,FMath::CeilToInt(ConcertoDuration/FMath::Max(.05f,Animation->GetPlayLength()))));
 Feedback=TEXT("협주곡 · ")+MainSnapshot.Definition->DisplayName.ToString();ClearNearbyBullets();return true;
}
void UBBSBEquipmentGrowthComponent::EmitBulletClear(const FVector& Point,const FLinearColor& Tint)
{
 if(!Player)return;
 if(!BulletClearFX)
 {
  BulletClearFX=NewObject<UBBSBBulletClearFXComponent>(Player,TEXT("ConcertoBulletClearFX"));Player->AddInstanceComponent(BulletClearFX);
  BulletClearFX->SetupAttachment(Player->GetRootComponent());BulletClearFX->SetAbsolute(true,true,true);
  BulletClearFX->EffectMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BBSB/VFX/Weapons/M_SwordBladeTrail.M_SwordBladeTrail"));BulletClearFX->RegisterComponent();
 }
 BulletClearFX->AddBurst(Point,Tint);
}
void UBBSBEquipmentGrowthComponent::ClearNearbyBullets()
{
 if(!Player || (ConcertoWeapons.Num()>0 && ConcertoWeapons[0].Main.Pattern==EBBSBPerformancePattern::Spiral))return;
 for(TActorIterator<ABBSBInkProjectile> It(GetWorld());It;++It)if(FVector::DistSquared(It->GetActorLocation(),Player->GetActorLocation())<=FMath::Square(Settings->BulletClearRadius))
 { EmitBulletClear(It->GetActorLocation(),It->Source.IsValid() ? It->Source->Tint:FLinearColor(.6f,.9f,.85f));It->bResolved=true;It->Destroy();++BulletsCleared; }
}
void UBBSBEquipmentGrowthComponent::SuspendConcertoEnemy(ABBSBInkEnemy* Enemy)
{ if(bConcertoActive && IsValid(Enemy) && !Enemy->bDefeated) { Enemy->SetConcertoSuspended(true,this);SuspendedEnemies.AddUnique(Enemy); } }
void UBBSBEquipmentGrowthComponent::CancelConcerto()
{
 for(const auto& Weak:SuspendedEnemies)if(auto* Enemy=Weak.Get())Enemy->SetConcertoSuspended(false,this);
 SuspendedEnemies.Reset();ConcertoPlayRate=1;
 if(bConcertoActive && Player)
 {
  if(ConcertoMontage)if(auto* Anim=Player->GetMesh()->GetAnimInstance())Anim->Montage_Stop(.12f,ConcertoMontage);
  Player->Traversal->EndAirWeaponHang();
 }
 if(bConcertoActive && bConcertoCameraActive && bCameraSampled) { bCameraReturning=true;CameraReturnAt=GetWorld()->GetTimeSeconds();CameraReturnFrom=LastCinematicView; }
 else if(!bCameraReturning)bConcertoCameraActive=false;
 ConcertoMontage=nullptr;bConcertoActive=false;ConcertoWeapons.Reset();PendingSupport.Reset();
 for(auto& P:Performances)if(P.Visual.IsValid())P.Visual->Destroy();Performances.Reset();
}

void UBBSBEquipmentGrowthComponent::EmitPerformance(const FBBSBWeaponSnapshot& Weapon,const FBBSBPerformanceRole& Role,const FVector& Point,bool Ultimate,bool Main,int32 Seat)
{
 if(!Weapon.InstanceId.IsValid() || !Player)return;
 FPerformance P;P.Weapon=Weapon;P.Role=Role;P.Role.Duration=FMath::Max(.12f,Role.Duration);P.Origin=Player->GetActorLocation()+FVector(0,0,60);P.Target=Point;P.StartedAt=GetWorld()->GetTimeSeconds();P.Main=Main;P.Ultimate=Ultimate;P.Seat=Seat;
 const FVector Delta=P.Target-P.Origin;if(Delta.Size()>Role.Range)P.Target=P.Origin+Delta.GetSafeNormal()*Role.Range;
 FActorSpawnParameters Params;Params.Owner=Player;Params.ObjectFlags|=RF_Transient;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Visual=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);
 if(Visual)
 {
  auto* Root=NewObject<USceneComponent>(Visual);Visual->AddInstanceComponent(Root);Visual->SetRootComponent(Root);Root->RegisterComponent();
  auto* Mesh=NewObject<UStaticMeshComponent>(Visual);Visual->AddInstanceComponent(Mesh);Mesh->SetupAttachment(Root);Mesh->SetStaticMesh(Weapon.Mesh ? Weapon.Mesh.Get():LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);Mesh->SetCanEverAffectNavigation(false);Mesh->RegisterComponent();
  if(Weapon.Mesh)Mesh->SetRelativeLocation(-Weapon.Mesh->GetBoundingBox().GetCenter());else { Mesh->SetRelativeScale3D(FVector(.05f,.06f,1.1f));BBSBCombatVisuals::Tint(Mesh,FLinearColor(.85f,.54f,.69f)); }
  P.Visual=Visual;Visual->SetActorScale3D(FVector(Main && Role.Pattern==EBBSBPerformancePattern::Spiral ? Role.SpiralBladeScale:Main ? 1.2f:.8f));
  if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/BBSB/VFX/Weapons/M_SwordBladeTrail.M_SwordBladeTrail")))
  {
   auto* Trail=NewObject<UBBSBBladeTrailComponent>(Visual);Visual->AddInstanceComponent(Trail);Trail->SetupAttachment(Root);Trail->SetAbsolute(true,true,true);Trail->TrailMaterial=Material;Trail->Lifetime=Main ? .4f:.2f;Trail->Color=Main ? FLinearColor(1.f,.76f,.42f,.7f):FLinearColor(.63f,.92f,.84f,.65f);Trail->RegisterComponent();
   if(Main && Role.Pattern==EBBSBPerformancePattern::Spiral)for(int32 Layer=0;Layer<3;++Layer)
   {
    auto* Wind=NewObject<UBBSBBladeTrailComponent>(Visual);Visual->AddInstanceComponent(Wind);Wind->SetupAttachment(Root);Wind->SetAbsolute(true,true,true);
    Wind->TrailMaterial=Material;Wind->bSoftEdges=true;Wind->Lifetime=.15f;Wind->Color=Layer==1 ? FLinearColor(1.f,.79f,.43f,.7f):FLinearColor(.83f,.96f,1.f,.64f);
    Wind->ComponentTags.Add(TEXT("ConcertoWind"));Wind->RegisterComponent();
   }
  }
 }
 if(Performances.Num()>=24) { if(Performances[0].Visual.IsValid())Performances[0].Visual->Destroy();Performances.RemoveAt(0); }
 Performances.Add(P);if(Main)++MainPerformances;else ++SubPerformances;
}

void UBBSBEquipmentGrowthComponent::PerformanceDamage(FPerformance& P)
{
 const FVector Origin=P.Origin,Direction=(P.Target-Origin).GetSafeNormal();
 for(TActorIterator<ABBSBInkEnemy> It(GetWorld());It;++It)
 {
  auto* Enemy=*It;if(Enemy->bDefeated)continue;const FVector Point=Enemy->GetAimPoint();
  bool Hit=false;
  switch(P.Role.Pattern)
  {
   case EBBSBPerformancePattern::Thrust:case EBBSBPerformancePattern::Volley:Hit=FVector::DistSquared(Point,FMath::ClosestPointOnSegment(Point,Origin,P.Target))<=FMath::Square(P.Role.Radius);break;
   case EBBSBPerformancePattern::Orbit:Hit=FVector::DistSquared(Point,Origin)<=FMath::Square(P.Role.Radius);break;
   default:Hit=FVector::DistSquared(Point,P.Target)<=FMath::Square(P.Role.Radius);break;
  }
  if(!Hit || FVector::DistSquared(Point,Origin)>FMath::Square(P.Role.Range))continue;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBPerformanceCover),false,Player);Q.AddIgnoredActor(Enemy);for(const auto& V:Performances)if(V.Visual.IsValid())Q.AddIgnoredActor(V.Visual.Get());
  FHitResult Cover;if(GetWorld()->LineTraceSingleByObjectType(Cover,Origin,Point,BBSBCombatVisuals::GeometryTypes(),Q))continue;
  const float Scale=P.Ultimate ? (P.Main ? Settings->MainDamageMultiplier:Settings->SubDamageMultiplier):1.f;
  const float Damage=(Player->FieldActions->BaseDamage*P.Weapon.DamageMultiplier+P.Weapon.FlatDamage)*P.Role.DamageMultiplier*Scale*ConditionalDamageMultiplier(P.Weapon);
  const float Actual=UGameplayStatics::ApplyDamage(Enemy,Damage,Player->GetController(),Player,nullptr);
  NotifyWeaponHit(P.Weapon,Enemy,Actual,Point,true);
 }
}

void UBBSBEquipmentGrowthComponent::TickPerformances(float DT)
{
 if(!Player)return;const double Now=GetWorld()->GetTimeSeconds();
 if(bConcertoActive)
 {
  ConcertoPlayRate=1;
  if(CameraPreset && bConcertoCameraActive && ConcertoProgress()>=CameraPreset->FaceSlowStart && FaceSlowElapsed<FMath::Clamp(CameraPreset->FaceSlowSeconds,0.f,.49f))
  {
   const float Portion=FMath::Min(DT,FMath::Clamp(CameraPreset->FaceSlowSeconds,0.f,.49f)-FaceSlowElapsed);
   FaceSlowElapsed+=Portion;ConcertoPlayRate=1-(1-FMath::Clamp(CameraPreset->FaceSlowRate,.7f,1.f))*Portion/FMath::Max(.0001f,DT);
  }
  ConcertoElapsed+=DT*ConcertoPlayRate;
  if(ConcertoMontage)if(auto* Anim=Player->GetMesh()->GetAnimInstance())Anim->Montage_SetPlayRate(ConcertoMontage,ConcertoPlayRate);
  if(ConcertoElapsed>=ConcertoDuration) { CancelConcerto();return; }
  ClearNearbyBullets();
  FVector Aim=Player->FieldActions->VisualAimPoint();if(Player->Combat->HasTargetLock())Aim=Player->Combat->FieldLockedTarget->GetAimPoint();
  if(ConcertoWeapons.Num()>0 && Now>=NextConcertoMainAt)
  { const auto& S=ConcertoWeapons[0];NextConcertoMainAt=S.Main.Pattern==EBBSBPerformancePattern::Spiral ? ConcertoStartedAt+ConcertoDuration+1:Now+FMath::Max(.1f,S.Main.Interval);EmitPerformance(S,S.Main,Aim,true,true); }
  const int64 Beat=FMath::FloorToDouble(MusicBeat());
  if(Beat>=NextConcertoSupportBeat)
  { NextConcertoSupportBeat=Beat+1;LastSupportBeat=Beat;++SupportBeatEvents;
   for(int32 I=1;I<ConcertoWeapons.Num();++I)EmitPerformance(ConcertoWeapons[I],NormalBeatRole(ConcertoWeapons[I]),Aim,true,false,I);
  }
 }
 const int64 Beat=FMath::FloorToDouble(MusicBeat());
 if(Beat!=LastMusicBeat)
 {
  LastMusicBeat=Beat;
  for(int32 I=PendingSupport.Num()-1;I>=0;--I)if(PendingSupport[I].Beat<=Beat)
  { auto P=PendingSupport[I];PendingSupport.RemoveAt(I);if(Player->FieldActions->EquipmentWheelInstances.Contains(P.Weapon.InstanceId)) { auto Role=NormalBeatRole(P.Weapon);Role.DamageMultiplier*=Settings->EnsembleDamageMultiplier;EmitPerformance(P.Weapon,Role,P.Target,false,false);++EnsembleAttacks;LastSupportBeat=Beat; } }
 }
 for(int32 I=Performances.Num()-1;I>=0;--I)
 {
  auto& P=Performances[I];const float Age=P.Main && P.Ultimate && P.Role.Pattern==EBBSBPerformancePattern::Spiral ? ConcertoElapsed:Now-P.StartedAt,T=FMath::Clamp(Age/P.Role.Duration,0.f,1.f);
  if(T>=1) { if(P.Visual.IsValid())P.Visual->Destroy();Performances.RemoveAt(I);continue; }
  if(P.Role.Pattern==EBBSBPerformancePattern::Spiral)
  {
   const float Start=FMath::Clamp(P.Role.WarmupFraction,0.f,.8f),Progress=FMath::Clamp((T-Start)/(1-Start),0.f,1.f);
   const FVector Position=SpiralPosition(P,Progress);WhirlwindBladePosition=Position;WhirlwindRadius=FVector::Dist2D(Position,ConcertoOrigin);
   if(T>=Start) { SweepSpiral(P,P.PreviousSpiral,Progress);P.PreviousSpiral=Progress; }
   if(P.Visual.IsValid())
   { const float A=-Progress*P.Role.SpiralTurns*2*PI+PI*.5f;const FVector Tangent=ConcertoFrame.RotateVector(FVector(FMath::Sin(A),-FMath::Cos(A),.08f));
    const FQuat Facing=FRotationMatrix::MakeFromZX(Tangent.GetSafeNormal(),FVector::UpVector).ToQuat()*FQuat(FVector::UpVector,Progress*PI*12);
    P.Visual->SetActorLocationAndRotation(Position,Facing);
    TArray<UBBSBBladeTrailComponent*> Trails;P.Visual->GetComponents(Trails);int32 Layer=0;
    for(auto* Trail:Trails)if(Trail->ComponentHasTag(TEXT("ConcertoWind")))
     Trail->UpdateWindSheet(ConcertoOrigin,ConcertoFrame,WhirlwindRadius,A,Layer++,P.Role.SpiralWindWidth,T>=Start ? FMath::Min(1.f,Progress*15.f):0.f);
    else Trail->AddBladeSample(Position-Tangent*P.Role.SweepRadius,Position+Tangent*P.Role.SweepRadius);
   }
   continue;
  }
  if(T>=(P.Role.Pattern==EBBSBPerformancePattern::Orbit ? 0.f:P.Role.Pattern==EBBSBPerformancePattern::Volley || P.Role.Pattern==EBBSBPerformancePattern::Thrust ? .65f:.3f) && (P.Main ? Now-P.LastDamageAt>=FMath::Max(.1f,P.Role.Interval):P.LastDamageAt<0)) { P.LastDamageAt=Now;PerformanceDamage(P); }
  if(!P.Visual.IsValid())continue;
  FVector Position=FMath::Lerp(P.Origin,P.Target,FMath::SmoothStep(0.f,.65f,T));FRotator Rotation=(P.Target-P.Origin).Rotation();
  switch(P.Role.Pattern)
  {
   case EBBSBPerformancePattern::Slash: { const FVector Side=FVector::CrossProduct(FVector::UpVector,(P.Target-P.Origin).GetSafeNormal()).GetSafeNormal();Position=P.Target+Side*(FMath::Cos(T*PI)*P.Role.Radius*.7f)+FVector::UpVector*(FMath::Sin(T*PI)*P.Role.Radius*.5f);Rotation.Roll=T*360;break; }
   case EBBSBPerformancePattern::Orbit: { const float A=T*2*PI+P.Seat;Position=P.Origin+FVector(FMath::Cos(A),FMath::Sin(A),.25f)*P.Role.Radius;Rotation.Yaw=FMath::RadiansToDegrees(A)+90;break; }
   case EBBSBPerformancePattern::Pulse:Position=P.Target+FVector::UpVector*(FMath::Sin(T*PI)*80);Rotation.Roll=T*360;break;
   case EBBSBPerformancePattern::Volley:Position+=FVector::UpVector*(FMath::Sin(T*PI)*100);Rotation.Roll=T*720;break;
   default:break;
  }
  P.Visual->SetActorLocationAndRotation(Position,Rotation);
  if(auto* Trail=P.Visual->FindComponentByClass<UBBSBBladeTrailComponent>())Trail->AddBladeSample(Position-FVector::UpVector*40,Position+FVector::UpVector*40);
 }
}

void UBBSBEquipmentGrowthComponent::DrawGrowthHUD(AHUD* HUD,UCanvas* Canvas) const
{
 const bool Shown=HUD && Canvas && Player && !Player->Hunting->bInventoryOpen && !Player->Interaction->bDialogueActive;
 const float Entry=GrowthHUDReveal.Sample(Shown,FPlatformTime::Seconds(),.035f);if(!Shown)return;
 const float BaseScale=FMath::Clamp(Canvas->SizeX/1280.f,.7f,1.5f),S=BaseScale*(.88f+.12f*Entry),R=40*S;
 const FVector2D Centre(Canvas->SizeX*.5f,Canvas->SizeY-89*BaseScale+110*BaseScale*(1-Entry));
 const FLinearColor Ink(.10f,.075f,.13f,.94f),Paper(.97f,.92f,.86f),Gold(1.f,.76f,.30f),Track(.38f,.33f,.42f,.95f);
 const float Value=FMath::Clamp(bConcertoActive ? 1-ConcertoProgress():ConcertoCharge/100.f,0.f,1.f);
 const bool Ready=ConcertoCharge>=100.f && !bConcertoActive;
 auto Ring=[&](float Inner,float Outer,float Fraction,const FLinearColor& Color)
 {
  TArray<FCanvasUVTri> Triangles;constexpr int32 Segments=128;
  const int32 Count=FMath::CeilToInt(Fraction*Segments);
  for(int32 I=0;I<Count;++I)
  {
   const float A=-PI*.5f+2*PI*float(I)/Segments,B=-PI*.5f+2*PI*FMath::Min(Fraction,float(I+1)/Segments);
   const FVector2D U(FMath::Cos(A),FMath::Sin(A)),V(FMath::Cos(B),FMath::Sin(B));
   auto Tri=[&](FVector2D X,FVector2D Y,FVector2D Z){FCanvasUVTri T;T.V0_Pos=X;T.V1_Pos=Y;T.V2_Pos=Z;T.V0_UV=T.V1_UV=T.V2_UV=FVector2D::ZeroVector;T.V0_Color=T.V1_Color=T.V2_Color=Color;Triangles.Add(T);};
   Tri(Centre+U*Inner,Centre+U*Outer,Centre+V*Outer);Tri(Centre+U*Inner,Centre+V*Outer,Centre+V*Inner);
  }
  if(!Triangles.IsEmpty()){FCanvasTriangleItem Item(Triangles,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);}
 };
 Ring(0,R+3*S,1,Ink);Ring(0,R-7*S,1,FLinearColor(.14f,.10f,.12f,.96f));Ring(R-7*S,R+2*S,1,Paper);Ring(R-5.5f*S,R+.5f*S,1,Track);
 // Cyan marks the immediately available gain. The original colour catches up after a pause.
 Ring(R-5.5f*S,R+.5f*S,Value,bConcertoActive ? Gold:FLinearColor(.37f,.84f,1.f));
 Ring(R-5.5f*S,R+.5f*S,bConcertoActive ? Value:DisplayedConcertoCharge()/100.f,Ready || bConcertoActive ? Gold:Paper);
 auto Text=[&](FString Value,FVector2D At,float Scale,FLinearColor Color)
 {
  FCanvasTextItem Item(At,FText::FromString(Value),GEngine->GetMediumFont(),Color);
  Item.Scale=FVector2D(Scale*S);Item.bCentreX=Item.bCentreY=true;Item.bOutlined=true;Item.OutlineColor=Ink;Canvas->DrawItem(Item);
 };
 BBSBItemIcon::DrawCanvas(HUD,Canvas,nullptr,TEXT("Concerto"),Centre+FVector2D(0,Ready ? 0:-10*S),(Ready ? 21:13)*S,1,S,Ready ? Gold:Paper);
 if(!Ready)Text(FString::Printf(TEXT("%d%%"),bConcertoActive ? FMath::CeilToInt(Value*100):FMath::FloorToInt(Value*100)),Centre+FVector2D(0,14*S),1.05f,Paper);
 const FVector2D KeyAt=Centre-FVector2D(0,R);
 HUD->DrawRect(Ink,KeyAt.X-17*S,KeyAt.Y-9*S,34*S,18*S);Text(TEXT("[Q]"),KeyAt,.85f,Ready ? Gold:Paper*.75f);
 FGuid Main=bConcertoActive ? ConcertoMainWeapon:Player->FieldActions->EquippedWeaponInstance;
 if(!Player->FieldActions->EquipmentWheelInstances.Contains(Main))Main=Player->FieldActions->LastCombatWeaponInstance;
 const auto Weapon=WeaponSnapshot(Main);
 const FString Name=Weapon.Definition ? Weapon.Definition->DisplayName.ToString()+TEXT(" 협주곡"):TEXT("협주곡");
 FCanvasTextItem Caption(Centre+FVector2D(0,R+14*S),FText::FromString(Name),BBSBHUDTypography::NumberFont(12*S),Paper);
 Caption.bCentreX=Caption.bCentreY=true;Caption.bOutlined=true;Caption.OutlineColor=Ink;Canvas->DrawItem(Caption);
 if(bConcertoActive)Text(FString::Printf(TEXT("탄막 소거 %d"),BulletsCleared-ConcertoClearStart),Centre+FVector2D(0,R+32*S),.72f,Gold);
 // Currency is anchored above the concerto HUD, independent of equipment rotation.
 const auto* Hunting=Player->Hunting.Get();const double Age=GetWorld()->GetTimeSeconds()-Hunting->LastNotesGainAt;
 const float Pulse=1.f+.18f*FMath::Square(1.f-FMath::Clamp(float(Age/.34),0.f,1.f));
 const FString Notes=FText::AsNumber(Hunting->Notes).ToString();const auto NotesFont=BBSBHUDTypography::NumberFont(14*S*Pulse);
 const float NoteWidth=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Notes,NotesFont).X;
 const FVector2D NotesAt=Centre-FVector2D(0,R+47*S);const float GroupWidth=NoteWidth+29*S;
 BBSBItemIcon::DrawCanvas(HUD,Canvas,nullptr,TEXT("Note"),NotesAt-FVector2D(GroupWidth*.5f-9*S,0),9*S,1,S,Paper);
 FCanvasTextItem NotesText(NotesAt+FVector2D(14.5f*S,0),FText::FromString(Notes),NotesFont,Paper);
 NotesText.bCentreX=NotesText.bCentreY=true;NotesText.bOutlined=true;NotesText.OutlineColor=Ink;Canvas->DrawItem(NotesText);
 if(Hunting->RecentNotesGain>0 && Age<2.6)
 {
  FCanvasTextItem Gain(NotesAt+FVector2D(0,23*S),FText::FromString(FString::Printf(TEXT("+%s"),*FText::AsNumber(Hunting->RecentNotesGain).ToString())),BBSBHUDTypography::NumberFont(11*S),FLinearColor(.37f,.84f,1.f,FMath::Clamp(float((2.6-Age)/.4),0.f,1.f)));
  Gain.bCentreX=Gain.bCentreY=true;Gain.bOutlined=true;Gain.OutlineColor=Ink;Canvas->DrawItem(Gain);
 }
}
