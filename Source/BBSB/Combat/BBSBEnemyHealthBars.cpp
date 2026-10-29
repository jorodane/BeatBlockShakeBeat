#include "Combat/BBSBCombat.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

void ABBSBInkEnemy::ResetHealthFeedback()
{
 DamageTrailHealth=DamageTrailStartHealth=Health;RecentDamageTotal=0;LastHealthDamageAt=-100;
}

void ABBSBInkEnemy::RecordHealthDamage(float Before)
{
 const float Actual=FMath::Max(0.f,Before-Health);if(Actual<=0.f || !GetWorld()) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(Now-LastHealthDamageAt>=FMath::Max(0.f,HealthTrailHoldSeconds))
 { DamageTrailStartHealth=Before;RecentDamageTotal=0; }
 RecentDamageTotal+=Actual;LastHealthDamageAt=Now;DamageTrailHealth=DamageTrailStartHealth;
}

float ABBSBInkEnemy::GetDamageTrailHealth() const
{
 if(!GetWorld() || RecentDamageTotal<=0.f) return Health;
 const float Age=FMath::Max(0.f,float(GetWorld()->GetTimeSeconds()-LastHealthDamageAt));
 const float Hold=FMath::Max(0.f,HealthTrailHoldSeconds),Fade=FMath::Max(.05f,HealthTrailFadeSeconds);
 return FMath::Lerp(FMath::Clamp(DamageTrailStartHealth,Health,FMath::Max(Health,MaximumHealth)),Health,FMath::SmoothStep(Hold,Hold+Fade,Age));
}

void ABBSBInkEnemy::TickHealthFeedback()
{
 DamageTrailHealth=GetDamageTrailHealth();
 if(GetWorld()->GetTimeSeconds()-LastHealthDamageAt>=FMath::Max(0.f,HealthTrailHoldSeconds)+FMath::Max(.05f,HealthTrailFadeSeconds))
 { RecentDamageTotal=0;DamageTrailHealth=DamageTrailStartHealth=Health; }
}

void UBBSBCombatComponent::DrawEnemyHealthBars(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || !Character || !GEngine) return;
 auto* PC=HUD->GetOwningPlayerController();if(!PC) return;
 const float Scale=FMath::Clamp(Canvas->SizeX/1280.f,.7f,1.35f);
 const float Width=124.f*Scale,Height=7.f*Scale,Border=1.5f*Scale;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
 for(TActorIterator<ABBSBInkEnemy> It(GetWorld());It;++It)
 {
  const auto* Enemy=*It;
  if(!Enemy->bShowHealthBar || Enemy->MaximumHealth<=0.f || (Enemy->bDefeated && Enemy->RecentDamageTotal<=0.f) ||
   FVector::DistSquared(Character->GetActorLocation(),Enemy->GetActorLocation())>FMath::Square(3000.f)) continue;
  const FVector Anchor=Enemy->GetActorLocation()+FVector(0,0,Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+18.f);
  if(FVector::DotProduct(Anchor-Eye,View.Vector())<=0.f) continue;
  FVector2D Screen;if(!PC->ProjectWorldLocationToScreen(Anchor,Screen,true) || Screen.X<0 || Screen.X>Canvas->SizeX || Screen.Y<20*Scale || Screen.Y>Canvas->SizeY) continue;
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBEnemyHealthVisibility),false,Character);Query.AddIgnoredActor(Enemy);FHitResult Hit;
  if(GetWorld()->LineTraceSingleByObjectType(Hit,Eye,Enemy->GetAimPoint(),BBSBCombatVisuals::GeometryTypes(),Query)) continue;
  const float Fraction=FMath::Clamp(Enemy->Health/Enemy->MaximumHealth,0.f,1.f);
  const float Trail=FMath::Clamp(Enemy->GetDamageTrailHealth()/Enemy->MaximumHealth,Fraction,1.f);
  const float X=Screen.X-Width*.5f,Y=Screen.Y;
  HUD->DrawRect(FLinearColor(.055f,.035f,.07f,.95f),X-Border,Y-Border,Width+2*Border,Height+2*Border);
  HUD->DrawRect(FLinearColor(.20f,.14f,.21f,.95f),X,Y,Width,Height);
  if(Trail>Fraction) HUD->DrawRect(FLinearColor(1.f,.77f,.27f),X+Width*Fraction,Y,Width*(Trail-Fraction),Height);
  HUD->DrawRect(FLinearColor(.82f,.38f,.50f),X,Y,Width*Fraction,Height);
  const FString Name=Enemy->EnemyName.ToString();float TW=0,TH=0;HUD->GetTextSize(Name,TW,TH,GEngine->GetSmallFont(),Scale);
  HUD->DrawText(Name,FLinearColor(.95f,.89f,.93f),Screen.X-TW*.5f,Y-TH-4*Scale,GEngine->GetSmallFont(),Scale);
  if(Enemy->RecentDamageTotal>0.f && Trail>Fraction)
   HUD->DrawText(FString::Printf(TEXT("%.0f"),Enemy->RecentDamageTotal),FLinearColor(1.f,.82f,.38f),X+Width+6*Scale,Y-5*Scale,GEngine->GetSmallFont(),Scale);
 }
}
