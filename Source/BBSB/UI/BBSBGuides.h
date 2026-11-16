#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Engine/DataAsset.h"
#include "UI/BBSBPaperWidget.h"
#include "BBSBGuides.generated.h"
class AHUD;class UCanvas;class UPrimitiveComponent;class ABBSBCharacter;
class UButton;class UVerticalBox;class UTextBlock;class UImage;class UTexture2D;class APlayerController;

USTRUCT(BlueprintType)
struct FBBSBGuideStep
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FText Name;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(MultiLine=true)) FText Description;
};
UCLASS(BlueprintType)
class BBSB_API UBBSBGuideDefinition : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName GuideId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FText Title;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FText Category;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(MultiLine=true)) FText Description;
 /** Optional replacement for the native illustrated example. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UTexture2D> Illustration;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FBBSBGuideStep> Steps;
};

UCLASS()
class BBSB_API UBBSBGuideSubsystem : public UGameInstanceSubsystem,public FTickableGameObject
{
 GENERATED_BODY()
public:
 UBBSBGuideSubsystem();
 virtual void Deinitialize() override;
 virtual void Tick(float DeltaTime) override;
 virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBBSBGuideSubsystem,STATGROUP_Tickables); }
 virtual bool IsTickable() const override { return !IsTemplate(); }
 virtual bool IsTickableWhenPaused() const override { return true; }
 virtual UWorld* GetTickableGameObjectWorld() const override;
 virtual UWorld* GetWorld() const override { return GetTickableGameObjectWorld(); }
 static UBBSBGuideSubsystem* Get(const UObject* Context);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Guides") TArray<TSoftObjectPtr<UBBSBGuideDefinition>> GuideDefinitions;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TSet<FName> SeenGuides;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bGuideOpen=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bOpeningActive=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FName SelectedGuide;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Hints") bool bLearnedMove=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Hints") bool bLearnedLook=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Hints") float MoveHintAlpha=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Hints") float LookHintAlpha=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Hints") float DragHintAlpha=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Hints") FVector2D DragHintAxis=FVector2D::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Hints") FText DragHintText;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Hints",meta=(ClampMin="0.15",Units="s")) float HintIdleSeconds=.65f;
 void NotifyMoveInput();
 void NotifyLookInput();
 void NotifySurfaceMotion(FVector2D Delta);
 void DrawHints(AHUD* HUD,UCanvas* Canvas) const;
 void GuideInputReleased(FKey Key);
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly) TObjectPtr<class UBBSBGuideNotebook> Notebook;
 UFUNCTION(BlueprintCallable) bool UnlockAndShow(FName Id);
 UFUNCTION(BlueprintCallable) void OpenNotes();
 UFUNCTION(BlueprintCallable) void SelectGuide(FName Id);
 UFUNCTION(BlueprintCallable) void CloseGuide();
 UFUNCTION(BlueprintCallable) void ResetForNewGame();
 UFUNCTION(BlueprintPure) UBBSBGuideDefinition* Definition(FName Id) const;
 UFUNCTION(BlueprintPure) TArray<FName> KnownGuides() const;
private:
 TWeakObjectPtr<APlayerController> Controller;
 TWeakObjectPtr<UWorld> GuideWorld;
 float DoorAimSeconds=0,GameplayHintSeconds=0;
 double LastSurfaceMotionAt=-100;
 TWeakObjectPtr<UPrimitiveComponent> HintSurface;
 FVector HintPoint=FVector::ZeroVector;
 bool bHintsVisible=false,bDragBothWays=true,bCarryHint=false;
 bool bPausedPrimary=false,bPausedShortcut=false,bReleasedPrimary=false,bReleasedShortcut=false;
 bool CanLearnControls() const;
 void TickHints(ABBSBCharacter* P,float DT);
 bool bFromMenu=false,bOwnPause=false,bOwnMoveIgnore=false,bOwnLookIgnore=false,bPreviousCursor=false;
 bool Open(FName Id,bool Notes);
};

UCLASS()
class BBSB_API UBBSBGuideIllustration : public UUserWidget
{
 GENERATED_BODY()
public:
 FName GuideId;
protected:
 virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Culling,FSlateWindowElementList& E,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override;
};
UCLASS()
class BBSB_API UBBSBGuideClick : public UObject
{
 GENERATED_BODY()
public:
 UPROPERTY() TObjectPtr<UBBSBGuideSubsystem> Owner;
 FName Id;
 UFUNCTION() void Click();
};
UCLASS()
class BBSB_API UBBSBGuideNotebook : public UBBSBPaperWidget
{
 GENERATED_BODY()
public:
 UPROPERTY() TObjectPtr<UBBSBGuideSubsystem> Guides;
 void Refresh();
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual FReply NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E) override;
 virtual FReply NativeOnKeyUp(const FGeometry& G,const FKeyEvent& E) override;
 virtual FReply NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E) override;
private:
 UPROPERTY() TObjectPtr<UVerticalBox> List;
 UPROPERTY() TObjectPtr<UVerticalBox> Steps;
 UPROPERTY() TObjectPtr<UTextBlock> Title;
 UPROPERTY() TObjectPtr<UTextBlock> Category;
 UPROPERTY() TObjectPtr<UTextBlock> Description;
 UPROPERTY() TObjectPtr<UTextBlock> Count;
 UPROPERTY() TObjectPtr<UImage> Artwork;
 UPROPERTY() TObjectPtr<UBBSBGuideIllustration> Diagram;
 UPROPERTY() TObjectPtr<UButton> CloseButton;
 UPROPERTY() TArray<TObjectPtr<UBBSBGuideClick>> Clicks;
 UFUNCTION() void Dismiss();
};
