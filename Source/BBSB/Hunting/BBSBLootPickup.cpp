#include "Hunting/BBSBLootPickup.h"
#include "Hunting/BBSBItem.h"
#include "Hunting/BBSBHunting.h"
#include "BBSBCharacter.h"
#include "Combat/BBSBCombatVisuals.h"
#include "Field/BBSBBladeTrail.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABBSBLootPickup::ABBSBLootPickup()
{
 PrimaryActorTick.bCanEverTick=true;
 PickupQuery=CreateDefaultSubobject<USphereComponent>(TEXT("PickupQuery"));SetRootComponent(PickupQuery);
 PickupQuery->InitSphereRadius(23.f);PickupQuery->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 PickupQuery->SetCollisionObjectType(ECC_PhysicsBody);PickupQuery->SetCollisionResponseToAllChannels(ECR_Ignore);
 PickupQuery->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);PickupQuery->SetGenerateOverlapEvents(false);
 PickupQuery->SetCanEverAffectNavigation(false);
 Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemVisual"));Visual->SetupAttachment(PickupQuery);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
 Visual->SetStaticMesh(Mesh.Object);Visual->SetRelativeScale3D(MeshScale);Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Visual->SetCanEverAffectNavigation(false);Visual->SetCastShadow(false);
 RarityBeam=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RarityBeam"));RarityBeam->SetupAttachment(PickupQuery);
 RarityBeam->SetStaticMesh(Plane.Object);RarityBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);RarityBeam->SetCastShadow(false);
 RarityBeam->SetCanEverAffectNavigation(false);RarityBeam->SetReceivesDecals(false);RarityBeam->SetVisibility(false);
 BeamMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/Hunting/Materials/MI_LootBeam.MI_LootBeam")));
 TrailMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/VFX/Weapons/M_SwordBladeTrail.M_SwordBladeTrail")));
 ResourceGlowMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BBSB/Hunting/Rewards/MI_ResourceGlow.MI_ResourceGlow")));
 Tags.Add(TEXT("BBSBLoot"));
}
void ABBSBLootPickup::BeginPlay()
{
 Super::BeginPlay();
 if(Item)
 {
  if(Item->EquipmentKind==EBBSBItemEquipmentKind::Weapon)
  {
   Quantity=1;WeaponPayload.InstanceId=FGuid::NewGuid();WeaponPayload.Item=Item;WeaponPayload.Quantity=1;
   WeaponPayload.WeaponLevel=FMath::RandRange(FMath::Max(1,Item->MinimumDropLevel),FMath::Max(Item->MinimumDropLevel,Item->MaximumDropLevel));
   WeaponPayload.SocketCount=FMath::RandRange(FMath::Max(0,Item->MinimumDropSockets),FMath::Max(Item->MinimumDropSockets,Item->MaximumDropSockets));WeaponPayload.NormalizeWeapon();
  }
  if(Item->WorldMesh)Visual->SetStaticMesh(Item->WorldMesh);
  BBSBCombatVisuals::Tint(Visual,Item->Color);
  MeshScale=Item->IconGlyph==TEXT("Core") ? FVector(.23f):Item->IconGlyph==TEXT("Shard") ? FVector(.14f,.14f,.3f):FVector(.2f);
  Visual->SetRelativeScale3D(MeshScale);
  if(auto* Material=Item->IsFieldResource() ? nullptr:BeamMaterial.LoadSynchronous())
  {
   BeamMID=UMaterialInstanceDynamic::Create(Material,this);BeamMID->SetVectorParameterValue(TEXT("Tint"),BeamColor());
   RarityBeam->SetMaterial(0,BeamMID);
  }
 }
 if(Item && Item->RewardResource==EBBSBRewardResource::Note) { MeshScale=FVector(.24f,.24f,.055f);Visual->SetRelativeScale3D(MeshScale);Visual->SetRelativeRotation(FRotator(60,0,0)); }
 if(Item && Item->IsFieldResource())
 {
  bLargeResource=Quantity>1;
  MeshScale=(Item->RewardResource==EBBSBRewardResource::Note ? FVector(.14f,.14f,.035f):FVector(.11f,.11f,.16f))*(bLargeResource ? 1.55f:1.f);Visual->SetRelativeScale3D(MeshScale);
  Visual->SetReceivesDecals(false);RarityBeam->SetVisibility(false);bNameVisible=false;
  if(auto* Material=ResourceGlowMaterial.LoadSynchronous())
  {
   ResourceGlowMID=UMaterialInstanceDynamic::Create(Material,this);Visual->SetMaterial(0,ResourceGlowMID);
   FLinearColor Glow=Item->Color*ResourceGlowStrength;Glow.A=1;ResourceGlowMID->SetVectorParameterValue(TEXT("Color"),Glow);
  }
  // More individual resources need no name queries, light components or full-rate idle ticks.
  SetActorTickInterval(1.f/30.f);
 }
 if(auto* Player=Cast<ABBSBCharacter>(UGameplayStatics::GetPlayerPawn(this,0));!bCollectionVisualOnly && Player && Player->Hunting)
 { RegisteredWith=Player->Hunting;Player->Hunting->RegisterLoot(this); }
}
void ABBSBLootPickup::EndPlay(const EEndPlayReason::Type Reason)
{
 if(RegisteredWith.IsValid())RegisteredWith->UnregisterLoot(this);
 Super::EndPlay(Reason);
}
void ABBSBLootPickup::BeginDrop(FVector RestPoint)
{
 FlightFrom=GetActorLocation();Rest=RestPoint;FlightStartedAt=GetWorld()->GetTimeSeconds();bDropping=true;bDropDeflected=false;
 DropSeconds=FMath::Clamp(DropSeconds*FMath::FRandRange(.87f,1.13f),.2f,2.f);
 DropGravity=8.f*FMath::Max(20.f,DropArcHeight)*FMath::FRandRange(.88f,1.12f)/FMath::Square(DropSeconds);
 DropVelocity=(Rest-FlightFrom)/DropSeconds+FVector::UpVector*(DropGravity*DropSeconds*.5f);
 if(auto* Material=(Item && Item->IsFieldResource() && Quantity==1) ? nullptr:TrailMaterial.LoadSynchronous())
 {
  DropTrail=NewObject<UBBSBBladeTrailComponent>(this,TEXT("LootDropTrail"));AddInstanceComponent(DropTrail);
  DropTrail->SetupAttachment(PickupQuery);DropTrail->SetAbsolute(true,true,true);DropTrail->SetWorldLocation(FlightFrom);
  DropTrail->TrailMaterial=Material;DropTrail->Lifetime=.3f;DropTrail->Color=BeamColor();
  DropTrail->Color.A=.72f;DropTrail->RegisterComponent();AddTrailSample();
 }
}
void ABBSBLootPickup::AddTrailSample()
{
 auto* Trail=bClaimed ? CollectionTrail.Get():DropTrail.Get();if(!IsValid(Trail))return;
 const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
 const FVector ToEye=Camera ? Camera->GetCameraLocation()-GetActorLocation():FVector::RightVector;
 FVector Side=FVector::CrossProduct(DropVelocity,ToEye).GetSafeNormal();if(Side.IsNearlyZero())Side=FVector::RightVector;
 const float Width=bClaimed ? 8.5f:3.5f;Trail->AddBladeSample(GetActorLocation()-Side*Width,GetActorLocation()+Side*Width);
}
void ABBSBLootPickup::FinishDrop(const FVector& Point)
{
 SetActorLocation(Point);Rest=Point;DropVelocity=FVector::ZeroVector;bDropping=false;
 if(IsValid(DropTrail))DropTrail->Finish();
}
void ABBSBLootPickup::TickDrop(float DT)
{
 // Short ballistic steps prevent a long frame from skipping nearby geometry.
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBLootScatter),false,this);Query.AddIgnoredActor(GetOwner());
 float Left=FMath::Max(0.f,DT);
 while(bDropping && Left>0.f)
 {
  const float H=FMath::Min(Left,1.f/90.f);Left-=H;
  const FVector Start=GetActorLocation();const FVector End=Start+DropVelocity*H-FVector::UpVector*(DropGravity*H*H*.5f);
  DropVelocity.Z-=DropGravity*H;
  FHitResult Hit;
  if(GetWorld()->SweepSingleByObjectType(Hit,Start,End,FQuat::Identity,BBSBCombatVisuals::GeometryTypes(),FCollisionShape::MakeSphere(6.f),Query))
  {
   SetActorLocation(Hit.Location+Hit.ImpactNormal*1.f);
   if(Hit.ImpactNormal.Z>.45f && DropVelocity.Z<0)FinishDrop(Hit.ImpactPoint+Hit.ImpactNormal*14.f);
   else
   {
    bDropDeflected=true;DropVelocity=FMath::GetReflectionVector(DropVelocity,Hit.ImpactNormal)*.32f;
    if(Hit.bStartPenetrating)SetActorLocation(GetActorLocation()+Hit.ImpactNormal*8.f);
   }
  }
  else SetActorLocation(End);
  AddTrailSample();
 }
 const float Age=GetWorld()->GetTimeSeconds()-FlightStartedAt;
 if(bDropping && !bDropDeflected && Age>=DropSeconds)FinishDrop(Rest);
 else if(bDropping && Age>3.f)
 {
  FHitResult Floor;FVector Goal=Rest;
  if(GetWorld()->LineTraceSingleByObjectType(Floor,GetActorLocation()+FVector::UpVector*40,GetActorLocation()-FVector::UpVector*2000,BBSBCombatVisuals::GeometryTypes(),Query))Goal=Floor.ImpactPoint+Floor.ImpactNormal*14.f;
  FinishDrop(Goal);
 }
}
int32 ABBSBLootPickup::TakeQuantity(int32 Requested)
{
 if(!IsAvailable() || Requested<=0)return 0;
 const int32 Taken=FMath::Min(Quantity,Requested);Quantity-=Taken;return Taken;
}
void ABBSBLootPickup::BeginCollectionFlight(UBBSBHuntingComponent* Collector)
{
 if(bClaimed || Quantity>0 || !Collector)return;
 bClaimed=true;bDropping=false;bNameVisible=false;CollectingWith=Collector;FlightFrom=GetActorLocation();FlightStartedAt=GetWorld()->GetTimeSeconds();
 SetActorTickInterval(0);
 PickupQuery->SetCollisionEnabled(ECollisionEnabled::NoCollision);RarityBeam->SetVisibility(false);
 if(IsValid(DropTrail))DropTrail->Finish();
 CollectionColor=BeamColor();CollectionDestination=Collector->CollectionMouth();
 if(ResourceGlowMID) { auto Glow=CollectionColor*ResourceGlowStrength;Glow.A=1;ResourceGlowMID->SetVectorParameterValue(TEXT("Color"),Glow); }
 else BBSBCombatVisuals::Tint(Visual,CollectionColor);
 if(auto* Material=TrailMaterial.LoadSynchronous())
 {
  CollectionTrail=NewObject<UBBSBBladeTrailComponent>(this,TEXT("LootCollectionTrail"));AddInstanceComponent(CollectionTrail);
  CollectionTrail->SetupAttachment(PickupQuery);CollectionTrail->SetAbsolute(true,true,true);CollectionTrail->SetWorldLocation(FlightFrom);
  CollectionTrail->TrailMaterial=Material;CollectionTrail->Lifetime=.24f;CollectionTrail->Color=CollectionColor;CollectionTrail->Color.A=.9f;CollectionTrail->bSoftEdges=true;
  CollectionTrail->RegisterComponent();DropVelocity=(CollectionDestination-FlightFrom).GetSafeNormal()*100;AddTrailSample();
 }
 Visual->SetRenderCustomDepth(false);Visual->SetCustomDepthStencilValue(0);
}
void ABBSBLootPickup::PlayCollectionFlight(UBBSBHuntingComponent* Collector)
{
 if(!Collector)return;
 if(Quantity<=0) { BeginCollectionFlight(Collector);return; }
 // A partial stack stays available in the field. This non-interactive copy is visual only.
 const FTransform Transform=GetActorTransform();
 auto* Flight=GetWorld()->SpawnActorDeferred<ABBSBLootPickup>(ABBSBLootPickup::StaticClass(),Transform,GetOwner(),nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Flight)return;
 Flight->Item=Item;Flight->Quantity=0;Flight->bCollectionVisualOnly=true;Flight->CollectionFlightSeconds=CollectionFlightSeconds;
 Flight->FinishSpawning(Transform);Flight->BeginCollectionFlight(Collector);
}
void ABBSBLootPickup::UpdateNameVisibility()
{
 const double Now=GetWorld()->GetTimeSeconds();if(Now<NameQueryAt)return;NameQueryAt=Now+.2;
 bNameVisible=false;
 if(Item && Item->IsFieldResource())return;
 auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!PC || !PC->PlayerCameraManager || !Item || !IsAvailable())return;
 const auto* Hunting=RegisteredWith.Get();const float Range=Hunting ? Hunting->LootLabelDistance:2400.f;
 const FVector Eye=PC->PlayerCameraManager->GetCameraLocation(),Point=GetActorLocation()+FVector::UpVector*30;
 if(FVector::DistSquared(Eye,Point)>FMath::Square(Range))return;
 FVector2D Screen;if(!PC->ProjectWorldLocationToScreen(Point,Screen,true))return;
 int32 W=0,H=0;PC->GetViewportSize(W,H);if(Screen.X<-100 || Screen.Y<-40 || Screen.X>W+100 || Screen.Y>H+40)return;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(BBSBLootNameCover),false,this);Query.AddIgnoredActor(GetOwner());Query.AddIgnoredActor(PC->GetPawn());
 FHitResult Hit;bNameVisible=!GetWorld()->LineTraceSingleByObjectType(Hit,Eye,Point,BBSBCombatVisuals::GeometryTypes(),Query);
}
void ABBSBLootPickup::Tick(float DT)
{
 Super::Tick(DT);const double Now=GetWorld()->GetTimeSeconds();
 if(bClaimed)
 {
  if(!CollectingWith.IsValid()) { Destroy();return; }
  if(bCollectionArrived)return;
  const float T=FMath::Clamp(float((Now-FlightStartedAt)/FMath::Clamp(CollectionFlightSeconds,.15f,1.f)),0.f,1.f);
  const FVector Before=GetActorLocation();CollectionDestination=CollectingWith->CollectionMouth();const FVector Axis=(CollectionDestination-FlightFrom).GetSafeNormal();FVector Right,Up;Axis.FindBestAxisVectors(Right,Up);
  const float Angle=T*3*PI,Radius=FMath::Sin(PI*T)*(1-T)*24;
  SetActorLocation(FMath::Lerp(FlightFrom,CollectionDestination,T*T)+(Right*FMath::Cos(Angle)+Up*FMath::Sin(Angle))*Radius+FVector::UpVector*FMath::Sin(PI*T)*25);
  DropVelocity=(GetActorLocation()-Before)/FMath::Max(.0001f,DT);AddTrailSample();
  Visual->SetRelativeScale3D(MeshScale*FMath::Max(.01f,1-T*.98f));Visual->AddLocalRotation(FRotator(0,DT*600,DT*220));
  if(T>=1.f) { bCollectionArrived=true;Visual->SetVisibility(false);if(IsValid(CollectionTrail))CollectionTrail->Finish();SetLifeSpan(.28f); }return;
 }
 if(bDropping)TickDrop(DT);
 Visual->SetRelativeLocation(FVector(0,0,FMath::Sin(Now*2.8+GetUniqueID())*3.f));Visual->AddLocalRotation(FRotator(0,DT*48,0));
 if(Item && Item->IsFieldResource())
 {
  RarityBeam->SetVisibility(false);bNameVisible=false;
  if(ResourceGlowMID)
  { auto Glow=Item->Color*(ResourceGlowStrength*(bLargeResource ? 1.25f:1.f)*(.9f+.1f*FMath::Sin(Now*3.8+GetUniqueID())));Glow.A=1;ResourceGlowMID->SetVectorParameterValue(TEXT("Color"),Glow); }
  return;
 }
 RarityBeam->SetVisibility(Item && BeamMID && !bDropping && IsAvailable());
 if(Item && BeamMID && !bDropping)
 {
  const int32 Grade=FMath::Clamp(int32(Item->Rarity),0,4);const float Height=140.f+Grade*42.f,Width=10.f+Grade*2.5f;
  const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
  const FVector EyeDirection=Camera ? (Camera->GetCameraLocation()-GetActorLocation()).GetSafeNormal2D():FVector::ForwardVector;
  RarityBeam->SetWorldLocation(GetActorLocation()+FVector::UpVector*(Height*.5f-10.f));
  RarityBeam->SetWorldRotation(FRotationMatrix::MakeFromYZ(FVector::UpVector,EyeDirection).ToQuat());
  RarityBeam->SetWorldScale3D(FVector(Width/100.f,Height/100.f,1.f));
  BeamMID->SetScalarParameterValue(TEXT("Opacity"),.52f+.1f*FMath::Sin(Now*2.2f+Grade));
 }
 UpdateNameVisibility();
}
