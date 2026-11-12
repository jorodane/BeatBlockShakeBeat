#include "UI/BBSBPaperWidget.h"
#include "HAL/PlatformTime.h"
#include "Blueprint/WidgetLayoutLibrary.h"

UBBSBPaperWidget::UBBSBPaperWidget(const FObjectInitializer& Initializer):Super(Initializer)
{ ForceVolatile(true); }
float UBBSBPaperWidget::PaperProgress() const
{ return bPaperMotionEnabled ? Motion.Value(FPlatformTime::Seconds()):bClosing ? 0.f:1.f; }
void UBBSBPaperWidget::OpenPaper(EBBSBPaperEntrance Entrance)
{
 PaperEntranceKind=Entrance;bClosing=false;Motion.SetOpen(true,FPlatformTime::Seconds(),!IsInViewport());ApplyPaperMotion();
}
void UBBSBPaperWidget::ClosePaper()
{
 if(bClosing || !IsInViewport())return;
 bClosing=true;Motion.SetOpen(false,FPlatformTime::Seconds());SetVisibility(ESlateVisibility::HitTestInvisible);
 if(!bPaperMotionEnabled)RemoveFromParent();else ApplyPaperMotion();
}
void UBBSBPaperWidget::ApplyPaperMotion()
{
 UWidget* Target=PaperAnimationTarget ? PaperAnimationTarget.Get():this;
 const float T=PaperProgress(),ViewportScale=FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(this));
 const FVector2D View=UWidgetLayoutLibrary::GetViewportSize(this)/ViewportScale;
 const float S=FMath::Clamp(float(FMath::Min(View.Y/720,View.X/1180)),.5f,2.5f);
 Target->SetRenderTransformPivot(FVector2D(.5));
 if(PaperEntranceKind==EBBSBPaperEntrance::Right)
 { Target->SetRenderScale(FVector2D(1));Target->SetRenderTranslation(FVector2D(680*S*(1-T),0)); }
 else
 { const float Scale=.12f+.88f*T;Target->SetRenderScale(FVector2D(Scale));Target->SetRenderTranslation(FVector2D(0,18*S*(1-T))); }
}
void UBBSBPaperWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
 Super::NativeTick(Geometry,DeltaTime);if(!Motion.bInitialized)return;ApplyPaperMotion();
 if(bClosing && Motion.IsComplete(FPlatformTime::Seconds()))RemoveFromParent();
}
