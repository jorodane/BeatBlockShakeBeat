#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BBSBInventoryWidget.generated.h"

class UBBSBHuntingComponent;
class UBBSBItemDefinition;
class UImage;
class UInputComponent;
struct FBBSBInventoryEntry;
class UBBSBModuleDefinition;

UCLASS(Blueprintable)
class BBSB_API UBBSBInventoryWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UBBSBInventoryWidget(const FObjectInitializer& Initializer);
 UPROPERTY(Transient,BlueprintReadOnly,Category="Inventory") TObjectPtr<UBBSBHuntingComponent> Hunting;
 UFUNCTION(BlueprintPure,Category="Inventory") FName DraggedModule() const { return HeldModule; }
 UFUNCTION(BlueprintPure,Category="Inventory") FVector2D InventoryPanelSize() const { return PaintedLayout.Size; }
 UFUNCTION(BlueprintPure,Category="Inventory") FVector2D EquipmentPanelPosition() const { return PaintedLayout.Equipment; }
 UFUNCTION(BlueprintPure,Category="Inventory") float ItemIconDiameter() const { return PaintedLayout.Cell*.68f; }
 UFUNCTION(BlueprintPure,Category="Inventory") FGuid DraggedItem() const { return HeldId; }
 UFUNCTION(BlueprintPure,Category="Inventory") bool HasCarriedItem() const { return HeldId.IsValid() || !HeldModule.IsNone() || bHoldingCompanionPosition; }
 UFUNCTION(BlueprintPure,Category="Inventory") int32 FocusedSlot() const { return bKeyboardMode ? FocusIndex:HoverIndex; }
 UFUNCTION(BlueprintPure,Category="Inventory") FVector2D SlotCenter(int32 Index) const;
 UFUNCTION(BlueprintPure,Category="Inventory") FVector2D CloseButtonCenter() const;
 UFUNCTION(BlueprintPure,Category="Inventory") FVector2D SortButtonCenter() const;
 UFUNCTION(BlueprintPure,Category="Inventory") FVector2D SortApplyCenter() const;
 UFUNCTION(BlueprintPure,Category="Inventory") bool IsSortPanelOpen() const { return bSortOpen; }
 UFUNCTION(BlueprintPure,Category="Inventory") FText SlotTooltip(int32 Index) const;
 UFUNCTION(BlueprintPure,Category="Inventory") int32 EquippedWheelSlot() const;
 UFUNCTION(BlueprintCallable,Category="Inventory") void ResetInteraction();
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Growth") int32 GrowthPage=0;
 UFUNCTION(BlueprintCallable,Category="Growth") void SetGrowthPage(int32 Page);
 UFUNCTION(BlueprintPure,Category="Growth") FVector2D GrowthTabCenter(int32 Page) const;
 UFUNCTION(BlueprintPure,Category="Growth") FVector2D GrowthControlCenter(FName Key) const;
 UFUNCTION(BlueprintPure,Category="Growth") FGuid InspectedWeaponId() const { return InspectedWeapon; }
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
 struct FLayout { FVector2D Panel,Size,Grid,Wheel,Passive,Close,CloseSize,Sort,SortSize,SortPanel,SortPanelSize,SortApply,SortApplySize,Equipment,EquipmentSize;int32 VisibleRows=8;float Cell=64.f,Scale=1.f,Outer=76.f,Inner=32.f; };
 bool IsClosePoint(const FLayout& Layout,FVector2D Point) const;
 FLayout Layout(const FGeometry& Geometry) const;
 int32 PointerSlot(const FLayout& L,FVector2D Local) const;
 int32 InventoryDestination(int32 Index) const;
 FVector2D CellTopLeft(const FLayout& L,int32 Index) const;
 const FBBSBInventoryEntry* EntryForSlot(int32 Index) const;
 void ActivateSlot(int32 Index);
 void Navigate(int32 DX,int32 DY);
 void UpdatePointerSurface();
 bool ProcessMenuKey(FKey Key);
 void HandleMenuKey(FKey Key);
 UPROPERTY(Transient) TObjectPtr<UImage> PanelHit;
 UPROPERTY(Transient) TObjectPtr<UImage> EquipmentHit;
 UPROPERTY(Transient) TObjectPtr<UInputComponent> MenuKeys;
 TWeakObjectPtr<APlayerController> MenuInputOwner;
 mutable FLayout PaintedLayout;
 struct FGrowthControl { FName Key;FVector2D Position,Size;int32 Action=0,Index=0;FGuid Item;FName Module;bool Enabled=true;FString Tooltip; };
 mutable TArray<FGrowthControl> GrowthControls;
 mutable FGuid InspectedWeapon;
 int32 SocketPage=0,OrbPage=0,ModulePage=0,GridScrollRow=0;
 FName HeldModule,SelectedModule;
 void ActivateModuleCell(int32 Cell);
 void ActivateModuleSlot(int32 Slot);
 void PaintEquipment(const FGeometry& G,FSlateWindowElementList& E,int32 Layer) const;
 int32 PaintTooltip(const FGeometry& G,FSlateWindowElementList& E,int32 Layer,const FBBSBInventoryEntry* Entry,const UBBSBModuleDefinition* Module,const FString& Hint=FString()) const;
 int32 PaintGrowthPanel(const FGeometry& Geometry,FSlateWindowElementList& Elements,int32 Layer) const;
 int32 GrowthControlAt(FVector2D Point) const;
 bool ActivateGrowthControl(int32 Index);
 int32 GrowthTabAt(const FLayout& L,FVector2D Point) const;
 void NavigateGrowth(int32 DX,int32 DY);
 FVector2D Pointer=FVector2D::ZeroVector;
 FGuid HeldId;
 int32 FocusIndex=0,HoverIndex=INDEX_NONE;
 bool bHoldingCompanionPosition=false,bKeyboardMode=false,bClosePressed=false,bPointerInside=false,bSortOpen=false,bSortPressed=false;
 TSet<FKey> UIButtons;
};
