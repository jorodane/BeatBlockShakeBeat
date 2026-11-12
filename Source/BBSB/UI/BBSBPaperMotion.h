#pragma once
#include "CoreMinimal.h"

/** Real-time UI motion, independent of pause, slow motion and gameplay state. */
struct FBBSBPaperMotion
{
 double StartedAt=0;
 float From=0;
 bool bOpen=false,bInitialized=false;
 static constexpr float OpenSeconds=.28f,CloseSeconds=.18f;
 static float OutBack(float T)
 {
  T=FMath::Clamp(T,0.f,1.f);const float U=T-1.f;
  return 1.f+1.9f*U*U*U+.9f*U*U;
 }
 float Value(double Now) const
 {
  if(!bInitialized)return 0;
  const float T=FMath::Clamp(float((Now-StartedAt)/(bOpen ? OpenSeconds:CloseSeconds)),0.f,1.f);
  return FMath::Lerp(From,bOpen ? 1.f:0.f,bOpen ? OutBack(T):T*T);
 }
 void SetOpen(bool Open,double Now,bool Restart=false)
 {
  if(bInitialized && bOpen==Open && !Restart)return;
  From=bInitialized ? Value(Now):0.f;bOpen=Open;bInitialized=true;StartedAt=Now;
 }
 bool IsComplete(double Now) const { return bInitialized && Now-StartedAt>=(bOpen ? OpenSeconds:CloseSeconds); }
};

/** Replays only when a hidden HUD group becomes visible, never on value changes. */
struct FBBSBHUDReveal
{
 bool bShown=false;
 FBBSBPaperMotion Motion;
 float Sample(bool Shown,double Now,float Delay=0)
 {
  if(Shown!=bShown) { bShown=Shown;Motion=FBBSBPaperMotion();if(Shown)Motion.SetOpen(true,Now+Delay); }
  return Shown ? Motion.Value(Now):0.f;
 }
};
