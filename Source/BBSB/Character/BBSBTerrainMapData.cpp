#include "Character/BBSBTerrainMapData.h"
#include "Engine/Texture2D.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#endif

bool BBSBTerrainMapRaster::Render(const TArray<float>& H,int32 N,float Step,const UBBSBTerrainMapData& S,TArray<FColor>& Out,int32& ValidCount,int32& CliffCount,int32& OutlineCount,float& MinHeight,float& MaxHeight)
{
 ValidCount=CliffCount=OutlineCount=0;MinHeight=MAX_flt;MaxHeight=-MAX_flt;Out.Reset();
 if(N<3 || int64(N)*N!=H.Num() || !FMath::IsFinite(Step) || Step<=0)return false;
 auto Valid=[](float V){return FMath::IsFinite(V) && V>NoHeight*.5f;};
 for(float Z:H)if(Valid(Z)) { ++ValidCount;MinHeight=FMath::Min(MinHeight,Z);MaxHeight=FMath::Max(MaxHeight,Z); }
 if(!ValidCount)return false;
 const int32 Span=FMath::Clamp(FMath::RoundToInt(S.GradientSpan/Step),1,N/2),Bands=FMath::Clamp(S.HeightBands,2,12);
 const float Threshold=FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(S.CliffAngle,20.f,85.f))),Range=FMath::Max(1.f,MaxHeight-MinHeight);
 TArray<uint8> Wall;Wall.Init(0,H.Num());Out.SetNumUninitialized(H.Num());
 auto Sample=[&](int32 X,int32 Y,float Fallback){const float Z=H[FMath::Clamp(Y,0,N-1)*N+FMath::Clamp(X,0,N-1)];return Valid(Z) ? Z:Fallback;};
 auto Visible=[&](float Z){return Valid(Z) && (!S.bHideLowGround || Z>MinHeight+FMath::Max(0.f,S.HiddenHeightAboveMinimum));};
 FColor Void=S.Background.ToFColorSRGB();Void.A=0;
 const FColor Face=S.CliffFace.ToFColorSRGB(),Ink=S.Outline.ToFColorSRGB();
 for(int32 Y=0;Y<N;++Y)for(int32 X=0;X<N;++X)
 {
  const int32 I=Y*N+X;const float Z=H[I];if(!Visible(Z)) { Out[I]=Void;continue; }
  const float L=Sample(X-Span,Y,Z),R=Sample(X+Span,Y,Z),U=Sample(X,Y-Span,Z),D=Sample(X,Y+Span,Z);
  const float DX=(R-L)/(FMath::Max(1,FMath::Min(N-1,X+Span)-FMath::Max(0,X-Span))*Step);
  const float DY=(D-U)/(FMath::Max(1,FMath::Min(N-1,Y+Span)-FMath::Max(0,Y-Span))*Step);
  const float Rise=FMath::Max(FMath::Max(L,R),FMath::Max(U,D))-FMath::Min(FMath::Min(L,R),FMath::Min(U,D));
  Wall[I]=FMath::Sqrt(DX*DX+DY*DY)>=Threshold && Rise>=FMath::Max(0.f,S.MinimumCliffRise);
  if(Wall[I]) { ++CliffCount;Out[I]=Face; }
  else
  {
   const float T=float(FMath::Clamp(FMath::FloorToInt((Z-MinHeight)/Range*Bands),0,Bands-1))/(Bands-1);
   Out[I]=FMath::Lerp(S.LowGround,S.HighGround,T).ToFColorSRGB();
  }
 }
 // Only cliff/coverage boundaries get ink. Height-colour bands and gentle slopes have no contour lines.
 const int32 Radius=FMath::Clamp(S.OutlineWidth,1,5);
 for(int32 Y=0;Y<N;++Y)for(int32 X=0;X<N;++X)
 {
  const int32 I=Y*N+X;if(!Visible(H[I]))continue;bool Edge=false;
  for(const FIntPoint Delta:{FIntPoint(-Radius,0),FIntPoint(Radius,0),FIntPoint(0,-Radius),FIntPoint(0,Radius)})
  {
   const int32 XX=X+Delta.X,YY=Y+Delta.Y;
   if(XX<0 || YY<0 || XX>=N || YY>=N || !Visible(H[YY*N+XX]) || Wall[I]!=Wall[YY*N+XX]) { Edge=true;break; }
  }
  if(Edge) { Out[I]=Ink;++OutlineCount; }
 }
 return true;
}

void UBBSBTerrainMapData::RebuildFromOpenLevel()
{
 bLastBakeSucceeded=false;
#if WITH_EDITOR
 if(!GEditor || GEditor->PlayWorld) { LastBakeMessage=TEXT("Stop PIE before rebuilding the terrain map.");return; }
 UWorld* World=GEditor->GetEditorWorldContext().World();if(!World) { LastBakeMessage=TEXT("No editor level is open.");return; }
 if(GetOutermost()==GetTransientPackage()) { LastBakeMessage=TEXT("Create a saved terrain-map data asset first.");return; }
 TArray<ALandscapeProxy*> Landscapes;FBox Bounds(ForceInit);
 for(TActorIterator<ALandscapeProxy> It(World);It;++It)if(!It->LandscapeComponents.IsEmpty())
 { Landscapes.Add(*It);Bounds+=It->GetComponentsBoundingBox(true); }
 if(Landscapes.IsEmpty() || !Bounds.IsValid) { LastBakeMessage=TEXT("No loaded landscape was found. Existing map image is retained.");return; }
 const int32 N=FMath::RoundUpToPowerOfTwo(FMath::Clamp(Resolution,256,4096));
 const FVector2D Center(Bounds.GetCenter().X,Bounds.GetCenter().Y);
 const float Half=FMath::Max(500.f,float(FMath::Max(Bounds.GetExtent().X,Bounds.GetExtent().Y))+FMath::Max(0.f,Padding)),Step=Half*2/N;
 TArray<float> Heights;Heights.Init(BBSBTerrainMapRaster::NoHeight,N*N);
 for(int32 Y=0;Y<N;++Y)for(int32 X=0;X<N;++X)
 {
  // Texture right is +world Y, texture up is +world X, matching WorldToMap and its markers.
  const FVector Position(Center.X+(.5-(Y+.5)/N)*Half*2,Center.Y+((X+.5)/N-.5)*Half*2,0);
  for(const auto* Landscape:Landscapes)
  {
   const TOptional<float> Z=Landscape->GetHeightAtLocation(Position);
   if(Z.IsSet())Heights[Y*N+X]=FMath::Max(Heights[Y*N+X],Z.GetValue());
  }
 }
 TArray<FColor> Pixels;int32 Valid=0,Cliffs=0,Edges=0;float Low=0,High=0;
 if(!BBSBTerrainMapRaster::Render(Heights,N,Step,*this,Pixels,Valid,Cliffs,Edges,Low,High))
 { LastBakeMessage=TEXT("No valid landscape collision heights. Existing image is retained.");return; }
 UTexture2D* Texture=Image;
 if(!Texture)
 {
  const FString Name=TEXT("T_TerrainMap_")+World->GetName(),PackageName=FPackageName::GetLongPackagePath(GetOutermost()->GetName())+TEXT("/")+Name;
  if(FPackageName::DoesPackageExist(PackageName))Texture=LoadObject<UTexture2D>(nullptr,*(PackageName+TEXT(".")+Name));
  else { UPackage* Package=CreatePackage(*PackageName);Texture=NewObject<UTexture2D>(Package,*Name,RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(Texture); }
 }
 if(!Texture) { LastBakeMessage=TEXT("Cannot create the terrain image.");return; }
 Modify();Texture->Modify();Texture->Source.Init(N,N,1,1,TSF_BGRA8,reinterpret_cast<const uint8*>(Pixels.GetData()));
 Texture->SRGB=true;Texture->CompressionSettings=TC_EditorIcon;Texture->CompressionNoAlpha=false;Texture->MipGenSettings=TMGS_SimpleAverage;Texture->LODGroup=TEXTUREGROUP_UI;Texture->NeverStream=true;
 Texture->Filter=TF_Bilinear;Texture->AddressX=TA_Clamp;Texture->AddressY=TA_Clamp;Texture->PostEditChange();Texture->MarkPackageDirty();
 TransparentPixels=0;for(const FColor& P:Pixels)if(P.A==0)++TransparentPixels;
 Image=Texture;SourceLevel=World;WorldCenter=Center;WorldHalfExtent=Half;SampledPixels=Valid;CliffPixels=Cliffs;OutlinePixels=Edges;MinimumHeight=Low;MaximumHeight=High;
 ++BakeRevision;bLastBakeSucceeded=true;LastBakeMessage=FString::Printf(TEXT("%d x %d; %d landscapes; %.1f to %.1f cm; %d cliff pixels."),N,N,Landscapes.Num(),Low,High,Cliffs);MarkPackageDirty();
#else
 LastBakeMessage=TEXT("Terrain map rebuilding is editor-only.");
#endif
}
