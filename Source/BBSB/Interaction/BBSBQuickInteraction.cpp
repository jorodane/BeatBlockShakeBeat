#include "Interaction/BBSBInteraction.h"
#include "Interaction/BBSBInteractionDoor.h"
#include "BBSBCharacter.h"
#include "Field/BBSBFieldActions.h"
#include "Hunting/BBSBLootPickup.h"
#include "Hunting/BBSBItem.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBEquipmentGrowth.h"
#include "Combat/BBSBCombat.h"
#include "Caravan/BBSBCaravan.h"
#include "Puzzle/BBSBPuzzleField.h"
#include "Components/PrimitiveComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Stage01/BBSBStage01.h"
#include "Character/BBSBRespawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"

void UBBSBInteractionComponent::RegisterInteractionCandidate(AActor* Actor)
{
 if(IsValid(Actor) && (BBSBBunniarSurface::Resolve(Actor) || Actor->IsA<ABBSBLootPickup>()))InteractionCandidates.Add(Actor);
}
bool UBBSBInteractionComponent::IsQuickInteractable(AActor* A) const
{
 if(!IsValid(A) || A==GetOwner() || A->IsHidden() || A->ActorHasTag(TEXT("BBSBIgnoreQuickInteraction")))return false;
 if(const auto* L=Cast<ABBSBLootPickup>(A))return L->IsAvailable() && !L->Item->IsFieldResource();
 if(const auto* O=Cast<ABBSBCarryObject>(A);O && O->bBroken)return false;
 return BBSBBunniarSurface::Resolve(A)!=nullptr;
}
FText UBBSBInteractionComponent::QuickInteractionName(AActor* A) const
{
 if(!IsQuickInteractable(A))return FText();
 if(const auto* L=Cast<ABBSBLootPickup>(A))return L->Item->DisplayName;
 if(const auto* N=Cast<ABBSBInteractionNPC>(A))return N->DisplayName;
 if(const auto* P=Cast<ABBSBRespawnPoint>(A))return P->PointName;
 if(const auto* D=Cast<ABBSBInteractionDoor>(A))return D->DoorName;
 if(const auto* P=Cast<ABBSBPuzzleStation>(A))return FText::FromString(P->Title());
 if(auto* Handler=BBSBBunniarSurface::Resolve(A))
 {
  if(const auto* D=Cast<ABBSBStage01Device>(Handler))return D->Title;
  if(const auto* D=Cast<ABBSBBunniarDevice>(Handler))return FText::FromString(D->DeviceLabel);
 }
 if(A->IsA<ABBSBStage01CarrySinger>())return FText::FromString(TEXT("버니아"));
 if(A->IsA<ABBSBCarryObject>())return FText::FromString(TEXT("상자"));
 return FText::FromString(TEXT("사물"));
}
FText UBBSBInteractionComponent::QuickInteractionText(AActor* A) const
{
 if(!IsQuickInteractable(A))return FText();
 if(A->IsA<ABBSBLootPickup>())return FText::FromString(TEXT("줍기"));
 if(A->IsA<ABBSBInteractionNPC>())return FText::FromString(TEXT("대화하기"));
 if(A->IsA<ABBSBRespawnPoint>())return FText::FromString(TEXT("쉬기"));
 if(const auto* D=Cast<ABBSBInteractionDoor>(A))return FText::FromString(D->bOpen ? TEXT("닫기"):TEXT("열기"));
 if(A->IsA<ABBSBCarryObject>())return FText::FromString(TEXT("옮기기"));
 if(const auto* D=Cast<ABBSBStage01Device>(BBSBBunniarSurface::Resolve(A)))
 {
  switch(D->Kind)
  {
  case EBBSBStage01Node::CabinDoor:case EBBSBStage01Node::CargoLever:case EBBSBStage01Node::LightLever:case EBBSBStage01Node::ReadyGate:return FText::FromString(TEXT("움직이기"));
  case EBBSBStage01Node::LowerLadder:return FText::FromString(D->bSolved ? TEXT("잡기"):TEXT("내리기"));
  case EBBSBStage01Node::ClimbLadder:return FText::FromString(TEXT("잡기"));
  case EBBSBStage01Node::Trapdoor:return FText::FromString(D->bSolved ? TEXT("올라가기"):TEXT("움직이기"));
  case EBBSBStage01Node::ToolCache:return FText::FromString(TEXT("챙기기"));
  case EBBSBStage01Node::CaptainNote:return FText::FromString(TEXT("읽기"));
  case EBBSBStage01Node::BuyTea:case EBBSBStage01Node::BuyParts:return FText::FromString(TEXT("구매하기"));
  case EBBSBStage01Node::TuneReward:return FText::FromString(TEXT("교환하기"));
  case EBBSBStage01Node::Departure:return FText::FromString(TEXT("출발하기"));
  default:return FText::FromString(TEXT("작동하기"));
  }
 }
 return FText::FromString(TEXT("조작하기"));
}
bool UBBSBInteractionComponent::FindQuickInteraction(FHitResult& Out) const
{
 Out=FHitResult();if(!Player || !Player->FieldActions || bDialogueActive || Player->Combat->bDead || Player->FieldActions->bEquipmentInputBlocked || Player->FieldActions->bEquipmentWheelOpen)return false;
 const auto* Actions=Player->FieldActions.Get();const bool Locked=Player->Combat->HasTargetLock();
 auto DepthAllowed=[&](AActor* Target)
 { return Target && (Target->ActorHasTag(TEXT("BBSBPhysicalInteraction")) || Actions->IsAimPointDepthAllowed(Target->GetActorLocation())); };
 auto Search=[&](FVector Origin,FVector Direction,FVector TraceOrigin,float Range,float Degrees,bool Camera,FHitResult& Result)
 {
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBQuickInteraction),true,Player);Query.AddIgnoredActor(Actions->BunniarEquipment);
  if(Camera)for(const auto& Weak:InteractionCandidates)if(auto* Target=Weak.Get();Target && !DepthAllowed(Target))Query.AddIgnoredActor(Target);
  if(Camera)
  {
   FHitResult Direct;
   if(GetWorld()->LineTraceSingleByChannel(Direct,TraceOrigin,Origin+Direction*(Range+1500),ECC_Visibility,Query) && IsQuickInteractable(Direct.GetActor()) &&
      DepthAllowed(Direct.GetActor()) && Actions->IsAimPointDepthAllowed(Direct.ImpactPoint) && FVector::DistSquared(Player->GetActorLocation(),Direct.ImpactPoint)<=FMath::Square(Range))
   { Result=Direct;return true; }
  }
  float Best=MAX_flt;
  for(const auto& Weak:InteractionCandidates)
  {
   auto* Target=Weak.Get();if(!IsQuickInteractable(Target) || (Camera && !DepthAllowed(Target)))continue;
   TInlineComponentArray<UPrimitiveComponent*> Parts(Target);
   for(auto* Part:Parts)
   {
    if(!Part->IsRegistered() || Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision || Part->GetCollisionResponseToChannel(ECC_Visibility)!=ECR_Block)continue;
    const FBox Box=Part->Bounds.GetBox();if(Box.ComputeSquaredDistanceToPoint(Player->GetActorLocation())>FMath::Square(Range))continue;
    const float AlongBox=FMath::Clamp(float(FVector::DotProduct(Part->Bounds.Origin-Origin,Direction)),45.f,Range);
    const FVector Ideal=Origin+Direction*AlongBox;
    TArray<FVector,TInlineAllocator<8>> Points;Points.Add(Box.GetClosestPointTo(Ideal));Points.Add(Part->Bounds.Origin);
    FVector Contact;if(!Part->IsA<UInstancedStaticMeshComponent>() && Part->GetClosestPointOnCollision(Ideal,Contact)>=0.f)Points.Add(Contact);
    // A ladder is thin rails and rungs, not a filled collision box. Try its nearest real pieces.
    if(auto* Instances=Cast<UInstancedStaticMeshComponent>(Part))
    {
     TArray<TPair<float,FVector>> Nearest;
     for(int32 I=0;I<Instances->GetInstanceCount();++I)
     {
      FTransform Frame;if(!Instances->GetInstanceTransform(I,Frame,true))continue;
      const FVector P=Frame.GetLocation();const float Distance=FVector::DistSquared(P,Origin);if(Distance>FMath::Square(Range))continue;
      Nearest.Emplace(Distance,P);
     }
     Nearest.Sort([](const auto& A,const auto& B){return A.Key<B.Key;});
     for(int32 I=0;I<FMath::Min(3,Nearest.Num());++I)Points.Add(Nearest[I].Value);
    }
    for(const FVector& Point:Points)
    {
     if((Camera && !Actions->IsAimPointDepthAllowed(Point)) || FVector::DistSquared(Point,Player->GetActorLocation())>FMath::Square(Range))continue;
     const FVector Delta=Point-Origin;const float Along=FVector::DotProduct(Delta,Direction);if(Along<=0.f)continue;
     const float Perp=(Delta-Direction*Along).Size();const float Radius=FMath::Clamp(Part->Bounds.SphereRadius,12.f,120.f);
     const float Angle=FMath::RadiansToDegrees(FMath::Atan2(FMath::Max(0.f,Perp-Radius),Along));
     if(Angle>Degrees || (Camera && Perp>QuickAimRadius+Radius))continue;
     FHitResult Hit;const FVector End=Point+(Point-TraceOrigin).GetSafeNormal()*4.f;
     if(!GetWorld()->LineTraceSingleByChannel(Hit,TraceOrigin,End,ECC_Visibility,Query) || !IsQuickInteractable(Hit.GetActor()) ||
       (Hit.GetActor()!=Target && BBSBBunniarSurface::Resolve(Hit.GetActor())!=BBSBBunniarSurface::Resolve(Target)) ||
       (Camera && !Actions->IsAimPointDepthAllowed(Hit.ImpactPoint)))continue;
     const float Score=Camera ? Angle*100.f+Along*.015f:Along+Perp*.7f;
     if(Score<Best) { Best=Score;Result=Hit; }
    }
   }
  }
  return Result.GetActor()!=nullptr;
 };
 if(!Locked)
 {
  FVector Origin,Direction,TraceOrigin,TraceDirection;
  if(Actions->CursorAimRay(Origin,Direction) && Actions->TargetingAimRay(TraceOrigin,TraceDirection) &&
     Search(Origin,Direction,TraceOrigin,Actions->WeaponReach,QuickAimDegrees,true,Out))return true;
 }
 // Only F gets proximity assistance. Ordinary Bunniar placement still uses the exact cursor ray.
 const FVector Front=Player->GetActorForwardVector().GetSafeNormal2D();
 const FVector Origin=Player->GetActorLocation()+FVector(0,0,10);
 return Search(Origin,Front,Origin,FMath::Min(Locked ? LockedFrontRange:NearbyFrontRange,Actions->WeaponReach),Locked ? LockedFrontDegrees:NearbyFrontDegrees,false,Out);
}
void UBBSBInteractionComponent::UpdateQuickInteraction()
{
 if(!Player || GetWorld()->GetTimeSeconds()<NextQuickScan)return;NextQuickScan=GetWorld()->GetTimeSeconds()+.06;
 for(auto It=InteractionCandidates.CreateIterator();It;++It)if(!It->IsValid())It.RemoveCurrent();
 FHitResult Hit;QuickFocusActor=FindQuickInteraction(Hit) ? Hit.GetActor():nullptr;QuickFocusText=QuickInteractionText(QuickFocusActor);QuickFocusName=QuickInteractionName(QuickFocusActor);
}
void UBBSBInteractionComponent::DrawQuickInteraction(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || !Player || !QuickFocusActor || QuickFocusText.IsEmpty() || Player->Hunting->bInventoryOpen || bDialogueActive || Player->FieldActions->IsGestureCaptured())return;
 const float S=FMath::Clamp(Canvas->SizeX/1280.f,.65f,1.5f);
 const FString Name=QuickFocusName.ToString(),Action=TEXT("[F] ")+QuickFocusText.ToString();float NW,NH,AW,AH;
 HUD->GetTextSize(Name,NW,NH,GEngine->GetSmallFont(),S);HUD->GetTextSize(Action,AW,AH,GEngine->GetSmallFont(),S);
 const float W=FMath::Max(NW,AW),X=FMath::Clamp(Canvas->SizeX*.5f-W*.5f,16.f,Canvas->SizeX-W-16.f),Y=Canvas->SizeY*.5f+82*S;
 HUD->DrawRect(FLinearColor(.13f,.1f,.16f,.9f),X-10*S,Y-6*S,W+20*S,NH+AH+20*S);
 HUD->DrawText(Name,FLinearColor(.97f,.92f,.84f),X+(W-NW)*.5f,Y,GEngine->GetSmallFont(),S);
 HUD->DrawText(Action,FLinearColor(.92f,.84f,.88f),X+(W-AW)*.5f,Y+NH+6*S,GEngine->GetSmallFont(),S);
}
