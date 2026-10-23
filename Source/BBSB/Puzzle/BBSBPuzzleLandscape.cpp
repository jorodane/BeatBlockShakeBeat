#include "Puzzle/BBSBPuzzleField.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "Materials/MaterialInterface.h"
#if WITH_EDITOR
#include "LandscapeInfo.h"
#include "LandscapeEditTypes.h"
#endif

namespace
{
 float MeadowBase(float X,float Y) { return 90.f+60.f*FMath::Sin(X/1700.f)*FMath::Cos(Y/1500.f)+22.f*FMath::Sin((X+Y)/700.f); }
}
FVector UBBSBPuzzleLandscapeTools::StationPosition(int32 Index)
{
 Index=FMath::Clamp(Index,0,19);const int32 Row=Index/5,Column=Row%2==0 ? Index%5:4-Index%5;
 const float X=-4200+Row*2800,Y=-4400+Column*2200;return FVector(X,Y,MeadowBase(X,Y));
}
float UBBSBPuzzleLandscapeTools::GroundHeight(float X,float Y)
{
 float Height=MeadowBase(X,Y)+16.f*FMath::Sin(X/240.f)*FMath::Cos(Y/310.f);
 for(int I=0;I<20;++I)
 {
  const FVector P=StationPosition(I);const float D=FVector2D(X-P.X,Y-P.Y).Size();
  const float W=1.f-FMath::SmoothStep(460.f,800.f,D);Height=FMath::Lerp(Height,float(P.Z),W);
 }
 const float R=FMath::Sqrt(FMath::Square(X/7100.f)+FMath::Square(Y/6600.f));
 const float Edge=FMath::SmoothStep(.96f,1.65f,R);
 Height+=Edge*(850.f+350.f*FMath::Sin(X/1600.f)*FMath::Sin(Y/1900.f));
 return Height;
}
ALandscape* UBBSBPuzzleLandscapeTools::CreateMeadow(UObject* Context,UMaterialInterface* Material)
{
#if WITH_EDITOR
 UWorld* World=GEngine ? GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull):nullptr;
 if(!World || World->IsGameWorld()) return nullptr;
 constexpr int32 Size=253;TArray<uint16> Heights;Heights.SetNumUninitialized(Size*Size);
 for(int Y=0;Y<Size;++Y) for(int X=0;X<Size;++X)
  Heights[Y*Size+X]=FMath::Clamp(FMath::RoundToInt(32768.f+GroundHeight((X-126)*100.f,(Y-126)*100.f)*1.28f),0,65535);
 auto* Land=World->SpawnActor<ALandscape>(FVector(-12600,-12600,0),FRotator::ZeroRotator);
 if(!Land) return nullptr;
 Land->SetActorLabel(TEXT("Landscape_BunniarMeadow"));Land->SetActorScale3D(FVector(100,100,100));
 Land->LandscapeMaterial=Material;
 TMap<FGuid,TArray<uint16>> HeightData;HeightData.Add(FGuid(),MoveTemp(Heights));
 TMap<FGuid,TArray<FLandscapeImportLayerInfo>> Layers;Layers.Add(FGuid(),TArray<FLandscapeImportLayerInfo>());
 Land->Import(FGuid::NewGuid(),0,0,252,252,1,63,HeightData,nullptr,Layers,ELandscapeImportAlphamapType::Additive,TArrayView<const FLandscapeLayer>());
 Land->UpdateAllComponentMaterialInstances();Land->RegisterAllComponents();Land->PostEditChange();Land->MarkPackageDirty();
 return Land;
#else
 return nullptr;
#endif
}
