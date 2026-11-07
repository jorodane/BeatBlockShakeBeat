#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Hunting/BBSBResourceGain.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBResourceGainTest,"BBSB.Rewards.ResourceGainPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBResourceGainTest::RunTest(const FString&)
{
 FBBSBResourceGain G;G.Gain(100,140,10);
 TestEqual(TEXT("Immediate balance gain leaves the old fill at 100"),G.Value(10),100.);
 TestEqual(TEXT("Gain stays highlighted during the hold"),G.Value(11),100.);
 TestTrue(TEXT("Original fill follows after the hold"),G.Value(11.5)>100 && G.Value(11.5)<140);
 const double Mid=G.Value(11.5);G.Gain(140,150,11.5);
 TestEqual(TEXT("A further pickup retains the current visible fill"),G.Value(11.5),Mid);
 TestEqual(TEXT("Consecutive gains renew the readable hold"),G.Value(12.5),Mid);
 TestEqual(TEXT("All accumulated gain eventually settles"),G.Value(14),150.);
 G.Gain(150,0,15);TestEqual(TEXT("Spending does not leave a false blue gain"),G.Value(15),0.);
 G.Reset(80);TestEqual(TEXT("Loading an existing balance has no pickup animation"),G.Value(16),80.);
 G.Gain(5,10,17);TestEqual(TEXT("A replaced source starts from its own old balance"),G.Value(17),5.);
 return true;
}
#endif
