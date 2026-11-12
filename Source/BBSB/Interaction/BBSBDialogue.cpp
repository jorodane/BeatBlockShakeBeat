#include "Interaction/BBSBInteraction.h"
#include "BBSBCharacter.h"
#include "Caravan/BBSBCaravan.h"
#include "Field/BBSBFieldActions.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

int32 ABBSBInteractionNPC::DialogueLineCount() const
{
 return Conversation.IsEmpty() ? Lines.Num():Conversation.Num();
}

FBBSBDialogueLine ABBSBInteractionNPC::DialogueLineAt(int32 Index) const
{
 if(!Conversation.IsEmpty()) return Conversation.IsValidIndex(Index) ? Conversation[Index]:FBBSBDialogueLine();
 FBBSBDialogueLine Line;
 if(!Lines.IsValidIndex(Index)) return Line;
 FString Text=Lines[Index].ToString();
 // Keep authored legacy conversations readable, including their existing speaker prefixes.
 const int32 Colon=Text.Find(TEXT(":"));
 if(Colon!=INDEX_NONE)
 {
  const FString Name=Text.Left(Colon).TrimStartAndEnd();
  if(Name==TEXT("레파라도") || Name.Equals(TEXT("Reparado"),ESearchCase::IgnoreCase))Line.Speaker=EBBSBDialogueSpeaker::Reparado;
  else if(Name==TEXT("버니아") || Name.Equals(TEXT("Bunniar"),ESearchCase::IgnoreCase))Line.Speaker=EBBSBDialogueSpeaker::Bunniar;
  if(Line.Speaker!=EBBSBDialogueSpeaker::Partner)Text=Text.Mid(Colon+1).TrimStart();
 }
 Line.Text=FText::FromString(Text);return Line;
}

EBBSBDialogueSpeaker UBBSBInteractionComponent::CurrentSpeaker() const
{
 return bDialogueActive && IsValid(ActiveNPC) ? ActiveNPC->DialogueLineAt(LineIndex).Speaker:EBBSBDialogueSpeaker::Partner;
}

EBBSBDialogueExpression UBBSBInteractionComponent::CurrentExpression() const
{
 return bDialogueActive && IsValid(ActiveNPC) ? ActiveNPC->DialogueLineAt(LineIndex).Expression:EBBSBDialogueExpression::Neutral;
}

EBBSBDialogueExpression UBBSBInteractionComponent::ExpressionForSpeaker(EBBSBDialogueSpeaker Speaker) const
{
 switch(Speaker)
 {
 case EBBSBDialogueSpeaker::Reparado:return ReparadoExpression;
 case EBBSBDialogueSpeaker::Bunniar:return BunniarExpression;
 default:return PartnerExpression;
 }
}

FText UBBSBInteractionComponent::DisplayedSpeakerName() const
{
 switch(CurrentSpeaker())
 {
 case EBBSBDialogueSpeaker::Reparado:return FText::FromString(TEXT("레파라도"));
 case EBBSBDialogueSpeaker::Bunniar:return FText::FromString(TEXT("버니아"));
 default:return IsValid(ActiveNPC) ? ActiveNPC->DisplayName:FText::GetEmpty();
 }
}

UTexture2D* UBBSBInteractionComponent::PortraitForSpeaker(EBBSBDialogueSpeaker Speaker) const
{
 const EBBSBDialogueExpression Expression=ExpressionForSpeaker(Speaker);
 auto Resolve=[&](const TMap<EBBSBDialogueExpression,TObjectPtr<UTexture2D>>& Choices)->UTexture2D*
 {
  if(const auto* Texture=Choices.Find(Expression);Texture && *Texture)return Texture->Get();
  if(const auto* Texture=Choices.Find(EBBSBDialogueExpression::Neutral);Texture && *Texture)return Texture->Get();
  return nullptr;
 };
 switch(Speaker)
 {
 case EBBSBDialogueSpeaker::Reparado:if(auto* Texture=Resolve(ReparadoExpressions))return Texture;return ReparadoPortrait;
 case EBBSBDialogueSpeaker::Bunniar:if(auto* Texture=Resolve(BunniarExpressions))return Texture;return BunniarPortrait;
 default:
  if(IsValid(ActiveNPC)) { if(auto* Texture=Resolve(ActiveNPC->PortraitExpressions))return Texture;if(ActiveNPC->Portrait)return ActiveNPC->Portrait; }
  if(auto* Texture=Resolve(DefaultPartnerExpressions))return Texture;return DefaultPartnerPortrait;
 }
}

void UBBSBInteractionComponent::UpdateSpeaker(float DT,bool bImmediate)
{
 const EBBSBDialogueSpeaker PreviousSpeaker=ActiveSpeaker;
 ActiveSpeaker=CurrentSpeaker();
 UpdatePresentation(DT,bImmediate,ActiveSpeaker!=PreviousSpeaker);
 switch(ActiveSpeaker)
 {
 case EBBSBDialogueSpeaker::Reparado:ReparadoExpression=CurrentExpression();break;
 case EBBSBDialogueSpeaker::Bunniar:BunniarExpression=CurrentExpression();break;
 default:PartnerExpression=CurrentExpression();break;
 }
 const FVector Target(ActiveSpeaker==EBBSBDialogueSpeaker::Reparado ? 1.f:.4f,
  ActiveSpeaker==EBBSBDialogueSpeaker::Bunniar ? 1.f:.4f,ActiveSpeaker==EBBSBDialogueSpeaker::Partner ? 1.f:.4f);
 PortraitBrightness=bImmediate ? Target:FMath::Lerp(PortraitBrightness,Target,1.f-FMath::Exp(-FMath::Max(0.f,DT)*16.f));
}

FString UBBSBInteractionComponent::DisplayedLine() const
{
 if(!bDialogueActive || !IsValid(ActiveNPC) || LineIndex<0 || LineIndex>=ActiveNPC->DialogueLineCount()) return FString();
 const FBBSBDialogueLine Turn=ActiveNPC->DialogueLineAt(LineIndex);const FString Line=Turn.Text.ToString();
 if(!ActiveNPC->bFaceCovered || Turn.Speaker!=EBBSBDialogueSpeaker::Partner) return Line;
 FString Mask;
 for(int32 I=0;I<Line.Len();++I)
 {
  const TCHAR C=Line[I];
  if(C=='\n' || C=='\r') Mask.AppendChar(C);
  else if(C>=0xDC00 && C<=0xDFFF && I>0 && Line[I-1]>=0xD800 && Line[I-1]<=0xDBFF) continue;
  else Mask.AppendChar(TEXT('.'));
 }
 return Mask;
}

void UBBSBInteractionComponent::UpdateCamera()
{
 if(!DialogueCamera || !IsValid(ActiveNPC) || !Player) return;
 auto* PC=Cast<APlayerController>(Player->GetController());if(!PC)return;
 int32 W=0,H=0;PC->GetViewportSize(W,H);const float Aspect=H>0 ? float(W)/H:16.f/9.f;
 const FVector Hero=Player->GetActorLocation(),Partner=ActiveNPC->GetActorLocation();
 // Root anchors keep idle head animation from moving the conversation camera.
 if(bFramingReady && FMath::Abs(Aspect-FramingAspect)<.02f && Hero.Equals(CameraPlayerAnchor,15) && Partner.Equals(CameraPartnerAnchor,15)) return;
 const FVector HeroHead=Player->GetMesh()->GetSocketLocation(TEXT("head"))+FVector(0,0,10),PartnerHead=ActiveNPC->HeadPoint()+FVector(0,0,10);
 const FVector Points[]={HeroHead+FVector(0,0,18),HeroHead-FVector(0,0,65),PartnerHead+FVector(0,0,15),PartnerHead-FVector(0,0,65)};
 const FVector Mid=(Points[0]+Points[1]+Points[2]+Points[3])*.25;
 FVector Along=(Partner-Hero).GetSafeNormal2D();if(Along.IsNearlyZero())Along=Player->GetActorRightVector();
 const FVector Preferred(Along.Y,-Along.X,0);
 const float FOV=FMath::Clamp(CameraFOV,35.f,70.f),TanH=FMath::Tan(FMath::DegreesToRadians(FOV*.5f)),TanV=TanH/FMath::Max(.6f,Aspect);
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBDialogueTwoShot),true,ActiveNPC);Q.AddIgnoredActor(Player);Q.AddIgnoredActor(DialogueCamera);
 if(Player->FieldActions)Q.AddIgnoredActor(Player->FieldActions->BunniarEquipment);
 FCollisionQueryParams Solid=Q;Solid.bTraceComplex=false;
 FVector OldEye;FRotator OldLook;PC->GetPlayerViewPoint(OldEye,OldLook);
 FVector BestEye=OldEye,BestFocus=Mid;float BestFOV=FOV;double BestScore=-DBL_MAX;
 for(int32 Side:{1,-1})for(float Turn:{0.f,-20.f,20.f,-40.f,40.f,-60.f,60.f,-80.f,80.f})for(float WantedElevation:{0.f,5.f,9.f,12.f})
 {
  const float Elevation=FMath::Min(WantedElevation,FMath::Clamp(CameraMaximumPitch,0.f,12.f));
  const FVector Out=(Preferred*Side).RotateAngleAxis(Turn,FVector::UpVector);
  const FVector Forward=(-Out*FMath::Cos(FMath::DegreesToRadians(Elevation))-FVector::UpVector*FMath::Sin(FMath::DegreesToRadians(Elevation))).GetSafeNormal();
  const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward).GetSafeNormal(),Up=FVector::CrossProduct(Forward,Right);
  double Distance=FMath::Max(230.f,CameraDistance)+FMath::Max(0.f,CameraSideOffset)*.25;
  for(const FVector& Point:Points)
  {
   const FVector Delta=Point-Mid;const double Depth=FVector::DotProduct(Delta,Forward);
   Distance=FMath::Max(Distance,FMath::Max(FMath::Abs(FVector::DotProduct(Delta,Right))/(TanH*.76)-Depth,FMath::Abs(FVector::DotProduct(Delta,Up))/(TanV*.48)-Depth));
  }
  // The people sit above the dialogue panel, with room around both heads.
  const FVector Focus=Mid-Up*(.23*Distance*TanV);
  const FVector Eye=Focus-Forward*Distance;
  // The midpoint can be inside a shop counter even while both people stand in
  // valid space. Sweeping outward from it would put the lens inside the counter.
  // Test the lens volume itself and visibility from each participant instead.
  if(GetWorld()->OverlapBlockingTestByChannel(Eye,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(14),Solid))continue;
  double Score=100-FMath::Abs(Turn)*.1+(Side==1 ? 10:0);
  for(const FVector Head:{HeroHead,PartnerHead})
  {
   FHitResult Block;Score+=GetWorld()->LineTraceSingleByChannel(Block,Eye,Head,ECC_Visibility,Q) ? -3000:1000;
   Score+=GetWorld()->LineTraceSingleByChannel(Block,Eye,Head-FVector(0,0,24),ECC_Visibility,Q) ? -80:80;
  }
  // Prefer the eyes' height and side-to-side separation over a top-down solution.
  Score-=FMath::Abs(Eye.Z-((HeroHead.Z+PartnerHead.Z)*.5+10))*.35;
  const FVector A=HeroHead-Eye,B=PartnerHead-Eye;
  const double ScreenGap=FMath::Abs(FVector::DotProduct(A,Right)/FMath::Max(40.,FVector::DotProduct(A,Forward))-FVector::DotProduct(B,Right)/FMath::Max(40.,FVector::DotProduct(B,Forward)))/TanH*.5;
  Score+=FMath::Min(.35,ScreenGap)*90;
  Score-=FVector::Dist(Eye,OldEye)*.015;
  if(Score>BestScore) { BestScore=Score;BestEye=Eye;BestFocus=Focus;BestFOV=FOV; }
 }
 FRotator Look=(BestFocus-BestEye).Rotation();const float Limit=FMath::Clamp(CameraMaximumPitch,0.f,12.f);Look.Pitch=FMath::Clamp(Look.Pitch,-Limit,Limit);
 DialogueCamera->SetActorLocationAndRotation(BestEye,Look);
 auto* Camera=DialogueCamera->GetCameraComponent();Camera->bConstrainAspectRatio=false;Camera->SetFieldOfView(BestFOV);
 CameraPlayerAnchor=Hero;CameraPartnerAnchor=Partner;FramingAspect=Aspect;bFramingReady=true;
}

