#include "Character/BBSBWorldMap.h"
#include "BBSBCharacter.h"
#include "Field/BBSBFieldActions.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBItemIcon.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"

void UBBSBWorldMapWidget::UpdateEquipmentMotion(int32 EquippedIndex) const
{
 constexpr int32 Count=UBBSBFieldActionComponent::EquipmentWheelSlotCount;
 const double Now=FPlatformTime::Seconds();
 const auto* Actions=Map && Map->MapPlayer() ? Map->MapPlayer()->FieldActions.Get():nullptr;
 const int64 Revision=Actions ? Actions->EquipmentWheelTravelRevision:0,Travel=Actions ? Actions->EquipmentPreviewTravel():0;
 if(!bEquipmentMotionInitialized)
 {
  bEquipmentMotionInitialized=true;EquipmentMotionTarget=EquippedIndex;
  PaintedEquipmentAngle=EquipmentFromAngle=EquipmentToAngle=EquippedIndex>=0 ? -EquippedIndex*360.f/Count:0;
  for(int32 I=0;I<Count;++I)PaintedEquipmentEmphasis[I]=EquipmentFromEmphasis[I]=I==EquippedIndex ? 1.f:0.f;
  EquipmentMotionStartedAt=Now-EquipmentMotionDuration;EquipmentWheelRevisionSeen=Revision;EquipmentWheelTravelSeen=Travel;return;
 }
 const float T=FMath::Clamp(float((Now-EquipmentMotionStartedAt)/EquipmentMotionDuration),0.f,1.f),Ease=T*T*(3.f-2.f*T);
 PaintedEquipmentAngle=FMath::Lerp(EquipmentFromAngle,EquipmentToAngle,Ease);
 for(int32 I=0;I<Count;++I)PaintedEquipmentEmphasis[I]=FMath::Lerp(EquipmentFromEmphasis[I],I==EquipmentMotionTarget ? 1.f:0.f,Ease);
 const bool WheelChanged=Travel!=EquipmentWheelTravelSeen && (Actions->HasQueuedWheelPreview() || Revision!=EquipmentWheelRevisionSeen);
 const bool Cancelled=Travel!=EquipmentWheelTravelSeen && !WheelChanged;
 if(EquippedIndex==EquipmentMotionTarget && !WheelChanged && !Cancelled) { EquipmentWheelRevisionSeen=Revision;EquipmentWheelTravelSeen=Travel;return; }
 // Continue toward the unwrapped wheel destination so rapid same-direction
 // input retains complete laps. Sampling the current pose prevents teleports.
 EquipmentFromAngle=PaintedEquipmentAngle;
 const float Desired=EquippedIndex>=0 ? -EquippedIndex*360.f/Count:PaintedEquipmentAngle;
 if(WheelChanged && EquippedIndex>=0)
 {
  const int64 Steps=Travel-EquipmentWheelTravelSeen;
  float Goal=EquipmentToAngle-float(Steps)*360.f/Count;
  // Non-wheel inventory changes may have happened while this HUD was hidden.
  // Correct their phase without discarding any complete turns from the wheel.
  Goal+=FMath::FindDeltaAngleDegrees(Goal,Desired);
  const float Direction=Actions->EquipmentPreviewDirection()>0 ? -1.f:1.f;
  if((Goal-PaintedEquipmentAngle)*Direction<-.01f)
  {
   float Remaining=FMath::Fmod((Desired-PaintedEquipmentAngle)*Direction,360.f);if(Remaining<0)Remaining+=360.f;
   const int64 FullTurns=FMath::Abs(Steps)/Count;
   Goal=PaintedEquipmentAngle+Direction*(Remaining+float(FullTurns)*360.f);
  }
  EquipmentToAngle=Goal;
 }
 else EquipmentToAngle=PaintedEquipmentAngle+FMath::FindDeltaAngleDegrees(PaintedEquipmentAngle,Desired);
 EquipmentWheelRevisionSeen=Revision;EquipmentWheelTravelSeen=Travel;
 for(int32 I=0;I<Count;++I)EquipmentFromEmphasis[I]=PaintedEquipmentEmphasis[I];
 EquipmentMotionTarget=EquippedIndex;EquipmentMotionStartedAt=Now;
}
void UBBSBWorldMapWidget::UpdateEquipmentMode(bool Bunniar) const
{
 const double Now=FPlatformTime::Seconds();constexpr double Half=.14;
 if(!bEquipmentModeInitialized)
 { bEquipmentModeInitialized=true;bPresentedBunniar=bEquipmentModeTarget=Bunniar;EquipmentModeStartedAt=-1; }
 if(EquipmentModeStartedAt>=0)
 {
  const double Age=Now-EquipmentModeStartedAt;const float T=FMath::Clamp(float(Age/Half),0.f,2.f);
  if(T<1)
  { const float Ease=T*T*(3-2*T);PaintedEquipmentOrbit=FMath::Lerp(EquipmentModeFromOrbit,88.f,Ease);PaintedEquipmentOpacity=FMath::Lerp(EquipmentModeFromOpacity,0.f,Ease); }
  else
  { bPresentedBunniar=bEquipmentModeTarget;const float U=T-1,Ease=U*U*(3-2*U);PaintedEquipmentOrbit=FMath::Lerp(88.f,140.f,Ease);PaintedEquipmentOpacity=Ease; }
  if(Age>=Half*2)EquipmentModeStartedAt=-1;
 }
 if(Bunniar!=bEquipmentModeTarget)
 {
  bEquipmentModeTarget=Bunniar;EquipmentModeFromOrbit=PaintedEquipmentOrbit;EquipmentModeFromOpacity=PaintedEquipmentOpacity;EquipmentModeStartedAt=Now;
 }
}
FVector2D UBBSBWorldMapWidget::EquipmentIconPosition(int32 WheelIndex) const
{
 if(WheelIndex<0 || WheelIndex>=UBBSBFieldActionComponent::EquipmentWheelSlotCount)return FVector2D::ZeroVector;
 const float Angle=FMath::DegreesToRadians(-135.f+WheelIndex*360.f/UBBSBFieldActionComponent::EquipmentWheelSlotCount+PaintedEquipmentAngle);
 return MapAt+MapSize*.5+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*(PaintedEquipmentOrbit*PaintScale);
}
float UBBSBWorldMapWidget::EquipmentIconEmphasis(int32 WheelIndex) const
{ return WheelIndex>=0 && WheelIndex<UBBSBFieldActionComponent::EquipmentWheelSlotCount ? PaintedEquipmentEmphasis[WheelIndex]:0; }
FString UBBSBWorldMapWidget::EquipmentTitle() const
{
 const auto* Player=Map ? Map->MapPlayer():nullptr;const auto* Actions=Player ? Player->FieldActions.Get():nullptr;
 if(!Actions)return FString();
 if(bPresentedBunniar)return Actions->EquipmentLabel(0);
 const int32 PreviewSlot=Actions->EquipmentPreviewSlot();
 const auto* Weapon=Player->Hunting && Actions->EquipmentWheelInstances.IsValidIndex(PreviewSlot) ? Player->Hunting->FindOwnedEntry(Actions->EquipmentWheelInstances[PreviewSlot]):nullptr;
 return Weapon && Weapon->Item ? Weapon->Item->DisplayName.ToString():Actions->EquipmentLabel(Actions->EquippedEquipmentId());
}
void UBBSBWorldMapWidget::PaintEquipmentRing(const FGeometry& G,FSlateWindowElementList& E,int32 Layer) const
{
 const auto* Player=Map ? Map->MapPlayer():nullptr;const auto* Actions=Player ? Player->FieldActions.Get():nullptr;
 if(!Actions)return;
 const auto* Hunting=Player->Hunting.Get();constexpr int32 Count=UBBSBFieldActionComponent::EquipmentWheelSlotCount;
 const int32 EquippedIndex=Actions->EquipmentPreviewSlot();
 UpdateEquipmentMode(Actions->EquippedEquipmentId()==0);
 UpdateEquipmentMotion(EquippedIndex);
 const float S=PaintScale,ModeAlpha=PaintedEquipmentOpacity;const FVector2D Center=MapAt+MapSize*.5;
 const FLinearColor Paper(.97f,.93f,.86f),Ink(.025f,.022f,.035f,.95f),Mint(.48f,.91f,.76f),Rose(.94f,.63f,.74f),Blue(.37f,.84f,1.f);
 const auto Renderer=FSlateApplication::Get().GetRenderer();const auto Transform=G.GetAccumulatedRenderTransform();const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 auto Arc=[&](FVector2D At,float R,float A,float B,FLinearColor Color,float Width,int32 Z)
 {
  if(B<=A)return;
  const int32 Segments=FMath::Max(1,FMath::CeilToInt((B-A)*16));
  TArray<FVector2D> P;for(int32 I=0;I<=Segments;++I) { const float T=FMath::Lerp(A,B,float(I)/Segments);P.Add(At+FVector2D(FMath::Cos(T),FMath::Sin(T))*R); }
  Color.A*=ModeAlpha;FSlateDrawElement::MakeLines(E,Layer+Z,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color,true,Width*S);
 };
 auto Badge=[&](FVector2D At,float R,float Weight,float EdgeFadeFactor)
 {
  R*=ModeAlpha;
  TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
  const FColor Color=FLinearColor(.105f,.085f,.105f,FMath::Lerp(.64f,.94f,Weight)*EdgeFadeFactor*ModeAlpha).ToFColorSRGB();Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform,FVector2f(At),FVector2f(.5,.5),Color));
  for(int32 I=0;I<=32;++I) { const float A=I*2*PI/32;Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform,FVector2f(At+FVector2D(FMath::Cos(A),FMath::Sin(A))*R),FVector2f(.5,.5),Color)); }
  for(int32 I=0;I<32;++I)Indices.Append({SlateIndex(0),SlateIndex(I+1),SlateIndex(I+2)});
  FSlateDrawElement::MakeCustomVerts(E,Layer+12,Renderer->GetResourceHandle(*White),Vertices,Indices,nullptr,0,0);
  Arc(At,R,0,2*PI,FLinearColor(Paper.R,Paper.G,Paper.B,FMath::Lerp(.28f,1.f,Weight)*EdgeFadeFactor),FMath::Lerp(1.f,2.f,Weight),13);
 };
 auto Text=[&](FString Value,FVector2D At,float FontSize,FLinearColor Color,float MaxWidth=144.f,bool Fit=false)
 {
  auto Font=FCoreStyle::GetDefaultFontStyle(TEXT("Bold"),FMath::RoundToInt(FontSize*S));Font.OutlineSettings.OutlineSize=1;Font.OutlineSettings.OutlineColor=Ink;
  const auto Measure=Renderer->GetFontMeasureService();
  if(Fit)while(Font.Size>FMath::RoundToInt(7*S) && Measure->Measure(Value,Font).X>MaxWidth*S)--Font.Size;
  else while(Value.Len()>1 && Measure->Measure(Value,Font).X>MaxWidth*S) { Value.LeftChopInline(Value.EndsWith(TEXT("…")) ? 2:1);Value+=TEXT("…"); }
  const float FitScale=Fit ? FMath::Min(1.f,float(MaxWidth*S/FMath::Max(1.,Measure->Measure(Value,Font).X))):1.f;
  const FVector2D Extent=Measure->Measure(Value,Font)*FitScale;
  At.X=FMath::Clamp(At.X,Extent.X*.5+6*S,G.GetLocalSize().X-Extent.X*.5-6*S);
  At.Y=FMath::Clamp(At.Y,Extent.Y*.5+6*S,G.GetLocalSize().Y-Extent.Y*.5-6*S);
  Color.A*=ModeAlpha;FSlateDrawElement::MakeText(E,Layer+20,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(FitScale,At-Extent*.5)),Value,Font,ESlateDrawEffect::None,Color);
 };
 const FVector2D Focus=Center+FVector2D(-1,-1)*(PaintedEquipmentOrbit*S*FMath::InvSqrt(2.f));
 if(bPresentedBunniar)
 {
  Arc(Center,PaintedEquipmentOrbit*S,0,2*PI,FLinearColor(Mint.R,Mint.G,Mint.B,.34f),1.4f,11);
  Badge(Focus,27*S,1,1);Arc(Focus,30*S*ModeAlpha,0,2*PI,Mint,2,14);
  BBSBItemIcon::DrawSlate(E,G,Layer+17,Hunting ? Hunting->EquippedItemAt(10):nullptr,TEXT("Bunniar"),Focus,18*S*ModeAlpha,ModeAlpha,S,Paper);
  Text(EquipmentTitle(),Focus-FVector2D(0,44*S),11,Paper);return;
 }
 for(int32 WheelIndex=0;WheelIndex<Count;++WheelIndex)
 {
  const int32 Id=Actions->WheelEquipmentId(WheelIndex);const float Weight=EquipmentIconEmphasis(WheelIndex);
  const FVector2D At=EquipmentIconPosition(WheelIndex);const float Angle=FMath::Atan2(float(At.Y-Center.Y),float(At.X-Center.X));
  const FLinearColor Accent=Id==0 ? Mint:Rose;
  Arc(Center,PaintedEquipmentOrbit*S,Angle-FMath::DegreesToRadians(28.f),Angle+FMath::DegreesToRadians(28.f),FMath::Lerp(FLinearColor(Paper.R,Paper.G,Paper.B,.13f),FLinearColor(Accent.R,Accent.G,Accent.B,.55f),Weight),FMath::Lerp(.8f,2.f,Weight),11);
  const float EdgeFade=FMath::Clamp(float(FMath::Min(G.GetLocalSize().X-At.X,G.GetLocalSize().Y-At.Y))/(32*S),0.f,1.f);
  if(EdgeFade<=0)continue;
  if(Id==INDEX_NONE) { Arc(At,8*S,0,2*PI,FLinearColor(Paper.R,Paper.G,Paper.B,.17f*EdgeFade),.8f,13);continue; }
  Badge(At,FMath::Lerp(18.f,27.f,Weight)*S,Weight,EdgeFade);
  const auto* Weapon=Hunting && Actions->EquipmentWheelInstances.IsValidIndex(WheelIndex) ? Hunting->FindOwnedEntry(Actions->EquipmentWheelInstances[WheelIndex]):nullptr;
  if(Id>0 && Weapon && Weapon->Item)
  {
   const bool Maxed=Hunting->IsEcstasyWeapon(Weapon->InstanceId);
   const int64 Required=Hunting->WeaponExperienceToNextLevel(Weapon->InstanceId),XP=Hunting->WeaponExperienceInLevel(Weapon->InstanceId);
   const float Actual=Maxed ? 1.f:Required>0 ? FMath::Clamp(float(XP)/Required,0.f,1.f):0.f;
   const double LevelBase=double(Weapon->Item->WeaponExperienceForLevel(Weapon->WeaponLevel));
   const float Displayed=Maxed ? 1.f:Required>0 ? FMath::Clamp(float((Hunting->DisplayedWeaponEcstatic(Weapon->InstanceId)-LevelBase)/Required),0.f,1.f):0.f;
   const float Radius=FMath::Lerp(21.f,30.f,Weight)*S*ModeAlpha,Alpha=FMath::Lerp(.42f,1.f,Weight)*EdgeFade;
   // Growth follows the weapon's own moving badge, clockwise from twelve o'clock.
   // Cyan gain appears immediately; the rose fill catches up after the reward pause.
   Arc(At,Radius,-HALF_PI,3*HALF_PI,FLinearColor(.24f,.20f,.28f,Alpha),4.f,14);
   Arc(At,Radius,-HALF_PI,-HALF_PI+2*PI*Actual,FLinearColor(Blue.R,Blue.G,Blue.B,Alpha),4.f,15);
   Arc(At,Radius,-HALF_PI,-HALF_PI+2*PI*Displayed,FLinearColor(Rose.R,Rose.G,Rose.B,Alpha),4.f,16);
  }
  else if(Weight>.001f)Arc(At,FMath::Lerp(21.f,30.f,Weight)*S,Angle-FMath::DegreesToRadians(65.f),Angle+FMath::DegreesToRadians(65.f),FLinearColor(Accent.R,Accent.G,Accent.B,Weight*EdgeFade),2.f,14);
  auto* Definition=Hunting ? Hunting->EquippedItemAt(WheelIndex):nullptr;
  const FName Kind=Id==0 ? FName(TEXT("Bunniar")):Definition ? Definition->IconGlyph:FName(TEXT("Sword"));
  BBSBItemIcon::DrawSlate(E,G,Layer+17,Definition,Kind,At-FVector2D(0,7*Weight*S*ModeAlpha),FMath::Lerp(11.5f,14.f,Weight)*S*ModeAlpha,FMath::Lerp(.42f,1.f,Weight)*EdgeFade*ModeAlpha,S*FMath::Lerp(.8f,1.f,Weight),Paper);
  if(Weapon && Weight>.01f && ModeAlpha>.8f)Text(FString::Printf(TEXT("[LV.%d]"),Weapon->WeaponLevel),At+FVector2D(0,13.5f*S*ModeAlpha),8,FLinearColor(Paper.R,Paper.G,Paper.B,Weight*EdgeFade),46,true);
 }
 if(EquippedIndex==INDEX_NONE || !Hunting)return;
 Text(EquipmentTitle(),Focus+FVector2D(0,-44)*S,11,Paper);
 if(Actions->PendingEquipmentId==INDEX_NONE && Actions->GetCharge()>0)Text(FString::Printf(TEXT("차지 %.2f / 2 s"),Actions->WeaponChargeSeconds),Focus+FVector2D(0,-62)*S,9,Rose);
 const FGuid WeaponId=Actions->EquipmentWheelInstances.IsValidIndex(EquippedIndex) ? Actions->EquipmentWheelInstances[EquippedIndex]:FGuid();const auto* Weapon=Hunting->FindOwnedEntry(WeaponId);
 if(Weapon && Weapon->Item && PaintedEquipmentOrbit>137)
 {
  Text(Hunting->IsEcstasyWeapon(WeaponId) ? TEXT("MAX"):FString::Printf(TEXT("%lld/%lld"),Hunting->WeaponExperienceInLevel(WeaponId),Hunting->WeaponExperienceToNextLevel(WeaponId)),Focus+FVector2D(0,42)*S,9,Paper,54,true);
 }
}
