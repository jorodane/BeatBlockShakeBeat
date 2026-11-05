#include "Hunting/BBSBEquipmentGrowth.h"
#include "Hunting/BBSBItem.h"
#include "Hunting/BBSBHunting.h"
#include "BBSBCharacter.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Field/BBSBFieldActions.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

double UBBSBEquipmentGrowthComponent::MusicBeat() const
{ if(bConcertoActive)return ConcertoMusicStart+(GetWorld()->GetTimeSeconds()-ConcertoStartedAt)*(Player->Combat->IsStageLocked() ? Player->Combat->Stage->BPM:Player->Combat->FieldBPM)/60.;return Player && Player->Combat->IsStageLocked() ? Player->Combat->Stage->CurrentBeat:GetWorld()->GetTimeSeconds()*(Player ? Player->Combat->FieldBPM:96.f)/60.; }
float UBBSBEquipmentGrowthComponent::ConcertoProgress() const
{ return bConcertoActive ? FMath::Clamp(float(ConcertoElapsed/ConcertoDuration),0.f,1.f):1.f; }
FBBSBPerformanceRole UBBSBEquipmentGrowthComponent::NormalBeatRole(const FBBSBWeaponSnapshot& Weapon) const
{
 auto Role=Weapon.Sub;Role.CameraPreset=nullptr;Role.DamageMultiplier=1;Role.Duration=FMath::Clamp(60.f/FMath::Max(20.f,Player->Combat->FieldBPM)*.8f,.2f,.7f);Role.Interval=100;
 switch(Weapon.TemplateId)
 {
  case 2:Role.Pattern=EBBSBPerformancePattern::Thrust;break;
  case 3:Role.Pattern=EBBSBPerformancePattern::Pulse;break;
  case 5:Role.Pattern=EBBSBPerformancePattern::Volley;break;
  default:Role.Pattern=EBBSBPerformancePattern::Slash;break;
 }
 return Role;
}
FVector UBBSBEquipmentGrowthComponent::SpiralPosition(const FPerformance& P,float T) const
{
 const float Radius=FMath::Lerp(90.f,FMath::Max(90.f,P.Role.Radius),FMath::Pow(FMath::Clamp(T,0.f,1.f),.85f));
 const float A=PI*.5f-T*FMath::Max(1.f,P.Role.SpiralTurns)*2*PI;
 return ConcertoOrigin+ConcertoFrame.RotateVector(FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,25.f+FMath::Sin(T*PI)*30.f));
}
void UBBSBEquipmentGrowthComponent::SweepSpiral(FPerformance& P,float Previous,float Current)
{
 const int32 Steps=FMath::Clamp(FMath::CeilToInt((Current-Previous)*FMath::Max(1.f,P.Role.SpiralTurns)*360.f/5.f),1,180);
 TArray<FVector> Points;Points.Reserve(Steps+1);for(int32 I=0;I<=Steps;++I)Points.Add(SpiralPosition(P,FMath::Lerp(Previous,Current,float(I)/Steps)));
 const float Radius=FMath::Max(10.f,P.Role.SweepRadius);const double Now=GetWorld()->GetTimeSeconds();
 auto Touches=[&](const FVector& Point,float Extra)
 { for(int32 I=1;I<Points.Num();++I)if(FVector::DistSquared(Point,FMath::ClosestPointOnSegment(Point,Points[I-1],Points[I]))<=FMath::Square(Radius+Extra))return true;return false; };
 for(TActorIterator<ABBSBInkProjectile> It(GetWorld());It;++It)if(!It->bResolved && Touches(It->GetActorLocation(),15.f))
 { EmitBulletClear(It->GetActorLocation(),It->Source.IsValid() ? It->Source->Tint:FLinearColor(.6f,.9f,.85f));It->bResolved=true;It->Destroy();++BulletsCleared; }
 for(TActorIterator<ABBSBInkEnemy> It(GetWorld());It;++It)
 {
  auto* Enemy=*It;const FVector Point=Enemy->GetAimPoint();
  // Damage persists inside the visibly growing whirlwind, rather than requiring
  // the single outer blade to pass the same target again.
  const float FilledRadius=FVector::Dist2D(SpiralPosition(P,Current),ConcertoOrigin)+Radius;
  if(Enemy->bDefeated || FVector::DistSquared2D(Point,ConcertoOrigin)>FMath::Square(FilledRadius+32.f) || FMath::Abs(Point.Z-ConcertoOrigin.Z)>Radius+60.f)continue;
  const auto* Last=P.LastSpiralHit.Find(Enemy);if(Last && Now-*Last<FMath::Max(.06f,P.Role.SpiralHitInterval))continue;
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBSpiralCover),false,Player);Query.AddIgnoredActor(Enemy);FHitResult Cover;
  if(GetWorld()->LineTraceSingleByObjectType(Cover,ConcertoOrigin+FVector(0,0,25),Enemy->GetAimPoint(),BBSBCombatVisuals::GeometryTypes(),Query))continue;
  P.LastSpiralHit.Add(Enemy,Now);
  const float Damage=(Player->FieldActions->BaseDamage*P.Weapon.DamageMultiplier+P.Weapon.FlatDamage)*P.Role.DamageMultiplier*Settings->MainDamageMultiplier*ConditionalDamageMultiplier(P.Weapon)*FMath::Max(0.f,P.Role.SpiralHitDamageScale);
  const float Actual=UGameplayStatics::ApplyDamage(Enemy,Damage,Player->GetController(),Player,nullptr);NotifyWeaponHit(P.Weapon,Enemy,Actual,Enemy->GetAimPoint(),true);
 }
}

namespace
{
float Smooth(float T) { T=FMath::Clamp(T,0.f,1.f);return T*T*T*(T*(T*6-15)+10); }
void BlendView(FMinimalViewInfo& Out,const FMinimalViewInfo& A,const FMinimalViewInfo& B,float T)
{ Out.Location=FMath::Lerp(A.Location,B.Location,T);Out.Rotation=FQuat::Slerp(A.Rotation.Quaternion(),B.Rotation.Quaternion(),T).Rotator();Out.FOV=FMath::Lerp(A.FOV,B.FOV,T); }
}
void UBBSBEquipmentGrowthComponent::ApplyConcertoCamera(float DT,FMinimalViewInfo& View)
{
 if(!bConcertoCameraActive || !Player || !CameraPreset)return;
 const FMinimalViewInfo Gameplay=View;
 if(bCameraReturning)
 {
  const float T=Smooth((GetWorld()->GetTimeSeconds()-CameraReturnAt)/FMath::Max(.05f,CameraPreset->InterruptedReturnSeconds));BlendView(View,CameraReturnFrom,Gameplay,T);
  if(T>=1) { bCameraReturning=bConcertoCameraActive=false;CameraPreset=nullptr; }return;
 }
 if(!bConcertoActive || CameraPreset->Keys.Num()<2) { bConcertoCameraActive=false;return; }
 const float T=ConcertoProgress();const auto& Keys=CameraPreset->Keys;int32 I=0;
 while(I+1<Keys.Num()-1 && T>Keys[I+1].Time)++I;
 const auto& A=Keys[I];const auto& B=Keys[I+1];const float Span=FMath::Max(.001f,B.Time-A.Time),U=FMath::Clamp((T-A.Time)/Span,0.f,1.f);
 auto Sample=[&](float FBBSBConcertoCameraKey::* Member)
 {
  const int32 L=FMath::Max(0,I-1),R=FMath::Min(Keys.Num()-1,I+2);
  const float M0=(B.*Member-Keys[L].*Member)/FMath::Max(.001f,B.Time-Keys[L].Time)*Span;
  const float M1=(Keys[R].*Member-A.*Member)/FMath::Max(.001f,Keys[R].Time-A.Time)*Span;
  if(FMath::IsNearlyEqual(A.*Member,B.*Member))return A.*Member;
  return FMath::CubicInterp(A.*Member,M0,B.*Member,M1,U);
 };
 const float Angle=FMath::DegreesToRadians(Sample(&FBBSBConcertoCameraKey::Azimuth)),Distance=FMath::Max(55.f,Sample(&FBBSBConcertoCameraKey::Distance));
 // Calibrate the authored canonical hand/head heights to the current character
 // skeleton. This keeps a close-up centred after mesh or body proportions change.
 const float HandWeight=1-Smooth((T-.08f)/.12f),FaceWeight=Smooth((T-.08f)/.12f)*(1-Smooth((T-.34f)/.22f));
 const auto* Mesh=Player->GetMesh();
 const float HandCorrection=Mesh->DoesSocketExist(TEXT("hand_r")) ? Mesh->GetSocketLocation(TEXT("hand_r")).Z-ConcertoOrigin.Z-25.f:0;
 const float HeadCorrection=Mesh->DoesSocketExist(TEXT("head")) ? Mesh->GetSocketLocation(TEXT("head")).Z-ConcertoOrigin.Z-65.f:0;
 const float HeightCorrection=HandWeight*HandCorrection+FaceWeight*HeadCorrection;
 const FVector Focus=ConcertoOrigin+FVector::UpVector*(Sample(&FBBSBConcertoCameraKey::FocusHeight)+HeightCorrection);
 FMinimalViewInfo Shot=View;Shot.Location=ConcertoOrigin+ConcertoFrame.RotateVector(FVector(FMath::Cos(Angle)*Distance,FMath::Sin(Angle)*Distance,Sample(&FBBSBConcertoCameraKey::Height)+HeightCorrection));
 // Use engine camera collision, without steering the underlying gameplay camera.
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBConcertoCamera),false,Player);Query.AddIgnoredActor(Player->FieldActions->BunniarEquipment);
 for(const auto& P:Performances)if(P.Visual.IsValid())Query.AddIgnoredActor(P.Visual.Get());FHitResult Wall;
 if(GetWorld()->SweepSingleByObjectType(Wall,Focus,Shot.Location,FQuat::Identity,BBSBCombatVisuals::GeometryTypes(),FCollisionShape::MakeSphere(8.f),Query))Shot.Location=Wall.Location+Wall.Normal*3;
 Shot.Rotation=(Focus-Shot.Location).Rotation();Shot.FOV=FMath::Clamp(Sample(&FBBSBConcertoCameraKey::FOV),25.f,110.f);
 const float In=Smooth(T/FMath::Max(.001f,CameraPreset->BlendInFraction));BlendView(View,Gameplay,Shot,In);
 const float Return=Smooth((T-CameraPreset->ReturnStart)/FMath::Max(.01f,1-CameraPreset->ReturnStart));const FMinimalViewInfo Cinematic=View;BlendView(View,Cinematic,Gameplay,Return);
 LastCinematicView=View;bCameraSampled=true;
}
