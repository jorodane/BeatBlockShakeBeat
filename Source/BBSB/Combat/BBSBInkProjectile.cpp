#include "BBSBCombat.h"
#include "BBSBCombatVisuals.h"
#include "Field/BBSBFieldActions.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

using namespace BBSBCombatVisuals;

namespace
{
 void PlayerHurtShape(const ABBSBInkProjectile* Shot,const APawn* Pawn,FVector& Center,float& Radius,float& HalfHeight)
 {
  Center=Pawn ? Pawn->GetActorLocation():FVector::ZeroVector;
  if(Shot->bFieldProjectile && Pawn)
  {
   if(const auto* Combat=Pawn->FindComponentByClass<UBBSBCombatComponent>())
   {
    Center=Pawn->GetActorTransform().TransformPosition(Combat->FieldProjectileHurtOffset);
    Radius=FMath::Clamp(Combat->FieldProjectileHurtRadius,1.f,80.f);HalfHeight=Radius;return;
   }
  }
  const auto* Character=Cast<ACharacter>(Pawn);const auto* Capsule=Character ? Character->GetCapsuleComponent():nullptr;
  Radius=Capsule ? Capsule->GetScaledCapsuleRadius():35.f;HalfHeight=Capsule ? Capsule->GetScaledCapsuleHalfHeight():92.f;
 }

 FCollisionQueryParams InkFlightQuery(const ABBSBInkProjectile* Shot,const APawn* Pawn)
 {
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBInkFlight),false,Shot);
  if(Pawn) Query.AddIgnoredActor(Pawn);
  for(TActorIterator<ABBSBInkEnemy> It(Shot->GetWorld());It;++It) Query.AddIgnoredActor(*It);
  return Query;
 }
}

ABBSBInkProjectile::ABBSBInkProjectile()
{
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostPhysics;
	Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ink")); SetRootComponent(Body);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Body->SetStaticMesh(Sphere.Object); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.22f)); Body->SetCastShadow(false);
 Body->SetCollisionResponseToAllChannels(ECR_Ignore);Body->SetGenerateOverlapEvents(false);Body->SetCanEverAffectNavigation(false);
 Tags.Add(TEXT("BBSBIgnoreAim"));
}
void ABBSBInkProjectile::Initialize(ABBSBCombatStage* InStage,ABBSBInkEnemy* InSource,FVector Target,EBBSBInkPattern InPattern,float FlightSeconds,int32 InHoldGroup)
{
	OwningStage=InStage; Source=InSource; bFieldProjectile=InSource && InSource->bFieldBulletHell; Pattern=InPattern; FrozenTarget=Target; HoldGroup=InHoldGroup;
	LaunchOrigin=GetActorLocation(); PreviousPosition=LaunchOrigin; TravelSeconds=FMath::Max(FlightSeconds,0.2f);
	Damage=InSource ? InSource->ShotDamage : 8.f; FlightVelocity=(Target-LaunchOrigin)/TravelSeconds;
	Side=FVector::CrossProduct(FVector::UpVector,FlightVelocity).GetSafeNormal();
	Tint(Body,Pattern==EBBSBInkPattern::Hold ? FLinearColor(0.08f,0.045f,0.1f) : FLinearColor(0.025f,0.018f,0.035f));
 if(bFieldProjectile) Tint(Body,FLinearColor(.9f,.31f,.51f));
	if (Pattern==EBBSBInkPattern::Bounce)
	{
		FVector Floor;
		if (Ground(GetWorld(),(Target+LaunchOrigin)*0.5f,Floor))
		{
			FVector Mirrored=Target; Mirrored.Z=2*(Floor.Z+11.f)-Target.Z;
			FlightVelocity=(Mirrored-LaunchOrigin)/TravelSeconds;
		}
	}
	if (Pattern==EBBSBInkPattern::Area)
	{
		SetActorLocation(Target); PreviousPosition=Target;
		Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Body->SetWorldScale3D(FVector(0.1f,0.1f,0.025f));
		Tint(Body,FLinearColor(0.28f,0.18f,0.25f));
	}
	SetLifeSpan(TravelSeconds+6.f);
}
void ABBSBInkProjectile::ResolveAndFade()
{
	if (bResolved) return; bResolved=true;
	Body->SetVisibility(false); SetLifeSpan(0.06f);
}
FVector ABBSBInkProjectile::FlightPosition(float AtAge) const
{
 FVector Position=LaunchOrigin+FlightVelocity*AtAge;
 const float Progress=FMath::Clamp(AtAge/TravelSeconds,0.f,1.f);
 // Settle before body contact: decorative motion must not change the authored defense direction.
 const float ShapeWeight=1.f-FMath::SmoothStep(.65f,.85f,Progress);
 if(Pattern==EBBSBInkPattern::Curve)
 {
  const FVector CurveSide=FVector::DotProduct(Side,ApproachOffset)<0 ? -Side:Side;
  Position+=CurveSide*(140.f*FMath::Sin(PI*AtAge/TravelSeconds)*ShapeWeight);
 }
 if(Pattern==EBBSBInkPattern::Zigzag) Position+=Side*(55.f*FMath::Sin(4*PI*AtAge/TravelSeconds)*ShapeWeight);
 return Position+ApproachOffset*(FMath::Sin(PI*Progress)*ShapeWeight);
}
bool ABBSBInkProjectile::PredictBodyContact(ACharacter* Target,float MaxSeconds,FVector& Contact,float& Seconds) const
{
 Contact=FVector::ZeroVector;Seconds=-1.f;
 if(EvadedContactPawn.Get()==Target) return false;
 if(bResolved || Pattern==EBBSBInkPattern::Area || !IsValid(Target) || !GetWorld() || !FMath::IsFinite(MaxSeconds) || MaxSeconds<=0) return false;
 FVector HurtCenter;float Radius,Half;PlayerHurtShape(this,Target,HurtCenter,Radius,Half);
 const float Horizon=GetLifeSpan()>0 ? FMath::Min(MaxSeconds,GetLifeSpan()):MaxSeconds;
 const float Reach=FlightVelocity.Size()*Horizon+2.f*ApproachOffset.Size()+280.f+Half+Radius;
 if(FVector::DistSquared(GetActorLocation(),Target->GetActorLocation())>FMath::Square(Reach)) return false;
 const auto Query=InkFlightQuery(this,Target);
 const int32 Steps=FMath::Clamp(FMath::CeilToInt(Horizon/.0125f),1,128);
 const float Step=Horizon/Steps;
 FVector Previous=GetActorLocation(),Velocity=FlightVelocity;int32 ForecastBounces=Bounces;
 for(int32 Index=0;Index<Steps;++Index)
 {
  const float Elapsed=(Index+1)*Step;
  FVector Next=Pattern==EBBSBInkPattern::Bounce ? Previous+Velocity*Step:FlightPosition(Age+Elapsed);
  FHitResult WorldHit;
  const bool WorldBlocked=GetWorld()->SweepSingleByChannel(WorldHit,Previous,Next,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(10.f),Query);
  FVector BodyContact;float Fraction=1.f;
  if(BBSBCombatRules::SweptCapsuleContact(Previous,Next,HurtCenter,Radius,Half,10.f,BodyContact,Fraction)
   && (!WorldBlocked || FVector::Dist(Previous,BodyContact)<=WorldHit.Distance))
  {
   Contact=BodyContact;Seconds=Index*Step+Fraction*Step;return true;
  }
  if(WorldBlocked)
  {
   if(Pattern!=EBBSBInkPattern::Bounce || ForecastBounces>=2) return false;
   Velocity=FMath::GetReflectionVector(Velocity,WorldHit.ImpactNormal);++ForecastBounces;
   Next=WorldHit.Location+WorldHit.ImpactNormal*.5f;
  }
  Previous=Next;
 }
 return false;
}
void ABBSBInkProjectile::Tick(float DT)
{
	Super::Tick(DT);
	if (bResolved) return;
	APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
	UBBSBCombatComponent* Combat=Pawn ? Pawn->FindComponentByClass<UBBSBCombatComponent>() : nullptr;
	const auto Query=InkFlightQuery(this,Pawn);
	if (Pattern==EBBSBInkPattern::Area)
	{
		Age+=DT;
		const float T=FMath::Clamp(Age/TravelSeconds,0.f,1.f);
		Body->SetWorldScale3D(FVector(2.9f*T,2.9f*T,0.02f));
		if (Age>=TravelSeconds)
		{
			if (Combat && !Combat->bDead && FVector::Dist2D(Pawn->GetActorLocation(),FrozenTarget)<145 && FMath::Abs(Pawn->GetActorLocation().Z-FrozenTarget.Z)<180)
			{
				FHitResult Cover;
				if (!GetWorld()->LineTraceSingleByChannel(Cover,FrozenTarget+FVector(0,0,60),Pawn->GetActorLocation(),ECC_Visibility,Query))
				{
					Combat->TakeCombatDamage(Damage*1.25f,this);
					if (!Combat->bDead) Combat->SetFeedback(TEXT("AREA  |  dodge or leave the marked ground"));
				}
			}
			ResolveAndFade();
		}
		return;
	}
	const int32 Steps=FMath::Clamp(FMath::CeilToInt(DT/0.0125f),1,20);
	const float Step=DT/Steps;
	for (int32 Index=0; Index<Steps && !bResolved; ++Index)
	{
		Age+=Step;
		FVector Next=Pattern==EBBSBInkPattern::Bounce ? PreviousPosition+FlightVelocity*Step:FlightPosition(Age);
		FHitResult WorldHit;
		const bool bWorldHit=GetWorld()->SweepSingleByChannel(WorldHit,PreviousPosition,Next,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(10.f),Query);
  FVector Contact;float BodyFraction=1.f;bool bBodyHit=false;
  if(Combat && !Combat->bDead)
  {
   FVector HurtCenter;float HurtRadius,HurtHalf;PlayerHurtShape(this,Pawn,HurtCenter,HurtRadius,HurtHalf);
   bBodyHit=BBSBCombatRules::SweptCapsuleContact(PreviousPosition,Next,HurtCenter,HurtRadius,HurtHalf,10.f,Contact,BodyFraction);
  }
  if(!bBodyHit) EvadedContactPawn.Reset();
  // A real body contact is the only parry/hit event. A nearer wall still intercepts first.
  if(bBodyHit && (!bWorldHit || FVector::Dist(PreviousPosition,Contact)<=WorldHit.Distance))
  {
   SetActorLocation(Contact);
   if(EvadedContactPawn.Get()!=Pawn)
   {
    if(Combat->ResolveContact(Contact,HoldGroup,Damage,this)!=EBBSBParryResult::Evade) { ResolveAndFade();return; }
    // Keep flying unchanged; ending a dash cannot hit again inside this same overlap.
    EvadedContactPawn=Pawn;
   }
  }
		if (bWorldHit)
		{
			if (Pattern==EBBSBInkPattern::Bounce && Bounces<2)
			{
				FlightVelocity=FMath::GetReflectionVector(FlightVelocity,WorldHit.ImpactNormal); ++Bounces;
				Next=WorldHit.Location+WorldHit.ImpactNormal*0.5f;
			}
			else { SetActorLocation(WorldHit.Location); ResolveAndFade(); return; }
		}
		SetActorLocation(Next); PreviousPosition=Next;
	}
}
