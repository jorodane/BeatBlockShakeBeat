#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Character/BBSBTerrainMapData.h"
#include "Character/BBSBWorldMap.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBTerrainMapRasterTest,"BBSB.Map.HeightCliffRaster",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBTerrainMapRasterTest::RunTest(const FString&)
{
 auto* Style=NewObject<UBBSBTerrainMapData>();Style->bHideLowGround=false;constexpr int32 N=64;
 TArray<float> Heights;Heights.Init(300,N*N);TArray<FColor> Pixels;int32 Valid=0,Cliffs=0,Edges=0;float Low=0,High=0;
 auto Render=[&](){return BBSBTerrainMapRaster::Render(Heights,N,100,*Style,Pixels,Valid,Cliffs,Edges,Low,High);};
 TestTrue(TEXT("Flat terrain can be baked"),Render());TestEqual(TEXT("A flat floor has no cliffs"),Cliffs,0);
 TestEqual(TEXT("Flat centre is ground, not ink"),Pixels[N/2*N+N/2],Style->LowGround.ToFColorSRGB());const int32 Perimeter=Edges;
 for(int32 Y=0;Y<N;++Y)for(int32 X=0;X<N;++X)Heights[Y*N+X]=X*20;
 TestTrue(TEXT("A gentle slope can be baked"),Render());TestEqual(TEXT("Gentle slope has no cliff ink"),Cliffs,0);TestEqual(TEXT("Height-colour bands add no contour lines"),Edges,Perimeter);
 TestTrue(TEXT("Different elevations use distinct ground shades"),Pixels[30*N+10]!=Pixels[30*N+50]);
 for(int32 Y=0;Y<N;++Y)for(int32 X=0;X<N;++X)Heights[Y*N+X]=X<N/2 ? 0:1000;
 TestTrue(TEXT("A ten-metre height break can be baked"),Render());TestTrue(TEXT("The break creates a steep wall"),Cliffs>0);TestTrue(TEXT("Cliff boundaries create additional ink"),Edges>Perimeter);
 TestEqual(TEXT("The cliff lip is outlined"),Pixels[32*N+30],Style->Outline.ToFColorSRGB());
 for(float& Z:Heights)Z=Z>0 ? 50:0;
 Render();TestEqual(TEXT("A small height discontinuity is not marked as a cliff"),Cliffs,0);
 Heights[32*N+32]=BBSBTerrainMapRaster::NoHeight;Render();TestEqual(TEXT("Missing coverage remains background"),Pixels[32*N+32],FColor(Style->Background.ToFColorSRGB().R,Style->Background.ToFColorSRGB().G,Style->Background.ToFColorSRGB().B,0));
 Heights.Init(BBSBTerrainMapRaster::NoHeight,N*N);TestFalse(TEXT("Empty coverage cannot overwrite a valid map"),Render());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBBSBTerrainMapProjectionTest,"BBSB.Map.TerrainProjection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBBSBTerrainMapProjectionTest::RunTest(const FString&)
{
 auto* Map=NewObject<UBBSBWorldMapComponent>();Map->MapCenter=FVector2D(1000,2000);Map->MapHalfExtent=500;
 TestTrue(TEXT("World north is the top edge of the baked image"),Map->WorldToMap(FVector(1500,2000,0)).Equals(FVector2D(.5,0),.0001));
 TestTrue(TEXT("World east is the right edge of the baked image"),Map->WorldToMap(FVector(1000,2500,0)).Equals(FVector2D(1,.5),.0001));
 const FVector P(1234,1876,0);TestTrue(TEXT("Map image and world markers use the same inverse projection"),Map->MapToWorld(Map->WorldToMap(P)).Equals(P,.001));
 return true;
}
#endif
