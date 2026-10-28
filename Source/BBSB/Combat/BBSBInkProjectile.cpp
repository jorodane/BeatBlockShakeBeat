#include "BBSBCombat.h"
#include "BBSBCombatVisuals.h"
#include "Field/BBSBFieldActions.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

using namespace BBSBCombatVisuals;

ABBSBInkProjectile::ABBSBInkProjectile()
{
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostPhysics;
	Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ink")); SetRootComponent(Body);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Body->SetStaticMesh(Sphere.Object); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.22f)); Body->SetCastShadow(false);
}
void ABBSBInkProjectile::Initialize(ABBSBCombatStage* InStage,ABBSBInkEnemy* InSource,FVector Target,EBBSBInkPattern InPattern,float FlightSeconds,int32 InHoldGroup)
{
	OwningStage=InStage; Source=InSource; Pattern=InPattern; FrozenTarget=Target; HoldGroup=InHoldGroup;
	LaunchOrigin=GetActorLocation(); PreviousPosition=LaunchOrigin; TravelSeconds=FMath::Max(FlightSeconds,0.2f);
	Damage=InSource ? InSource->ShotDamage : 8.f; FlightVelocity=(Target-LaunchOrigin)/TravelSeconds;
	Side=FVector::CrossProduct(FVector::UpVector,FlightVelocity).GetSafeNormal();
	Tint(Body,Pattern==EBBSBInkPattern::Hold ? FLinearColor(0.08f,0.045f,0.1f) : FLinearColor(0.025f,0.018f,0.035f));
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
void ABBSBInkProjectile::Tick(float DT)
{
	Super::Tick(DT);
	if (bResolved) return;
	APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
	UBBSBCombatComponent* Combat=Pawn ? Pawn->FindComponentByClass<UBBSBCombatComponent>() : nullptr;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBInkFlight),false,this);
	if (Pawn) Query.AddIgnoredActor(Pawn);
	for (TActorIterator<ABBSBInkEnemy> It(GetWorld()); It; ++It) Query.AddIgnoredActor(*It);
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
		FVector Next=LaunchOrigin+FlightVelocity*Age;
		if (Pattern==EBBSBInkPattern::Bounce) Next=PreviousPosition+FlightVelocity*Step;
		if (Pattern==EBBSBInkPattern::Curve) Next+=Side*(140.f*FMath::Sin(PI*Age/TravelSeconds));
		if (Pattern==EBBSBInkPattern::Zigzag) Next+=Side*(55.f*FMath::Sin(4*PI*Age/TravelSeconds));
		FHitResult WorldHit;
		const bool bWorldHit=GetWorld()->SweepSingleByChannel(WorldHit,PreviousPosition,Next,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(10.f),Query);
  FVector Contact;float BodyFraction=1.f;bool bBodyHit=false;
  if(Combat && !Combat->bDead)
  {
   const auto* Character=Cast<ACharacter>(Pawn);const auto* Capsule=Character ? Character->GetCapsuleComponent():nullptr;
   bBodyHit=BBSBCombatRules::SweptCapsuleContact(PreviousPosition,Next,Pawn->GetActorLocation(),Capsule ? Capsule->GetScaledCapsuleRadius():35.f,
    Capsule ? Capsule->GetScaledCapsuleHalfHeight():92.f,10.f,Contact,BodyFraction);
  }
  // A real body contact is the only parry/hit event. A nearer wall still intercepts first.
  if(bBodyHit && (!bWorldHit || FVector::Dist(PreviousPosition,Contact)<=WorldHit.Distance))
  {
   SetActorLocation(Contact);
   if(Combat->ResolveContact(Contact,HoldGroup,Damage,this)!=EBBSBParryResult::Evade) { ResolveAndFade();return; }
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
