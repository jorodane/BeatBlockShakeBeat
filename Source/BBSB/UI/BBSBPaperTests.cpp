#include "UI/BBSBPaperMotion.h"
#include "UI/BBSBPaperTheme.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBPaperMotionTest,"BBSB.UI.PaperMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBPaperMotionTest::RunTest(const FString&)
{
 FBBSBPaperMotion M;M.SetOpen(true,100);
 TestEqual(TEXT("New panel starts at its anchor"),M.Value(100),0.f);
 TestTrue(TEXT("Entrance overshoots before settling"),M.Value(100.21)>1.f);
 TestEqual(TEXT("Opening ends exactly at rest"),M.Value(100.30),1.f);
 M.SetOpen(false,100.35);const float Before=M.Value(100.43);M.SetOpen(true,100.43);
 TestTrue(TEXT("Reopening reverses the current pose without a jump"),FMath::IsNearlyEqual(Before,M.Value(100.43)));
 TestEqual(TEXT("Reopening settles fully"),M.Value(100.75),1.f);
 M.SetOpen(false,100.8);TestEqual(TEXT("Closing finishes at zero"),M.Value(101),0.f);
 FBBSBHUDReveal HUD;HUD.Sample(true,200);TestEqual(TEXT("HUD settles"),HUD.Sample(true,201),1.f);
 TestEqual(TEXT("A value update does not restart HUD"),HUD.Sample(true,202),1.f);
 HUD.Sample(false,203);TestEqual(TEXT("Returning HUD replays its anchor motion"),HUD.Sample(true,204),0.f);
 TestTrue(TEXT("Paper and ink have strong luminance separation"),BBSBPaper::Paper().GetLuminance()-BBSBPaper::Ink().GetLuminance()>.7f);
 return true;
}
#endif
