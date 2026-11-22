#include "Interaction/BBSBInteraction.h"
#include "Interaction/BBSBInteractionDoor.h"
#include "Interaction/BBSBClimbable.h"
#include "Interaction/BBSBInteractable.h"
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
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "UI/BBSBPaperTheme.h"

void UBBSBInteractionComponent::RegisterInteractionCandidate(AActor* Actor)
{
 if (!IsValid(Actor)) return;
 if (BBSBBunniarSurface::Resolve(Actor) || Actor->IsA<ABBSBLootPickup>()) InteractionCandidates.Add(Actor);
 if (auto* Climbable = Actor->FindComponentByClass<UBBSBClimbable>()) ClimbableCandidates.Add(Climbable);
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
 if (const auto* Interaction = UBBSBInteractable::Find(BBSBBunniarSurface::Resolve(A)); Interaction && !Interaction->DisplayName.IsEmpty()) return Interaction->DisplayName;
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
 if (const auto* Interaction = UBBSBInteractable::Find(BBSBBunniarSurface::Resolve(A)))
 {
  if (!Interaction->CanStartInteraction(Player)) return Interaction->UnavailableText;
  const auto Prompt = Interaction->GetPrompt();
  if (!Prompt.ActionText.IsEmpty()) return Prompt.ActionText;
 }
 if(A->IsA<ABBSBLootPickup>())return FText::FromString(TEXT("줍기"));
 if(A->IsA<ABBSBInteractionNPC>())return FText::FromString(TEXT("대화하기"));
 if(A->IsA<ABBSBRespawnPoint>())return FText::FromString(TEXT("쉬기"));
 if(const auto* D=Cast<ABBSBInteractionDoor>(A))return FText::FromString(D->bOpen ? TEXT("닫기"):TEXT("열기"));
 if(const auto* Prop=Cast<ABBSBCarryObject>(A))return FText::FromString(Prop->CanBeCarriedBy(Player) ? TEXT("옮기기"):TEXT("조금 떨어져서 옮기기"));
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
 auto Reachable=[&](const FHitResult& Contact)
 {
  if(!IsQuickInteractable(Contact.GetActor()))return false;
  const FVector Start=Player->GetActorLocation()+FVector(0,0,10);
  FCollisionQueryParams Q(SCENE_QUERY_STAT(BBSBQuickInteractionReach),true,Player);Q.AddIgnoredActor(Actions->BunniarEquipment);
  FHitResult Block;const FVector End=Contact.ImpactPoint+(Contact.ImpactPoint-Start).GetSafeNormal()*2.f;
  if(!GetWorld()->LineTraceSingleByChannel(Block,Start,End,ECC_Visibility,Q))return true;
  if(Block.GetActor()==Contact.GetActor())return true;
  AActor* Handler=BBSBBunniarSurface::Resolve(Contact.GetActor());
  return Handler && BBSBBunniarSurface::Resolve(Block.GetActor())==Handler;
 };
 auto Search=[&](FVector Origin,FVector Direction,FVector TraceOrigin,float Range,float Degrees,bool Camera,FHitResult& Result)
 {
  FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBQuickInteraction),true,Player);Query.AddIgnoredActor(Actions->BunniarEquipment);
  if(Camera)for(const auto& Weak:InteractionCandidates)if(auto* Target=Weak.Get();Target && !DepthAllowed(Target))Query.AddIgnoredActor(Target);
  if(Camera)
  {
   FHitResult Direct;
   if(GetWorld()->LineTraceSingleByChannel(Direct,TraceOrigin,Origin+Direction*(Range+1500),ECC_Visibility,Query) && IsQuickInteractable(Direct.GetActor()) &&
      DepthAllowed(Direct.GetActor()) && Actions->IsAimPointDepthAllowed(Direct.ImpactPoint) && FVector::DistSquared(Player->GetActorLocation(),Direct.ImpactPoint)<=FMath::Square(Range) && Reachable(Direct))
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
       (Camera && !Actions->IsAimPointDepthAllowed(Hit.ImpactPoint)) || !Reachable(Hit))continue;
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
 if (!Player || GetWorld()->GetTimeSeconds() < NextQuickScan) return;
 NextQuickScan = GetWorld()->GetTimeSeconds() + .06;
 for (auto It = InteractionCandidates.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
 for (auto It = ClimbableCandidates.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();

 FHitResult Hit;
 QuickFocusActor = FindQuickInteraction(Hit) ? Hit.GetActor() : nullptr;
 QuickFocusText = QuickInteractionText(QuickFocusActor);
 QuickFocusName = QuickInteractionName(QuickFocusActor);
 const auto* Handler = BBSBBunniarSurface::Resolve(QuickFocusActor);
 const auto* Interaction = UBBSBInteractable::Find(Handler);
 bQuickFocusAvailable = QuickFocusActor && (!Interaction || Interaction->CanStartInteraction(Player));
 QuickFocusIcon = Interaction ? Interaction->GetPrompt().Icon : nullptr;
 if (!QuickFocusActor || QuickFocusIcon) return;

 const auto* Catalog = UBBSBInteractTypes::DefaultCatalog();
 if (!Catalog) return;
 FName Type = TEXT("Use");
 if (QuickFocusActor->IsA<ABBSBInteractionNPC>()) Type = TEXT("Talk");
 else if (QuickFocusActor->IsA<ABBSBLootPickup>()) Type = TEXT("Pickup");
 else if (QuickFocusActor->IsA<ABBSBRespawnPoint>()) Type = TEXT("Rest");
 else if (QuickFocusActor->IsA<ABBSBCarryObject>()) Type = TEXT("Grab");
 else if (QuickFocusActor->IsA<ABBSBInteractionDoor>()) Type = TEXT("Switch");
 else if (const auto* Device = Cast<ABBSBStage01Device>(Handler))
 {
  switch (Device->Kind)
  {
   case EBBSBStage01Node::CabinDoor:
   case EBBSBStage01Node::CargoLever:
   case EBBSBStage01Node::LightLever:
   case EBBSBStage01Node::Trapdoor:
   case EBBSBStage01Node::ReadyGate: Type = TEXT("Drag"); break;
   case EBBSBStage01Node::LowerLadder: Type = Device->bSolved ? TEXT("Climb") : TEXT("Drag"); break;
   case EBBSBStage01Node::ClimbLadder: Type = TEXT("Climb"); break;
   default: break;
  }
 }
 const auto* Entry = Catalog->Types.Find(Type);
 if (!Entry) Entry = Catalog->Types.Find(TEXT("Use"));
 if (Entry) QuickFocusIcon = Entry->Icon;
}

bool UBBSBInteractionComponent::CanUsePrimaryOnQuickFocus() const
{
 const auto* Actions = Player ? Player->FieldActions.Get() : nullptr;
 if (!Actions || !bQuickFocusAvailable || !IsValid(QuickFocusActor) || bDialogueActive ||
     Actions->bEquipmentInputBlocked || Actions->bEquipmentWheelOpen || Actions->IsGestureCaptured() ||
     Player->Hunting->bInventoryOpen) return false;

 // This is the hit already used by the reticle, including the exact cursor ray and reach.
 // F's assisted search can select a nearby actor that primary click never hit.
 auto* AimedActor = Actions->AimPresentationActor.Get();
 if (!IsValid(AimedActor)) return false;
 auto* Handler = BBSBBunniarSurface::Resolve(QuickFocusActor);
 if (AimedActor != QuickFocusActor && (!Handler || BBSBBunniarSurface::Resolve(AimedActor) != Handler)) return false;

 // PrimaryDown handles a directly aimed checkpoint before equipment input.
 if (const auto* Point = Cast<ABBSBRespawnPoint>(AimedActor))
 {
  return Point->bTravelAvailable && !Player->Respawn->IsSequenceActive() &&
         FVector::DistSquared(Player->GetActorLocation(), Point->GetActorLocation()) <= FMath::Square(Point->ActivationRange);
 }

 if (!Actions->IsBunniarControlActive() || !Actions->bBunniarAvailable || Actions->bBunniarShortcutActive ||
     Actions->PendingEquipmentId != INDEX_NONE || Actions->IsEquipmentActionBusy() || Player->Combat->IsStageLocked() ||
     Player->Hunting->bCollectionHeld) return false;
 const auto* Buddy = Cast<ABBSBBunniarCompanion>(Actions->InteractionEquipment());
 if (!Buddy || Buddy->bAirHovering || Buddy->bSurfaceAttached) return false;

 // Climbing is a body action, whereas an undeployed ladder is a draggable surface.
 if (const auto* Ladder = Cast<UBBSBClimbable>(UBBSBInteractable::Find(Handler)); Ladder && Ladder->bPrimaryInteraction) return false;
 if (const auto* Device = Cast<ABBSBStage01Device>(Handler))
 {
  if (Device->Kind == EBBSBStage01Node::ClimbLadder ||
      (Device->Kind == EBBSBStage01Node::LowerLadder && Device->bSolved)) return false;
 }
 const auto* Carry = Cast<ABBSBCarryObject>(AimedActor);
 return !Carry || Carry->CanBeCarriedBy(Player);
}

void UBBSBInteractionComponent::DrawQuickInteraction(AHUD* HUD, UCanvas* Canvas) const
{
 if (!HUD || !Canvas || !Player || !QuickFocusActor || QuickFocusText.IsEmpty() ||
     Player->Hunting->bInventoryOpen || bDialogueActive || Player->FieldActions->IsGestureCaptured()) return;

 const float Scale = FMath::Clamp(Canvas->SizeX / 1280.f, .65f, 1.5f);
 const FString Name = QuickFocusName.ToString();
 const FString Action = QuickFocusText.ToString();
 auto* ActionFont = GEngine->GetMediumFont();
 auto* NameFont = GEngine->GetSmallFont();
 float ActionWidth, ActionHeight, NameWidth, NameHeight;
 HUD->GetTextSize(Action, ActionWidth, ActionHeight, ActionFont, Scale);
 HUD->GetTextSize(Name, NameWidth, NameHeight, NameFont, Scale);

 const float GlyphSize = 24 * Scale;
 const float GlyphSpace = QuickFocusIcon ? GlyphSize + 9 * Scale : 0;
 const auto* Catalog = bQuickFocusAvailable ? UBBSBInteractTypes::DefaultCatalog() : nullptr;
 auto* PrimaryIcon = Catalog && CanUsePrimaryOnQuickFocus() ? Catalog->HintIcons.FindRef(TEXT("MouseLeft")).Get() : nullptr;
 const float MouseOffset = PrimaryIcon ? 30 * Scale : 0;
 const float KeySpace = bQuickFocusAvailable ? 34 * Scale : 0;
 const FVector2D CardSize(FMath::Max(147.f * Scale, FMath::Max(ActionWidth, NameWidth) + GlyphSpace + 25 * Scale),
                          FMath::Max(44.f * Scale, ActionHeight + NameHeight + 12 * Scale));
 const float RightLimit = Canvas->SizeX - CardSize.X - KeySpace - 14 * Scale;
 const float RowX = FMath::Max(14.f * Scale + MouseOffset, FMath::Min(Canvas->SizeX * .5f + 53 * Scale, RightLimit));
 const float RowY = Canvas->SizeY * .5f + 23 * Scale;
 const FVector2D Card(RowX + KeySpace, RowY);
 const auto Rim = bQuickFocusAvailable ? BBSBPaper::Paper().CopyWithNewOpacity(.84f) : BBSBPaper::Muted();
 BBSBPaper::CanvasPolygon(Canvas, BBSBPaper::Shape(Card + FVector2D(3, 4) * Scale, CardSize, Scale), BBSBPaper::Shadow());
 BBSBPaper::CanvasPolygon(Canvas, BBSBPaper::Shape(Card, CardSize, Scale), Rim);
 BBSBPaper::CanvasPolygon(Canvas, BBSBPaper::Shape(Card + FVector2D(Scale), CardSize - FVector2D(2 * Scale), Scale),
                         BBSBPaper::Ink().CopyWithNewOpacity(.93f));

 auto DrawIcon = [&](UTexture2D* Texture, FVector2D Centre, float Size, float Opacity)
 {
  if (!Texture) return;
  const float Aspect = float(Texture->GetSizeX()) / FMath::Max(1, Texture->GetSizeY());
  const FVector2D Pixels(FMath::Min(1.f, Aspect) * Size, FMath::Min(1.f, 1.f / Aspect) * Size);
  const FVector2D Position = Centre - Pixels * .5f;
  HUD->DrawTexture(Texture, Position.X, Position.Y, Pixels.X, Pixels.Y, 0, 0, 1, 1, FLinearColor::White.CopyWithNewOpacity(Opacity));
 };
 const float CentreY = Card.Y + CardSize.Y * .5f;
 if (bQuickFocusAvailable)
 {
  const FVector2D Centre(RowX + 12 * Scale, CentreY);
  auto* Keycap = Catalog ? Catalog->HintIcons.FindRef(TEXT("Keycap")).Get() : nullptr;
  DrawIcon(Keycap, Centre, 25 * Scale, 1.f);
  DrawIcon(PrimaryIcon, FVector2D(Centre.X - MouseOffset, CentreY), 28 * Scale, 1.f);
  float KeyWidth, KeyHeight;
  HUD->GetTextSize(TEXT("F"), KeyWidth, KeyHeight, ActionFont, Scale);
  HUD->DrawText(TEXT("F"), Keycap ? BBSBPaper::Ink() : BBSBPaper::Paper(), Centre.X - KeyWidth * .5f,
                Centre.Y - KeyHeight * .5f, ActionFont, Scale);
 }
 DrawIcon(QuickFocusIcon, FVector2D(Card.X + 11 * Scale + GlyphSize * .5f, CentreY), GlyphSize, bQuickFocusAvailable ? 1.f : .55f);
 const float TextX = Card.X + GlyphSpace + 11 * Scale;
 const float TextY = CentreY - (ActionHeight + NameHeight) * .5f;
 const auto ActionColor = bQuickFocusAvailable ? BBSBPaper::Paper() : BBSBPaper::Cell();
 HUD->DrawText(Action, ActionColor, TextX, TextY, ActionFont, Scale);
 HUD->DrawText(Name, FLinearColor(.77f, .66f, .69f), TextX, TextY + ActionHeight, NameFont, Scale);
}

