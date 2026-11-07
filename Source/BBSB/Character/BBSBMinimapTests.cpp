#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Character/BBSBMinimapMath.h"
#include "Character/BBSBWorldMap.h"
#include "Character/BBSBTerrainMapData.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBCompassProjectionTest,"BBSB.Map.CameraCompassProjection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBCompassProjectionTest::RunTest(const FString&)
{
 TestTrue(TEXT("North is up while looking north"),BBSBMinimap::Project(FVector2D(100,0),0,1).Equals(FVector2D(0,-100),.001));
 TestTrue(TEXT("East is up while looking east"),BBSBMinimap::Project(FVector2D(0,100),90,1).Equals(FVector2D(0,-100),.001));
 TestTrue(TEXT("North is left while looking east"),BBSBMinimap::Project(FVector2D(100,0),90,1).Equals(FVector2D(-100,0),.001));
 TestTrue(TEXT("South is up while looking south"),BBSBMinimap::Project(FVector2D(-100,0),180,1).Equals(FVector2D(0,-100),.001));
 TestTrue(TEXT("West is up while looking west"),BBSBMinimap::Project(FVector2D(0,-100),270,1).Equals(FVector2D(0,-100),.001));
 const FVector2D Delta(325,-781);
 for(float Yaw:{-179.f,-45.f,0.f,38.f,91.f,181.f,359.f})TestTrue(TEXT("Inverse texture sampling and marker projection agree"),BBSBMinimap::Unproject(BBSBMinimap::Project(Delta,Yaw,.03f),Yaw,.03f).Equals(Delta,.005));
 TestTrue(TEXT("Yaw wrapping does not jump the map"),BBSBMinimap::Project(Delta,-1,1).Equals(BBSBMinimap::Project(Delta,359,1),.001));
 auto* Map=NewObject<UBBSBWorldMapComponent>();Map->MapHalfExtent=50000;TestEqual(TEXT("A kilometre-wide map opens at a 500 m span"),Map->DefaultViewZoom(),2.f);Map->MapHalfExtent=7500;TestEqual(TEXT("Small maps show their full extent rather than zooming outside them"),Map->DefaultViewZoom(),1.f);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBLowTerrainAlphaTest,"BBSB.Map.LowTerrainTransparency",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBLowTerrainAlphaTest::RunTest(const FString&)
{
 auto* S=NewObject<UBBSBTerrainMapData>();constexpr int32 N=64;TArray<float> H;H.Init(0,N*N);for(int32 Y=0;Y<N;++Y)for(int32 X=32;X<N;++X)H[Y*N+X]=500;
 TArray<FColor> P;int32 Valid=0,Cliffs=0,Edges=0;float Low=0,High=0;
 auto Render=[&](){return BBSBTerrainMapRaster::Render(H,N,100,*S,P,Valid,Cliffs,Edges,Low,High);};
 TestTrue(TEXT("Terrain with low ground still bakes"),Render());TestEqual(TEXT("Lowest ground is genuinely transparent"),P[20*N+15].A,uint8(0));TestEqual(TEXT("Higher terrain remains visible"),P[20*N+45].A,uint8(255));
 TestEqual(TEXT("Transparency does not erase the height sample range"),High-Low,500.f);
 S->bHideLowGround=false;Render();TestEqual(TEXT("The transparency option can be disabled"),P[20*N+15].A,uint8(255));
 S->bHideLowGround=true;S->HiddenHeightAboveMinimum=600;Render();TestEqual(TEXT("The editable cutoff affects the image"),P[20*N+45].A,uint8(0));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBMapMarkerSaveTest,"BBSB.Map.MarkerSaveIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBMapMarkerSaveTest::RunTest(const FString&)
{
 auto* Save=Cast<UBBSBMapMarkerSave>(UGameplayStatics::CreateSaveGameObject(UBBSBMapMarkerSave::StaticClass()));
 for(FName Level:{FName(TEXT("/Game/BBSB/Levels/CombatFieldTest")),FName(TEXT("/Game/BBSB/PuzzleField/L_BunniarMeadow"))})
 { FBBSBMapMarker M;M.Id=FGuid::NewGuid();M.Level=Level;M.Position=FVector(1234,5678,910);Save->Markers.Add(M); }
 TestTrue(TEXT("Markers from separate levels are valid"),BBSBMapMarkerRules::Valid(Save->Markers));
 const FString Slot=TEXT("BBSB_MapMarker_Test_")+FGuid::NewGuid().ToString();if(!TestFalse(TEXT("Never overwrite another profile"),UGameplayStatics::DoesSaveGameExist(Slot,0)))return false;
 ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot,0); };
 TestTrue(TEXT("Marker save writes only the isolated test slot"),UGameplayStatics::SaveGameToSlot(Save,Slot,0));auto* Loaded=Cast<UBBSBMapMarkerSave>(UGameplayStatics::LoadGameFromSlot(Slot,0));
 if(!TestNotNull(TEXT("Marker save reloads"),Loaded))return false;
 if(!TestEqual(TEXT("Both levels survive the round trip"),Loaded->Markers.Num(),2))return false;
 TestTrue(TEXT("Marker identity and exact position survive"),Loaded->Markers[0].Id==Save->Markers[0].Id && Loaded->Markers[0].Position==Save->Markers[0].Position);TestTrue(TEXT("Level identity survives"),Loaded->Markers[1].Level==Save->Markers[1].Level);
 const FBBSBMapMarker Duplicate=Loaded->Markers[0];Loaded->Markers.Add(Duplicate);TestFalse(TEXT("Duplicate identifiers are refused before overwrite"),BBSBMapMarkerRules::Valid(Loaded->Markers));
 return true;
}
#endif
