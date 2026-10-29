#include "BBSBCombatRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBFieldSphereTest,"BBSB.Combat.Field.ProjectileHurtSphere",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBFieldSphereTest::RunTest(const FString& Parameters)
{
 FVector Hit;float Fraction=1;
 auto Sphere=[&](FVector A,FVector B) { return BBSBCombatRules::SweptCapsuleContact(A,B,FVector::ZeroVector,18.f,18.f,10.f,Hit,Fraction); };
 TestTrue(TEXT("A crossing projectile contacts the central sphere"),Sphere(FVector(-100,0,0),FVector(100,0,0)));
 TestTrue(TEXT("Contact includes the visible pellet radius and finds first entry"),FMath::IsNearlyEqual(Hit.X,-28.f,.01f) && FMath::IsNearlyEqual(Fraction,.36f,.0001f));
 TestTrue(TEXT("A near graze inside combined radii is a hit"),Sphere(FVector(-100,27,0),FVector(100,27,0)));
 TestFalse(TEXT("A lateral graze outside the sphere does not hit"),Sphere(FVector(-100,29,0),FVector(100,29,0)));
 TestFalse(TEXT("Hair and feet outside the central sphere do not count"),Sphere(FVector(-100,0,70),FVector(100,0,70)));
 TestTrue(TEXT("The old tall capsule still contacts this same high shot for rhythm defense"),BBSBCombatRules::SweptCapsuleContact(FVector(-100,0,70),FVector(100,0,70),FVector::ZeroVector,35.f,92.f,10.f,Hit,Fraction));
 TestTrue(TEXT("A fast pellet crossing in one frame cannot tunnel through the sphere"),Sphere(FVector(-10000,0,0),FVector(10000,0,0)));
 TestTrue(TEXT("Starting inside gives immediate contact"),Sphere(FVector::ZeroVector,FVector(50,0,0)) && Fraction==0.f);
 TestFalse(TEXT("Stationary pellets outside do not create phantom contacts"),Sphere(FVector(100,100,0),FVector(100,100,0)));
 return true;
}
#endif
