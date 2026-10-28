#include "Combat/BBSBDefensePresentation.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Field/BBSBFieldActions.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Character.h"

UAnimSequence* UBBSBDefensePresentation::FindParry(uint8 Mask) const
{
 for(const auto& Clip:Parries) if(Clip.Directions==Mask) return Clip.Animation;
 return nullptr;
}

ABBSBDefenseImpactEffect::ABBSBDefenseImpactEffect()
{
 PrimaryActorTick.bCanEverTick=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Impact"));
}
void ABBSBDefenseImpactEffect::Initialize(const FVector& Point,const FVector& Normal,bool bBlock,const FLinearColor& Color,float Scale)
{
 SetActorLocation(Point);bBlockEffect=bBlock;EffectScale=FMath::Clamp(Scale,.1f,2.f);
 Out=Normal.GetSafeNormal();if(Out.IsNearlyZero()) Out=FVector::ForwardVector;
 Across=FVector::CrossProduct(FVector::UpVector,Out).GetSafeNormal();
 if(Across.IsNearlyZero()) Across=FVector::RightVector;
 Up=FVector::CrossProduct(Out,Across).GetSafeNormal();
 auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 for(int32 I=0;I<16;++I)
 {
  auto* Part=NewObject<UStaticMeshComponent>(this);
  Part->SetupAttachment(RootComponent);Part->SetStaticMesh(Sphere);
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCastShadow(false);Part->bReceivesDecals=false;
  AddInstanceComponent(Part);Part->RegisterComponent();
  const FLinearColor Tint=bBlock && I%3==0 ? FLinearColor(1.f,.92f,.68f):Color;
  BBSBCombatVisuals::Tint(Part,Tint);Pieces.Add(Part);
 }
 SetLifeSpan(bBlock ? .48f:.6f);Tick(0);
}
void ABBSBDefenseImpactEffect::Tick(float DT)
{
 Super::Tick(DT);Age+=DT;
 const float Duration=bBlockEffect ? .46f:.58f,T=FMath::Clamp(Age/Duration,0.f,1.f),Fade=1.f-FMath::SmoothStep(.45f,1.f,T);
 for(int32 I=0;I<Pieces.Num();++I)
 {
  const float Angle=2.f*PI*I/Pieces.Num();const FVector Radial=Across*FMath::Cos(Angle)+Up*FMath::Sin(Angle);
  const float Distance=(bBlockEffect ? 7.f+52.f*FMath::Sin(T*HALF_PI):4.f+34.f*T)*EffectScale;
  const FVector Offset=Radial*Distance+Out*(bBlockEffect ? 3.f:6.f+10.f*T)-FVector::UpVector*(bBlockEffect ? 0.f:55.f*T*T);
  auto* Part=Pieces[I].Get();Part->SetWorldLocation(GetActorLocation()+Offset);
  Part->SetWorldRotation(FRotationMatrix::MakeFromZ(Radial).Rotator());
  const float Size=FMath::Max(.001f,Fade*EffectScale);
  Part->SetWorldScale3D(bBlockEffect ? FVector(.025f,.025f,.13f)*Size:FVector(.05f,.04f,.06f)*Size);
 }
}

uint8 UBBSBCombatComponent::HeldParryMask() const
{
 uint8 Mask=0;for(int32 I=0;I<4;++I) if(Inputs[I].bHeld) Mask|=1<<I;return Mask;
}
void UBBSBCombatComponent::StartDefenseMotion(uint8 Mask,bool bHit)
{
 if(!GetWorld() || !DefensePresentation) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(!bHit && bDefenseHitReaction && Now-DefenseStartedAt<.18) return;
 UAnimSequence* Clip=bHit ? DefensePresentation->HitReaction.Get():DefensePresentation->FindParry(Mask);
 if(!Clip) return;
 const bool SameChord=!bHit && !bDefenseHitReaction && Now-DefenseStartedAt<=.045;
 if(!SameChord) { DefenseStartedAt=Now;++DefenseMotionSerial; }
 ActiveDefenseMask=bHit ? 0:Mask;bDefenseHitReaction=bHit;DefenseAnimation=Clip;DefenseDuration=Clip->GetPlayLength();
 UpdateDefensePresentation();
}
void UBBSBCombatComponent::CancelDefenseMotion()
{
 DefenseStartedAt=-100;DefenseDuration=0;DefensePoseWeight=DefensePoseTime=0;ActiveDefenseMask=0;bDefenseHitReaction=false;DefenseAnimation=nullptr;
}
bool UBBSBCombatComponent::IsDefenseMotionActive() const
{
 return DefenseAnimation && GetWorld() && GetWorld()->GetTimeSeconds()<DefenseStartedAt+DefenseDuration;
}
void UBBSBCombatComponent::UpdateDefensePresentation()
{
 const float Age=GetWorld() ? GetWorld()->GetTimeSeconds()-DefenseStartedAt:100.f;
 DefensePoseTime=FMath::Clamp(Age,0.f,DefenseDuration);
 DefensePoseWeight=DefenseAnimation ? FMath::SmoothStep(0.f,.018f,Age)*(1.f-FMath::SmoothStep(FMath::Max(.02f,DefenseDuration-.075f),DefenseDuration+.04f,Age)):0;
 if(Age>DefenseDuration+.04f) { DefenseAnimation=nullptr;DefensePoseWeight=0;ActiveDefenseMask=0;bDefenseHitReaction=false; }
}
void UBBSBCombatComponent::GetDefensePose(UAnimSequence*& Clip,float& Time,float& Weight) const
{
 Clip=DefenseAnimation;Time=DefensePoseTime;Weight=DefensePoseWeight;
}
FVector UBBSBCombatComponent::DefenseTarget(int32 Direction) const
{
 FVector Origin,Normal,Right;GetParryPlane(Origin,Normal,Right);
 const auto* Capsule=Character ? Character->GetCapsuleComponent():nullptr;
 const float Radius=Capsule ? Capsule->GetScaledCapsuleRadius():42.f,Half=Capsule ? Capsule->GetScaledCapsuleHalfHeight():92.f;
 if(Direction==0) return Origin+FVector::UpVector*(Half*.70f);
 if(Direction==2) return Origin-FVector::UpVector*(Half*.62f);
 return Origin+Right*(Radius*.88f*(Direction==1 ? 1.f:-1.f));
}
void UBBSBCombatComponent::DefenseImpact(const FVector& Point,bool bBlock)
{
 if(!Character || !GetWorld()) return;
 LastDefenseImpactAt=GetWorld()->GetTimeSeconds();LastDefenseImpact=Point;bLastDefenseBlocked=bBlock;
 if(bBlock) { ++DefenseCombo;BestDefenseCombo=FMath::Max(BestDefenseCombo,DefenseCombo); }
 else { BrokenDefenseCombo=DefenseCombo;DefenseCombo=0; }
 FVector Origin,Normal,Right;GetParryPlane(Origin,Normal,Right);
 auto* FX=GetWorld()->SpawnActor<ABBSBDefenseImpactEffect>(ABBSBDefenseImpactEffect::StaticClass(),Point,FRotator::ZeroRotator);
 if(FX)
 {
  const FLinearColor Color=DefensePresentation ? (bBlock ? DefensePresentation->BlockColor:DefensePresentation->HitColor):
   (bBlock ? FLinearColor(.98f,.79f,.9f):FLinearColor(.56f,.24f,.36f));
  FX->Initialize(Point,Normal,bBlock,Color,DefensePresentation ? DefensePresentation->EffectScale:1.f);++DefenseEffectsSpawned;
 }
 if(!bBlock) StartDefenseMotion(0,true);
 else if(!IsDefenseMotionActive() || (!bDefenseHitReaction && GetWorld()->GetTimeSeconds()-DefenseStartedAt>DefenseDuration*.8f))
  StartDefenseMotion(HeldParryMask() ? HeldParryMask():uint8(1<<int32(LastDirection)));
}
void UBBSBCombatComponent::DrawDefenseFeedback(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || !GetWorld()) return;
 DrawDefenseDirectionCues(HUD,Canvas);
 const float Age=GetWorld()->GetTimeSeconds()-LastDefenseImpactAt;
 if((!IsStageLocked() && Age>2.f) || (DefenseCombo==0 && Age>2.f)) return;
 const float S=FMath::Clamp(Canvas->SizeX/1280.f,.7f,1.35f),X=Canvas->SizeX*.5f-75.f*S,Y=70.f*S;
 const FLinearColor Color=bLastDefenseBlocked ? FLinearColor(.96f,.82f,.91f):FLinearColor(.98f,.61f,.68f);
 const FString Label=Age<1.2f && !bLastDefenseBlocked ? FString::Printf(TEXT("HIT  |  %d COMBO BREAK"),BrokenDefenseCombo):
  FString::Printf(TEXT("BLOCK  %02d COMBO"),DefenseCombo);
 HUD->DrawText(Label,Color,X,Y,GEngine->GetSmallFont(),1.1f*S);
 const float Fill=FMath::Min(1.f,DefenseCombo/10.f);
 HUD->DrawRect(FLinearColor(.12f,.1f,.16f,.8f),X,Y+18.f*S,150.f*S,5.f*S);
 HUD->DrawRect(Color,X,Y+18.f*S,150.f*S*Fill,5.f*S);
 for(int32 I=1;I<10;++I) HUD->DrawRect(FLinearColor(.12f,.1f,.16f,.8f),X+I*15.f*S,Y+18.f*S,S,5.f*S);
}
