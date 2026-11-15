#include "Character/BBSBCameraRotation.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBCameraPoleTest,"BBSB.Camera.Level.SteepFocusRelease",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBCameraPoleTest::RunTest(const FString& Parameters)
{
 const FRotator Local(-22,0,0),View(80,-37,0);
 const FRotator Legacy=(View.Quaternion()*Local.Quaternion().Inverse()).Rotator();
 TestTrue(TEXT("Reported geometry used to produce a half-turn in roll"),FMath::Abs(Legacy.Roll)>179.);
 const FRotator Safe=BBSBCameraRotation::ControlFromView(View,Local);
 const FRotator Displayed=(Safe.Quaternion()*Local.Quaternion()).Rotator();
 TestTrue(TEXT("Free look retains the displayed yaw rather than flipping 180 degrees"),FMath::Abs(FRotator::NormalizeAxis(Displayed.Yaw-View.Yaw))<.001);
 TestTrue(TEXT("The released control and rendered camera remain level"),FMath::Abs(Safe.Roll)<.001 && FMath::Abs(Displayed.Roll)<.001);
 TestTrue(TEXT("A steep focus returns to the attainable pitch without crossing a pole"),Safe.Pitch<=85. && Displayed.Pitch<=85. && Displayed.Pitch>=-85.);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBCameraLevelBlendTest,"BBSB.Camera.Level.WrapAndRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBCameraLevelBlendTest::RunTest(const FString& Parameters)
{
 const FRotator Local(-22,0,0);
 for(double Pitch:{-120.,-89.9,-65.,0.,85.,120.})for(double Roll:{-180.,-89.9,0.,90.,180.})
 {
  const FRotator Safe=BBSBCameraRotation::ConstrainControl(FRotator(Pitch,171,Roll),Local);
  const FRotator View=(Safe.Quaternion()*Local.Quaternion()).Rotator();
  TestTrue(TEXT("Residual roll and out-of-range pitch cannot invert the field view"),FMath::Abs(View.Roll)<.001 && FMath::Abs(Safe.Roll)<.001 && FMath::Abs(View.Pitch)<=85.001);
 }
 for(int32 Frame=0;Frame<=30;++Frame)
 {
  const FRotator View=BBSBCameraRotation::BlendLevel(FRotator(80,170,0),FRotator(-65,-170,0),Frame/30.f);
  TestTrue(TEXT("Crossing the yaw seam with a pitch change never banks the horizon"),FMath::Abs(View.Roll)<.001 && FMath::Abs(FRotator::NormalizeAxis(View.Yaw-170.))<=20.001);
 }
 return true;
}
#endif
