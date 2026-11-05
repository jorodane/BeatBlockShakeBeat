#include "Combat/BBSBCombat.h"
#include "Combat/BBSBDamageCameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Caravan/BBSBCaravan.h"
#include "Field/BBSBFieldActions.h"
#include "Camera/CameraTypes.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "RenderUtils.h"

namespace
{
 float HitPulse(float Age,float Duration)
 {
  if(Age<0.f || Duration<=0.f || Age>=Duration) return 0.f;
  const float Attack=FMath::Clamp(Age/.018f,0.f,1.f);
  return Attack*FMath::Pow(1.f-Age/Duration,1.35f);
 }
}

void UBBSBCombatComponent::ShowEnemyDamage(float Damage,const FVector& Point,bool bCritical)
{
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 if(Damage<=0.f || !PC || !PC->IsLocalController() || DamageNumberLifetime<=0.f) return;
 if(DamagePopups.Num()>=24) DamagePopups.RemoveAt(0);
 auto& Popup=DamagePopups.AddDefaulted_GetRef();
 Popup.bCritical=bCritical;Popup.Amount=Damage;Popup.WorldPoint=Point+FVector(0,0,18);Popup.StartedAt=GetWorld()->GetTimeSeconds();
 const int32 Serial=DamagePopupSerial++;
 Popup.Drift=(Serial%2==0 ? 1.f:-1.f)*(32.f+float(Serial%3)*9.f);
 Popup.ScreenOffset=FVector2D::ZeroVector;Popup.Opacity=1.f;Popup.SizeScale=1.f;
}

void UBBSBCombatComponent::StartPlayerDamageFeedback(float Damage)
{
 LastPlayerDamageAt=GetWorld()->GetTimeSeconds();
 PlayerDamageFeedbackScale=FMath::Clamp(Damage/12.f,.65f,1.f);
 ++PlayerDamageFeedbackSerial;
 auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 if(!PC || !PC->IsLocalController() || !PC->PlayerCameraManager) return;
 if(DamageShakeDuration<=0.f || DamageShakeStrength<=0.f || PC->GetViewTarget()!=Character)
 {
  if(DamageCameraShake) PC->PlayerCameraManager->StopCameraShake(DamageCameraShake,true);
  DamageCameraShake=nullptr;return;
 }
 FAddCameraShakeParams Params(FMath::Clamp(DamageShakeStrength,0.f,2.f)*PlayerDamageFeedbackScale);
 Params.DurationOverride=FMath::Clamp(DamageShakeDuration,.01f,2.f);
 DamageCameraShake=PC->PlayerCameraManager->StartCameraShake(UBBSBDamageCameraShake::StaticClass(),Params);
}

void UBBSBCombatComponent::ResetDamageFeedback()
{
 if(auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr)
  if(PC->PlayerCameraManager && DamageCameraShake) PC->PlayerCameraManager->StopCameraShake(DamageCameraShake,true);
 DamageCameraShake=nullptr;
 DamagePopups.Reset();LastPlayerDamageAt=-100;DamageVignetteAlpha=DamageShakeAlpha=0.f;
}

void UBBSBCombatComponent::TickDamageFeedback()
{
 const double Now=GetWorld()->GetTimeSeconds();const float Age=Now-LastPlayerDamageAt;
 DamageVignetteAlpha=FMath::Clamp(DamageVignetteOpacity,0.f,.6f)*PlayerDamageFeedbackScale*HitPulse(Age,DamageVignetteDuration);
 DamageShakeAlpha=PlayerDamageFeedbackScale*HitPulse(Age,DamageShakeDuration);
 const float Lifetime=FMath::Max(.1f,DamageNumberLifetime);
 DamagePopups.RemoveAll([&](const FBBSBDamagePopup& Popup){return Now-Popup.StartedAt>=Lifetime;});
 for(auto& Popup:DamagePopups)
 {
  const float T=FMath::Max(0.f,float(Now-Popup.StartedAt));
  Popup.ScreenOffset=FVector2D(Popup.Drift*T,-DamageNumberRiseSpeed*T+.5f*DamageNumberGravity*T*T);
  Popup.Opacity=1.f-FMath::SmoothStep(Lifetime*.55f,Lifetime,T);
  // Pop quickly on impact, then continuously shrink throughout the arc and fade.
  const float PopSeconds=FMath::Min(.06f,Lifetime*.2f);
  Popup.SizeScale=T<PopSeconds ? FMath::InterpEaseOut(1.f,1.6f,T/PopSeconds,3.f)
   : FMath::Lerp(1.6f,.55f,FMath::SmoothStep(PopSeconds,Lifetime,T));
  if(Popup.bCritical)Popup.SizeScale*=1.55f;
 }
}

void UBBSBCombatComponent::DrawDamageFeedback(AHUD* HUD,UCanvas* Canvas) const
{ DrawDamageVignette(HUD,Canvas);DrawDamageNumbers(HUD,Canvas); }
void UBBSBCombatComponent::DrawDamageVignette(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || !Canvas->Canvas) return;
 const auto* PC=HUD->GetOwningPlayerController();if(!PC) return;
 const float W=Canvas->SizeX,H=Canvas->SizeY;
 if(DamageVignetteAlpha>.001f)
 {
  // A soft elliptical screen-edge tint, drawn beneath the ordinary HUD.
  TArray<FCanvasUVTri> Tris;constexpr int32 Columns=24,Rows=16;Tris.Reserve(Columns*Rows*2);
  auto Color=[&](const FVector2D& Point)
  {
   const FVector2D N(Point.X/W*2.f-1.f,Point.Y/H*2.f-1.f);
   return FLinearColor(.67f,.025f,.065f,DamageVignetteAlpha*FMath::SmoothStep(.52f,1.28f,float(N.Size())));
  };
  auto Triangle=[&](const FVector2D& A,const FVector2D& B,const FVector2D& C)
  {
   FCanvasUVTri Tri;Tri.V0_Pos=A;Tri.V1_Pos=B;Tri.V2_Pos=C;
   Tri.V0_UV=Tri.V1_UV=Tri.V2_UV=FVector2D::ZeroVector;
   Tri.V0_Color=Color(A);Tri.V1_Color=Color(B);Tri.V2_Color=Color(C);Tris.Add(Tri);
  };
  for(int32 Y=0;Y<Rows;++Y) for(int32 X=0;X<Columns;++X)
  {
   const FVector2D A(W*X/Columns,H*Y/Rows),B(W*(X+1)/Columns,A.Y),C(B.X,H*(Y+1)/Rows),D(A.X,C.Y);
   Triangle(A,B,C);Triangle(A,C,D);
  }
  FCanvasTriangleItem Item(Tris,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
 }
}
void UBBSBCombatComponent::DrawDamageNumbers(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || !Canvas->Canvas)return;
 const auto* PC=HUD->GetOwningPlayerController();if(!PC)return;
 const float W=Canvas->SizeX,H=Canvas->SizeY;
 const float ScreenScale=FMath::Clamp(W/1280.f,.7f,1.4f);
 FNumberFormattingOptions Format;Format.SetMaximumFractionalDigits(1);Format.SetMinimumFractionalDigits(0);
 for(const auto& Popup:DamagePopups)
 {
  FVector2D At;if(Popup.Opacity<=0.f || !PC->ProjectWorldLocationToScreen(Popup.WorldPoint,At,true)) continue;
  At+=Popup.ScreenOffset*ScreenScale;
  if(At.X<-80 || At.X>W+80 || At.Y<-80 || At.Y>H+80) continue;
  FLinearColor Tint=Popup.bCritical ? FLinearColor(1.f,.75f,.22f):DamageNumberColor;Tint.A*=Popup.Opacity;
  FCanvasTextItem Text(At,Popup.bCritical ? FText::FromString(FText::AsNumber(Popup.Amount,&Format).ToString()+TEXT(" !")):FText::AsNumber(Popup.Amount,&Format),GEngine->GetMediumFont(),Tint);
  Text.Scale=FVector2D(ScreenScale*DamageNumberScale*Popup.SizeScale);
  Text.bCentreX=Text.bCentreY=true;Text.bOutlined=true;Text.OutlineColor=FLinearColor(.09f,.035f,.065f,Popup.Opacity);
  Text.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Text);
 }
}
