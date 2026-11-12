#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/BBSBPaperMotion.h"
#include "BBSBPaperWidget.generated.h"

UENUM(BlueprintType)
enum class EBBSBPaperEntrance : uint8 { Right, Center };

/** Presentation only: owners retain their existing input and gameplay policies. */
UCLASS(Abstract)
class BBSB_API UBBSBPaperWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UBBSBPaperWidget(const FObjectInitializer& Initializer);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UI|Motion") bool bPaperMotionEnabled=true;
 UFUNCTION(BlueprintCallable,Category="UI|Motion") void OpenPaper(EBBSBPaperEntrance Entrance);
 UFUNCTION(BlueprintCallable,Category="UI|Motion") void ClosePaper();
 UFUNCTION(BlueprintPure,Category="UI|Motion") float PaperProgress() const;
 UFUNCTION(BlueprintPure,Category="UI|Motion") bool IsPaperClosing() const { return bClosing; }
protected:
 UPROPERTY(Transient) TObjectPtr<UWidget> PaperAnimationTarget;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
private:
 FBBSBPaperMotion Motion;
 EBBSBPaperEntrance PaperEntranceKind=EBBSBPaperEntrance::Center;
 bool bClosing=false;
 void ApplyPaperMotion();
};
