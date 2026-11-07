#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBInventoryWidget.h"
#include "BBSBCharacter.h"
#include "Character/BBSBRespawn.h"
#include "Character/BBSBWorldMap.h"
#include "Caravan/BBSBCaravan.h"
#include "Combat/BBSBCombat.h"
#include "Character/BBSBTraversal.h"
#include "Field/BBSBFieldActions.h"
#include "Interaction/BBSBInteraction.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"

void UBBSBHuntingComponent::ToggleInventory()
{
 if(bInventoryOpen)CloseInventory();
 else OpenInventory();
}
bool UBBSBHuntingComponent::OpenInventory()
{
 if(bInventoryOpen)return true;
 auto* PC=Player ? Cast<APlayerController>(Player->GetController()):nullptr;
 if(!PC || !PC->IsLocalController() || (Player->Respawn && Player->Respawn->IsSequenceActive()))return false;
 if(Player->WorldMap)Player->WorldMap->CloseMap();
 const bool CursorAlreadyVisible=PC->bShowMouseCursor;
 if(!InventoryWidget)
 {
  InventoryWidget=CreateWidget<UBBSBInventoryWidget>(PC,InventoryWidgetClass ? InventoryWidgetClass.Get():UBBSBInventoryWidget::StaticClass());
  if(!InventoryWidget)return false;InventoryWidget->Hunting=this;
 }
 bInventoryPending=false;bInventoryOpen=true;MenuController=PC;bMenuOriginalCursor=Player->FieldActions->bEquipmentWheelOpen ? Player->FieldActions->EquipmentWheelSavedCursor():CursorAlreadyVisible;
 InventoryWidget->ResetInteraction();
 float X=0,Y=0;bMenuSavedMouse=PC->GetMousePosition(X,Y);MenuMouse=FVector2D(X,Y);
 PC->SetIgnoreLookInput(true);bMenuOwnLook=true;
 InventoryWidget->AddToViewport(1000);InventoryWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
 InventoryWidget->RegisterMenuKeys();
 RefreshInventoryInput();
 if(!CursorAlreadyVisible) { int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);if(Width>0 && Height>0)PC->SetMouseLocation(Width/2,Height/2); }
 return true;
}
void UBBSBHuntingComponent::RefreshInventoryInput()
{
 auto* PC=MenuController.Get();if(!bInventoryOpen || !PC || !InventoryWidget)return;
 PC->bShowMouseCursor=true;PC->RotationInput=FRotator::ZeroRotator;
 // Keep keyboard focus in the viewport so opening a panel never flushes held
 // movement/gesture keys; only menu keys are consumed by its input component.
 PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen));
}
void UBBSBHuntingComponent::CloseInventory()
{
 bInventoryPending=false;if(!bInventoryOpen && !bMenuOwnLook)return;
 if(Player && Player->FieldActions && Player->FieldActions->bEquipmentWheelOpen)Player->FieldActions->CancelEquipmentSelection();
 bInventoryOpen=false;if(InventoryWidget) { InventoryWidget->UnregisterMenuKeys();InventoryWidget->ResetInteraction();InventoryWidget->RemoveFromParent(); }
 if(auto* PC=MenuController.Get())
 {
  if(bMenuOwnLook)PC->SetIgnoreLookInput(false);
  PC->bShowMouseCursor=bMenuOriginalCursor;PC->RotationInput=FRotator::ZeroRotator;
  if(bMenuOriginalCursor)
  {
   PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
   if(bMenuSavedMouse)PC->SetMouseLocation(FMath::RoundToInt(MenuMouse.X),FMath::RoundToInt(MenuMouse.Y));
  }
  else PC->SetInputMode(FInputModeGameOnly().SetConsumeCaptureMouseDown(false));
 }
 bMenuOwnLook=false;MenuController.Reset();
}
void UBBSBHuntingComponent::DrawFeedback(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || bInventoryOpen)return;
 DrawLootNames(HUD,Canvas);
 const float W=Canvas->SizeX,H=Canvas->SizeY,S=FMath::Clamp(W/1280.f,.7f,1.4f);
 const FLinearColor Paper(.98f,.93f,.84f),Mint(.65f,.93f,.81f);
 const double Now=GetWorld()->GetTimeSeconds();
 if(Now-LastLevelUpAt<2.3)
 {
  const float Age=Now-LastLevelUpAt,Alpha=FMath::Clamp((2.3f-Age)/.5f,0.f,1.f),Scale=(1.2f+.18f*FMath::Sin(FMath::Min(Age/.22f,1.f)*PI))*S;
  const auto* Upgraded=FindOwnedEntry(LastExperienceWeapon);
  const FString Text=Upgraded ? FString::Printf(TEXT("%s   Lv.%d%s"),*Upgraded->Item->DisplayName.ToString(),Upgraded->WeaponLevel,IsEcstasyWeapon(LastExperienceWeapon) ? TEXT(" · 엑스타시"):TEXT("")):TEXT("");float TW=0,TH=0;HUD->GetTextSize(Text,TW,TH,GEngine->GetMediumFont(),Scale);
  HUD->DrawText(Text,FLinearColor(Paper.R,Paper.G,Paper.B,Alpha),W*.5f-TW*.5f,110*S,GEngine->GetMediumFont(),Scale);
 }
 int32 Row=0;
 const float MiniScale=FMath::Clamp(FMath::Min(W/1280.f,H/720.f),.55f,1.8f);
 const float NoticeBottom=Player && Player->WorldMap && Player->WorldMap->ShouldDrawMinimap() ? 286*MiniScale+30*S:144*S;
 for(int32 I=Notices.Num()-1;I>=0;--I)
 {
  const auto& Notice=Notices[I];const float Age=Now-Notice.At;if(!Notice.Item || Age>2.5f)continue;
  const float Alpha=FMath::Clamp((2.5f-Age)/.5f,0.f,1.f),Top=H-NoticeBottom-Row++*30*S;
  HUD->DrawRect(FLinearColor(.11f,.08f,.14f,.72f*Alpha),W-298*S,Top-3*S,280*S,25*S);
  HUD->DrawRect(FLinearColor(Notice.Item->Color.R,Notice.Item->Color.G,Notice.Item->Color.B,Alpha),W-291*S,Top+3*S,5*S,13*S);
  HUD->DrawText(FString::Printf(TEXT("%s  ×%d"),*Notice.Item->DisplayName.ToString(),Notice.Quantity),FLinearColor(Paper.R,Paper.G,Paper.B,Alpha),W-275*S,Top,GEngine->GetSmallFont(),1.05f*S);
 }
 if(Now<StatusUntil)HUD->DrawText(StatusNotice,Paper,W*.5f-120*S,H*.76f,GEngine->GetSmallFont(),1.15f*S);
 if(bVacuuming)HUD->DrawText(TEXT("버니아 흡입 중 · 놓으면 종료"),Mint,W*.5f-102*S,H*.5f+64*S,GEngine->GetSmallFont(),S);
}
