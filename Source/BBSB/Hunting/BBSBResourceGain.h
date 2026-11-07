#pragma once
#include "CoreMinimal.h"

/** Presentation only: the authoritative balance changes before the pale-blue gain fills in. */
struct FBBSBResourceGain
{
 double From=0,Target=0,At=-100;
 bool Initialized=false;
 static constexpr double Hold=1.1,Follow=.75;
 void Reset(double Value) { From=Target=Value;At=-100;Initialized=true; }
 double Value(double Now) const
 {
  const double T=FMath::Clamp((Now-At-Hold)/Follow,0.,1.);
  return FMath::Lerp(From,Target,T*T*(3.-2.*T));
 }
 void Gain(double Before,double After,double Now)
 {
  if(!Initialized || !FMath::IsNearlyEqual(Target,Before,.0001))Reset(Before);
  if(After<=Before) { Reset(After);return; }
  From=Value(Now);Target=After;At=Now;
 }
};
