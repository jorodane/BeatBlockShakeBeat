#include "Combat/BBSBCombat.h"
#include "Field/BBSBFieldActions.h"
#include "CanvasItem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "RenderUtils.h"

TArray<float> UBBSBCombatComponent::DefenseCueTimes() const
{
 TArray<float> Times;Times.Init(-1.f,4);
 if(!bShowDefenseDirectionCues || bDead || !Character || !IsStageLocked() || !bCombatStance || BlockWindow<=0
  || (Stage->Phase!=EBBSBStagePhase::Fighting && Stage->Phase!=EBBSBStagePhase::Replacing)) return Times;
 const auto* Actions=Character->FindComponentByClass<UBBSBFieldActionComponent>();
 if(Actions && (!Actions->IsConductorAvailable() || Actions->IsDodgeInvulnerable())) return Times;
 // Contact is judged at the end of a game frame. Leave one frame for input to arrive inside BlockWindow.
 const float Horizon=FMath::Max(0.f,BlockWindow-GetWorld()->GetDeltaSeconds());
 FVector Origin,Normal,Right;GetParryPlane(Origin,Normal,Right);
 for(TActorIterator<ABBSBInkProjectile> It(GetWorld());It;++It)
 {
  FVector Contact;float Seconds;
  if(!It->PredictBodyContact(Character,Horizon,Contact,Seconds)) continue;
  const FVector Local=Contact-Origin;
  const int32 Direction=int32(BBSBCombatRules::DirectionAtPlane(FVector2D(FVector::DotProduct(Local,Right),Local.Z),ParryHalfSize));
  if(Times[Direction]<0 || Seconds<Times[Direction]) Times[Direction]=Seconds;
 }
 return Times;
}

int32 UBBSBCombatComponent::DefenseCueMask() const
{
 const TArray<float> Times=DefenseCueTimes();int32 Mask=0;
 for(int32 Direction=0;Direction<4;++Direction) if(Times[Direction]>=0) Mask|=1<<Direction;
 return Mask;
}

void UBBSBCombatComponent::DrawDefenseDirectionCues(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || !Canvas->Canvas || !Character) return;
 const TArray<float> Times=DefenseCueTimes();
 if(!Times.ContainsByPredicate([](float Time){return Time>=0;})) return;
 APlayerController* PC=HUD->GetOwningPlayerController();if(!PC) return;
 FVector2D Center;
 if(!PC->ProjectWorldLocationToScreen(Character->GetActorLocation(),Center,true)
  || Center.X<0 || Center.X>Canvas->SizeX || Center.Y<0 || Center.Y>Canvas->SizeY) return;
 const float ScreenScale=FMath::Clamp(Canvas->SizeX/1280.f,.7f,1.35f);
 const float Scale=ScreenScale*FMath::Clamp(DefenseCueScale,.5f,2.f);
 const auto* Capsule=Character->GetCapsuleComponent();
 const float Radius=Capsule ? Capsule->GetScaledCapsuleRadius():35.f,Half=Capsule ? Capsule->GetScaledCapsuleHalfHeight():92.f;
 FVector Origin,Normal,Right;GetParryPlane(Origin,Normal,Right);
 FVector2D Across,Above;
 const bool HasAcross=PC->ProjectWorldLocationToScreen(Origin+Right*(Radius+30.f),Across,true);
 const bool HasAbove=PC->ProjectWorldLocationToScreen(Origin+FVector::UpVector*(Half+18.f),Above,true);
 const float Horizontal=HasAcross ? FMath::Clamp(float(FVector2D::Distance(Across,Center))+22.f*Scale,60.f*Scale,125.f*Scale):75.f*Scale;
 const float Vertical=HasAbove ? FMath::Clamp(float(FVector2D::Distance(Above,Center))+18.f*Scale,78.f*Scale,155.f*Scale):100.f*Scale;
 const FVector2D Directions[]={FVector2D(0,-1),FVector2D(1,0),FVector2D(0,1),FVector2D(-1,0)};
 for(int32 Index=0;Index<4;++Index)
 {
  if(Times[Index]<0) continue;
  const float Urgency=1.f-FMath::Clamp(Times[Index]/FMath::Max(BlockWindow,.001f),0.f,1.f);
  const FVector2D D=Directions[Index],Side(-D.Y,D.X);
  FVector2D At=Center+D*((Index==0 || Index==2) ? Vertical:Horizontal);
  const float Margin=25.f*Scale;
  At.X=FMath::Clamp(At.X,Margin,float(Canvas->SizeX)-Margin);
  At.Y=FMath::Clamp(At.Y,Margin,float(Canvas->SizeY)-Margin);
  auto Triangle=[&](float Size,const FLinearColor& Color)
  {
   FCanvasTriangleItem Item(At+D*(16.f*Size),At-D*(10.f*Size)+Side*(12.f*Size),At-D*(10.f*Size)-Side*(12.f*Size),GWhiteTexture);
   Item.SetColor(Color);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
  };
  const float Size=Scale*(1.f+.15f*Urgency);
  Triangle(Size*1.25f,FLinearColor(.08f,.05f,.12f,.95f));
  FLinearColor Color=FMath::Lerp(DefenseCueColor,FLinearColor(1.f,.97f,.78f),.45f*Urgency);Color.A=.88f+.12f*Urgency;
  Triangle(Size,Color);
 }
}
