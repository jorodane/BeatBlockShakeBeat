#pragma once
#include "CoreMinimal.h"

/** Horizon-level rotation for the field rig, whose camera has a local pitch offset. */
namespace BBSBCameraRotation
{
 inline FRotator ConstrainControl(FRotator Control,const FRotator& CameraLocal)
 {
  Control.Normalize();
  // Keep both the boom and the pitched camera away from the Euler poles.
  const double LocalPitch=FRotator::NormalizeAxis(CameraLocal.Pitch);
  const double Minimum=FMath::Max(-85.,-85.-LocalPitch);
  const double Maximum=FMath::Min(85.,85.-LocalPitch);
  Control.Pitch=FMath::Clamp(Control.Pitch,Minimum,Maximum);Control.Roll=0;
  return Control;
 }
 inline FRotator ControlFromView(FRotator View,const FRotator& CameraLocal)
 {
  View.Normalize();
  // Inverting quaternions then converting to Euler flips yaw/roll by 180 degrees
  // when View.Pitch - CameraLocal.Pitch crosses a pole. Retain the displayed yaw.
  return ConstrainControl(FRotator(View.Pitch-CameraLocal.Pitch,View.Yaw-CameraLocal.Yaw,0),CameraLocal);
 }
 inline FRotator BlendLevel(FRotator From,FRotator To,float Alpha)
 {
  From.Normalize();To.Normalize();
  const FRotator Delta=(To-From).GetNormalized();
  FRotator Result=From+Delta*FMath::Clamp(Alpha,0.f,1.f);Result.Roll=0;
  return Result.GetNormalized();
 }
}
