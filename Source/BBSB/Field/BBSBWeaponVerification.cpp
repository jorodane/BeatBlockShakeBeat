#include "BBSBWeaponVerification.h"
#include "BBSBFieldActions.h"
#include "Combat/BBSBCombat.h"
#include "BBSB.h"
#include "BBSBCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

ABBSBWeaponVerification::ABBSBWeaponVerification() { PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork; }
void ABBSBWeaponVerification::Check(bool C,const TCHAR* M)
{
 if (C) { ++ChecksPassed; UE_LOG(LogBBSB,Display,TEXT("WeaponVerify PASS: %s"),M); }
 else { Failures.Add(M); UE_LOG(LogBBSB,Error,TEXT("WeaponVerify FAIL: %s"),M); }
}
void ABBSBWeaponVerification::Advance(int32 N) { Step=N; StepAt=GetWorld()->GetTimeSeconds(); Frame=0; }
void ABBSBWeaponVerification::AimAt(FVector P)
{
 APlayerController* PC=Cast<APlayerController>(Character->GetController()); FVector Eye; FRotator View; PC->GetPlayerViewPoint(Eye,View);
 if (Combat && Combat->IsStageLocked()) { FVector2D Screen; if (PC->ProjectWorldLocationToScreen(P,Screen)) PC->SetMouseLocation(Screen.X,Screen.Y); }
 else PC->SetControlRotation(PC->GetControlRotation()+((P-Eye).Rotation()-View).GetNormalized());
}
void ABBSBWeaponVerification::BeginPlay()
{
 Super::BeginPlay(); Character=Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this,0));
 Actions=Character ? Character->FindComponentByClass<UBBSBFieldActionComponent>() : nullptr;
 Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>() : nullptr;
 if (!Actions || !Combat) { Check(false,TEXT("Player components exist")); Finish(); return; }
 OriginalTransform=Character->GetActorTransform(); OriginalView=Character->GetController()->GetControlRotation(); OriginalWeapon=uint8(Actions->PrototypeWeapon);
 OriginalHP=Combat->Health; OriginalMaxHP=Combat->MaximumHealth; OriginalEcstatic=Combat->Ecstatic;
 Combat->GoToPractice(); Combat->MaximumHealth=Combat->Health=100000; Combat->Ecstatic=0;
 const FVector Position=Character->GetActorLocation()+FVector(250,0,-92);
 Target=GetWorld()->SpawnActor<ABBSBBreakableProp>(Position,FRotator::ZeroRotator); Target->MaximumHealth=10000; Target->ResetProp();
 Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Greatsword); HealthBefore=Target->Health; Advance(0);
}
void ABBSBWeaponVerification::Tick(float DT)
{
 Super::Tick(DT); if (bComplete || !Actions || !Target) return;
 const double Age=GetWorld()->GetTimeSeconds()-StepAt;
 if (Age>8) { Check(false,TEXT("Weapon step completes before timeout")); Finish(); return; }
 ++Frame;
 if (Actions->ConductorWeapon)
  bConductorStayedHeld &= Actions->ConductorWeapon->GetRootComponent()->GetAttachParent()==Character->GetMesh()
   && FVector::Dist(Actions->ConductorWeapon->GetActorLocation(),Character->GetMesh()->GetSocketLocation(TEXT("HandGrip_R")))<1.f;
 if (Step<=15) AimAt(Target->GetActorLocation()+FVector(0,0,35));
 switch(Step)
 {
 case 0:
  if (Age>0.5)
  {
   Check(Actions->Blade->GetStaticMesh()->GetPathName().Contains(TEXT("/BroadSword/")),TEXT("Greatsword floating model uses the authored weapon rather than Reparaton"));
   Check(Actions->ActiveWeaponAnimationSet && Actions->ActiveWeaponAnimationSet->Clips.Num()>0,TEXT("Game loads the editor-authored weapon animation set"));
   Check(Actions->ConductorFull->IsVisible() && Actions->ConductorFull->GetStaticMesh()->GetPathName().Contains(TEXT("/Reparaton/"))
    && FVector::Dist(Actions->FloatingWeapon->GetActorLocation(),Actions->ConductorWeapon->GetActorLocation())>60,TEXT("Visible hand-held Reparaton and floating weapon remain separate"));
   Actions->FieldGesturePress(0); Advance(1);
  }
  break;
 case 1:
  if (Age>2.25 && FMath::Frac(Actions->WeaponBeatPosition())>0.35 && FMath::Frac(Actions->WeaponBeatPosition())<0.5)
  {
   Check(FMath::IsNearlyEqual(Target->Health,HealthBefore),TEXT("Greatsword press and Hold do not cause early damage"));
   Check(FMath::IsNearlyEqual(Actions->WeaponChargeSeconds,2.f,0.01f),TEXT("Greatsword Hold caps charge at two seconds"));
   Actions->FieldGestureMotion(220,0); Advance(2);
  }
  break;
 case 2:
  if (Age>0.025) { Actions->FieldGestureRelease(0); Check(Actions->LastGesture==EBBSBFieldGesture::Flick,TEXT("Greatsword accepts real recognized Flick Release")); Advance(3); }
  break;
 case 3:
  if (Age>1.5)
  {
   Check(FMath::IsNearlyEqual(HealthBefore-Target->Health,54.f,0.05f),TEXT("Two-second off-beat greatsword Release deals 54 damage"));
   Check(Actions->WeaponChargeSeconds==0,TEXT("Greatsword Release consumes the stored charge"));
   Actions->FieldGesturePress(0); HealthBefore=Target->Health; Advance(4);
  }
  break;
 case 4:
  if (Age>2.1 && FMath::Abs(Actions->WeaponBeatPosition()-FMath::RoundToDouble(Actions->WeaponBeatPosition()))*60/110<0.015)
  { Actions->FieldGestureRelease(0); Check(Actions->LastGesture==EBBSBFieldGesture::Dive,TEXT("Greatsword also accepts stationary Dive Release")); Advance(5); }
  break;
 case 5:
  if (Age>1.5)
  {
   Check(Actions->bLastWeaponOnBeat && FMath::IsNearlyEqual(HealthBefore-Target->Health,81.f,0.05f),TEXT("Audible beat-aligned Release applies exactly the 1.5x bonus"));
   Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Hammer); Actions->FieldGesturePress(0); HealthBefore=Target->Health; Advance(6);
  }
  break;
 case 6:
  if (Age>1.4)
  {
   Check(Target->Health==HealthBefore,TEXT("Hammer charge causes no free hit")); ChargeBefore=Actions->WeaponChargeSeconds; Actions->FieldGesturePress(1); Advance(7);
   Check(Actions->Blade->GetStaticMesh()->GetPathName().Contains(TEXT("/Hammer/")) && Actions->FormUpper->IsVisible()
    && Actions->FormUpper->GetOwner()==Actions->ConductorWeapon,TEXT("Hammer floats while Reparaton transforms in the character hand"));
  }
  break;
 case 7:
  if (!RetargetProp)
  {
   RetargetProp=GetWorld()->SpawnActor<ABBSBBreakableProp>(Target->GetActorLocation()+FVector(0,240,0),FRotator::ZeroRotator);
   RetargetProp->MaximumHealth=10000; RetargetProp->ResetProp();
  }
  AimAt(RetargetProp->GetActorLocation()+FVector(0,0,35));
  Actions->FieldGestureMotion(Frame%2 ? 35 : -35,0);
  if (Age>0.24)
  {
   Check(Actions->WeaponChargeSeconds<ChargeBefore && Actions->WeaponChargeSeconds>0,TEXT("Recognized Shake consumes part of the hammer charge")); ChargeBefore=Actions->WeaponChargeSeconds; Advance(8);
   Check(Actions->CurrentAction.IsSet() && Actions->CurrentAction->AimActor.Get()==Target && Actions->LockedAimActor==Target,TEXT("Hammer Shake keeps the right-click actor when aiming at another prop"));
  }
  break;
 case 8:
  if (Age>0.95)
  {
   Check(FMath::IsNearlyEqual(Actions->WeaponChargeSeconds,ChargeBefore,0.01f),TEXT("Holding after Shake does not regenerate spent charge"));
   Check(Target->Health<HealthBefore,TEXT("Hammer spin physically hits the aimed region"));
   Actions->FieldGestureRelease(1); Actions->FieldGestureRelease(0); Advance(9);
  }
  break;
 case 9:
  if (Age>0.2)
  {
   Actions->WeaponChargeSeconds=1.4f; Actions->FieldGesturePress(1); Actions->FieldGestureMotion(220,0); HealthBefore=Target->Health; Advance(10);
  }
  break;
 case 10:
  if (Age>0.025)
  {
   AimAt(RetargetProp->GetActorLocation()+FVector(0,0,35));
   Actions->FieldGestureRelease(1);
   Check(Actions->CurrentAction.IsSet() && Actions->CurrentAction->AimActor.Get()==Target,TEXT("Hammer Flick uses the original right-click actor rather than release aim"));
   Advance(11);
  }
  break;
 case 11:
  if (Age>1.4)
  {
   Check(Actions->WeaponChargeSeconds==0,TEXT("Hammer Flick spends its entire reservoir"));
   Check(FMath::IsNearlyEqual(HealthBefore-Target->Health,18.9f,0.05f),TEXT("Hammer Flick converts charge at exactly 0.75 of Shake efficiency"));
   Check(RetargetProp && RetargetProp->Health==RetargetProp->MaximumHealth,TEXT("Changing aim during hammer gestures does not redirect damage to prop B"));
   RetargetProp->Destroy(); RetargetProp=nullptr;
   Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Sword); HealthBefore=Target->Health; Counter=0; Advance(12);
   Check(Actions->Blade->GetStaticMesh()->GetPathName().Contains(TEXT("/Sword/")) && Actions->OtherConductorWeapon->GetAttachParentActor()==Character,TEXT("Sword floats independently of both held conductor halves"));
  }
  break;
 case 12:
  if (Age>1.05)
  {
   Actions->FieldGesturePress(0); Actions->FieldGestureRelease(0); ++Counter;
   Check(Actions->SwordComboStep==1 && Actions->CanMove(),TEXT("Repeated sword Beats use one throw while allowing locomotion"));
   if (Counter==3) Advance(13); else Advance(12);
  }
  break;
 case 13:
  if (Age>2.05)
  {
   Check(FMath::IsNearlyEqual(HealthBefore-Target->Health,54.f,0.05f),TEXT("Three independent sword Beats deliver equal-damage blade hits"));
   Actions->FieldGesturePress(0); Actions->FieldGestureRelease(0);
   Check(Actions->SwordComboStep==1,TEXT("A later sword Beat still selects the same single action")); Advance(14);
  }
  break;
 case 14:
  if (Age>1.3)
  {
   Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Crescent); HealthBefore=Target->Health;
   Check(Actions->Blade->GetStaticMesh()->GetPathName().Contains(TEXT("/CrescentBlade/")) && Actions->OtherBlade->IsVisible()
    && Actions->OtherBlade->GetStaticMesh()==Actions->Blade->GetStaticMesh(),TEXT("Crescent selection creates two actual floating Crescent models"));
   Actions->FieldGesturePress(0); Actions->FieldGesturePress(1);
   Check(Actions->CurrentAction.IsSet() && Actions->OtherWeaponAction.IsSet(),TEXT("Left and right Crescent Beats coexist independently"));
   Actions->FieldGestureRelease(0); Actions->FieldGestureRelease(1); Advance(15);
  }
  break;
 case 15:
  if (Age>1.3)
  {
   Check(FMath::IsNearlyEqual(HealthBefore-Target->Health,36.f,0.05f),TEXT("Two Crescent blades each physically apply their own hit"));
   Check(Actions->HandAttacks[0]==1 && Actions->HandAttacks[1]==1,TEXT("Each mouse button owns exactly one Crescent attack"));
   Actions->FieldGesturePress(0); Check(Actions->TryDodge(),TEXT("Dodge cancels a prototype attack immediately")); HealthBefore=Target->Health; Advance(16);
  }
  break;
 case 16:
  if (Age>0.65)
  {
   Check(Target->Health==HealthBefore && !Actions->CurrentAction.IsSet() && !Actions->OtherWeaponAction.IsSet(),TEXT("Canceled prototype attacks cause no delayed or queued hit"));
   Combat->GoToPractice(); Combat->MaximumHealth=Combat->Health=100000;
   Enemy=nullptr; for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It) if (!Enemy || It->GetActorLocation().X<Enemy->GetActorLocation().X) Enemy=*It;
   if (!Enemy) { Check(false,TEXT("Enemy fixture exists")); Finish(); return; }
   UGameplayStatics::ApplyDamage(Enemy,1,Character->GetController(),Character,nullptr);
   if (!Combat->Stage) { Check(false,TEXT("Locked-on fixture forms a real stage")); Finish(); return; }
   Combat->Stage->bPausePatternEmission=true; Combat->Stage->Phase=EBBSBStagePhase::Fighting; Combat->Stage->JoinRadius=1;
   Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Spear); Advance(17);
  }
  break;
 case 17:
  AimAt(Enemy->GetAimPoint());
  if (Age>2.2)
  {
   Check(Actions->Blade->GetStaticMesh()->GetPathName().Contains(TEXT("/Spear/")),TEXT("Spear attacks use the authored spear model"));
   APlayerController* PC=Cast<APlayerController>(Character->GetController());
   CameraBefore=PC->GetControlRotation(); Cast<ABBSBCharacter>(Character)->DoLook(35,20); PC->AddYawInput(40);
   Check(PC->IsLookInputIgnored() && PC->RotationInput.IsNearlyZero() && Actions->GetLookInputScale()==0,TEXT("Combat lock rejects rotation input before it can move the camera"));
   FVector2D Screen; int32 Width,Height; PC->GetViewportSize(Width,Height); PC->ProjectWorldLocationToScreen(Character->GetActorLocation(),Screen);
   Check(Width>0 && Height>0 && Screen.X/Width<0.4 && Screen.Y/Height>0.55,TEXT("Combat frame keeps the player at lower left"));
   Actions->FieldGesturePress(0); HealthBefore=Enemy->Health; Counter=0;
   Check(Actions->LockedAimActor==Enemy,TEXT("Spear Beat captures the enemy actually clicked in the combat frame")); Advance(18);
   Check(Actions->CurrentAction.IsSet() && Actions->CurrentAction->bAuthoredMotion && Actions->CurrentAction->Motion.Sequence
    && FMath::IsNearlyEqual(Actions->CurrentAction->Duration,Actions->CurrentAction->Motion.Duration),TEXT("Spear action consumes its editable Sequencer clip and duration"));
  }
  break;
 case 18:
  if (Age>0.75 && Counter==0)
  {
   Check(Enemy->Health<HealthBefore,TEXT("Spear Beat executes a fast physical thrust"));
   Check((Character->GetController()->GetControlRotation()-CameraBefore).GetNormalized().IsNearlyZero(0.001f),TEXT("Combat camera remains exact instead of rotating and easing back"));
   Check(Actions->bSpearEmbedded && Actions->SpearPiercedActor.Get()==Enemy && Actions->FloatingWeapon->GetAttachParentActor()==Enemy
    && !Actions->CurrentAction.IsSet(),TEXT("Spear stays embedded in struck enemy after the Beat animation finishes"));
   SpearRelativeBefore=Actions->FloatingWeapon->GetActorTransform().GetRelativeTransform(Enemy->GetActorTransform());
   Enemy->AddActorWorldOffset(FVector(0,35,0),false); HealthBefore=Enemy->Health; Counter=1;
  }
  if (Age>1.25 && Counter==1)
  {
   Check(Actions->bSpearEmbedded && Actions->FloatingWeapon->GetActorTransform().GetRelativeTransform(Enemy->GetActorTransform()).Equals(SpearRelativeBefore,0.1f),TEXT("Embedded spear follows the same enemy instead of returning or retargeting"));
   Check(Enemy->Health==HealthBefore,TEXT("Leaving the spear embedded adds no repeated damage"));
   Combat->CycleTarget(1); RetargetEnemy=Combat->SelectedEnemy;
   Check(RetargetEnemy && RetargetEnemy!=Enemy,TEXT("A different enemy can be selected while the spear remains in enemy A"));
   RetargetHealthBefore=RetargetEnemy ? RetargetEnemy->Health:0;
   if (RetargetEnemy) AimAt(RetargetEnemy->GetAimPoint());
   Actions->FieldGestureMotion(220,0); Advance(19);
  }
  break;
 case 19:
  if (Age>0.025)
  {
   EnemyBefore=Enemy->GetActorLocation(); Actions->FieldGestureRelease(0);
   Check(Actions->CurrentAction.IsSet() && Actions->CurrentAction->AimActor.Get()==Enemy
    && !Actions->bSpearEmbedded && !Actions->FloatingWeapon->GetAttachParentActor(),TEXT("Spear Flick detaches from and attacks the pierced enemy, despite selecting B"));
   Advance(20);
  }
  break;
 case 20:
  if (Age>0.7)
  {
   Check(Actions->LastWeaponImpulse.Size()>900 && FVector::Dist2D(EnemyBefore,Enemy->GetActorLocation())>15,TEXT("Directional spear Flick physically launches the living target"));
   Check(RetargetEnemy && RetargetEnemy->Health==RetargetHealthBefore,TEXT("Spear Flick cannot redirect damage or launch onto selected enemy B"));
   Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Dagger); Combat->bCombatStance=false;
   Target->SetActorLocation(Character->GetActorLocation()+FVector(180,180,-92)); Target->ResetProp(); Advance(21);
  }
  break;
 case 21:
  Combat->bCombatStance=false; AimAt(Target->GetActorLocation()+FVector(0,0,35));
  if (Age>0.5)
  {
   HealthBefore=Target->Health; EnemyBefore=Enemy->GetActorLocation(); Actions->FieldGesturePress(0); Actions->FieldGestureRelease(0);
   Check(!Actions->Arbiter.Rule.bLockMovement && (!Actions->ActionMontage || (Actions->CurrentAction.IsSet() && Actions->CurrentAction->Motion.CharacterAnimation)),TEXT("Dagger Beat keeps movement free and only plays an explicitly authored character animation"));
   Check(FVector::Dist(Actions->LastAimPoint,Enemy->GetAimPoint())>100,TEXT("Dagger ignores selected-enemy aim correction while locked on"));
   Check(Target->Health==HealthBefore,TEXT("Dagger does not damage the destination before traveling")); Advance(22);
   bool CorrectProjectile=false;
   for (TActorIterator<ABBSBPlayerWeaponProjectile> It(GetWorld()); It; ++It) CorrectProjectile|=It->Body->GetStaticMesh() && It->Body->GetStaticMesh()->GetPathName().Contains(TEXT("/Dagger/"));
   Check(CorrectProjectile && Actions->FormUpper->IsVisible() && Actions->OtherFormLower->IsVisible(),TEXT("A real dagger flies while both Reparaton halves remain visible in hand"));
  }
  break;
 case 22:
  if (Age>0.9)
  {
   Check(Target->Health<HealthBefore,TEXT("Manually pointed dagger projectile physically reaches and damages its target"));
   Combat->ResetEncounter(); Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Greatsword);
   Character->GetController()->SetControlRotation(FRotator::ZeroRotator);
   Wall=GetWorld()->SpawnActor<AActor>();
   UBoxComponent* Shape=NewObject<UBoxComponent>(Wall,TEXT("TemporaryWeaponWall")); Wall->AddInstanceComponent(Shape); Wall->SetRootComponent(Shape);
   Shape->SetMobility(EComponentMobility::Movable); Shape->SetBoxExtent(FVector(500,8,250)); Shape->SetCollisionProfileName(TEXT("BlockAll")); Shape->RegisterComponent();
   Wall->SetActorLocation(Character->GetActorLocation()+FVector(0,200,0)); Advance(23);
  }
  break;
 case 23:
  if (Age>0.4)
  {
   Check(FVector::Dist(Actions->FloatingWeapon->GetActorLocation(),Character->GetActorLocation())<FVector::Dist(Actions->GetWeaponRestTransform().GetLocation(),Character->GetActorLocation())-10,TEXT("A side wall retracts the actual floating weapon toward the character"));
   Check(PresentationClearsGeometry(),TEXT("The actual greatsword bounds remain outside the blocking wall"));
   Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Hammer); Advance(24);
  }
  break;
 case 24:
  if (Age>0.6)
  {
   Check(PresentationClearsGeometry(),TEXT("The actual hammer bounds remain outside the blocking wall"));
   Check(bConductorStayedHeld,TEXT("Reparaton stays at the animated hand grip throughout charging, attacks and flight"));
   Wall->Destroy(); Wall=nullptr;
   Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Crescent);
   Target->SetActorLocation(Character->GetActorLocation()+FVector(250,0,-92)); Target->ResetProp(); Advance(25);
  }
  break;
 case 25:
  AimAt(Target->GetActorLocation()+FVector(0,0,35));
  if (Age>0.5)
  {
   HealthBefore=Target->Health;
   Actions->FieldGesturePress(0); Actions->FieldGesturePress(1); Actions->FieldGestureRelease(0); Actions->FieldGestureRelease(1);
   if (Actions->CurrentAction.IsSet()) Actions->CurrentAction->StartedAt-=1.5;
   if (Actions->OtherWeaponAction.IsSet()) Actions->OtherWeaponAction->StartedAt-=0.35;
   Actions->TickPrototypeWeapon(1.5f);
   Check(FMath::IsNearlyEqual(HealthBefore-Target->Health,36.f,0.05f),TEXT("Both Crescent hits survive a frame that skips the entire active animation interval"));
   Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Sword); Advance(26);
  }
  break;

 case 26:
  AimAt(Target->GetActorLocation()+FVector(0,0,35));
  if (Age>0.45)
  {
   CameraBefore=Character->GetController()->GetControlRotation(); Actions->FieldGesturePress(0);
   Check(Actions->GetLookInputScale()>0.99f && !Actions->bWeaponAimLocked,TEXT("Sword Beat leaves mouse look free even while the button is held"));
   Check(!Actions->CanMove() && Actions->CurrentAction.IsSet() && Actions->CurrentAction->Duration>=0.9f,TEXT("Sword commits movement for a deliberate swing"));
   Cast<ABBSBCharacter>(Character)->DoLook(15,3); Advance(27);
  }
  break;
 case 27:
  if (Age>0.8)
  {
   Check(FMath::Abs((Character->GetController()->GetControlRotation()-CameraBefore).GetNormalized().Yaw)>0.1f,TEXT("Real sword look input changes the view without waiting for Release"));
   const bool Lingering=Actions->CurrentAction.IsSet() && FVector::Dist(Actions->FloatingWeapon->GetActorLocation(),Actions->PrototypeAttackPose(*Actions->CurrentAction,1.f).GetLocation())<1;
   Check(Lingering,TEXT("Sword stays at its follow-through before returning"));
   const FTransform Previous=Actions->FloatingWeapon->GetActorTransform();
   Actions->FieldGestureRelease(0); Actions->FieldGesturePress(0);
   Check(Actions->SwordComboStep==2 && Actions->CurrentAction.IsSet() && FVector::Dist(Actions->CurrentAction->StartPose.GetLocation(),Previous.GetLocation())<1,TEXT("Second sword Beat chains directly from the previous end pose"));
   Actions->FieldGestureRelease(0); Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Dagger);
   Actions->FieldGesturePress(0);
   Check(Actions->CanMove() && Actions->GetLookInputScale()>0.99f,TEXT("Dagger Beat preserves movement and mouse look"));
   Actions->FieldGestureRelease(0); Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Crescent);
   Actions->FieldGesturePress(0); Actions->FieldGesturePress(1);
   Check(!Actions->CanMove() && Actions->GetLookInputScale()>0.99f,TEXT("Both Crescent Beats lock movement while keeping mouse look free"));
   Actions->FieldGestureRelease(0); Actions->FieldGestureRelease(1); Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Greatsword); Advance(28);
  }
  break;
 case 28:
  AimAt(Target->GetActorLocation()+FVector(0,0,35));
  if (Age>0.45)
  {
   Actions->FieldGesturePress(0); SavedAim=Actions->LockedAimPoint;
   Check(Actions->bWeaponAimLocked && Actions->LockedAimActor==Target,TEXT("Greatsword press captures the pointed actor"));
   Character->AddActorWorldOffset(FVector(0,45,0),false); Advance(29);
  }
  break;
 case 29:
  if (Age>0.7)
  {
   Check(FVector::Dist(SavedAim,Actions->LockedAimPoint)<0.1f && Actions->GetLookInputScale()==0,TEXT("Moving the player cannot retarget a charged greatsword"));
   ChargeDistanceBefore=FVector::Dist(Actions->FloatingWeapon->GetActorLocation(),Actions->LockedAimPoint);
   Target->AddActorWorldOffset(FVector(0,20,0),false); SavedAim+=FVector(0,20,0); Advance(30);
  }
  break;
 case 30:
  if (Age>1.0)
  {
   Check(FVector::Dist(SavedAim,Actions->LockedAimPoint)<0.1f,TEXT("Actor lock follows that same actor when it moves"));
   Check(FVector::Dist(Actions->FloatingWeapon->GetActorLocation(),Actions->LockedAimPoint)>ChargeDistanceBefore+50,TEXT("Greatsword visibly draws farther back as charge increases"));
   Check(Target->Body->bRenderCustomDepth && Target->Body->CustomDepthStencilValue==3,TEXT("Charged actor keeps the larger target outline"));
   Check(Actions->TryDodge() && !Actions->bWeaponAimLocked && Actions->WeaponChargeSeconds==0,TEXT("Dodge clears charge and target capture immediately")); Advance(31);
  }
  break;
 case 31:
  if (Age<0.35) break;
  AimAt(Character->GetActorLocation()+FVector(180,-240,-92));
  if (Age>0.8)
  {
   Actions->FieldGesturePress(0); SavedAim=Actions->LockedAimPoint;
   Check(Actions->bWeaponAimLocked && !Actions->LockedAimActor,TEXT("Terrain input captures a world point for the spherical marker"));
   Character->AddActorWorldOffset(FVector(0,40,0),false); Advance(32);
  }
  break;
 case 32:
  if (Age>0.7)
  {
   Check(FVector::Dist(SavedAim,Actions->LockedAimPoint)<0.1f,TEXT("Terrain anchor stays fixed when the player moves"));
   Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Hammer); Advance(33);
  }
  break;
 case 33:
  AimAt(Target->GetActorLocation()+FVector(0,0,35));
  if (Age>0.5) { Actions->FieldGesturePress(0); HealthBefore=Target->Health; Advance(34); }
  break;
 case 34:
  if (Age>0.9)
  {
   Check(Actions->WeaponChargeSeconds>0.5f && Actions->GetLookInputScale()>0.99f && !Actions->bWeaponAimLocked,TEXT("Left Hold charges the hammer without capturing aim or camera"));
   Actions->FieldGestureMotion(220,0); Advance(35);
  }
  break;
 case 35:
  if (Age>0.025)
  {
   Actions->FieldGestureRelease(0);
   Check(Target->Health==HealthBefore && !Actions->CurrentAction.IsSet() && Actions->WeaponChargeSeconds>0,TEXT("Left hammer Release retains charge and cannot execute Flick attack"));
   ChargeBefore=Actions->WeaponChargeSeconds; Actions->FieldGesturePress(1);
   Check(Actions->bWeaponAimLocked && Actions->GetLookInputScale()==0,TEXT("Only right press captures the hammer target and camera")); Advance(36);
  }
  break;
 case 36:
  if (Age>0.4) { Actions->FieldGestureRelease(1); Advance(37); }
  break;
 case 37:
  if (Age>0.5)
  {
   Check(!Actions->bWeaponAimLocked && Actions->GetLookInputScale()>0.99f && FMath::IsNearlyEqual(Actions->WeaponChargeSeconds,ChargeBefore,0.01f),TEXT("Right Dive releases hammer capture while keeping its remaining charge"));
   Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Spear);
   Target->SetActorLocation(Character->GetActorLocation()+FVector(250,0,-92)); Target->ResetProp(); Advance(38);
  }
  break;
 case 38:
  AimAt(Target->GetActorLocation()+FVector(0,0,35));
  if (Age>0.6) { Actions->FieldGesturePress(0); Advance(39); }
  break;
 case 39:
  if (Age>1.0)
  {
   Check(Actions->bSpearEmbedded && Actions->SpearPiercedActor.Get()==Target,TEXT("Field spear also waits embedded in the clicked prop"));
   Actions->FieldGestureRelease(0);
   Check(!Actions->bSpearEmbedded && !Actions->FloatingWeapon->GetAttachParentActor(),TEXT("Plain Dive release safely recalls an embedded spear without a Flick")); Advance(40);
  }
  break;
 case 40:
  AimAt(Target->GetActorLocation()+FVector(0,0,35));
  if (Age>0.8) { Actions->FieldGesturePress(0); Advance(41); }
  break;
 case 41:
  if (Age>0.8)
  {
   Check(Actions->bSpearEmbedded && Actions->TryDodge(),TEXT("An embedded spear does not prevent dodge cancellation"));
   Check(!Actions->bSpearEmbedded && !Actions->bWeaponAimLocked && !Actions->FloatingWeapon->GetAttachParentActor(),TEXT("Dodge detaches the spear and clears its captured target")); Advance(42);
  }
  break;
 case 42:
  AimAt(Character->GetActorLocation()+FVector(-180,-240,-92));
  if (Age>0.9) { Actions->FieldGesturePress(0); Actions->FieldGestureMotion(220,0); Advance(43); }
  break;
 case 43:
  if (Age>0.025)
  {
   Actions->FieldGestureRelease(0);
   Check(!Actions->bSpearEmbedded && (!Actions->CurrentAction.IsSet() || Actions->CurrentAction->Gesture!=EBBSBFieldGesture::Flick),TEXT("A missed spear Beat cannot Flick-launch a newly aimed victim")); Advance(44);
  }
  break;
 case 44:
  if (Age>0.9)
  {
   Check(!Actions->bWeaponAimLocked,TEXT("A missed spear chain releases its capture cleanly"));
   Actions->ResetActionState(); Actions->SelectPrototypeWeapon(EBBSBPrototypeWeapon::Hammer); Advance(45);
  }
  break;
 case 45:
  if (Age>2.0)
  {
   FTransform Expected;
   const bool Sampled=Actions->ActiveWeaponAnimationSet && UBBSBWeaponAnimationSet::SampleTransform(Actions->ActiveWeaponAnimationSet->FormSequence,TEXT("Upper"),1.f,Expected);
   Check(Sampled && Actions->FormUpper->GetRelativeTransform().Equals(Expected,0.01f),TEXT("Equipped hammer form matches the editable Sequencer end pose"));
   Check(!Actions->ConductorFull->IsVisible() && Actions->FormUpper->IsVisible() && Actions->WeaponChargeSeconds==0 && !Actions->IsGestureCaptured(),TEXT("Idle hammer retains its selected form beyond the old 1.5-second timeout"));
   Finish();
  }
  break;
 }
}
bool ABBSBWeaponVerification::PresentationClearsGeometry() const
{
 const FBox Bounds=Actions->Blade->GetStaticMesh()->GetBoundingBox().TransformBy(Actions->Blade->GetRelativeTransform());
 const FTransform Pose=Actions->FloatingWeapon->GetActorTransform();
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBWeaponWallVerification),false,Character); Q.AddIgnoredActor(Actions->FloatingWeapon); Q.AddIgnoredActor(Actions->OtherFloatingWeapon);
 return !GetWorld()->OverlapBlockingTestByChannel(Pose.TransformPosition(Bounds.GetCenter()),Pose.GetRotation(),ECC_Camera,FCollisionShape::MakeBox(Bounds.GetExtent()+FVector(0.5)),Q);
}
void ABBSBWeaponVerification::Finish()
{
 bComplete=true;
 if (Actions) Actions->ResetActionState();
 if (Combat) { Combat->ResetEncounter(); Combat->MaximumHealth=OriginalMaxHP; Combat->Health=OriginalHP; Combat->Ecstatic=OriginalEcstatic; }
 if (Actions) Actions->SelectPrototypeWeapon(static_cast<EBBSBPrototypeWeapon>(OriginalWeapon));
 if (Target) Target->Destroy();
 if (RetargetProp) RetargetProp->Destroy();
 if (Wall) Wall->Destroy();
 for (TActorIterator<ABBSBPlayerWeaponProjectile> It(GetWorld()); It; ++It) It->Destroy();
 if (Character) { Character->SetActorTransform(OriginalTransform,false,nullptr,ETeleportType::TeleportPhysics); Character->GetController()->SetControlRotation(OriginalView); }
 UE_LOG(LogBBSB,Display,TEXT("WeaponVerify COMPLETE passed=%d failed=%d"),ChecksPassed,Failures.Num()); SetActorTickEnabled(false);
}
