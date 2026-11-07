#include "Character/BBSBWorldMap.h"
#include "Character/BBSBTerrainMapData.h"
#include "BBSBCharacter.h"
#include "Hunting/BBSBHunting.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

FName UBBSBWorldMapComponent::AnnotationLevel() const
{ return GetWorld() ? FName(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName())):NAME_None; }
void UBBSBWorldMapComponent::InitializeMapMarkers()
{
 if(!Player || !Player->Hunting)return;
 const FString Slot=Player->Hunting->SaveSlotName+TEXT("_MapMarkers");if(Slot==MarkerSlot)return;
 if(bMarkersDirty && !MarkerSlot.IsEmpty())SaveMapMarkers();
 MarkerSlot=Slot;UserMarkers.Reset();bMarkersDirty=false;bMarkerSaveReadOnly=false;
 if(!UGameplayStatics::DoesSaveGameExist(Slot,0))return;
 const auto* Save=Cast<UBBSBMapMarkerSave>(UGameplayStatics::LoadGameFromSlot(Slot,0));
 if(!Save || Save->Version!=1 || !BBSBMapMarkerRules::Valid(Save->Markers)) { bMarkerSaveReadOnly=true;return; }
 UserMarkers=Save->Markers;
}
bool UBBSBWorldMapComponent::ResolveAnnotationPoint(FVector2D UV,FVector& Position) const
{
 if(!GetWorld() || UV.ContainsNaN() || UV.X<0 || UV.Y<0 || UV.X>1 || UV.Y>1)return false;
 Position=MapToWorld(UV);const float High=AppliedTerrainMap ? AppliedTerrainMap->MaximumHeight+1500:14000,Low=AppliedTerrainMap ? AppliedTerrainMap->MinimumHeight-5000:-10000;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBMapAnnotationFloor),false,GetOwner());FHitResult Hit;
 if(!GetWorld()->LineTraceSingleByObjectType(Hit,FVector(Position.X,Position.Y,High),FVector(Position.X,Position.Y,Low),BBSBCombatVisuals::GeometryTypes(),Query))return false;
 Position=Hit.ImpactPoint;return true;
}
bool UBBSBWorldMapComponent::AddMapMarker(FVector2D UV)
{
 InitializeMapMarkers();FVector Position;if(bMarkerSaveReadOnly || !ResolveAnnotationPoint(UV,Position) || UserMarkers.Num()>=4096)return false;
 FBBSBMapMarker Marker;Marker.Id=FGuid::NewGuid();Marker.Level=AnnotationLevel();Marker.Position=Position;UserMarkers.Add(Marker);
 bMarkersDirty=true;MarkerSaveAt=GetWorld()->GetRealTimeSeconds()+.5;return true;
}
bool UBBSBWorldMapComponent::RemoveMapMarker(FGuid Id)
{
 InitializeMapMarkers();if(bMarkerSaveReadOnly)return false;
 const int32 Removed=UserMarkers.RemoveAll([&](const auto& M){return M.Id==Id && M.Level==AnnotationLevel();});
 if(Removed) { bMarkersDirty=true;MarkerSaveAt=GetWorld()->GetRealTimeSeconds()+.5; }return Removed>0;
}
bool UBBSBWorldMapComponent::SaveMapMarkers()
{
 if(!bMarkersDirty || bMarkerSaveReadOnly || MarkerSlot.IsEmpty() || !Player || !Player->Hunting || !Player->Hunting->bPersistProgress || !BBSBMapMarkerRules::Valid(UserMarkers))return false;
 auto* Save=Cast<UBBSBMapMarkerSave>(UGameplayStatics::CreateSaveGameObject(UBBSBMapMarkerSave::StaticClass()));Save->Markers=UserMarkers;
 if(!UGameplayStatics::SaveGameToSlot(Save,MarkerSlot,0)) { MarkerSaveAt=GetWorld()->GetRealTimeSeconds()+3;return false; }bMarkersDirty=false;return true;
}
bool UBBSBWorldMapComponent::PingMapLocation(FVector2D UV)
{
 FVector Position;if(!ResolveAnnotationPoint(UV,Position))return false;PingPosition=Position;PingUntil=GetWorld()->GetRealTimeSeconds()+FMath::Clamp(PingLifetime,2.f,15.f);return true;
}
float UBBSBWorldMapComponent::PingRemaining() const
{ return GetWorld() ? FMath::Max(0.f,float(PingUntil-GetWorld()->GetRealTimeSeconds())):0; }

void UBBSBWorldMapWidget::PaintAnnotations(const FGeometry& G,FSlateWindowElementList& E,int32 Layer) const
{
 if(!Map)return;const float S=PaintScale;
 auto Project=[&](FVector P){return bFullMap ? MapAt+(Map->WorldToMap(P)-ViewMinimum())*Zoom*MapSize:WorldToMinimap(P);};
 auto Inside=[&](FVector World,FVector2D P){return bFullMap ? P.X>=MapAt.X+8*S && P.Y>=MapAt.Y+8*S && P.X<=MapAt.X+MapSize.X-8*S && P.Y<=MapAt.Y+MapSize.Y-8*S:IsInsideMinimap(World,9);};
 auto Line=[&](const TArray<FVector2D>& P,FLinearColor C,float W,int32 Z){FSlateDrawElement::MakeLines(E,Layer+Z,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,W*S);};
 for(const auto& M:Map->UserMarkers)if(M.Level==Map->AnnotationLevel())
 {
  const FVector2D P=Project(M.Position);if(!Inside(M.Position,P))continue;
  const TArray<FVector2D> Flag={P+FVector2D(-3,7)*S,P+FVector2D(-3,-8)*S,P+FVector2D(6,-5)*S,P+FVector2D(-3,-2)*S};
  Line(Flag,FLinearColor(.02f,.025f,.03f),4,0);Line(Flag,FLinearColor(.48f,.91f,.76f),2,1);
 }
 if(Map->PingRemaining()>0)
 {
  const FVector2D P=Project(Map->PingPosition);if(!Inside(Map->PingPosition,P))return;const float A=FMath::Clamp(Map->PingRemaining()/.8f,0.f,1.f),R=8*S;
  TArray<FVector2D> Ring;for(int32 I=0;I<=32;++I) { const float T=I*2*PI/32;Ring.Add(P+FVector2D(FMath::Cos(T),FMath::Sin(T))*R); }
  Line(Ring,FLinearColor(.02f,.025f,.03f,A),4,2);Line(Ring,FLinearColor(.37f,.84f,1,A),2,3);
  Line({P+FVector2D(-3,0)*S,P+FVector2D(3,0)*S},FLinearColor(.37f,.84f,1,A),2,3);Line({P+FVector2D(0,-3)*S,P+FVector2D(0,3)*S},FLinearColor(.37f,.84f,1,A),2,3);
 }
}
void UBBSBWorldMapWidget::PaintWorldPing(const FGeometry& G,FSlateWindowElementList& E,int32 Layer) const
{
 if(!Map || Map->PingRemaining()<=0 || !Map->MapPlayer())return;
 const auto* Player=Map->MapPlayer();auto* PC=Cast<APlayerController>(Player->GetController());if(!PC || !PC->PlayerCameraManager)return;
 const FVector Point=Map->PingPosition+FVector::UpVector*65;FVector2D At=FVector2D::ZeroVector;const FVector2D Size=G.GetLocalSize(),Center=Size*.5;
 const float S=FMath::Clamp(float(Size.Y/720),.55f,1.8f);const FVector Delta=Point-PC->PlayerCameraManager->GetCameraLocation();
 const FQuat View=PC->PlayerCameraManager->GetCameraRotation().Quaternion();const bool Front=FVector::DotProduct(Delta,View.GetForwardVector())>0;
 const bool Projected=Front && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Point,At,true);
 if(!Projected)
 {
  FVector2D Direction(FVector::DotProduct(Delta,View.GetRightVector()),-FVector::DotProduct(Delta,View.GetUpVector()));if(Direction.IsNearlyZero())Direction=FVector2D(0,1);
  Direction.Normalize();At=Center+Direction*FMath::Max(Size.X,Size.Y);
 }
 const FVector2D Before=At;At.X=FMath::Clamp(At.X,48.*S,Size.X-48*S);At.Y=FMath::Clamp(At.Y,140.*S,Size.Y-72*S);
 const float Alpha=FMath::Clamp(Map->PingRemaining()/.8f,0.f,1.f);const FLinearColor Blue(.37f,.84f,1,Alpha),Dark(.02f,.025f,.035f,Alpha);
 auto Line=[&](const TArray<FVector2D>& P,FLinearColor C,float Width,int32 Z){FSlateDrawElement::MakeLines(E,Layer+Z,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,Width*S);};
 const float R=10*S;const TArray<FVector2D> Diamond={At+FVector2D(0,-R),At+FVector2D(R,0),At+FVector2D(0,R),At+FVector2D(-R,0),At+FVector2D(0,-R)};Line(Diamond,Dark,4,0);Line(Diamond,Blue,2,1);
 if(!Projected || !Before.Equals(At,.1))
 {
  const FVector2D D=(Before-Center).GetSafeNormal(),Right(-D.Y,D.X),Tip=At+D*18*S;
  const TArray<FVector2D> Arrow={Tip-D*6*S+Right*4*S,Tip,Tip-D*6*S-Right*4*S};Line(Arrow,Dark,4,0);Line(Arrow,Blue,2,1);
 }
 const FString Text=FString::Printf(TEXT("핑 · %.0f m"),FVector::Distance(Player->GetActorLocation(),Map->PingPosition)/100);
 auto Font=FCoreStyle::GetDefaultFontStyle("Bold",FMath::RoundToInt(12*S));Font.OutlineSettings.OutlineSize=1;Font.OutlineSettings.OutlineColor=Dark;
 const FVector2D TextAt(FMath::Clamp(At.X-38*S,18.*S,Size.X-130*S),At.Y+16*S);
 FSlateDrawElement::MakeText(E,Layer+2,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(TextAt)),Text,Font,ESlateDrawEffect::None,Blue);
}
