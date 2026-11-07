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
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"

void UBBSBInteractionComponent::RegisterInteractionCandidate(AActor* Actor)
{
 if(IsValid(Actor) && (Actor->Implements<UBBSBBunniarSurface>() || Actor->IsA<ABBSBLootPickup>()))InteractionCandidates.Add(Actor);
}
bool UBBSBInteractionComponent::IsQuickInteractable(AActor* A) const
{
 if(!IsValid(A) || A==GetOwner() || A->IsHidden() || A->ActorHasTag(TEXT("BBSBIgnoreQuickInteraction")))return false;
 if(const auto* L=Cast<ABBSBLootPickup>(A))return L->IsAvailable() && !L->Item->IsFieldResource();
 if(const auto* O=Cast<ABBSBCarryObject>(A);O && O->bBroken)return false;
 return A->Implements<UBBSBBunniarSurface>();
}
FText UBBSBInteractionComponent::QuickInteractionText(AActor* A) const
{
 if(!IsQuickInteractable(A))return FText();
 if(const auto* L=Cast<ABBSBLootPickup>(A))return FText::FromString(TEXT("줍기 · ")+L->Item->DisplayName.ToString()+TEXT("  |  길게: 주변 흡입"));
 if(A->Implements<UBBSBBunniarSurface>())
 { const auto Custom=IBBSBBunniarSurface::Execute_BunniarInteractionText(A);if(!Custom.IsEmpty())return Custom; }
 if(const auto* N=Cast<ABBSBInteractionNPC>(A))return FText::FromString(TEXT("대화 · ")+N->DisplayName.ToString());
 if(A->IsA<ABBSBCarryObject>())return FText::FromString(TEXT("잡기  |  유지: 옮기기 · 우클릭: 조작/던지기"));
 if(const auto* P=Cast<ABBSBPuzzleStation>(A))return FText::FromString(P->Title()+TEXT(" · ")+P->Hint());
 if(const auto* D=Cast<ABBSBBunniarDevice>(A))return FText::FromString(D->DeviceLabel+TEXT(" · 조작"));
 return FText::FromString(TEXT("상호작용"));
}
bool UBBSBInteractionComponent::FindQuickInteraction(FHitResult& Out) const
{
 Out=FHitResult();if(!Player || !Player->FieldActions || bDialogueActive || Player->Combat->bDead || Player->FieldActions->bEquipmentInputBlocked || Player->FieldActions->bEquipmentWheelOpen)return false;
 const auto* A=Player->FieldActions.Get();const bool Locked=Player->Combat->HasTargetLock();
 FVector Origin,Direction,TraceOrigin;
 if(Locked) { Origin=TraceOrigin=Player->GetActorLocation()+FVector(0,0,10);Direction=Player->GetActorForwardVector().GetSafeNormal2D(); }
 else
 {
  FVector TraceDirection;
  if(!A->CursorAimRay(Origin,Direction) || !A->TargetingAimRay(TraceOrigin,TraceDirection))return false;
 }
 const float Range=Locked ? FMath::Min(LockedFrontRange,A->WeaponReach):A->WeaponReach;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBQuickInteraction),true,Player);
 Query.AddIgnoredActor(A->BunniarEquipment);
 // Keep the original camera cone for F assistance, but remove candidates and
 // occluders that are farther toward the camera than the player-side allowance.
 for(const auto& Weak:InteractionCandidates)if(auto* Target=Weak.Get();Target && !A->IsAimPointDepthAllowed(Target->GetActorLocation()))Query.AddIgnoredActor(Target);
 // Projectile meshes have no visibility collision. They are never candidates.
 FHitResult Direct;
 if(!Locked && GetWorld()->LineTraceSingleByChannel(Direct,TraceOrigin,Origin+Direction*(Range+1500),ECC_Visibility,Query) && IsQuickInteractable(Direct.GetActor()) && A->IsAimPointDepthAllowed(Direct.GetActor()->GetActorLocation()) && A->IsAimPointDepthAllowed(Direct.ImpactPoint) && FVector::DistSquared(Player->GetActorLocation(),Direct.ImpactPoint)<=FMath::Square(Range)) { Out=Direct;return true; }
 float Best=MAX_flt;
 for(const auto& Weak:InteractionCandidates)
 {
  auto* Target=Weak.Get();if(!IsQuickInteractable(Target) || !A->IsAimPointDepthAllowed(Target->GetActorLocation()))continue;
  TArray<UPrimitiveComponent*> Parts;Target->GetComponents<UPrimitiveComponent>(Parts);
  for(auto* Part:Parts)
  {
   if(!Part->IsRegistered() || Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision || Part->GetCollisionResponseToChannel(ECC_Visibility)!=ECR_Block)continue;
   FVector Point=Part->Bounds.Origin;if(!A->IsAimPointDepthAllowed(Point))continue;
   const FVector Delta=Point-Origin;const float Along=FVector::DotProduct(Delta,Direction);
   if(Along<=0 || FVector::DistSquared(Point,Player->GetActorLocation())>FMath::Square(Range))continue;
   const float Perp=(Delta-Direction*Along).Size();const float Radius=FMath::Clamp(Part->Bounds.SphereRadius,12.f,120.f);
   const float Angle=FMath::RadiansToDegrees(FMath::Atan2(FMath::Max(0.f,Perp-Radius),Along));
   if(Angle>(Locked ? LockedFrontDegrees:QuickAimDegrees) || (!Locked && Perp>QuickAimRadius+Radius))continue;
   FHitResult Hit;
   // Never assist through cover. A separate ray to the candidate must actually hit it.
   if(!GetWorld()->LineTraceSingleByChannel(Hit,TraceOrigin,Point,ECC_Visibility,Query) || Hit.GetActor()!=Target || !A->IsAimPointDepthAllowed(Hit.ImpactPoint))continue;
   const float Score=Locked ? Along+Perp*.7f:Angle*100.f+Along*.015f;
   if(Score<Best) { Best=Score;Out=Hit; }
  }
 }
 return Out.GetActor()!=nullptr;
}
void UBBSBInteractionComponent::UpdateQuickInteraction()
{
 if(!Player || GetWorld()->GetTimeSeconds()<NextQuickScan)return;NextQuickScan=GetWorld()->GetTimeSeconds()+.06;
 for(auto It=InteractionCandidates.CreateIterator();It;++It)if(!It->IsValid())It.RemoveCurrent();
 FHitResult Hit;QuickFocusActor=FindQuickInteraction(Hit) ? Hit.GetActor():nullptr;QuickFocusText=QuickInteractionText(QuickFocusActor);
}
void UBBSBInteractionComponent::DrawQuickInteraction(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || !Player || !QuickFocusActor || QuickFocusText.IsEmpty() || Player->Hunting->bInventoryOpen || bDialogueActive || Player->FieldActions->IsGestureCaptured())return;
 const float S=FMath::Clamp(Canvas->SizeX/1280.f,.65f,1.5f);const FString Text=TEXT("F  ")+QuickFocusText.ToString();float W,H;HUD->GetTextSize(Text,W,H,GEngine->GetSmallFont(),S);
 const float X=FMath::Clamp(Canvas->SizeX*.5f-W*.5f,16.f,Canvas->SizeX-W-16.f),Y=Canvas->SizeY*.5f+88*S;
 HUD->DrawRect(FLinearColor(.13f,.1f,.16f,.9f),X-10*S,Y-5*S,W+20*S,H+12*S);HUD->DrawText(Text,FLinearColor(.97f,.92f,.84f),X,Y,GEngine->GetSmallFont(),S);
}
