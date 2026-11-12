#include "Interaction/BBSBInteraction.h"
#include "Hunting/BBSBHUDTypography.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "RenderUtils.h"

namespace
{
 double Cross(FVector2D A,FVector2D B,FVector2D C)
 { return (B.X-A.X)*(C.Y-A.Y)-(B.Y-A.Y)*(C.X-A.X); }

 // Triangulate the authored angular balloon and its name ribbon.
 void FillPolygon(UCanvas* Canvas,const TArray<FVector2D>& Points,FLinearColor Color)
 {
  if(Points.Num()<3)return;
  double Area=0;for(int32 I=0;I<Points.Num();++I) { const FVector2D A=Points[I],B=Points[(I+1)%Points.Num()];Area+=A.X*B.Y-B.X*A.Y; }
  const double Direction=Area>=0 ? 1:-1;TArray<int32> Ring;for(int32 I=0;I<Points.Num();++I)Ring.Add(I);
  TArray<FCanvasUVTri> Triangles;
  auto Triangle=[&](FVector2D A,FVector2D B,FVector2D C)
  {
   FCanvasUVTri T;T.V0_Pos=A;T.V1_Pos=B;T.V2_Pos=C;
   T.V0_UV=T.V1_UV=T.V2_UV=FVector2D::ZeroVector;T.V0_Color=T.V1_Color=T.V2_Color=Color;Triangles.Add(T);
  };
  for(int32 Guard=0;Ring.Num()>3 && Guard<Points.Num()*Points.Num();++Guard)
  {
   bool Found=false;
   for(int32 I=0;I<Ring.Num();++I)
   {
    const int32 A=Ring[(I+Ring.Num()-1)%Ring.Num()],B=Ring[I],C=Ring[(I+1)%Ring.Num()];
    if(Cross(Points[A],Points[B],Points[C])*Direction<.001)continue;
    bool Contains=false;
    for(int32 P:Ring)if(P!=A && P!=B && P!=C && Cross(Points[A],Points[B],Points[P])*Direction>=-.001 && Cross(Points[B],Points[C],Points[P])*Direction>=-.001 && Cross(Points[C],Points[A],Points[P])*Direction>=-.001) { Contains=true;break; }
    if(Contains)continue;
    Triangle(Points[A],Points[B],Points[C]);Ring.RemoveAt(I);Found=true;break;
   }
   if(!Found)break;
  }
  if(Ring.Num()==3)Triangle(Points[Ring[0]],Points[Ring[1]],Points[Ring[2]]);
  if(!Triangles.IsEmpty()) { FCanvasTriangleItem Item(Triangles,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item); }
 }
}

float UBBSBInteractionComponent::PresentationIntroSeconds() const
{
 return FMath::Max(.05f,PortraitRiseSeconds)+FMath::Max(.03f,PortraitSettleSeconds)+2.f*FMath::Max(0.f,PortraitStaggerSeconds);
}

float UBBSBInteractionComponent::PortraitEntranceProgress(EBBSBDialogueSpeaker Speaker) const
{
 const float Order=Speaker==EBBSBDialogueSpeaker::Partner ? 0.f:Speaker==EBBSBDialogueSpeaker::Bunniar ? 1.f:2.f;
 const float T=FMath::Clamp((PresentationElapsed-Order*FMath::Max(0.f,PortraitStaggerSeconds))/FMath::Max(.05f,PortraitRiseSeconds),0.f,1.f);
 return 1.f-FMath::Pow(1.f-T,3.f);
}

float UBBSBInteractionComponent::PortraitOvershootOffset(EBBSBDialogueSpeaker Speaker) const
{
 const float Order=Speaker==EBBSBDialogueSpeaker::Partner ? 0.f:Speaker==EBBSBDialogueSpeaker::Bunniar ? 1.f:2.f;
 const float Age=PresentationElapsed-Order*FMath::Max(0.f,PortraitStaggerSeconds);
 if(Age<=0)return 0;
 const float Settle=FMath::Clamp((Age-FMath::Max(.05f,PortraitRiseSeconds))/FMath::Max(.03f,PortraitSettleSeconds),0.f,1.f);
 const float Ease=Settle*Settle*(3.f-2.f*Settle);
 return -FMath::Max(0.f,PortraitOvershootPixels)*PortraitEntranceProgress(Speaker)*(1.f-Ease);
}

float UBBSBInteractionComponent::DialogueCloseLeadSeconds() const
{
 return ClosingBubbleFrom>.02f ? FMath::Max(.03f,BubbleCloseSeconds):0.f;
}

float UBBSBInteractionComponent::DialogueCloseDuration() const
{
 return DialogueCloseLeadSeconds()+FMath::Max(.08f,PortraitExitSeconds)+2.f*FMath::Max(0.f,PortraitStaggerSeconds);
}

float UBBSBInteractionComponent::PortraitExitProgress(EBBSBDialogueSpeaker Speaker) const
{
 if(!bDialogueClosing)return 0;
 const float Order=Speaker==EBBSBDialogueSpeaker::Reparado ? 0.f:Speaker==EBBSBDialogueSpeaker::Bunniar ? 1.f:2.f;
 const float T=FMath::Clamp((DialogueCloseElapsed-DialogueCloseLeadSeconds()-Order*FMath::Max(0.f,PortraitStaggerSeconds))/FMath::Max(.08f,PortraitExitSeconds),0.f,1.f);
 return T*T;
}

float UBBSBInteractionComponent::BubbleRevealProgress() const
{
 if(bDialogueClosing)
 {
  const float T=FMath::Clamp(DialogueCloseElapsed/FMath::Max(.03f,BubbleCloseSeconds),0.f,1.f);
  return ClosingBubbleFrom*(1.f-T*T*(3.f-2.f*T));
 }
 if(PresentationElapsed<PresentationIntroSeconds())return 0;
 const float T=FMath::Clamp(BubbleRevealElapsed/FMath::Max(.05f,BubbleGrowSeconds),0.f,1.f);
 return 1.f-FMath::Pow(1.f-T,3.f);
}

void UBBSBInteractionComponent::UpdatePresentation(float DT,bool bImmediate,bool bNewSpeaker)
{
 UpdateBunniarPortraitAttachment(DT,bImmediate);
 const FVector Lower(ActiveSpeaker==EBBSBDialogueSpeaker::Reparado ? 0.f:1.f,
  ActiveSpeaker==EBBSBDialogueSpeaker::Bunniar ? 0.f:1.f,ActiveSpeaker==EBBSBDialogueSpeaker::Partner ? 0.f:1.f);
 if(bImmediate) { PresentationElapsed=BubbleRevealElapsed=0;PortraitLowering=Lower;return; }
 if(bNewSpeaker)BubbleRevealElapsed=0;
 const float Before=PresentationElapsed;PresentationElapsed+=FMath::Max(0.f,DT);
 // Count only the part of a frame after all portraits have finished arriving.
 BubbleRevealElapsed+=FMath::Max(0.f,PresentationElapsed-PresentationIntroSeconds())-FMath::Max(0.f,Before-PresentationIntroSeconds());
 PortraitLowering=FMath::Lerp(PortraitLowering,Lower,1.f-FMath::Exp(-FMath::Max(0.f,DT)*18.f));
}

void UBBSBInteractionComponent::DrawDialogue(AHUD* HUD,UCanvas* Canvas) const
{
 if(!bDialogueActive || !IsValid(ActiveNPC) || !HUD || !Canvas)return;
 const float W=Canvas->ClipX,H=Canvas->ClipY,S=FMath::Clamp(FMath::Min(W/1280.f,H/720.f),.5f,1.8f);
 const FLinearColor Paper(.96f,.92f,.83f),Ink(.075f,.048f,.065f),Rose(.48f,.22f,.31f),Shadow(.028f,.019f,.027f,.95f);
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 auto Cutout=[&](EBBSBDialogueSpeaker Speaker,float X,float Y,float Height,float Bright,float Rotation=0.f,float Facing=1.f)
 {
  UTexture2D* Texture=PortraitForSpeaker(Speaker);if(!Texture)return;
  const float Width=Height*float(Texture->GetSizeX())/FMath::Max(1,Texture->GetSizeY());
  // Every source asset faces screen-right. A right-side partner faces inward
  // by reversing only the horizontal UVs, keeping a single editable portrait.
  const bool Mirror=(Speaker==EBBSBDialogueSpeaker::Partner) != (Facing<0.f);
  // Turn about the portrait's centre: narrow to an edge, reverse UVs there,
  // then open to the other side without moving its attachment/recall anchor.
  const float DrawWidth=Width*FMath::Max(.015f,FMath::Abs(Facing));
  HUD->DrawTexture(Texture,X+(Width-DrawWidth)*.5f,Y,DrawWidth,Height,Mirror ? 1.f:0.f,0,Mirror ? -1.f:1.f,1,FLinearColor(Bright,Bright,Bright,1),BLEND_Translucent,1,false,Rotation,FVector2D(.5,.5));
 };
 auto EntryY=[&](EBBSBDialogueSpeaker Speaker,float RestY,float Lower)
 { return RestY+(H-RestY+24*S)*(1.f-PortraitEntranceProgress(Speaker))+PortraitOvershootOffset(Speaker)*S+Lower*InactivePortraitDrop*S; };
 auto RiseY=[&](EBBSBDialogueSpeaker Speaker,float RestY,float Lower)
 { return FMath::Lerp(EntryY(Speaker,RestY,Lower),H+24*S,PortraitExitProgress(Speaker)); };
 const float HeroHeight=500*S,HeroX=-32*S,HeroY=RiseY(EBBSBDialogueSpeaker::Reparado,H-HeroHeight+32*S,PortraitLowering.X);
 const float Hover=GetWorld() ? FMath::Sin(GetWorld()->GetTimeSeconds()*1.8f)*3*S:0;
 const float HomeBuddyHeight=174*S,HomeBuddyX=240*S,HomeBuddyY=RiseY(EBBSBDialogueSpeaker::Bunniar,H-545*S+Hover,PortraitLowering.Y);
 const float PartnerHeight=345*S,PartnerX=W-270*S,PartnerY=RiseY(EBBSBDialogueSpeaker::Partner,H-PartnerHeight+25*S,PortraitLowering.Z);
 const float PartnerContactY=bDialogueClosing ? EntryY(EBBSBDialogueSpeaker::Partner,H-PartnerHeight+25*S,PortraitLowering.Z):PartnerY;
 const float A=FMath::Clamp(BunniarPortraitAttachmentAlpha,0.f,1.f),Attachment=A*A*(3.f-2.f*A);
 UTexture2D* PartnerTexture=PortraitForSpeaker(EBBSBDialogueSpeaker::Partner);
 const float PartnerAspect=PartnerTexture ? float(PartnerTexture->GetSizeX())/FMath::Max(1,PartnerTexture->GetSizeY()):.75f;
 const FVector2D Contact=BunniarAttachmentPortraitUV();
 const float AttachedHeight=HomeBuddyHeight*AttachedBunniarPortraitScale;
 const float AttachedX=PartnerX+Contact.X*PartnerHeight*PartnerAspect-AttachedHeight*.5f;
 const float AttachedY=RiseY(EBBSBDialogueSpeaker::Bunniar,PartnerContactY+Contact.Y*PartnerHeight-AttachedHeight*.5f,0);
 const float BuddyHeight=FMath::Lerp(HomeBuddyHeight,AttachedHeight,Attachment),BuddyX=FMath::Lerp(HomeBuddyX,AttachedX,Attachment);
 const float BuddyY=FMath::Lerp(HomeBuddyY,AttachedY,Attachment)-FMath::Sin(A*PI)*28*S*(1.f-PortraitExitProgress(EBBSBDialogueSpeaker::Bunniar));
 const float AttachedTilt=FMath::Clamp(float(-BunniarPortraitLocalNormal.Z*20-BunniarPortraitLocalNormal.X*9+BunniarPortraitLocalNormal.Y*6),-22.f,22.f);
 Cutout(EBBSBDialogueSpeaker::Reparado,HeroX,HeroY,HeroHeight,PortraitBrightness.X);
 Cutout(EBBSBDialogueSpeaker::Partner,PartnerX,PartnerY,PartnerHeight,PortraitBrightness.Z);
 // Bunniar remains legible over the partner, including a contact on the far side.
 Cutout(EBBSBDialogueSpeaker::Bunniar,BuddyX,BuddyY,BuddyHeight,PortraitBrightness.Y,FMath::Lerp(-3.f,AttachedTilt,Attachment),BunniarPortraitFacing);
 const float Reveal=BubbleRevealProgress();if(Reveal<.02f)return;

 const float X=310*S,Width=W-X-245*S,Padding=32*S,LineHeight=32*S,Pixels=22*S;
 const FSlateFontInfo Font=BBSBHUDTypography::NumberFont(Pixels);TArray<FString> Rows;FString Row;
 for(TCHAR Character:DisplayedLine())
 {
  if(Character=='\r')continue;
  if(Character=='\n') { Rows.Add(Row);Row.Reset();continue; }
  if(!Row.IsEmpty() && Measure->Measure(Row+FString::Chr(Character),Font).X>Width-Padding*2) { Rows.Add(Row);Row.Reset(); }
  if(Row.IsEmpty() && Character==' ')continue;Row.AppendChar(Character);
 }
 if(!Row.IsEmpty() || Rows.IsEmpty())Rows.Add(Row);
 const float Height=FMath::Max(170*S,90*S+Rows.Num()*LineHeight),Y=H-Height-30*S;
 TArray<FVector2D> Body={{X+12*S,Y+17*S},{X+Width-28*S,Y-12*S},{X+Width+9*S,Y+Height-23*S},{X+Width-30*S,Y+Height+8*S},{X+28*S,Y+Height-7*S},{X-6*S,Y+Height-34*S}};

 // Splice the pointer into the actual body edge. A single silhouette avoids
 // a detached triangle, an internal base line, or overlapping shadow seams.
 FVector2D SpeakerPoint;int32 TailEdge=0;float TailAt=0;
 switch(CurrentSpeaker())
 {
 case EBBSBDialogueSpeaker::Reparado:SpeakerPoint=FVector2D(206*S,HeroY+HeroHeight*.38f);TailEdge=5;TailAt=.58f;break;
 case EBBSBDialogueSpeaker::Bunniar:SpeakerPoint=FVector2D(BuddyX+BuddyHeight*.56f,BuddyY+BuddyHeight*.70f);TailEdge=0;TailAt=FMath::Lerp(.36f,.92f,Attachment);break;
 default:SpeakerPoint=FVector2D(W-125*S,PartnerY+PartnerHeight*.34f);TailEdge=1;TailAt=.42f;break;
 }
 const FVector2D EdgeStart=Body[TailEdge],EdgeEnd=Body[(TailEdge+1)%Body.Num()],AlongEdge=(EdgeEnd-EdgeStart).GetSafeNormal();
 const FVector2D Base=FMath::Lerp(EdgeStart,EdgeEnd,TailAt),Outward(AlongEdge.Y,-AlongEdge.X);
 FVector2D Direction=(SpeakerPoint-Base).GetSafeNormal();
 const double OutwardAmount=FVector2D::DotProduct(Direction,Outward);
 if(OutwardAmount<.35)Direction=(Direction+Outward*(.35-OutwardAmount)).GetSafeNormal();
 const FVector2D Tip=Base+Direction*FMath::Clamp(float(FVector2D::Distance(Base,SpeakerPoint))-70.f*S,24.f*S,64.f*S);
 const float RootHalf=FMath::Min(15.f*S,float(FVector2D::Distance(EdgeStart,EdgeEnd))*.18f);
 TArray<FVector2D> Balloon;Balloon.Reserve(Body.Num()+3);
 for(int32 I=0;I<Body.Num();++I)
 {
  Balloon.Add(Body[I]);
  if(I==TailEdge) { Balloon.Add(Base-AlongEdge*RootHalf);Balloon.Add(Tip);Balloon.Add(Base+AlongEdge*RootHalf); }
 }
 // Grow from this speaker's pointer. Geometry, text and the hard shadow share
 // the same transform, so the balloon remains one piece during its entrance.
 auto Transform=[&](FVector2D P) { return Tip+(P-Tip)*Reveal; };
 auto Polygon=[&](const TArray<FVector2D>& Points,FVector2D Offset=FVector2D::ZeroVector)
 { TArray<FVector2D> Result;Result.Reserve(Points.Num());for(FVector2D P:Points)Result.Add(Transform(P+Offset));return Result; };
 auto Outline=[&](const TArray<FVector2D>& Points,FLinearColor Color,float Width)
 { const auto P=Polygon(Points);for(int32 I=0;I<P.Num();++I)HUD->DrawLine(P[I].X,P[I].Y,P[(I+1)%P.Num()].X,P[(I+1)%P.Num()].Y,Color,Width*S*Reveal); };
 auto Shape=[&](const TArray<FVector2D>& Points)
 { FillPolygon(Canvas,Polygon(Points),Paper);Outline(Points,Ink,2.5f); };
 auto Text=[&](const FString& Value,FLinearColor Color,float PX,float PY,float Size)
 { FCanvasTextItem Item(Transform(FVector2D(PX,PY)),FText::FromString(Value),BBSBHUDTypography::NumberFont(Size*Reveal),Color);Canvas->DrawItem(Item); };
 const FVector2D ShadowOffset(10*S,12*S);
 FillPolygon(Canvas,Polygon(Balloon,ShadowOffset),Shadow);Shape(Balloon);
 const FString Name=DisplayedSpeakerName().ToString();const FVector2D NameSize=Measure->Measure(Name,BBSBHUDTypography::NumberFont(18*S));
 const float NameWidth=FMath::Max(140*S,float(NameSize.X+44*S)),NameBottom=Y+17*S,NameTextY=NameBottom-float(NameSize.Y)-7*S;
 // The upper edge keeps a slight angle; the text and lower edge stay level.
 const TArray<FVector2D> NameTag={{X+20*S,NameTextY-7*S},{X+NameWidth,NameTextY-11*S},{X+NameWidth+18*S,NameBottom},{X+27*S,NameBottom}};
 FillPolygon(Canvas,Polygon(NameTag,ShadowOffset),Shadow);Shape(NameTag);Text(Name,Ink,X+39*S,NameTextY,18*S);
 const FVector2D RuleStart=Transform(FVector2D(X+Padding,Y+36*S)),RuleEnd=Transform(FVector2D(X+Width-Padding,Y+26*S));
 HUD->DrawLine(RuleStart.X,RuleStart.Y,RuleEnd.X,RuleEnd.Y,Rose,1.5*S*Reveal);
 for(int32 I=0;I<Rows.Num();++I)Text(Rows[I],Ink,X+Padding,Y+49*S+I*LineHeight,Pixels);
 Text(TEXT("Enter / 클릭 / F   다음"),FLinearColor(.22f,.16f,.20f),X+Padding,H-62*S,12*S);
 const FString Close=TEXT("Backspace   닫기");const float CloseWidth=Measure->Measure(Close,BBSBHUDTypography::NumberFont(12*S)).X;
 Text(Close,FLinearColor(.22f,.16f,.20f),X+Width-Padding-CloseWidth,H-62*S,12*S);
}
