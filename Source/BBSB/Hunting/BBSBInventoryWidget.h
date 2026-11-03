#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BBSBInventoryWidget.generated.h"

class UBBSBHuntingComponent;
class UBBSBItemDefinition;
class UImage;
class UInputComponent;
struct FBBSBInventoryEntry;

UCLASS(Blueprintable)
class BBSB_API UBBSBInventoryWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UBBSBInventoryWidget(const FObjectInitializer& Initializer);
 UPROPERTY(Transient,BlueprintReadOnly,Category="Inventory") TObjectPtr<UBBSBHuntingComponent> Hunting;
 UFUNCTION(BlueprintPure,Category="Inventory") FGuid DraggedItem() const { return HeldId; }
 UFUNCTION(BlueprintPure,Category="Inventory") bool HasCarriedItem() const { return HeldId.IsValid() || bHoldingCompanionPosition; }
 UFUNCTION(BlueprintPure,Category="Inventory") int32 FocusedSlot() const { return bKeyboardMode ? FocusIndex:HoverIndex; }
 UFUNCTION(BlueprintPure,Category="Inventory") FVector2D SlotCenter(int32 Index) const;
 UFUNCTION(BlueprintPure,Category="Inventory") FVector2D CloseButtonCenter() const;
 UFUNCTION(BlueprintPure,Category="Inventory") FVector2D SortButtonCenter() const;
 UFUNCTION(BlueprintPure,Category="Inventory") FVector2D SortApplyCenter() const;
 UFUNCTION(BlueprintPure,Category="Inventory") bool IsSortPanelOpen() const { return bSortOpen; }
 UFUNCTION(BlueprintPure,Category="Inventory") FText SlotTooltip(int32 Index) const;
 UFUNCTION(BlueprintPure,Category="Inventory") int32 EquippedWheelSlot() const;
 UFUNCTION(BlueprintCallable,Category="Inventory") void ResetInteraction();
 void RegisterMenuKeys();
 void UnregisterMenuKeys();
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled) const override;
 virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseWheel(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual void NativeOnMouseLeave(const FPointerEvent& Event) override;
 virtual FReply NativeOnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event) override;
 virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
 virtual void NativeDestruct() override;
private:
 struct FLayout { FVector2D Panel,Size,Grid,Wheel,Passive,Close,CloseSize,Sort,SortSize,SortPanel,SortPanelSize,SortApply,SortApplySize;float Cell=40.f,Scale=1.f,Outer=76.f,Inner=32.f; };
 bool IsClosePoint(const FLayout& Layout,FVector2D Point) const;
 FLayout Layout(const FGeometry& Geometry) const;
 int32 PointerSlot(const FLayout& L,FVector2D Local) const;
 FVector2D CellTopLeft(const FLayout& L,int32 Index) const;
 const FBBSBInventoryEntry* EntryForSlot(int32 Index) const;
 void ActivateSlot(int32 Index);
 void Navigate(int32 DX,int32 DY);
 void UpdatePointerSurface();
 bool ProcessMenuKey(FKey Key);
 void HandleMenuKey(FKey Key);
 UPROPERTY(Transient) TObjectPtr<UImage> PanelHit;
 UPROPERTY(Transient) TObjectPtr<UInputComponent> MenuKeys;
 TWeakObjectPtr<APlayerController> MenuInputOwner;
 mutable FLayout PaintedLayout;
 FVector2D Pointer=FVector2D::ZeroVector;
 FGuid HeldId;
 int32 FocusIndex=0,HoverIndex=INDEX_NONE;
 bool bHoldingCompanionPosition=false,bKeyboardMode=false,bClosePressed=false,bPointerInside=false,bSortOpen=false,bSortPressed=false;
 TSet<FKey> UIButtons;
};
