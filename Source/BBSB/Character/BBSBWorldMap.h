#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "Character/BBSBMapAnnotations.h"
#include "BBSBWorldMap.generated.h"

class ABBSBCharacter;
class ABBSBRespawnPoint;
class UBBSBWorldMapComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UTexture2D;
class UTexture;
class UBBSBTerrainMapData;

UCLASS()
class BBSB_API UBBSBWorldMapWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UPROPERTY(Transient) TObjectPtr<UBBSBWorldMapComponent> Map;
 bool bFullMap=false;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map|Navigation") float Zoom=1;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map|Navigation") FVector2D ViewCenterUV=FVector2D(.5,.5);
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map|Navigation") bool bFollowPlayer=false;
 UFUNCTION(BlueprintCallable,Category="Map|Navigation") void ZoomBy(float Factor);
 UFUNCTION(BlueprintCallable,Category="Map|Navigation") void ZoomAtViewport(float NewZoom,FVector2D Anchor);
 UFUNCTION(BlueprintCallable,Category="Map|Navigation") void ResetView();
 UFUNCTION(BlueprintCallable,Category="Map|Navigation") void CenterOnPlayer();
 UFUNCTION(BlueprintPure,Category="Map|Navigation") FVector2D NavigationButtonCenter(FName Key) const;
 UFUNCTION(BlueprintPure,Category="Map|Navigation") FVector2D CanvasPoint(FVector2D Fraction) const { return MapAt+Fraction*MapSize; }
 UFUNCTION(BlueprintPure,Category="Map|Navigation") FVector2D MarkerPosition(const ABBSBRespawnPoint* Point) const;
 UFUNCTION(BlueprintPure,Category="Map|Minimap") FVector2D WorldToMinimap(FVector Position) const;
 UFUNCTION(BlueprintPure,Category="Map|Minimap") bool IsInsideMinimap(FVector Position,float EdgePadding=8) const;
 UFUNCTION(BlueprintPure,Category="Map|Minimap") FVector2D CompassPosition(FName Direction) const;
 UFUNCTION(BlueprintPure,Category="Map|Minimap") float MinimapHeading() const { return PaintedMinimapYaw; }
 UFUNCTION(BlueprintPure,Category="Map|Equipment") FVector2D EquipmentIconPosition(int32 WheelIndex) const;
 UFUNCTION(BlueprintPure,Category="Map|Equipment") float EquipmentIconEmphasis(int32 WheelIndex) const;
 UFUNCTION(BlueprintPure,Category="Map|Equipment") float EquipmentRingRotation() const { return PaintedEquipmentAngle; }
 UFUNCTION(BlueprintPure,Category="Map|Equipment") FString EquipmentTitle() const;
 UFUNCTION(BlueprintPure,Category="Map|Equipment") float EquipmentOrbitRadius() const { return PaintedEquipmentOrbit*PaintScale; }
 UFUNCTION(BlueprintPure,Category="Map|Equipment") float EquipmentModeOpacity() const { return PaintedEquipmentOpacity; }
 UFUNCTION(BlueprintPure,Category="Map|Equipment") bool IsShowingBunniar() const { return bPresentedBunniar; }
 void UpdateFollowingPlayer();
 void CancelPointerDrag();
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled) const override;
 virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
 virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseWheel(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) override;
 virtual FReply NativeOnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event) override;
private:
 mutable FVector2D MapAt,MapSize,ListAt,CloseAt,CloseSize,ToolbarAt;
 mutable float PaintScale=1;
 FVector2D DragStart,PreviousDrag;
 bool bPanPending=false,bDragging=false;
 FVector2D ViewMinimum() const { return ViewCenterUV-FVector2D(.5f/Zoom); }
 void ClampCenter();
 FVector2D Pointer=FVector2D(-10000);
 mutable float PaintedMinimapYaw=0;
 mutable bool bEquipmentMotionInitialized=false;
 mutable int32 EquipmentMotionTarget=INDEX_NONE;
 mutable double EquipmentMotionStartedAt=0;
 static constexpr double EquipmentMotionDuration=.28;
 mutable int64 EquipmentWheelRevisionSeen=0,EquipmentWheelTravelSeen=0;
 mutable float EquipmentFromAngle=0,EquipmentToAngle=0,PaintedEquipmentAngle=0;
 mutable float EquipmentFromEmphasis[5]={0},PaintedEquipmentEmphasis[5]={0};
 mutable bool bEquipmentModeInitialized=false,bPresentedBunniar=false,bEquipmentModeTarget=false;
 mutable double EquipmentModeStartedAt=-1;
 mutable float PaintedEquipmentOrbit=140,EquipmentModeFromOrbit=140,PaintedEquipmentOpacity=1,EquipmentModeFromOpacity=1;
 void UpdateEquipmentMode(bool Bunniar) const;
 void UpdateEquipmentMotion(int32 EquippedIndex) const;
 void PaintEquipmentRing(const FGeometry& G,FSlateWindowElementList& E,int32 Layer) const;
 int32 PaintCircularMinimap(const FGeometry& G,FSlateWindowElementList& E,int32 Layer) const;
 void PaintAnnotations(const FGeometry& G,FSlateWindowElementList& E,int32 Layer) const;
 void PaintWorldPing(const FGeometry& G,FSlateWindowElementList& E,int32 Layer) const;
};

UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBWorldMapComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UBBSBWorldMapComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Map") FVector2D MapCenter=FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Map",meta=(ClampMin="500")) float MapHalfExtent=7500;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Map",meta=(ClampMin="200")) float MinimapHalfExtent=1100;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Map",meta=(ClampMin="50",ClampMax="5000",Units="m")) float DefaultViewWidthMeters=500;
 UFUNCTION(BlueprintPure,Category="Map") float DefaultViewZoom() const { return FMath::Clamp(MapHalfExtent*2/FMath::Max(1.f,DefaultViewWidthMeters*100),1.f,32.f); }
 UFUNCTION(BlueprintPure,Category="Map|Minimap") float MinimapViewYaw() const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Map",meta=(ClampMin="512",ClampMax="4096")) int32 TextureResolution=2048;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map") TObjectPtr<UTextureRenderTarget2D> MapTexture;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Map|Terrain") bool bUseTerrainMap=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Map|Terrain") TMap<FName,TSoftObjectPtr<UBBSBTerrainMapData>> TerrainMaps;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map|Terrain") TObjectPtr<UBBSBTerrainMapData> AppliedTerrainMap;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map|Terrain") TObjectPtr<UTexture2D> TerrainTexture;
 UFUNCTION(BlueprintPure,Category="Map|Terrain") UTexture* DisplayTexture() const;
 UFUNCTION(BlueprintCallable,Category="Map|Terrain") void SetTerrainMapEnabled(bool Enabled);
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map") TObjectPtr<UBBSBWorldMapWidget> MiniWidget;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map") TObjectPtr<UBBSBWorldMapWidget> FullWidget;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Map") bool bMapOpen=false;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map") TArray<TObjectPtr<ABBSBRespawnPoint>> Points;
 UFUNCTION(BlueprintCallable,Category="Map") void ToggleMap();
 UFUNCTION(BlueprintCallable,Category="Map") bool OpenMap();
 UFUNCTION(BlueprintCallable,Category="Map") void CloseMap();
 UFUNCTION(BlueprintCallable,Category="Map") bool TravelTo(ABBSBRespawnPoint* Point);
 UFUNCTION(BlueprintPure,Category="Map") FVector2D WorldToMap(FVector World) const;
 UFUNCTION(BlueprintPure,Category="Map") FVector MapToWorld(FVector2D UV) const;
 UFUNCTION(BlueprintPure,Category="Map") bool IsPointAvailable(ABBSBRespawnPoint* Point) const;
 UFUNCTION(BlueprintPure,Category="Map") FVector2D MapMarkerPosition(ABBSBRespawnPoint* Point) const;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map|Markers") TArray<FBBSBMapMarker> UserMarkers;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map|Markers") bool bMarkerSaveReadOnly=false;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map|Ping") FVector PingPosition=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Map|Ping",meta=(ClampMin="2",ClampMax="15",Units="s")) float PingLifetime=6;
 UFUNCTION(BlueprintPure,Category="Map|Markers") FName AnnotationLevel() const;
 UFUNCTION(BlueprintCallable,Category="Map|Markers") bool AddMapMarker(FVector2D UV);
 UFUNCTION(BlueprintCallable,Category="Map|Markers") bool RemoveMapMarker(FGuid Id);
 UFUNCTION(BlueprintCallable,Category="Map|Markers") bool SaveMapMarkers();
 UFUNCTION(BlueprintCallable,Category="Map|Ping") bool PingMapLocation(FVector2D UV);
 UFUNCTION(BlueprintPure,Category="Map|Ping") float PingRemaining() const;
 ABBSBCharacter* MapPlayer() const { return Player; }
 bool ShouldDrawMinimap() const;
 bool OwnsOnlyMapInputBlock() const { return bMapOpen && bOwnInput && !bOriginalBlock; }
 void RefreshPoints();
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> Capture;
 bool bOwnInput=false,bOriginalCursor=false,bOriginalBlock=false;
 double NextCaptureAt=0,NextPointScanAt=0;
 bool bMapConfigured=false,bOriginalBoundsSaved=false;
 FVector2D OriginalMapCenter;
 float OriginalMapHalfExtent=7500,OriginalMinimapHalfExtent=1100;
 void EnsurePresentation();
 FString MarkerSlot;
 bool bMarkersDirty=false;
 double MarkerSaveAt=0,PingUntil=-100;
 void InitializeMapMarkers();
 bool ResolveAnnotationPoint(FVector2D UV,FVector& Position) const;
};
