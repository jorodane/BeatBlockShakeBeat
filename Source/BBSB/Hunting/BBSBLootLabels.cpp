#include "Hunting/BBSBHunting.h"
#include "Hunting/BBSBLootPickup.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

void UBBSBHuntingComponent::DrawLootNames(AHUD* HUD,UCanvas* Canvas) const
{
 if(!HUD || !Canvas || !Canvas->Canvas)return;
 auto* PC=HUD->GetOwningPlayerController();if(!PC)return;
 const float Scale=FMath::Clamp(Canvas->SizeX/1280.f,.75f,1.4f)*1.1f;
 TArray<const ABBSBLootPickup*> Visible;
 for(const auto& Weak:LootActors)if(const auto* Loot=Weak.Get();IsValid(Loot) && Loot->bNameVisible && Loot->IsAvailable())Visible.Add(Loot);
 Visible.Sort([](const ABBSBLootPickup& A,const ABBSBLootPickup& B)
 { return A.Item->Rarity==B.Item->Rarity ? A.GetUniqueID()<B.GetUniqueID():uint8(A.Item->Rarity)>uint8(B.Item->Rarity); });
 TArray<FBox2D> Placed;
 for(const auto* Loot:Visible)
 {
  FVector2D Point;if(!PC->ProjectWorldLocationToScreen(Loot->GetActorLocation()+FVector::UpVector*38.f,Point,true))continue;
  FString Name=FString::Printf(TEXT("[%s] %s"),*Loot->Item->RarityLabel().ToString(),*Loot->Item->DisplayName.ToString());
  if(Loot->Quantity>1)Name+=FString::Printf(TEXT(" ×%d"),Loot->Quantity);
  float W=0,H=0;HUD->GetTextSize(Name,W,H,GEngine->GetSmallFont(),Scale);W+=12*Scale;H+=6*Scale;
  FBox2D Box;bool Fits=false;
  for(int32 Row=0;Row<7;++Row)
  {
   Box=FBox2D(Point-FVector2D(W*.5f,H*.5f),Point+FVector2D(W*.5f,H*.5f));Fits=true;
   for(const auto& Other:Placed)if(Box.Intersect(Other)) { Fits=false;break; }
   if(Fits)break;Point.Y-=H+3*Scale;
  }
  if(!Fits || Box.Min.X<2 || Box.Max.X>Canvas->SizeX-2 || Box.Min.Y<2 || Box.Max.Y>Canvas->SizeY-2)continue;
  Placed.Add(Box);HUD->DrawRect(FLinearColor(.07f,.055f,.085f,.72f),Box.Min.X,Box.Min.Y,W,H);
  FCanvasTextItem Text(Point-FVector2D(0,(H-6*Scale)*.5f),FText::FromString(Name),GEngine->GetSmallFont(),Loot->Item->RarityColor());
  Text.Scale=FVector2D(Scale);Text.bCentreX=true;Text.bOutlined=true;Text.OutlineColor=FLinearColor(.04f,.025f,.055f,.95f);
  Canvas->DrawItem(Text);
 }
}
