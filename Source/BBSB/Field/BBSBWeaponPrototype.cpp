#include "BBSBFieldActions.h"
#include "BBSB.h"
#include "BBSBCharacter.h"
#include "Character/BBSBTraversal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Combat/BBSBCombat.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"
#include "BBSBBladeTrail.h"

namespace
{
 const TCHAR* FullPath=TEXT("/Game/Meshes/Weapons/Reparaton/Full/Reparaton_Full.Reparaton_Full");
 const TCHAR* TopPath=TEXT("/Game/Meshes/Weapons/Reparaton/Top/Reparaton_Top.Reparaton_Top");
 const TCHAR* BottomPath=TEXT("/Game/Meshes/Weapons/Reparaton/Bottom/Reparaton_Bottom.Reparaton_Bottom");
 AActor* WeaponActor(UWorld* World,ACharacter* Owner)
 {
  FActorSpawnParameters P; P.Owner=Owner; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  AActor* A=World->SpawnActor<AActor>(AActor::StaticClass(),Owner->GetActorTransform(),P);
  USceneComponent* Root=NewObject<USceneComponent>(A,TEXT("ReparatonRoot")); A->AddInstanceComponent(Root); A->SetRootComponent(Root); Root->RegisterComponent(); return A;
 }
 UStaticMeshComponent* Part(AActor* Owner,const TCHAR* Name,const TCHAR* Asset)
 {
  UStaticMeshComponent* M=NewObject<UStaticMeshComponent>(Owner,Name); Owner->AddInstanceComponent(M); M->SetupAttachment(Owner->GetRootComponent());
  M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Asset)); M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetMobility(EComponentMobility::Movable); M->RegisterComponent(); return M;
 }
 bool IsDual(EBBSBPrototypeWeapon W) { return W==EBBSBPrototypeWeapon::Sword || W==EBBSBPrototypeWeapon::Dagger || W==EBBSBPrototypeWeapon::Crescent; }
 const TCHAR* WeaponPath(EBBSBPrototypeWeapon W)
 {
  switch (W)
  {
   case EBBSBPrototypeWeapon::Greatsword: return TEXT("/Game/Meshes/Weapons/BroadSword/Meshy_AI_weapon_broadsword_10k_1020060827_texture.Meshy_AI_weapon_broadsword_10k_1020060827_texture");
   case EBBSBPrototypeWeapon::Spear: return TEXT("/Game/Meshes/Weapons/Spear/Meshy_AI_weapon_spear_10k_1020060820_texture.Meshy_AI_weapon_spear_10k_1020060820_texture");
   case EBBSBPrototypeWeapon::Hammer: return TEXT("/Game/Meshes/Weapons/Hammer/Meshy_AI_weapon_hammer_10k_1020060807_texture.Meshy_AI_weapon_hammer_10k_1020060807_texture");
   case EBBSBPrototypeWeapon::Sword: return TEXT("/Game/Meshes/Weapons/Sword/Meshy_AI_weapon_sword_10k_1020060814_texture.Meshy_AI_weapon_sword_10k_1020060814_texture");
   case EBBSBPrototypeWeapon::Dagger: return TEXT("/Game/Meshes/Weapons/Dagger/Meshy_AI_weapon_dagger_10k_1020060839_texture.Meshy_AI_weapon_dagger_10k_1020060839_texture");
   case EBBSBPrototypeWeapon::Crescent: return TEXT("/Game/Meshes/Weapons/CrescentBlade/Meshy_AI_weapon_crescent_blade_1020060846_texture.Meshy_AI_weapon_crescent_blade_1020060846_texture");
   default: return nullptr;
  }
 }
}

FString UBBSBFieldActionComponent::PrototypeWeaponLabel() const
{
 if (FieldEquipment) return FieldEquipmentName.ToString();
 const TCHAR* Names[]={TEXT("Legacy"),TEXT("Greatsword"),TEXT("Spear"),TEXT("Hammer"),TEXT("Sword"),TEXT("Dagger"),TEXT("Crescent Blades")};
 return Names[FMath::Clamp(int32(PrototypeWeapon),0,6)];
}
void UBBSBFieldActionComponent::WeaponOne() { SelectPrototypeWeapon(EBBSBPrototypeWeapon::Greatsword); }
void UBBSBFieldActionComponent::WeaponTwo() { SelectPrototypeWeapon(EBBSBPrototypeWeapon::Spear); }
void UBBSBFieldActionComponent::WeaponThree() { SelectPrototypeWeapon(EBBSBPrototypeWeapon::Hammer); }
void UBBSBFieldActionComponent::WeaponFour() { SelectPrototypeWeapon(EBBSBPrototypeWeapon::Sword); }
void UBBSBFieldActionComponent::WeaponFive() { SelectPrototypeWeapon(EBBSBPrototypeWeapon::Dagger); }
void UBBSBFieldActionComponent::WeaponSix() { SelectPrototypeWeapon(EBBSBPrototypeWeapon::Crescent); }
void UBBSBFieldActionComponent::NextPrototypeWeapon() { SelectPrototypeWeapon(static_cast<EBBSBPrototypeWeapon>(int32(PrototypeWeapon)%6+1)); }

bool UBBSBFieldActionComponent::SelectPrototypeWeapon(EBBSBPrototypeWeapon Type)
{
 if (!Character || int32(Type)>6 || bDodging || IsGestureCaptured() || CurrentAction.IsSet() || OtherWeaponAction.IsSet() || Arbiter.IsBusy(GetWorld()->GetTimeSeconds())) return false;
 UBBSBCombatComponent* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
 if (Combat && !Combat->CanAttack()) return false;
 ClearFieldEquipment();
 CancelCurrentAction(true); Arbiter.Clear(); ActiveState=EBBSBActionState::Idle;
 PrototypeWeapon=Type; HandAttacks={0,0}; LastWeaponDamage=0; bLastWeaponOnBeat=false;
 if (Type!=EBBSBPrototypeWeapon::Legacy) LastCombatWeapon=Type;
 ActiveWeaponAnimationSet=nullptr;
 if (const auto* Set=WeaponAnimationSets.Find(Type)) ActiveWeaponAnimationSet=Set->LoadSynchronous();
 if (IsValid(FloatingWeapon)) FloatingWeapon->Destroy();
 if (IsValid(OtherFloatingWeapon)) OtherFloatingWeapon->Destroy();
 if (IsValid(ConductorWeapon)) ConductorWeapon->Destroy();
 if (IsValid(OtherConductorWeapon)) OtherConductorWeapon->Destroy();
 FloatingWeapon=nullptr; OtherFloatingWeapon=nullptr; ConductorWeapon=nullptr; OtherConductorWeapon=nullptr;
 FormUpper=nullptr; FormLower=nullptr; OtherFormLower=nullptr; ConductorFull=nullptr; Blade=nullptr; OtherBlade=nullptr;
 for (auto& Original:HiddenOriginalStaff) if (Original.IsValid()) Original->SetVisibility(Type==EBBSBPrototypeWeapon::Legacy);
 if (FieldMetronome) FieldMetronome->Stop();
 CreateWeapon(); FormUntil=GetWorld()->GetTimeSeconds()+1.5;
 if (Combat && bEnablePracticeControls) Combat->SetFeedback(TEXT("WEAPON  ")+PrototypeWeaponLabel());
 return true;
}

void UBBSBFieldActionComponent::ResetPrototypeState()
{
 ReleaseSpearEmbed();
 WeaponChargeSeconds=0; ChargeAtPress=0; ChargingChannel=-1; bHammerSpending=false; SwordComboStep=0; LastSwordBeatAt=-100;
}

void UBBSBFieldActionComponent::CreatePrototypeWeapon()
{
 if (HiddenOriginalStaff.IsEmpty())
 {
  TArray<UStaticMeshComponent*> Parts; Character->GetComponents(Parts);
  for (UStaticMeshComponent* M:Parts) if (M->GetStaticMesh() && M->GetStaticMesh()->GetPathName().Contains(TEXT("/Reparaton/")) && M->IsVisible()) { HiddenOriginalStaff.Add(M); M->SetVisibility(false); }
 }
 // Reparaton remains the hand-held conductor. Only the selected weapon moves out to attack.
 FloatingWeapon=WeaponActor(GetWorld(),Character); OtherFloatingWeapon=WeaponActor(GetWorld(),Character);
 Blade=Part(FloatingWeapon,TEXT("FloatingBlade"),WeaponPath(PrototypeWeapon));
 OtherBlade=Part(OtherFloatingWeapon,TEXT("OtherFloatingBlade"),WeaponPath(PrototypeWeapon));
 if (!ActiveWeaponAnimationSet) if (const auto* Set=WeaponAnimationSets.Find(PrototypeWeapon)) ActiveWeaponAnimationSet=Set->LoadSynchronous();
 if (ActiveWeaponAnimationSet && ActiveWeaponAnimationSet->WeaponMesh)
 { Blade->SetStaticMesh(ActiveWeaponAnimationSet->WeaponMesh); OtherBlade->SetStaticMesh(ActiveWeaponAnimationSet->WeaponMesh); }
 if (Blade->GetStaticMesh()) Blade->SetRelativeLocation(-Blade->GetStaticMesh()->GetBoundingBox().GetCenter());
 if (OtherBlade->GetStaticMesh()) OtherBlade->SetRelativeLocation(-OtherBlade->GetStaticMesh()->GetBoundingBox().GetCenter());
 ConductorWeapon=WeaponActor(GetWorld(),Character); OtherConductorWeapon=WeaponActor(GetWorld(),Character);
 ConductorFull=Part(ConductorWeapon,TEXT("ReparatonFull"),FullPath);
 FormUpper=Part(ConductorWeapon,TEXT("ReparatonUpper"),TopPath); FormLower=Part(ConductorWeapon,TEXT("ReparatonLower"),BottomPath);
 OtherFormLower=Part(OtherConductorWeapon,TEXT("ReparatonOtherHand"),BottomPath);
 USkeletalMeshComponent* Body=Character->GetMesh();
 const FName RightGrip=Body->DoesSocketExist(TEXT("HandGrip_R")) ? TEXT("HandGrip_R") : TEXT("hand_r");
 ConductorWeapon->AttachToComponent(Body,FAttachmentTransformRules::SnapToTargetNotIncludingScale,RightGrip);
 if (Body->DoesSocketExist(TEXT("HandGrip_L"))) OtherConductorWeapon->AttachToComponent(Body,FAttachmentTransformRules::SnapToTargetNotIncludingScale,TEXT("HandGrip_L"));
 else
 {
  OtherConductorWeapon->AttachToComponent(Body,FAttachmentTransformRules::SnapToTargetNotIncludingScale,TEXT("hand_l"));
  OtherConductorWeapon->SetActorRelativeTransform(Body->GetSocketTransform(RightGrip).GetRelativeTransform(Body->GetSocketTransform(TEXT("hand_r"))));
 }
 CurrentUpperPose=FTransform(FRotator::ZeroRotator,FVector(0,0,-6));
 CurrentLowerPose=FTransform(FRotator::ZeroRotator,FVector(0,0,-90));
 FormProgress=0;
 FloatingWeapon->SetActorTransform(GetWeaponRestTransform()); OtherFloatingWeapon->SetActorTransform(GetWeaponRestTransform());
 PresentPrototypeWeapon(); LastWeaponPosition=FloatingWeapon->GetActorLocation();
}

double UBBSBFieldActionComponent::WeaponBeatPosition() const
{
 const UBBSBCombatComponent* C=Character ? Character->FindComponentByClass<UBBSBCombatComponent>() : nullptr;
 return C && C->IsStageLocked() ? C->Stage->CurrentBeat : BBSBCombatRules::BeatAt(GetWorld()->GetTimeSeconds(),0,110);
}

void UBBSBFieldActionComponent::TickPrototypeState()
{
 if (FieldEquipment) return;
 UpdateWeaponAim();
 if (PrototypeWeapon==EBBSBPrototypeWeapon::Legacy) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if (ChargingChannel>=0 && !Channels[ChargingChannel].bHeld)
 {
  ChargingChannel=-1;
  if (PrototypeWeapon==EBBSBPrototypeWeapon::Greatsword) WeaponChargeSeconds=0;
 }
 if (ChargingChannel>=0 && Channels[ChargingChannel].bHeld && Channels[ChargingChannel].bHoldEmitted && !bHammerSpending)
  WeaponChargeSeconds=FMath::Clamp(ChargeAtPress+float(Now-ChargePressAt),0.f,FMath::Clamp(ChargeMaximumSeconds,0.1f,2.f));
 UBBSBCombatComponent* C=Character->FindComponentByClass<UBBSBCombatComponent>();
 if (C && C->IsStageLocked()) { if (FieldMetronome) FieldMetronome->Stop(); return; }
 const int32 Beat=FMath::FloorToInt(WeaponBeatPosition());
 if (Beat==LastFieldClick) return;
 LastFieldClick=Beat;
 if (!FieldMetronome)
 {
  FieldMetronome=NewObject<UAudioComponent>(Character,TEXT("FieldWeaponMetronome")); FieldMetronome->bAutoActivate=false; FieldMetronome->bIsUISound=true; FieldMetronome->RegisterComponent();
  FieldClick=NewObject<USoundWaveProcedural>(this); FieldClick->SetSampleRate(24000); FieldClick->NumChannels=1; FieldClick->Duration=INDEFINITELY_LOOPING_DURATION;
  FieldMetronome->SetSound(FieldClick);
 }
 TArray<int16> Samples; Samples.SetNumUninitialized(960);
 for (int32 I=0; I<Samples.Num(); ++I) Samples[I]=int16(1500.f*FMath::Sin(2*PI*(Beat%4==0 ? 1100.f : 750.f)*I/24000.f)*FMath::Exp(-I/160.f));
 FieldClick->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()),Samples.Num()*sizeof(int16));
 if (!FieldMetronome->IsPlaying()) FieldMetronome->Play();
}

FVector UBBSBFieldActionComponent::FindPrototypeCursorPoint(AActor** TargetActor) const
{
 if (TargetActor) *TargetActor=nullptr;
 // Placed preview characters and pawns awaiting possession have no local mouse view.
 if (!IsValid(Character)) return GetOwner() ? GetOwner()->GetActorLocation():FVector::ZeroVector;
 const FVector ForwardAim=Character->GetPawnViewLocation()+Character->GetActorForwardVector()*FMath::Max(WeaponReach,100.f);
 const APlayerController* PC=Cast<APlayerController>(Character->GetController());
 if (!IsValid(PC) || !PC->IsLocalController() || !GetWorld()) return ForwardAim;
 FVector Eye,Direction; FRotator View; PC->GetPlayerViewPoint(Eye,View); Direction=View.Vector();
 if (PC->bShowMouseCursor) PC->DeprojectMousePositionToWorld(Eye,Direction);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBManualWeaponAim),false,Character); Q.AddIgnoredActor(FloatingWeapon); Q.AddIgnoredActor(OtherFloatingWeapon);
 FHitResult Hit; FVector Target=Eye+Direction*WeaponReach; AActor* Candidate=nullptr;
 if (GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+Direction*2500,ECC_Visibility,Q)) { Target=Hit.ImpactPoint; Candidate=Hit.GetActor(); }
 const FVector Shoulder=Character->GetActorLocation()+FVector(0,0,30);
 if (FVector::Dist(Shoulder,Target)>WeaponReach) { Target=Shoulder+(Target-Shoulder).GetSafeNormal()*WeaponReach; Candidate=nullptr; }
 if (GetWorld()->LineTraceSingleByChannel(Hit,Shoulder,Target,ECC_Visibility,Q)) { Target=Hit.ImpactPoint; Candidate=Hit.GetActor(); }
 if (TargetActor && IsEquipmentTarget(Candidate)) *TargetActor=Candidate;
 return Target;
}

bool UBBSBFieldActionComponent::PrototypeGesture(EBBSBFieldGesture G,int32 Channel)
{
 if (!Character || Channel<0 || Channel>1 || bDodging) return false;
 UBBSBCombatComponent* C=Character->FindComponentByClass<UBBSBCombatComponent>();
 const double Now=GetWorld()->GetTimeSeconds();
 if ((C && !C->CanAttack()) || (Arbiter.IsBusy(Now) && Arbiter.State!=EBBSBActionState::Attack)) return false;
 const bool Release=G==EBBSBFieldGesture::Flick || G==EBBSBFieldGesture::Dive;
 if (PrototypeWeapon==EBBSBPrototypeWeapon::Greatsword)
 {
  if (G==EBBSBFieldGesture::Beat)
  {
   if (IsGestureCaptured() || Arbiter.IsBusy(Now)) return false;
   AcquireWeaponAim(Channel); ChargingChannel=Channel; ChargePressAt=Now; ChargeAtPress=WeaponChargeSeconds; bHammerSpending=false; FormUntil=Now+3; return true;
  }
  if (G==EBBSBFieldGesture::Hold) { ++GestureCounts[int32(G)]; FormUntil=Now+3; return true; }
  if (Release)
  {
   const float Seconds=WeaponChargeSeconds;
   const double Beat=WeaponBeatPosition(); const float BPM=C && C->IsStageLocked() ? C->Stage->BPM : 110.f;
   const bool OnBeat=FMath::Abs(Beat-FMath::RoundToDouble(Beat))*60.0/BPM<=RhythmWindowSeconds;
   if (!StartPrototypeAttack(G,Channel,BaseDamage*(1.f+Seconds)*(OnBeat ? 1.5f : 1.f),1.2f)) return false;
   bLastWeaponOnBeat=OnBeat; WeaponChargeSeconds=0; ChargingChannel=-1; return true;
  }
  return true;
 }
 if (PrototypeWeapon==EBBSBPrototypeWeapon::Hammer)
 {
  if (Channel==0)
  {
   if (G==EBBSBFieldGesture::Beat) { ChargingChannel=0; ChargePressAt=Now; ChargeAtPress=WeaponChargeSeconds; bHammerSpending=false; FormUntil=Now+3; }
   if (G==EBBSBFieldGesture::Hold) { ++GestureCounts[int32(G)]; FormUntil=Now+3; }
   if (Release) ChargingChannel=-1;
   return true;
  }
  if (G==EBBSBFieldGesture::Beat) { if (Arbiter.IsBusy(Now)) return false; AcquireWeaponAim(1); return true; }
  if (G==EBBSBFieldGesture::Shake && WeaponChargeSeconds>0.001f)
  {
   const float Spent=FMath::Min(0.35f,WeaponChargeSeconds);
   if (!StartPrototypeAttack(G,1,BaseDamage*Spent,0.9f)) return false;
   WeaponChargeSeconds=FMath::Max(0.f,WeaponChargeSeconds-Spent); bHammerSpending=true; return true;
  }
  if (G==EBBSBFieldGesture::Flick && WeaponChargeSeconds>0.001f)
  {
   if (!StartPrototypeAttack(G,1,BaseDamage*WeaponChargeSeconds*0.75f,1.05f)) return false;
   WeaponChargeSeconds=0; ChargingChannel=-1; bHammerSpending=true; return true;
  }
  return true;
 }
 if (PrototypeWeapon==EBBSBPrototypeWeapon::Spear)
 {
  if (G==EBBSBFieldGesture::Beat)
  {
   if (IsGestureCaptured()) return false; // One spear cannot start a second held chain.
   const bool WasLocked=bWeaponAimLocked;
   AcquireWeaponAim(Channel);
   if (StartPrototypeAttack(G,Channel,BaseDamage,0.55f)) return true;
   if (!WasLocked) ClearWeaponAim();
   return false;
  }
  if (G==EBBSBFieldGesture::Flick)
  {
   if (!bSpearEmbedded || !SpearPiercedActor.IsValid()) return false;
   FVector Eye; FRotator View; Cast<APlayerController>(Character->GetController())->GetPlayerViewPoint(Eye,View);
   const FVector2D Motion=Channels[Channel].ReleaseMotion;
   FVector Direction=View.RotateVector(FVector(0,Motion.X,Motion.Y)).GetSafeNormal();
   if (Direction.IsNearlyZero()) Direction=View.Vector();
   const bool Accepted=StartPrototypeAttack(G,Channel,BaseDamage,0.75f,Direction*950.f+FVector(0,0,140));
   ReleaseSpearEmbed(!Accepted);
   return Accepted;
  }
  if (G==EBBSBFieldGesture::Dive) ReleaseSpearEmbed(true);
  return true;
 }
 if (G!=EBBSBFieldGesture::Beat) return true;
 if (PrototypeWeapon==EBBSBPrototypeWeapon::Sword && Channel==0)
 {
  if (!StartPrototypeAttack(G,Channel,BaseDamage,.95f)) return false;
  SwordComboStep=1; LastSwordBeatAt=Now; return true;
 }
 return StartPrototypeAttack(G,Channel,BaseDamage,PrototypeWeapon==EBBSBPrototypeWeapon::Dagger ? 0.18f : 0.92f);
}

bool UBBSBFieldActionComponent::StartPrototypeAttack(EBBSBFieldGesture G,int32 Channel,float Damage,float Duration,FVector Impulse)
{
 const double Now=GetWorld()->GetTimeSeconds(); const bool Independent=PrototypeWeapon==EBBSBPrototypeWeapon::Crescent;
 const auto* Player=Cast<ABBSBCharacter>(Character);auto* Traversal=Player ? Player->Traversal.Get():nullptr;
 const bool GlideAttack=Traversal && Traversal->bGliding;
 const bool SwordThrow=PrototypeWeapon==EBBSBPrototypeWeapon::Sword && Channel==0 && G==EBBSBFieldGesture::Beat;
 const int32 Combo=1;
 const FBBSBWeaponAnimationClip* Clip=ActiveWeaponAnimationSet ? ActiveWeaponAnimationSet->FindClip(G,Channel,Combo,GlideAttack) : nullptr;
 if (Clip) Duration=FMath::Max(Clip->Duration,0.05f);
 auto& Slot=Independent && Channel==1 ? OtherWeaponAction : CurrentAction;
 const float Recovery=Clip ? FMath::Max(0.f,Clip->RecoverySeconds):0.f;
 FBBSBWeaponActionRule Rule; Rule.Duration=Duration+Recovery; Rule.ChainCancelOpen=PrototypeWeapon==EBBSBPrototypeWeapon::Spear ? 0.4f : 0.65f;
 Rule.bAllowGestureChain=!SwordThrow && Recovery<=0;
 Rule.bLockMovement=!SwordThrow && !GlideAttack && PrototypeWeapon!=EBBSBPrototypeWeapon::Dagger; Rule.bOccupyConductor=PrototypeWeapon!=EBBSBPrototypeWeapon::Dagger;
 if ((Independent && Slot.IsSet() && Now<Slot->StartedAt+Slot->Duration) || (!Independent && !Arbiter.CanAttack(Rule,Channel,Now,true))) { ++RejectedActions; return false; }
 AActor* Visual=Independent && Channel==1 ? OtherFloatingWeapon.Get() : FloatingWeapon.Get();
 const FTransform StartPose=Visual->GetActorTransform();
 if (!Independent) { CancelCurrentAction(false); Arbiter.Start(EBBSBActionState::Attack,Rule,Channel,Now); }
 else if (!Arbiter.IsBusy(Now)) Arbiter.Start(EBBSBActionState::Attack,Rule,-1,Now);
 else Arbiter.Rule.Duration=FMath::Max(Arbiter.Rule.Duration,float(Now-Arbiter.StartedAt)+Duration);
 UBBSBCombatComponent* C=Character->FindComponentByClass<UBBSBCombatComponent>();
 if (C) C->OnAttackStarted();
 if (Rule.bLockMovement) { Character->ConsumeMovementInputVector(); Character->GetCharacterMovement()->StopMovementImmediately(); }
 FWeaponAction A; A.bPrototype=true; A.Gesture=G; A.Channel=Channel; A.StartedAt=Now; A.Duration=Duration; A.Revision=Arbiter.Revision;
 A.bGlideAttack=GlideAttack;A.bSwordThrow=SwordThrow;
 if (Clip) { A.Motion=*Clip; A.bAuthoredMotion=true; }
 A.Target=bWeaponAimLocked ? LockedAimPoint : PrototypeWeapon==EBBSBPrototypeWeapon::Dagger ? FindPrototypeCursorPoint() : FindAimPoint();
 if (bWeaponAimLocked)
 {
  A.AimActor=LockedAimActor;
  if (A.AimActor.IsValid()) A.AimLocalPoint=A.AimActor->GetActorTransform().InverseTransformPosition(A.Target);
  A.bRestrictToAimActor=PrototypeWeapon==EBBSBPrototypeWeapon::Spear;
 }
 A.StartPose=StartPose; A.ComboStep=Combo;
 if(SwordThrow)
 {
  FVector Eye=Character->GetActorLocation()+FVector(0,0,50);FRotator View=Character->GetActorRotation();
  if(const auto* PC=Cast<APlayerController>(Character->GetController())) PC->GetPlayerViewPoint(Eye,View);
  FVector Forward=(A.Target-Eye).GetSafeNormal();if(Forward.IsNearlyZero()) Forward=View.Vector();
  FVector Slash=FVector::VectorPlaneProject(View.RotateVector(FVector(0,-.8660254f,-.5f)),Forward).GetSafeNormal();
  if(Slash.IsNearlyZero()) Slash=FVector::CrossProduct(Forward,FVector::UpVector).GetSafeNormal();
  A.SwordThrowFrame=FRotationMatrix::MakeFromZX(Forward,Slash).ToQuat();
 }
 bHasFollowThrough[Independent && Channel==1 ? 1:0]=false;
 A.Direction=(A.Target-Character->GetActorLocation()).GetSafeNormal2D(); if (A.Direction.IsNearlyZero()) A.Direction=Character->GetActorForwardVector();
 if(SwordThrow) A.Direction=A.SwordThrowFrame.GetAxisZ();
 A.ExactDamage=Damage*(C ? BBSBCombatRules::EcstaticDamageMultiplier(C->Ecstatic) : 1.f); A.Impulse=Impulse;
 Slot.Emplace(A); ActiveState=EBBSBActionState::Attack; LastGesture=G; ++GestureCounts[int32(G)]; GestureTimes[int32(G)]=Now; ++HandAttacks[Channel];
 LastWeaponDamage=A.ExactDamage; LastAimPoint=A.Target; LastWeaponImpulse=Impulse; FormUntil=Now+1;
 OnGesture.Broadcast(G,Channel,WeaponChargeSeconds/FMath::Max(ChargeMaximumSeconds,0.01f));
 const bool Airborne=Character->GetCharacterMovement()->IsFalling();
 UAnimSequence* BodyClip=Clip ? (!GlideAttack && Airborne && Clip->AirCharacterAnimation ? Clip->AirCharacterAnimation.Get():Clip->CharacterAnimation.Get()):nullptr;
 if(!SwordThrow && !GlideAttack && Traversal && Traversal->Presentation)
 {
  const auto& ComboClips=Airborne ? Traversal->Presentation->AirWeaponCombo:Traversal->Presentation->GroundWeaponCombo;
  if((Airborne ? !(Clip && Clip->AirCharacterAnimation):!BodyClip) && ComboClips.IsValidIndex(Combo-1) && ComboClips[Combo-1]) BodyClip=ComboClips[Combo-1];
 }
 if(!SwordThrow && !GlideAttack && BodyClip) if(UAnimInstance* Anim=Character->GetMesh()->GetAnimInstance())
 {
  if(ActionMontage) Anim->Montage_Stop(.035f,ActionMontage);
  ActionMontage=Anim->PlaySlotAnimationAsDynamicMontage(BodyClip,TEXT("DefaultSlot"),.045f,.08f,BodyClip->GetPlayLength()/Duration);
 }
 if(Airborne && Traversal && !GlideAttack && !SwordThrow) Traversal->BeginAirWeaponHang(Duration);
 if (PrototypeWeapon==EBBSBPrototypeWeapon::Dagger)
 {
  ABBSBPlayerWeaponProjectile* P=GetWorld()->SpawnActor<ABBSBPlayerWeaponProjectile>(FloatingWeapon->GetActorLocation(),FloatingWeapon->GetActorRotation());
  if (P) P->Launch(Character,A.Target,A.ExactDamage,Blade->GetStaticMesh(),ActiveWeaponAnimationSet);
  DaggerAwayUntil[0]=Now+FVector::Dist(FloatingWeapon->GetActorLocation(),A.Target)/FMath::Max(100.f,ActiveWeaponAnimationSet ? ActiveWeaponAnimationSet->ProjectileSpeed:1900.f)+0.08;
 }
 else
 {
  UAnimInstance* Anim=Character->GetMesh()->GetAnimInstance();
  if (!BodyClip && PrototypeWeapon!=EBBSBPrototypeWeapon::Sword && (!Independent || !Anim || !ActionMontage || !Anim->Montage_IsPlaying(ActionMontage))) PlayGestureAnimation(G);
  if (!SwordThrow && !GlideAttack && !BodyClip && PrototypeWeapon==EBBSBPrototypeWeapon::Sword && GestureAnimations.Num()>=4)
  {
   const int32 Next=Now-LastSwordBeatAt>SwordComboWindow ? 1 : SwordComboStep%3+1;
    if (Anim)
   {
    if (ActionMontage) Anim->Montage_Stop(0.025f,ActionMontage);
    UAnimSequence* Sequence=GestureAnimations[Next==1 ? 0 : Next==2 ? 2 : 3];
    if (Sequence) ActionMontage=Anim->PlaySlotAnimationAsDynamicMontage(Sequence,TEXT("DefaultSlot"),0.03f,0.04f,Sequence->GetPlayLength()/Duration);
   }
  }
 }
 UE_LOG(LogBBSB,Display,TEXT("Weapon %s gesture=%d hand=%d damage=%.2f charge=%.2f"),*PrototypeWeaponLabel(),int32(G),Channel,A.ExactDamage,WeaponChargeSeconds);
 return true;
}

bool UBBSBFieldActionComponent::IsGlideAttacking() const
{
 const auto* Player=Cast<ABBSBCharacter>(Character);
 if(!Player || !Player->Traversal || !Player->Traversal->bGliding) return false;
 for(const auto* Slot:{&CurrentAction,&OtherWeaponAction})
  if(Slot->IsSet() && Slot->GetValue().bGlideAttack && GetWorld()->GetTimeSeconds()<Slot->GetValue().StartedAt+Slot->GetValue().Duration) return true;
 return false;
}
void UBBSBFieldActionComponent::GetGlideAttackPose(UAnimSequence*& Animation,float& Time,float& Weight) const
{
 Animation=nullptr;Time=Weight=0;
 if(!IsGlideAttacking()) return;
 const FWeaponAction* Latest=nullptr;
 for(const auto* Slot:{&CurrentAction,&OtherWeaponAction})
  if(Slot->IsSet() && Slot->GetValue().bGlideAttack && (!Latest || Slot->GetValue().StartedAt>Latest->StartedAt)) Latest=&Slot->GetValue();
 if(!Latest || Latest->bSwordThrow) return;
 Animation=Latest->Motion.CharacterAnimation;
 const float Age=GetWorld()->GetTimeSeconds()-Latest->StartedAt;
 Time=Animation ? FMath::Clamp(Age/Latest->Duration,0.f,1.f)*Animation->GetPlayLength():0;
 Weight=FMath::SmoothStep(0.f,.06f,Age)*(1.f-FMath::SmoothStep(FMath::Max(.06f,Latest->Duration-.1f),Latest->Duration,Age));
}

float UBBSBFieldActionComponent::GetWeaponRecoveryRemaining() const
{
 return !CurrentAction.IsSet() && !OtherWeaponAction.IsSet() && Arbiter.State==EBBSBActionState::Attack ? FMath::Max(0.f,float(Arbiter.StartedAt+Arbiter.Rule.Duration-GetWorld()->GetTimeSeconds())):0.f;
}
float UBBSBFieldActionComponent::GetWeaponActionProgress() const
{
 return CurrentAction.IsSet() ? FMath::Clamp(CurrentAction.GetValue().Elapsed/FMath::Max(CurrentAction.GetValue().Duration,.001f),0.f,1.f):0.f;
}
void UBBSBFieldActionComponent::GetWeaponStepPose(UAnimSequence*& Animation,float& Time,float& Weight) const
{
 Animation=nullptr;Time=Weight=0;
 if(!Character || !CurrentAction.IsSet() || !Character->GetCharacterMovement()->IsMovingOnGround()) return;
 const auto& A=CurrentAction.GetValue();if(A.bGlideAttack || !A.Motion.StepAnimation || A.Motion.ForwardTravel<=0) return;
 Animation=A.Motion.StepAnimation;
 const float T=FMath::Clamp(float(GetWorld()->GetTimeSeconds()-A.StartedAt)/A.Duration,0.f,1.f);
 Time=T*Animation->GetPlayLength();Weight=FMath::SmoothStep(0.f,.1f,T)*(1.f-FMath::SmoothStep(.88f,1.f,T));
}
bool UBBSBFieldActionComponent::GetWeaponArmPose(FVector& Hand,FVector& Elbow,float& Weight,float& BodyTurn) const
{
 Weight=BodyTurn=0;
 if(!Character || !CurrentAction.IsSet()) return false;
 const auto& A=CurrentAction.GetValue();if(!A.bAuthoredMotion || (A.bGlideAttack && !A.bSwordThrow)) return false;
 const float T=FMath::Clamp(float(GetWorld()->GetTimeSeconds()-A.StartedAt)/A.Duration,0.f,1.f);
 FTransform Arm;if(!UBBSBWeaponAnimationSet::SampleTransform(A.Motion.Sequence,TEXT("ArmMotion"),T,Arm)) return false;
 const float Start=FMath::Clamp(A.Motion.ActiveStart,0.f,.99f),End=FMath::Clamp(A.Motion.ActiveEnd,Start+.001f,1.f);
 BodyTurn=A.bSwordThrow ? 0.f:A.Motion.BodyTurnDegrees*FMath::SmoothStep(Start,End,T);
 const FRotator Frame(0,A.bSwordThrow ? Character->GetActorRotation().Yaw:A.Direction.Rotation().Yaw+BodyTurn,0);
 Hand=Character->GetActorLocation()+Frame.RotateVector(Arm.GetLocation());
 Elbow=Character->GetActorLocation()+Frame.RotateVector(FVector(30,65,35));
 Weight=FMath::SmoothStep(0.f,.1f,T)*(1.f-FMath::SmoothStep(.88f,1.f,T));
 if(A.bSwordThrow)
 {
  const float Age=T*A.Duration,Impact=End*A.Duration;
  Weight=FMath::SmoothStep(0.f,.06f,Age)*(1.f-FMath::SmoothStep(FMath::Max(.06f,Impact-.12f),Impact,Age));
 }
 return true;
}

FTransform UBBSBFieldActionComponent::PrototypeAttackPose(const FWeaponAction& A,float T) const
{
 FTransform Local;
 if (A.bAuthoredMotion && UBBSBWeaponAnimationSet::SampleTransform(A.Motion.Sequence,TEXT("WeaponMotion"),T,Local))
 {
  if(A.bSwordThrow && Blade && ActiveWeaponAnimationSet)
  {
   const auto* Settings=ActiveWeaponAnimationSet.Get();const FQuat EndRotation=A.SwordThrowFrame;
   const FVector TipOffset=Blade->GetRelativeTransform().TransformPosition(Settings->BladeTrailTip);
   const FVector EndCenter=A.Target-EndRotation.RotateVector(TipOffset);
   const FVector Forward=EndRotation.GetAxisZ(),Slash=EndRotation.GetAxisX();
   const float Impact=FMath::Clamp(A.Motion.ActiveEnd,.001f,.98f),Recall=FMath::Clamp(Settings->SwordReturnStart,Impact+.001f,.99f);
   FVector Center=FMath::Lerp(A.StartPose.GetLocation(),EndCenter,FMath::Clamp(float(Local.GetLocation().X),0.f,1.f))-Slash*Local.GetLocation().Z;
   if(T>Impact) Center+=Forward*Settings->SwordEmbedDepth*FMath::SmoothStep(Impact,Recall,T);
   FTransform Pose((EndRotation*Local.GetRotation()).GetNormalized(),Center);
   if(T>=Recall) { FTransform Blend;Blend.Blend(Pose,GetWeaponRestTransform(),FMath::SmoothStep(Recall,1.f,T));return Blend; }
   if(A.Motion.BlendIn>0 && T<A.Motion.BlendIn) { FTransform Blend;Blend.Blend(A.StartPose,Pose,FMath::SmoothStep(0.f,A.Motion.BlendIn,T));return Blend; }
   return Pose;
  }
  if (A.Motion.bMirrorLeftHand && A.Channel==1)
  { FVector P=Local.GetTranslation(); P.Y=-P.Y; Local.SetTranslation(P); FRotator R=Local.Rotator(); R.Roll=-R.Roll; Local.SetRotation(R.Quaternion()); }
  const FRotator Frame=A.Motion.bUseImpulseFrame && !A.Impulse.IsNearlyZero() ? A.Impulse.Rotation() : A.Direction.Rotation();
  const FVector Origin=A.Motion.bUseOwnerFrame ? Character->GetActorLocation():A.Target;
  FTransform Pose=Local*FTransform(Frame,Origin);
  if (A.Motion.bUseImpulseFrame) Pose.SetRotation((A.StartPose.Rotator()+Local.Rotator()).Quaternion());
  if (A.Motion.BlendIn>0 && T<A.Motion.BlendIn) { FTransform Blend; Blend.Blend(A.StartPose,Pose,FMath::SmoothStep(0.f,A.Motion.BlendIn,T)); return Blend; }
  return Pose;
 }
 const FVector Forward=A.Direction, Right=FVector::CrossProduct(FVector::UpVector,Forward);
 const float Swing=FMath::InterpEaseInOut(0.f,1.f,FMath::Clamp((T-0.2f)/0.44f,0.f,1.f),1.6f);
 const float Side=PrototypeWeapon==EBBSBPrototypeWeapon::Sword ? (A.ComboStep==2 ? -1.f:1.f) : (A.Channel==0 ? 1.f:-1.f);
 FVector Position=A.Target; FRotator Rotation=Forward.Rotation();
 if (PrototypeWeapon==EBBSBPrototypeWeapon::Spear)
 {
  if (A.Gesture==EBBSBFieldGesture::Flick)
  { Position+=A.Impulse.GetSafeNormal()*Swing*100.f; Rotation=A.StartPose.Rotator()+FRotator(0,0,Swing*40.f); }
  else { Position+=Forward*FMath::Lerp(-135.f,25.f,Swing); Rotation+=FRotator(90,0,0); }
 }
 else if (PrototypeWeapon==EBBSBPrototypeWeapon::Greatsword)
 { Position-=Forward*FMath::Lerp(135.f,-60.f,Swing); Position+=Right*FMath::Sin(Swing*PI)*45.f+FVector(0,0,FMath::Lerp(55.f,-20.f,Swing)); Rotation+=FRotator(0,0,FMath::Lerp(-75.f,110.f,Swing)); }
 else if (PrototypeWeapon==EBBSBPrototypeWeapon::Hammer && A.Gesture==EBBSBFieldGesture::Shake)
 { Position+=Right*FMath::Cos(Swing*2*PI)*70.f+FVector::UpVector*FMath::Sin(Swing*2*PI)*55.f; Rotation+=FRotator(0,0,Swing*360.f); }
 else
 {
  Position+=Right*FMath::Lerp(-125.f,125.f,Swing)*Side+Forward*FMath::Sin(Swing*PI)*25.f;
  Position.Z+=PrototypeWeapon==EBBSBPrototypeWeapon::Sword && A.ComboStep==3 ? FMath::Lerp(80.f,-25.f,Swing) : FMath::Sin(Swing*PI)*20.f;
  Rotation+=FRotator(0,0,FMath::Lerp(-85.f,95.f,Swing)*Side);
 }
 FTransform Pose(Rotation,Position);
 if (T<0.2f) { FTransform Blended; Blended.Blend(A.StartPose,Pose,FMath::SmoothStep(0.f,0.2f,T)); return Blended; }
 return Pose;
}

void UBBSBFieldActionComponent::UpdateBladeTrail(FWeaponAction& A,const FTransform& WeaponPose)
{
 if(A.bBladeTrailFinished || !Blade || !ActiveWeaponAnimationSet || !ActiveWeaponAnimationSet->bEnableBladeTrail) return;
 const auto* Settings=ActiveWeaponAnimationSet.Get();
 const FTransform MeshPose=Blade->GetRelativeTransform()*WeaponPose;
 LastBladeTrailRoot=MeshPose.TransformPosition(Settings->BladeTrailRoot);
 LastBladeTrailTip=MeshPose.TransformPosition(Settings->BladeTrailTip);
 if(!A.BladeTrail.IsValid() && Settings->BladeTrailMaterial)
 {
  auto* Trail=NewObject<UBBSBBladeTrailComponent>(FloatingWeapon);
  FloatingWeapon->AddInstanceComponent(Trail);Trail->SetupAttachment(FloatingWeapon->GetRootComponent());
  Trail->SetAbsolute(true,true,true);Trail->SetWorldLocation((LastBladeTrailRoot+LastBladeTrailTip)*.5);
  Trail->TrailMaterial=Settings->BladeTrailMaterial;Trail->Lifetime=FMath::Max(.03f,Settings->BladeTrailLifetime);Trail->Color=Settings->BladeTrailColor;
  Trail->RegisterComponent();A.BladeTrail=Trail;
 }
 if(auto* Trail=A.BladeTrail.Get()) { Trail->AddBladeSample(LastBladeTrailRoot,LastBladeTrailTip);bBladeTrailEmitting=true; }
}

void UBBSBFieldActionComponent::StopBladeTrail(FWeaponAction& A)
{
 if(auto* Trail=A.BladeTrail.Get()) Trail->Finish();
 A.BladeTrail.Reset();A.bBladeTrailFinished=true;bBladeTrailEmitting=false;
}

void UBBSBFieldActionComponent::TraceSwordBlade(const FTransform& From,const FTransform& To,FWeaponAction& A)
{
 if(!Blade || !ActiveWeaponAnimationSet) return;
 const auto* Settings=ActiveWeaponAnimationSet.Get();const FTransform Relative=Blade->GetRelativeTransform();
 const FTransform Before=Relative*From,After=Relative*To;
 const FVector R0=Before.TransformPosition(Settings->BladeTrailRoot),T0=Before.TransformPosition(Settings->BladeTrailTip);
 const FVector R1=After.TransformPosition(Settings->BladeTrailRoot),T1=After.TransformPosition(Settings->BladeTrailTip);
 const float Radius=FMath::Max(1.f,Settings->BladeHitRadius);const int32 Slices=FMath::Max(1,FMath::CeilToInt(FVector::Dist(R1,T1)/(Radius*1.5f)));
 const uint32 Revision=Arbiter.Revision;
 for(int32 I=0;I<=Slices;++I)
 {
  const float U=float(I)/Slices;TraceWeapon(FMath::Lerp(R0,T0,U),FMath::Lerp(R1,T1,U),A,Radius);
  if(Arbiter.Revision!=Revision) return;
 }
}

void UBBSBFieldActionComponent::TickPrototypeWeapon(float DT)
{
 for (int32 Index=0; Index<2; ++Index)
 {
  auto& Slot=Index==0 ? CurrentAction : OtherWeaponAction; if (!Slot.IsSet()) continue;
  FWeaponAction& A=Slot.GetValue(); const uint32 Revision=Arbiter.Revision;
  const float PreviousT=FMath::Clamp(A.Elapsed/A.Duration,0.f,1.f);
  A.Elapsed=GetWorld()->GetTimeSeconds()-A.StartedAt;
  // Movement during a glide does not move the captured attack point.
  // Explicit actor locks still follow their actor below.
  const float T=FMath::Clamp(A.Elapsed/A.Duration,0.f,1.f);
  if(A.bAuthoredMotion && !A.bGlideAttack && A.Motion.ForwardTravel>0 && Character->GetCharacterMovement()->IsMovingOnGround())
  {
   const float Start=FMath::Clamp(A.Motion.ActiveStart,0.f,.99f),End=FMath::Clamp(A.Motion.ActiveEnd,Start+.001f,1.f);
   const float Step=A.Motion.ForwardTravel*(FMath::SmoothStep(Start,End,T)-FMath::SmoothStep(Start,End,PreviousT));
   if(Step>0) { FHitResult Hit;Character->SetActorLocation(Character->GetActorLocation()+A.Direction*Step,true,&Hit,ETeleportType::None); }
  }
  if (!A.bSwordThrow && A.AimActor.IsValid() && !A.HitActors.Contains(A.AimActor.Get()))
   A.Target=A.AimActor->GetActorTransform().TransformPosition(A.AimLocalPoint);
  AActor* Visual=PrototypeWeapon==EBBSBPrototypeWeapon::Crescent && A.Channel==1 ? OtherFloatingWeapon.Get() : FloatingWeapon.Get();
  if (PrototypeWeapon==EBBSBPrototypeWeapon::Spear && A.Gesture==EBBSBFieldGesture::Beat && bSpearEmbedded)
  {
   // The attached spear waits without another damage trace or automatic return.
   if (T>=1.f) Slot.Reset();
   continue;
  }
  if (PrototypeWeapon!=EBBSBPrototypeWeapon::Dagger)
  {
   Visual->SetActorTransform(PrototypeAttackPose(A,T));
   // Damage exists only while crossing the swing; the visible follow-through never adds another hit.
   const float ActiveStart=A.bAuthoredMotion ? FMath::Clamp(A.Motion.ActiveStart,0.f,1.f):0.2f;
   const float ActiveEnd=A.bAuthoredMotion ? FMath::Clamp(A.Motion.ActiveEnd,ActiveStart,1.f):0.64f;
   if(PrototypeWeapon==EBBSBPrototypeWeapon::Sword)
   {
    if(T>=ActiveStart && PreviousT<=ActiveEnd) UpdateBladeTrail(A,PrototypeAttackPose(A,FMath::Min(T,ActiveEnd)));
    else if(T>ActiveEnd) StopBladeTrail(A);
   }
   if (T>=ActiveStart && PreviousT<=ActiveEnd)
   {
    const float Begin=FMath::Max(PreviousT,ActiveStart), End=FMath::Min(T,ActiveEnd);
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(A.bSwordThrow ? (End-Begin)/FMath::Max(.001f,ActiveEnd-ActiveStart)*540.f/12.f:(End-Begin)*24.f));
    FTransform FromPose=PrototypeAttackPose(A,Begin);
    FVector From=FromPose.GetLocation();
    for (int32 Step=1;Step<=Steps;++Step)
    {
     const FTransform ToPose=PrototypeAttackPose(A,FMath::Lerp(Begin,End,float(Step)/Steps));
     const FVector To=ToPose.GetLocation();
     if(A.bSwordThrow) TraceSwordBlade(FromPose,ToPose,A);else TraceWeapon(From,To,A);
     if (Arbiter.Revision!=Revision) return;From=To;FromPose=ToPose;
    }
   }
   if (T>=1.f)
   {
    const int32 PoseIndex=PrototypeWeapon==EBBSBPrototypeWeapon::Crescent && A.Channel==1 ? 1:0;
    FollowThroughPose[PoseIndex]=PrototypeAttackPose(A,1.f); bHasFollowThrough[PoseIndex]=!A.bSwordThrow;
    FollowThroughUntil[PoseIndex]=GetWorld()->GetTimeSeconds()+WeaponReturnSeconds+(PrototypeWeapon==EBBSBPrototypeWeapon::Sword ? 0.75f:0.f);
   }
  }
  if (T>=1.f) Slot.Reset();
 }
 if (!CurrentAction.IsSet() && !OtherWeaponAction.IsSet() && Arbiter.State==EBBSBActionState::Attack && !Arbiter.IsBusy(GetWorld()->GetTimeSeconds())) { Arbiter.Clear(); ActiveState=EBBSBActionState::Idle; }
}

void UBBSBFieldActionComponent::PresentPrototypeWeapon()
{
 if (!Character || !Character->IsLocallyControlled() || !IsValid(FloatingWeapon) || !OtherFloatingWeapon || !ConductorWeapon || !FormUpper) return;
 const double Now=GetWorld()->GetTimeSeconds(); const bool Dual=IsDual(PrototypeWeapon);
 const bool Split=ActiveWeaponAnimationSet ? ActiveWeaponAnimationSet->bSplitReparaton && (ActiveWeaponAnimationSet->bKeepFormEquipped || IsGestureCaptured() || WeaponChargeSeconds>0 || Now<FormUntil)
  : Dual || (PrototypeWeapon==EBBSBPrototypeWeapon::Hammer && (IsGestureCaptured() || WeaponChargeSeconds>0 || Now<FormUntil));
 ConductorFull->SetVisibility(!Split); FormUpper->SetVisibility(Split); FormLower->SetVisibility(Split && !Dual); OtherFormLower->SetVisibility(Dual);
 Blade->SetVisibility(PrototypeWeapon!=EBBSBPrototypeWeapon::Dagger || Now>=DaggerAwayUntil[0]);
 OtherBlade->SetVisibility(PrototypeWeapon==EBBSBPrototypeWeapon::Crescent);
 FTransform Top(FRotator::ZeroRotator,FVector(0,0,-6)), Bottom(FRotator::ZeroRotator,FVector(0,0,-90));
 if (PrototypeWeapon==EBBSBPrototypeWeapon::Hammer && Split)
 {
  // Exact field Top3/Bottom3 relative pose, translated as one assembly to its grip.
  Top=FTransform(FRotator(-89.580769f,180,-180),FVector(-44.12132f,0,23.73694f)); Bottom=FTransform(FRotator::ZeroRotator,FVector(0,0,-70));
 }
 if (Dual)
 {
  const FRotator UpperRotation(29.881766f,0,0), LowerRotation(-22.405616f,0,180);
  Top=FTransform(UpperRotation,-UpperRotation.RotateVector(FVector(0,0,25)));
  Bottom=FTransform(LowerRotation,-LowerRotation.RotateVector(FVector(0,0,90)));
 }
 if (ActiveWeaponAnimationSet && ActiveWeaponAnimationSet->FormSequence)
 {
  FormProgress=FMath::FInterpConstantTo(FormProgress,Split ? 1.f:0.f,GetWorld()->GetDeltaSeconds(),1.f/FMath::Max(ActiveWeaponAnimationSet->FormTransitionSeconds,0.01f));
  UBBSBWeaponAnimationSet::SampleTransform(ActiveWeaponAnimationSet->FormSequence,TEXT("Upper"),FormProgress,Top);
  UBBSBWeaponAnimationSet::SampleTransform(ActiveWeaponAnimationSet->FormSequence,TEXT("Lower"),FormProgress,Bottom);
  CurrentUpperPose=Top; CurrentLowerPose=Bottom;
 }
 else
 {
  const float Alpha=FMath::Clamp(GetWorld()->GetDeltaSeconds()/0.12f,0.f,1.f);
  CurrentUpperPose.Blend(CurrentUpperPose,Top,Alpha); CurrentLowerPose.Blend(CurrentLowerPose,Bottom,Alpha);
 }
 FormUpper->SetRelativeTransform(CurrentUpperPose); FormLower->SetRelativeTransform(CurrentLowerPose); OtherFormLower->SetRelativeTransform(CurrentLowerPose);
 const FTransform Rest=GetWeaponRestTransform();
 FVector Eye; FRotator View; Cast<APlayerController>(Character->GetController())->GetPlayerViewPoint(Eye,View);
 // The floating weapon keeps the screen-right position, independently of the hand-held conductor form.
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBWeaponRestWall),false,Character); Q.AddIgnoredActor(FloatingWeapon); Q.AddIgnoredActor(OtherFloatingWeapon);
 const bool Paired=PrototypeWeapon==EBBSBPrototypeWeapon::Crescent;
 const bool Active[2]={CurrentAction.IsSet(),OtherWeaponAction.IsSet()};
 for (int32 Index=0; Index<(Paired ? 2 : 1); ++Index)
 {
  if (Active[Index] && PrototypeWeapon!=EBBSBPrototypeWeapon::Dagger) continue;
  AActor* Visual=Index==0 ? FloatingWeapon.Get() : OtherFloatingWeapon.Get();
  UStaticMeshComponent* Model=Index==0 ? Blade.Get() : OtherBlade.Get();
  if (!Model->GetStaticMesh()) continue;

  if (Index==0 && bSpearEmbedded) continue;

  if (Index==0 && bWeaponAimLocked && !CurrentAction.IsSet())
  {
   const float Pull=75.f+WeaponChargeSeconds/FMath::Max(ChargeMaximumSeconds,0.01f)*170.f;
   FTransform Ready(LockedApproach.Rotation()+FRotator(0,0,-45),LockedAimPoint-LockedApproach*Pull+FVector(0,0,35));
   FTransform Pose; Pose.Blend(AimLockWeaponFrom,Ready,FMath::SmoothStep(0.f,0.22f,float(Now-AimLockStartedAt)));
   Visual->SetActorTransform(Pose); continue;
  }
  if (bHasFollowThrough[Index])
  {
   if (Now<FollowThroughUntil[Index])
   {
    FTransform Pose; Pose.Blend(FollowThroughPose[Index],Rest,FMath::SmoothStep(0.f,1.f,float((Now-(FollowThroughUntil[Index]-WeaponReturnSeconds))/WeaponReturnSeconds)));
    Visual->SetActorTransform(Pose); continue;
   }
   bHasFollowThrough[Index]=false;
  }
  FTransform Pose=Rest;
  Pose.SetRotation(View.Quaternion()*FRotator(10,90,Index==0 ? -15 : 15).Quaternion());
  if (Paired) Pose.AddToTranslation(View.RotateVector(FVector(0,Index==0 ? -24 : 24,Index==0 ? 30 : -30)));
  const FBox LocalBounds=Model->GetStaticMesh()->GetBoundingBox().TransformBy(Model->GetRelativeTransform());
  const FVector CenterOffset=Pose.GetRotation().RotateVector(LocalBounds.GetCenter());
  const FCollisionShape Shape=FCollisionShape::MakeBox(LocalBounds.GetExtent()+FVector(2));
  Q.bFindInitialOverlaps=true;
  FHitResult Wall;
  if (GetWorld()->SweepSingleByChannel(Wall,Character->GetActorLocation()+CenterOffset,Pose.GetLocation()+CenterOffset,Pose.GetRotation(),ECC_Camera,Shape,Q))
  {
   Pose.SetLocation(Wall.bStartPenetrating ? Character->GetActorLocation()+Wall.Normal*(Wall.PenetrationDepth+2) : Wall.Location-CenterOffset);
   for (int32 Resolve=0; Resolve<3; ++Resolve)
   {
    const FVector Center=Pose.GetLocation()+CenterOffset;
    if (!GetWorld()->SweepSingleByChannel(Wall,Center,Center,Pose.GetRotation(),ECC_Camera,Shape,Q) || !Wall.bStartPenetrating) break;
    Pose.AddToTranslation(Wall.Normal*(Wall.PenetrationDepth+2));
   }
  }
  Visual->SetActorTransform(Pose);
 }
 LastWeaponPosition=FloatingWeapon->GetActorLocation();
}

ABBSBPlayerWeaponProjectile::ABBSBPlayerWeaponProjectile()
{
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostPhysics;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ThrowOrigin")));
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlyingWeapon")); Body->SetupAttachment(RootComponent); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ABBSBPlayerWeaponProjectile::Launch(ACharacter* Shooter,FVector Target,float Amount,UStaticMesh* Mesh,UBBSBWeaponAnimationSet* AnimationSet)
{
 SourceCharacter=Shooter; SetOwner(Shooter); FrozenTarget=Target; Damage=Amount; Body->SetStaticMesh(Mesh);
 if (Mesh) Body->SetRelativeLocation(-Mesh->GetBoundingBox().GetCenter());
 const float Speed=FMath::Max(100.f,AnimationSet ? AnimationSet->ProjectileSpeed:1900.f);
 FlightOrigin=GetActorLocation(); FlightDistance=FVector::Dist(Target,FlightOrigin); FlightSeconds=FMath::Max(0.01f,FlightDistance/Speed);
 if (AnimationSet) if (const auto* Clip=AnimationSet->FindClip(EBBSBFieldGesture::Beat,0,1)) FlightSequence=Clip->Sequence;
 FlightVelocity=(Target-FlightOrigin).GetSafeNormal()*Speed; SetActorRotation(FlightVelocity.Rotation()+FRotator(-90,0,0)); SetLifeSpan(FMath::Max(2.f,FlightSeconds+0.5f));
}
void ABBSBPlayerWeaponProjectile::Tick(float DT)
{
 Super::Tick(DT); if (!SourceCharacter.IsValid()) { Destroy(); return; }
 const FVector From=GetActorLocation(); const FVector Delta=FlightVelocity*DT;
 const bool Arrive=Age+DT>=FlightSeconds;
 FVector To=Arrive ? FrozenTarget : From+Delta;
 FTransform Local;
 const bool Authored=UBBSBWeaponAnimationSet::SampleTransform(FlightSequence,TEXT("WeaponMotion"),FMath::Clamp((Age+DT)/FlightSeconds,0.f,1.f),Local);
 if (Authored)
 {
  const FVector P=Local.GetLocation();
  To=Arrive ? FrozenTarget : FrozenTarget+FlightVelocity.Rotation().RotateVector(FVector(P.X*FlightDistance/250.f,P.Y,P.Z));
  SetActorRotation((Local*FTransform(FlightVelocity.Rotation(),FVector::ZeroVector)).GetRotation());
 }
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBPlayerThrow),false,SourceCharacter.Get()); Q.AddIgnoredActor(this);
 FHitResult Hit;
 if (GetWorld()->SweepSingleByChannel(Hit,From,To,GetActorQuat(),ECC_Visibility,FCollisionShape::MakeCapsule(16,54),Q))
 {
  ABBSBBreakableProp* Prop=Cast<ABBSBBreakableProp>(Hit.GetActor());
  if (Cast<ABBSBInkEnemy>(Hit.GetActor()) || Prop)
  {
   const bool WasBroken=Prop && Prop->bBroken;
   UGameplayStatics::ApplyPointDamage(Hit.GetActor(),Damage,FlightVelocity.GetSafeNormal(),Hit,SourceCharacter->GetController(),SourceCharacter.Get(),nullptr);
   if (UBBSBFieldActionComponent* A=SourceCharacter->FindComponentByClass<UBBSBFieldActionComponent>())
   {
    A->LastHitAt=GetWorld()->GetTimeSeconds(); A->LastHitLabel=FString::Printf(TEXT("Dagger %.0f"),Damage);
    if (Prop && !WasBroken && Prop->bBroken) ++A->PropsBroken;
   }
  }
  SetActorLocation(Hit.Location); Destroy(); return;
 }
 SetActorLocation(To); if (!Authored) AddActorLocalRotation(FRotator(0,0,DT*720)); Age+=DT;
 if (Arrive || Age>FlightSeconds+0.25f) Destroy();
}
