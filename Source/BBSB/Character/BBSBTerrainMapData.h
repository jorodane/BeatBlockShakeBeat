#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BBSBTerrainMapData.generated.h"

class UTexture2D;
class UWorld;

/** A north-up, baked height diagram. Rebuild explicitly after editing terrain. */
UCLASS(BlueprintType)
class BBSB_API UBBSBTerrainMapData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Terrain map") bool bEnabled=true;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Terrain map") TSoftObjectPtr<UWorld> SourceLevel;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Terrain map") TObjectPtr<UTexture2D> Image;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Terrain map") FVector2D WorldCenter=FVector2D::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Terrain map") float WorldHalfExtent=500;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Terrain map",meta=(ClampMin="500",ClampMax="10000",Units="cm")) float MinimapHalfExtent=2800;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Bake",meta=(ClampMin="256",ClampMax="4096")) int32 Resolution=2048;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Bake",meta=(ClampMin="0",ClampMax="2000",Units="cm")) float Padding=200;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Style",meta=(ClampMin="2",ClampMax="12")) int32 HeightBands=5;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Style") FLinearColor LowGround=FLinearColor(.075f,.078f,.075f);
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Style") FLinearColor HighGround=FLinearColor(.43f,.40f,.32f);
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Style") FLinearColor CliffFace=FLinearColor(.025f,.030f,.032f);
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Style") FLinearColor Outline=FLinearColor(.008f,.012f,.012f);
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Style") FLinearColor Background=FLinearColor(.006f,.008f,.009f);
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Style|Transparency") bool bHideLowGround=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Style|Transparency",meta=(ClampMin="0",ClampMax="2000",Units="cm")) float HiddenHeightAboveMinimum=100;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bake result") int32 TransparentPixels=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Cliff outline",meta=(ClampMin="20",ClampMax="85",Units="deg")) float CliffAngle=47;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Cliff outline",meta=(ClampMin="25",ClampMax="1000",Units="cm")) float MinimumCliffRise=120;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Cliff outline",meta=(ClampMin="50",ClampMax="500",Units="cm")) float GradientSpan=150;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Cliff outline",meta=(ClampMin="1",ClampMax="5")) int32 OutlineWidth=2;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bake result") bool bLastBakeSucceeded=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bake result") FString LastBakeMessage;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bake result") int32 BakeRevision=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bake result") int32 SampledPixels=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bake result") int32 CliffPixels=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bake result") int32 OutlinePixels=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bake result") float MinimumHeight=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bake result") float MaximumHeight=0;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="Bake") void RebuildFromOpenLevel();
};

namespace BBSBTerrainMapRaster
{
 constexpr float NoHeight=-MAX_flt;
 bool Render(const TArray<float>& Heights,int32 Size,float PixelWorldSize,const UBBSBTerrainMapData& Style,TArray<FColor>& Pixels,int32& ValidCount,int32& CliffCount,int32& OutlineCount,float& MinHeight,float& MaxHeight);
}
