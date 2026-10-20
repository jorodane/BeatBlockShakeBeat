#include "BBSBFieldGesture.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBFieldReleaseTest,"BBSB.Field.Gestures.ReleaseClassification",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBFieldReleaseTest::RunTest(const FString& Parameters)
{
	const FBBSBFieldGestureSettings Settings;
	FBBSBFieldGestureRecognizer Input;
	Input.Begin(0);
	Input.Update(0.02,FVector2D::ZeroVector,Settings);
	TestTrue(TEXT("Plain mouse-up is Dive"),Input.End(0.03,Settings)==EBBSBFieldGesture::Dive);
	for (int32 FPS : {30,60,120})
	{
		Input.Begin(0);
		for (int32 Frame=1; Frame<=FPS/5; ++Frame) Input.Update(double(Frame)/FPS,FVector2D(1800.0/FPS,0),Settings);
		TestTrue(*FString::Printf(TEXT("Fast release is Flick at %d FPS"),FPS),Input.End(0.2,Settings)==EBBSBFieldGesture::Flick);
		TestFalse(TEXT("Release clears held state"),Input.bHeld);
	}
	Input.Begin(0);
	Input.Update(0.05,FVector2D(200,0),Settings);
	Input.Update(0.3,FVector2D::ZeroVector,Settings);
	TestTrue(TEXT("Old fast motion is not a late Flick"),Input.End(0.31,Settings)==EBBSBFieldGesture::Dive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBFieldHoldTest,"BBSB.Field.Gestures.HoldAndCancel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBFieldHoldTest::RunTest(const FString& Parameters)
{
	FBBSBFieldGestureSettings Settings;
	FBBSBFieldGestureRecognizer Input;
	Input.Begin(1);
	TestEqual(TEXT("No hold before threshold"),Input.Update(1.3,FVector2D::ZeroVector,Settings).Num(),0);
	auto Events=Input.Update(1.36,FVector2D::ZeroVector,Settings);
	TestTrue(TEXT("One Hold at threshold"),Events.Num()==1 && Events[0]==EBBSBFieldGesture::Hold);
	TestEqual(TEXT("Hold is not emitted every frame"),Input.Update(2,FVector2D::ZeroVector,Settings).Num(),0);
	Input.Cancel();
	TestFalse(TEXT("Cancel releases capture"),Input.bHeld);
	TestEqual(TEXT("Canceled motion has no actions"),Input.Update(3,FVector2D(200,0),Settings).Num(),0);
	Input.Begin(4);
	TestFalse(TEXT("New press resets hold"),Input.bHoldEmitted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBFieldShakeTest,"BBSB.Field.Gestures.ShakeAndIndependentChannels",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBFieldShakeTest::RunTest(const FString& Parameters)
{
	FBBSBFieldGestureSettings Settings;
	FBBSBFieldGestureRecognizer Left,Right;
	Left.Begin(0); Right.Begin(0);
	int32 Shakes=0;
	for (int32 Stroke=0; Stroke<5; ++Stroke)
		for (auto Event : Left.Update(0.05*(Stroke+1),FVector2D(Stroke%2==0 ? 40 : -40,0),Settings))
			if (Event==EBBSBFieldGesture::Shake) ++Shakes;
	TestEqual(TEXT("Three deliberate reversals emit one Shake"),Shakes,1);
	Left.End(0.3,Settings);
	TestTrue(TEXT("Releasing left preserves right capture"),Right.bHeld);
	TestFalse(TEXT("Left capture released"),Left.bHeld);
	Right.Cancel();
	Left.Begin(1);
	Shakes=0;
	for (int32 Frame=0; Frame<30; ++Frame)
		for (auto Event : Left.Update(1+(Frame+1)*0.01,FVector2D(Frame%2 ? 1 : -1,0),Settings))
			if (Event==EBBSBFieldGesture::Shake) ++Shakes;
	TestEqual(TEXT("Small hand jitter does not count as Shake"),Shakes,0);
	return true;
}
#endif
