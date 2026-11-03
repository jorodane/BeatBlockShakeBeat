#include "BBSBFieldActions.h"
#include "Character/BBSBVisibilityStencil.h"
#include "BBSBCharacter.h"
#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBItemIcon.h"
#include "Character/BBSBTraversal.h"
#include "BBSB.h"
#include "Caravan/BBSBCaravan.h"
#include "Combat/BBSBCombat.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "RenderUtils.h"

void UBBSBFieldActionComponent::EnsureBunniarEquipment()
{
 if (!Character || IsValid(BunniarEquipment)) return;
 ABBSBBunniarCompanion* Buddy=nullptr;
 for (TActorIterator<ABBSBBunniarCompanion> It(GetWorld());It;++It)
  if (!It->GetOwner() || It->GetOwner()==Character) { Buddy=*It; break; }
 if (!Buddy)
 {
  FActorSpawnParameters Params; Params.Owner=Character; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  Buddy=GetWorld()->SpawnActor<ABBSBBunniarCompanion>(Character->GetActorLocation(),FRotator::ZeroRotator,Params);
 }
 if (Buddy)
 {
  Buddy->SetOwner(Character); Buddy->bFollowing=true; BunniarEquipment=Buddy;
  BBSBVisibilityStencil::UpdateHandOccluders(Character,BBSBVisibilityStencil::Enabled(GetWorld()));
  EquipmentGesture.BindUObject(Buddy,&ABBSBBunniarCompanion::HandleEquipmentGesture);
 }
}

int32 UBBSBFieldActionComponent::EquippedEquipmentId() const { return FieldEquipment ? 0:bInventoryLoadoutManaged && PrototypeWeapon==EBBSBPrototypeWeapon::Legacy ? INDEX_NONE:int32(PrototypeWeapon); }

FString UBBSBFieldActionComponent::EquipmentLabel(int32 Id) const
{
 if(Id<0)return TEXT("장비 없음");
 const TCHAR* Names[]={TEXT("버니아"),TEXT("대검"),TEXT("창"),TEXT("해머"),TEXT("한손검"),TEXT("단검"),TEXT("크레센트")};
 return Names[FMath::Clamp(Id,0,6)];
}

int32 UBBSBFieldActionComponent::WheelEquipmentId(int32 Index) const
{
 if (Index<0 || Index>=EquipmentWheelSlotCount) return INDEX_NONE;
 if(bInventoryLoadoutManaged)return EquipmentWheelIds.IsValidIndex(Index) && (EquipmentWheelIds[Index]!=0 || bBunniarAvailable) ? EquipmentWheelIds[Index]:INDEX_NONE;
 return Index==0 ? 0:AvailableWeapons.IsValidIndex(Index-1) ? int32(AvailableWeapons[Index-1]):INDEX_NONE;
}

bool UBBSBFieldActionComponent::IsEquipmentActionBusy() const
{
 if (const auto* Player=Cast<ABBSBCharacter>(Character);Player && Player->Traversal && (Player->Traversal->bGlideDashing || Player->Traversal->IsWireTraversalActive() || Player->Traversal->IsParkourActive())) return true;
 if (!Character || bDodging || IsGestureCaptured() || CurrentAction.IsSet() || OtherWeaponAction.IsSet() || IsWeaponReturning() || Arbiter.IsBusy(GetWorld()->GetTimeSeconds())) return true;
 const auto* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
 return Combat && !Combat->CanAttack();
}

bool UBBSBFieldActionComponent::RequestEquipment(int32 Id)
{
 if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && P->Traversal->IsWireTraversalActive()) return false;
 EnsureBunniarEquipment();
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 if (!Character || bEquipmentInputBlocked || (Combat && Combat->bDead)) return false;
 if (Id==0 ? (!IsValid(BunniarEquipment) || !bBunniarAvailable):Id<1 || Id>6 || !AvailableWeapons.Contains(EBBSBPrototypeWeapon(Id))) return false;
 // Choosing the equipped item cancels an earlier request. Requests never accumulate.
 PendingEquipmentId=Id==EquippedEquipmentId() ? INDEX_NONE:Id;
 UE_LOG(LogBBSB,Display,TEXT("Equipment request id=%d pending=%d busy=%d"),Id,PendingEquipmentId,IsEquipmentActionBusy());
 return true;
}

void UBBSBFieldActionComponent::EquipmentTabDown()
{
 if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && P->Traversal->IsWireTraversalActive()) return;
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 if (!Character || bEquipmentTabHeld || bEquipmentInputBlocked || (Combat && Combat->bDead)) return;
 bEquipmentTabHeld=true; bEquipmentTabCancelled=false; EquipmentTabPressedAt=FPlatformTime::Seconds();
}

void UBBSBFieldActionComponent::EquipmentTabUp()
{
 if (!bEquipmentTabHeld) return;
 bEquipmentTabHeld=false;
 const bool Long=bEquipmentWheelOpen || FPlatformTime::Seconds()-EquipmentTabPressedAt>=EquipmentHoldSeconds;
 if (Long)
 {
  const int32 Choice=bEquipmentWheelOpen ? WheelEquipmentId(EquipmentWheelIndex):INDEX_NONE;
  CloseEquipmentWheel();
  if (!bEquipmentTabCancelled && Choice!=INDEX_NONE) RequestEquipment(Choice);
 }
 else if (!bEquipmentTabCancelled)
 {
  const int32 Intended=PendingEquipmentId!=INDEX_NONE ? PendingEquipmentId:EquippedEquipmentId();
  if(Intended==0) { if(!AvailableWeapons.IsEmpty())RequestEquipment(AvailableWeapons.Contains(LastCombatWeapon) ? int32(LastCombatWeapon):int32(AvailableWeapons[0])); }
  else if(bBunniarAvailable)RequestEquipment(0);
  else if(!AvailableWeapons.IsEmpty()) { const int32 Current=AvailableWeapons.IndexOfByKey(EBBSBPrototypeWeapon(Intended));RequestEquipment(int32(AvailableWeapons[(Current+1)%AvailableWeapons.Num()])); }
 }
}

void UBBSBFieldActionComponent::OpenEquipmentWheel()
{
 if (bEquipmentWheelOpen || !Character) return;
 auto* PC=Cast<APlayerController>(Character->GetController()); if (!PC) return;
 bEquipmentWheelOpen=true; EquipmentWheelIndex=INDEX_NONE;
 const auto* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
 bWheelOpenedInCombat=Combat && Combat->IsStageLocked();
 bWheelOriginalCursor=PC->bShowMouseCursor;
 float X=0,Y=0; bWheelSavedMousePosition=PC->GetMousePosition(X,Y); WheelSavedMousePosition=FVector2D(X,Y);
 WheelSavedTimeDilation=GetWorld()->GetWorldSettings()->TimeDilation;
 UGameplayStatics::SetGlobalTimeDilation(this,WheelSavedTimeDilation*FMath::Clamp(EquipmentWheelTimeScale,.001f,1.f));
 WheelAppliedTimeDilation=GetWorld()->GetWorldSettings()->TimeDilation;
 PC->bShowMouseCursor=true;
 PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways));
 int32 W=0,H=0; PC->GetViewportSize(W,H); PC->SetMouseLocation(W/2,H/2); PC->RotationInput=FRotator::ZeroRotator;
 UE_LOG(LogBBSB,Display,TEXT("Equipment wheel opened dilation=%.4f"),WheelAppliedTimeDilation);
}

void UBBSBFieldActionComponent::CloseEquipmentWheel()
{
 if (!bEquipmentWheelOpen) return;
 bEquipmentWheelOpen=false; EquipmentWheelIndex=INDEX_NONE;
 if (GetWorld() && FMath::IsNearlyEqual(GetWorld()->GetWorldSettings()->TimeDilation,WheelAppliedTimeDilation,.00001f))
  UGameplayStatics::SetGlobalTimeDilation(this,WheelSavedTimeDilation);
 if (auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr)
 {
  const auto* Combat=Character->FindComponentByClass<UBBSBCombatComponent>();
  const bool Locked=Combat && Combat->IsStageLocked();
  const bool Cursor=Locked || (bWheelOpenedInCombat ? PC->bShowMouseCursor:bWheelOriginalCursor);
  PC->bShowMouseCursor=Cursor;
  if (Cursor) PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen));
  else PC->SetInputMode(FInputModeGameOnly().SetConsumeCaptureMouseDown(false));
  if (Cursor && bWheelSavedMousePosition) PC->SetMouseLocation(FMath::RoundToInt(WheelSavedMousePosition.X),FMath::RoundToInt(WheelSavedMousePosition.Y));
  PC->RotationInput=FRotator::ZeroRotator;
  if(auto* H=Character->FindComponentByClass<UBBSBHuntingComponent>();H && H->bInventoryOpen)H->RefreshInventoryInput();
 }
 // Cursor travel chooses a menu item; it must never become a weapon Flick or Shake.
 SyntheticMotion=FVector2D::ZeroVector; SampledMouseFrame=GFrameCounter;
 for (auto& Channel:Channels)
 {
  Channel.Samples.Reset(); Channel.Reversals.Reset(); Channel.ReleaseMotion=FVector2D::ZeroVector;
  Channel.StrokeDirection=FVector2D::ZeroVector; Channel.StrokeTravel=0;
  Channel.PreviousTime=GetWorld() ? GetWorld()->GetTimeSeconds():0;
 }
 EquipmentLookResumeAt=FPlatformTime::Seconds()+.05;
 UE_LOG(LogBBSB,Display,TEXT("Equipment wheel closed"));
}

void UBBSBFieldActionComponent::CancelEquipmentSelection()
{
 bEquipmentTabCancelled=true; bEquipmentTabHeld=false; PendingEquipmentId=INDEX_NONE; CloseEquipmentWheel();
}

int32 UBBSBFieldActionComponent::ReadEquipmentWheelIndex(bool RestrictToRing) const
{
 auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;float X=0,Y=0;int32 W=0,H=0;
 if(!PC || !PC->GetMousePosition(X,Y))return INDEX_NONE;
 PC->GetViewportSize(W,H);const FVector2D Offset(X-W*.5,Y-H*.5);
 const float Scale=FMath::Clamp(FMath::Min(W/1280.f,H/720.f),.6f,1.5f),Radius=Offset.Size();
 if(Radius<70*Scale || (RestrictToRing && Radius>200*Scale))return INDEX_NONE;
 const float Angle=FMath::Fmod(FMath::Atan2(Offset.X,-Offset.Y)+2*PI,2*PI);
 return FMath::RoundToInt(Angle/(2*PI/EquipmentWheelSlotCount))%EquipmentWheelSlotCount;
}
bool UBBSBFieldActionComponent::ConfirmEquipmentWheelClick()
{
 if(!bEquipmentWheelOpen)return false;
 const int32 Choice=WheelEquipmentId(ReadEquipmentWheelIndex(true));if(Choice==INDEX_NONE)return false;
 // The later physical Q release must not submit a second choice or toggle.
 bEquipmentTabCancelled=true;bEquipmentTabHeld=false;CloseEquipmentWheel();return RequestEquipment(Choice);
}

void UBBSBFieldActionComponent::TickEquipmentSelection()
{
 if(const auto* P=Cast<ABBSBCharacter>(Character);P && P->Traversal && P->Traversal->IsWireTraversalActive()) { CancelEquipmentSelection();return; }
 EnsureBunniarEquipment();
 const auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr;
 if (!Character || bEquipmentInputBlocked || (Combat && Combat->bDead) || (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive()))
 { CancelEquipmentSelection(); return; }
 if (bEquipmentTabHeld && !bEquipmentTabCancelled && !bEquipmentWheelOpen && FPlatformTime::Seconds()-EquipmentTabPressedAt>=FMath::Max(.1f,EquipmentHoldSeconds)) OpenEquipmentWheel();
 if (bEquipmentWheelOpen)
 {
  EquipmentWheelIndex=ReadEquipmentWheelIndex(false);
  return;
 }
 if (bEquipmentTabHeld || PendingEquipmentId==INDEX_NONE || IsEquipmentActionBusy()) return;
 const int32 Choice=PendingEquipmentId;
 const bool Applied=Choice==0 ? EquipFieldEquipment(BunniarEquipment,FName(TEXT("Bunniar"))):SelectPrototypeWeapon(EBBSBPrototypeWeapon(Choice));
 if (Applied) { PendingEquipmentId=INDEX_NONE; UE_LOG(LogBBSB,Display,TEXT("Equipment applied id=%d"),Choice); }
}

void UBBSBFieldActionComponent::DrawEquipmentWheel(AHUD* HUD,UCanvas* Canvas) const
{
 if (!HUD || !Canvas) return;
 const float W=Canvas->SizeX,H=Canvas->SizeY;
 const float S=FMath::Clamp(FMath::Min(W/1280.f,H/720.f),.6f,1.5f),Outer=200*S,Inner=70*S;
 const FVector2D Center(W*.5,H*.5);
 const FLinearColor Paper(.98f,.91f,.80f),Rose(.73f,.43f,.52f),Mint(.43f,.64f,.57f);
 HUD->DrawRect(FLinearColor(.06f,.05f,.08f,.70f),0,0,W,H);
 const int32 Count=EquipmentWheelSlotCount;
 auto Point=[&](float Angle,float Radius) { return Center+FVector2D(FMath::Sin(Angle),-FMath::Cos(Angle))*Radius; };
 auto Text=[&](const FString& Value,FVector2D P,float Scale,FLinearColor Color)
 {
  float TW=0,TH=0; HUD->GetTextSize(Value,TW,TH,GEngine->GetMediumFont(),Scale);
  HUD->DrawText(Value,Color,P.X-TW*.5,P.Y-TH*.5,GEngine->GetMediumFont(),Scale);
 };
 for (int32 Index=0;Index<Count;++Index)
 {
  const bool Selected=Index==EquipmentWheelIndex;
  const bool Occupied=WheelEquipmentId(Index)!=INDEX_NONE;
  const float Middle=Index*2*PI/Count,Half=PI/Count-.018f;
  const FLinearColor Color=Occupied ? (Selected ? (WheelEquipmentId(Index)==0 ? Mint:Rose):FLinearColor(.20f,.17f,.23f,.94f)):FLinearColor(.12f,.11f,.15f,.90f);
  TArray<FCanvasUVTri> Tris;
  for (int32 Segment=0;Segment<36;++Segment)
  {
   const float A=Middle-Half+Segment*(2*Half/36),B=Middle-Half+(Segment+1)*(2*Half/36);
   const FVector2D Points[]={Point(A,Inner),Point(A,Outer),Point(B,Outer),Point(B,Inner)};
   for (int32 T=0;T<2;++T)
   {
    FCanvasUVTri Tri; Tri.V0_Pos=Points[0]; Tri.V1_Pos=Points[T+1]; Tri.V2_Pos=Points[T+2];
    Tri.V0_Color=Tri.V1_Color=Tri.V2_Color=Color; Tris.Add(Tri);
   }
   HUD->DrawLine(Points[1].X,Points[1].Y,Points[2].X,Points[2].Y,Selected && Occupied ? Paper:FLinearColor(.40f,.34f,.40f),Selected && Occupied ? 3*S:1*S);
  }
  FCanvasTriangleItem Item(Tris,GWhiteTexture); Item.BlendMode=SE_BLEND_Translucent; Canvas->DrawItem(Item);
  const FVector2D P=Point(Middle,137*S);
  if (!Occupied)
  {
   Text(TEXT("·"),P,1.25f*S,FLinearColor(.42f,.37f,.43f));
   continue;
  }
  Text(EquipmentLabel(WheelEquipmentId(Index)),P+FVector2D(0,12*S),1.2f*S,Paper);
  if (WheelEquipmentId(Index)==EquippedEquipmentId()) Text(TEXT("●"),Point(Middle,182*S),.65f*S,Paper);
  const auto* Hunting=Character ? Character->FindComponentByClass<UBBSBHuntingComponent>():nullptr;
  auto* Definition=Hunting ? Hunting->EquippedItemAt(Index==Hunting->CompanionWheelSlot ? 10:Index):nullptr;
  const FName Kind=WheelEquipmentId(Index)==0 ? FName(TEXT("Bunniar")):Definition ? Definition->IconGlyph:FName(TEXT("Sword"));
  BBSBItemIcon::DrawCanvas(HUD,Canvas,Definition,Kind,P-FVector2D(0,13*S),18*S,1.f,S,Paper);
 }
 Text(TEXT("장비 선택"),Center-FVector2D(0,Outer+34*S),1.65f*S,Paper);
 const int32 Choice=WheelEquipmentId(EquipmentWheelIndex);
 Text(EquipmentWheelIndex==INDEX_NONE ? TEXT("중앙: 취소"):Choice==INDEX_NONE ? TEXT("빈 슬롯"):EquipmentLabel(Choice),Center-FVector2D(0,5*S),1.35f*S,Paper);
 Text(TEXT("Q 놓기"),Center+FVector2D(0,20*S),1.0f*S,FLinearColor(.75f,.67f,.71f));
 Text(TEXT("클릭 또는 Q 놓아 장착"),Center+FVector2D(0,Outer+26*S),1.25f*S,Paper);
 if (IsEquipmentActionBusy()) Text(TEXT("행동이 끝난 뒤 장착됩니다"),Center+FVector2D(0,Outer+52*S),1.15f*S,FLinearColor(.97f,.76f,.58f));
}
