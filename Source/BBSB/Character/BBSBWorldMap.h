#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "BBSBWorldMap.generated.h"

class ABBSBCharacter;
class ABBSBRespawnPoint;
class UBBSBWorldMapComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

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
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Map",meta=(ClampMin="512",ClampMax="4096")) int32 TextureResolution=2048;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Map") TObjectPtr<UTextureRenderTarget2D> MapTexture;
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
 ABBSBCharacter* MapPlayer() const { return Player; }
 bool ShouldDrawMinimap() const;
 bool OwnsOnlyMapInputBlock() const { return bMapOpen && bOwnInput && !bOriginalBlock; }
 void RefreshPoints();
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> Capture;
 bool bOwnInput=false,bOriginalCursor=false,bOriginalBlock=false;
 double NextCaptureAt=0,NextPointScanAt=0;
 void EnsurePresentation();
};
