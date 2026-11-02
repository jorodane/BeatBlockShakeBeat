#include "Character/BBSBViewOcclusion.h"
#include "BBSBCharacter.h"
#include "Character/BBSBVisibilityStencil.h"
#include "Combat/BBSBCombat.h"
#include "Combat/BBSBFieldAttack.h"
#include "Caravan/BBSBCaravan.h"
#include "Camera/CameraComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Interaction/BBSBInteraction.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstance.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollectionInstance.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "StaticMeshResources.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "UObject/UObjectIterator.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#endif
#include "Algo/Sort.h"
#include "Misc/ScopeExit.h"

namespace ViewOcclusion
{
 constexpr int32 InstanceSlot=15; // Reserved by the shared visibility function.
 struct FTriangleTree
 {
  struct FNode { FBox Box=FBox(ForceInit);int32 Begin=0,Count=0,Left=-1,Right=-1; };
  TArray<FVector> Points;TArray<int32> Indices,Order;TArray<FNode> Nodes;
  int32 BuildNode(int32 Begin,int32 Count)
  {
   const int32 Index=Nodes.AddDefaulted();FBox Centers(ForceInit),Bounds(ForceInit);
   for(int32 I=Begin;I<Begin+Count;++I)
   {
    const int32 T=Order[I]*3;const FVector A=Points[Indices[T]],B=Points[Indices[T+1]],C=Points[Indices[T+2]];
    Bounds+=A;Bounds+=B;Bounds+=C;Centers+=(A+B+C)/3.;
   }
   Nodes[Index].Box=Bounds;Nodes[Index].Begin=Begin;Nodes[Index].Count=Count;
   if(Count<=8)return Index;
   const FVector Size=Centers.GetSize();const int32 Axis=Size.X>Size.Y ? (Size.X>Size.Z ? 0:2):(Size.Y>Size.Z ? 1:2);
   Algo::Sort(MakeArrayView(Order.GetData()+Begin,Count),[&](int32 A,int32 B)
   {return Points[Indices[A*3]][Axis]+Points[Indices[A*3+1]][Axis]+Points[Indices[A*3+2]][Axis]<Points[Indices[B*3]][Axis]+Points[Indices[B*3+1]][Axis]+Points[Indices[B*3+2]][Axis];});
   const int32 Half=Count/2;const int32 Left=BuildNode(Begin,Half),Right=BuildNode(Begin+Half,Count-Half);
   Nodes[Index].Left=Left;Nodes[Index].Right=Right;Nodes[Index].Count=0;return Index;
  }
  void Build()
  {
   Nodes.Reset();Order.SetNum(Indices.Num()/3);for(int32 I=0;I<Order.Num();++I)Order[I]=I;
   if(Order.Num())BuildNode(0,Order.Num());
  }
  // The skeletal topology does not change with its pose. Keep the partitions
  // and refit their bounds instead of sorting every triangle on every scan.
  void Refit()
  {
   for(int32 N=Nodes.Num()-1;N>=0;--N)
   {
    auto& Node=Nodes[N];FBox Bounds(ForceInit);
    if(Node.Count)
     for(int32 I=Node.Begin;I<Node.Begin+Node.Count;++I)
     {
      const int32 T=Order[I]*3;Bounds+=Points[Indices[T]];Bounds+=Points[Indices[T+1]];Bounds+=Points[Indices[T+2]];
     }
    else { Bounds=Nodes[Node.Left].Box;Bounds+=Nodes[Node.Right].Box; }
    Node.Box=Bounds;
   }
  }
  static bool BoxHit(const FBox& Box,const FVector& Start,const FVector& Delta,double MaxT)
  {
   double Near=0.,Far=MaxT;
   for(int32 A=0;A<3;++A)
   {
    if(FMath::Abs(Delta[A])<1.e-10) { if(Start[A]<Box.Min[A] || Start[A]>Box.Max[A])return false;continue; }
    double T0=(Box.Min[A]-Start[A])/Delta[A],T1=(Box.Max[A]-Start[A])/Delta[A];if(T0>T1)Swap(T0,T1);
    Near=FMath::Max(Near,T0);Far=FMath::Min(Far,T1);if(Near>Far)return false;
   }
   return true;
  }
  bool TraceNode(int32 N,const FVector& Start,const FVector& Delta,double& Best) const
  {
   const auto& Node=Nodes[N];if(!BoxHit(Node.Box,Start,Delta,Best))return false;bool Found=false;
   if(!Node.Count) { Found|=TraceNode(Node.Left,Start,Delta,Best);Found|=TraceNode(Node.Right,Start,Delta,Best);return Found; }
   for(int32 I=Node.Begin;I<Node.Begin+Node.Count;++I)
   {
    const int32 T=Order[I]*3;const FVector A=Points[Indices[T]],E1=Points[Indices[T+1]]-A,E2=Points[Indices[T+2]]-A;
    const FVector P=FVector::CrossProduct(Delta,E2);const double Det=FVector::DotProduct(E1,P);if(FMath::Abs(Det)<1.e-10)continue;
    const double Inv=1./Det;const FVector V=Start-A;const double U=FVector::DotProduct(V,P)*Inv;if(U<0. || U>1.)continue;
    const FVector Q=FVector::CrossProduct(V,E1);const double W=FVector::DotProduct(Delta,Q)*Inv;if(W<0. || U+W>1.)continue;
    const double Time=FVector::DotProduct(E2,Q)*Inv;
    if(Time>=0. && Time<Best) { Best=Time;Found=true; }
   }
   return Found;
  }
  bool Trace(const FVector& Start,const FVector& End,double& Time) const
  {return Nodes.Num() && TraceNode(0,Start,End-Start,Time);}
 };
}

struct FBBSBViewOcclusionQueries
{
 TWeakObjectPtr<USkeletalMesh> PlayerAsset;
 TMap<TWeakObjectPtr<UStaticMesh>,ViewOcclusion::FTriangleTree> StaticTrees;
 ViewOcclusion::FTriangleTree PlayerTree;
 TArray<FVector> Samples;
 bool MakePlayerSamples(USkeletalMeshComponent* Mesh,FVector Eye,FRotator Rotation,float FOV,float Aspect)
 {
  Samples.Reset();auto* Asset=Mesh->GetSkeletalMeshAsset();auto* Render=Asset ? Asset->GetResourceForRendering():nullptr;
  if(!Render || Render->LODRenderData.IsEmpty())return false;
  const auto& LOD=Render->LODRenderData[0];auto* Weights=Mesh->GetSkinWeightBuffer(0);if(!Weights)return false;
  TArray<FMatrix44f> Matrices;Mesh->GetCurrentRefToLocalMatrices(Matrices,0);
  TArray<FVector3f> Positions;USkinnedMeshComponent::ComputeSkinnedPositions(Mesh,Positions,Matrices,LOD,*Weights);
  const FTransform Transform=Mesh->GetComponentTransform();PlayerTree.Points.SetNum(Positions.Num());
  for(int32 I=0;I<Positions.Num();++I)PlayerTree.Points[I]=Transform.TransformPosition(FVector(Positions[I]));
  const bool TopologyChanged=PlayerAsset!=Asset || PlayerTree.Indices.Num()!=LOD.MultiSizeIndexContainer.GetIndexBuffer()->Num();
  if(TopologyChanged)
  {
   const auto* Buffer=LOD.MultiSizeIndexContainer.GetIndexBuffer();PlayerTree.Indices.SetNum(Buffer->Num());
   for(int32 I=0;I<Buffer->Num();++I)PlayerTree.Indices[I]=Buffer->Get(I);PlayerAsset=Asset;
  }
  if(TopologyChanged || PlayerTree.Nodes.IsEmpty())PlayerTree.Build();else PlayerTree.Refit();
  if(PlayerTree.Nodes.IsEmpty())return false;
  const FRotationMatrix Basis(Rotation);const FVector Forward=Basis.GetUnitAxis(EAxis::X),Right=Basis.GetUnitAxis(EAxis::Y),Up=Basis.GetUnitAxis(EAxis::Z);
  const double Tan=FMath::Tan(FMath::DegreesToRadians(FOV*.5));FVector2D Min(DBL_MAX,DBL_MAX),Max(-DBL_MAX,-DBL_MAX);
  for(const auto& P:PlayerTree.Points)
  {
   const FVector D=P-Eye;const double Z=FVector::DotProduct(D,Forward);if(Z<1.)continue;
   const FVector2D UV(.5+FVector::DotProduct(D,Right)/(2.*Tan*Z),.5-FVector::DotProduct(D,Up)*Aspect/(2.*Tan*Z));
   Min.X=FMath::Min(Min.X,UV.X);Min.Y=FMath::Min(Min.Y,UV.Y);Max.X=FMath::Max(Max.X,UV.X);Max.Y=FMath::Max(Max.Y,UV.Y);
  }
  if(Min.X>Max.X)return false;
  auto Sample=[&](FVector2D UV)
  {
   const FVector End=Eye+(Forward+Right*((UV.X-.5)*2.*Tan)-Up*((UV.Y-.5)*2.*Tan/Aspect))*5000.;double T=1.;
   if(PlayerTree.Trace(Eye,End,T))Samples.Add(FMath::Lerp(Eye,End,FMath::Max(0.,T-.0001)));
  };
  for(int32 Y=0;Y<20;++Y)for(int32 X=0;X<12;++X)Sample(FVector2D(FMath::Lerp(Min.X,Max.X,(X+.5)/12.),FMath::Lerp(Min.Y,Max.Y,(Y+.5)/20.)));
  // Thin hands and ears can lie between regular grid samples.
  for(const TCHAR* Bone:{TEXT("head"),TEXT("pelvis"),TEXT("spine_03"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")})
  {
   const FVector D=Mesh->GetSocketLocation(Bone)-Eye;const double Z=FVector::DotProduct(D,Forward);if(Z>1.)Sample(FVector2D(.5+FVector::DotProduct(D,Right)/(2.*Tan*Z),.5-FVector::DotProduct(D,Up)*Aspect/(2.*Tan*Z)));
  }
  return Samples.Num()>0;
 }
 const ViewOcclusion::FTriangleTree* Tree(UStaticMesh* Mesh,const UBBSBViewOcclusionMaterials* Library)
 {
  if(auto* Existing=StaticTrees.Find(Mesh))return Existing;
  const auto* Data=Library->Geometry.Find(Mesh);if(!Data || Data->Indices.IsEmpty())return nullptr;
  auto& Result=StaticTrees.Add(Mesh);Result.Points=Data->Positions;Result.Indices=Data->Indices;Result.Build();return &Result;
 }
 bool Blocks(const ViewOcclusion::FTriangleTree& Tree,const FTransform& Transform,FVector Eye) const
 {
  const FVector Start=Transform.InverseTransformPosition(Eye);
  for(const auto& Target:Samples) { double T=.9999;if(Tree.Trace(Start,Transform.InverseTransformPosition(Target),T))return true; }
  return false;
 }
};

TArray<UMaterialInterface*> UBBSBViewOcclusionMaterials::SceneMaterials()
{
 TArray<UMaterialInterface*> Result;
#if WITH_EDITOR
 for(TObjectIterator<UMeshComponent> It;It;++It)
 {
  const AActor* Owner=It->GetOwner();const UWorld* World=It->GetWorld();
  const bool Scene=World && World->WorldType==EWorldType::Editor && Owner && !Owner->IsEditorOnly();
  const bool Template=Owner && Owner->HasAnyFlags(RF_ClassDefaultObject) && (Owner->GetClass()->GetPathName().StartsWith(TEXT("/Script/BBSB.")) || Owner->GetClass()->GetPathName().StartsWith(TEXT("/Game/")));
  if(!Scene && !Template)continue;
  for(auto* Material:It->GetMaterials())if(Material && !Material->GetPathName().Contains(TEXT("/BBSB/Visibility/")))Result.AddUnique(Material);
 }
#endif
 return Result;
}
bool UBBSBViewOcclusionMaterials::PrepareMaskedMaterial(UMaterial* Material)
{
#if WITH_EDITOR
 if(!Material || !Material->GetPathName().StartsWith(TEXT("/Game/BBSB/Visibility/Materials/")))return false;
 auto* EditorData=Material->GetEditorOnlyData();
 const bool UsesImportedConstant=EditorData && EditorData->OpacityMask.Expression && EditorData->OpacityMask.UseConstant;
 if(Material->bCanMaskedBeAssumedOpaque || Material->BlendMode!=BLEND_Masked || UsesImportedConstant)
 {
  Material->Modify();Material->bCanMaskedBeAssumedOpaque=false;Material->BlendMode=BLEND_Masked;
  // Imported materials can retain UseConstant=1 even after Connect() wires an expression.
  // FScalarMaterialInput compiles that constant before consulting the connected graph.
  if(EditorData && EditorData->OpacityMask.Expression)EditorData->OpacityMask.UseConstant=false;
  Material->PostEditChange();Material->MarkPackageDirty();return true;
 }
#endif
 return false;
}
int32 UBBSBViewOcclusionMaterials::CacheLoadedGeometry()
{
 int32 Changed=0;
#if WITH_EDITOR
 TSet<UStaticMesh*> Used;
 for(TObjectIterator<UStaticMeshComponent> It;It;++It)
 {
  const AActor* Owner=It->GetOwner();const UWorld* World=It->GetWorld();
  const bool Scene=World && World->WorldType==EWorldType::Editor && Owner && !Owner->IsEditorOnly();
  const bool Template=Owner && Owner->HasAnyFlags(RF_ClassDefaultObject) && (Owner->GetClass()->GetPathName().StartsWith(TEXT("/Script/BBSB.")) || Owner->GetClass()->GetPathName().StartsWith(TEXT("/Game/")));
  if((Scene || Template) && It->GetStaticMesh())Used.Add(It->GetStaticMesh());
 }
 for(auto* Mesh:Used)
 {
  auto* Render=Mesh->GetRenderData();if(!Render || Render->LODResources.IsEmpty())continue;
  if(const auto* Old=Geometry.Find(Mesh);Old && Old->SourceGuid==Mesh->GetLightingGuid())continue;
  const auto& LOD=Render->LODResources[0];if(!LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices() || !LOD.IndexBuffer.GetNumIndices())continue;
  auto& Data=Geometry.FindOrAdd(Mesh);Data.SourceGuid=Mesh->GetLightingGuid();Data.Positions.SetNum(LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices());
  for(int32 I=0;I<Data.Positions.Num();++I)Data.Positions[I]=FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I));
  Data.Indices.SetNum(LOD.IndexBuffer.GetNumIndices());for(int32 I=0;I<Data.Indices.Num();++I)Data.Indices[I]=LOD.IndexBuffer.GetIndex(I);++Changed;
 }
 if(Changed)MarkPackageDirty();
 FAssetCompilingManager::Get().FinishAllCompilation();
 if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
#endif
 return Changed;
}

namespace
{
 constexpr int32 WeightIndex=24,KindIndex=25,InstanceKindIndex=26;
 bool OtherCharacter(const AActor* A) { return A && (A->IsA<APawn>() || A->IsA<ABBSBInteractionNPC>() || A->FindComponentByClass<USkeletalMeshComponent>()); }
 bool KeepVisible(const AActor* A,const ABBSBCharacter* Player)
 { return !IsValid(A) || A==Player || A->IsOwnedBy(Player) || A->IsA<ALandscapeProxy>() || A->ActorHasTag(TEXT("BBSBKeepVisible")) || A->ActorHasTag(TEXT("BBSBIgnoreAim")); }
 struct FViewOcclusionProjection
 {
  FVector Eye,Forward,Right,Up;float Scale,Aspect;
  FViewOcclusionProjection(FVector InEye,FRotator Rotation,float FOV,float InAspect):Eye(InEye),Aspect(InAspect)
  {
   FRotationMatrix Basis(Rotation);Forward=Basis.GetUnitAxis(EAxis::X);Right=Basis.GetUnitAxis(EAxis::Y);Up=Basis.GetUnitAxis(EAxis::Z);
   Scale=1.f/(2.f*FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FOV,10.f,160.f)*.5f)));
  }
  double Depth(FVector P) const { return FVector::DotProduct(P-Eye,Forward); }
  bool Project(FVector P,FVector2D& UV) const
  {
   const FVector D=P-Eye;const double Z=Depth(P);if(Z<1.) return false;
   UV=FVector2D(.5+FVector::DotProduct(D,Right)*Scale/Z,.5-FVector::DotProduct(D,Up)*Scale*Aspect/Z);return true;
  }
 };
 void Corners(const FBox& Box,TArray<FVector>& Out)
 {
  for(int32 I=0;I<8;++I) Out.Add(FVector(I&1 ? Box.Max.X:Box.Min.X,I&2 ? Box.Max.Y:Box.Min.Y,I&4 ? Box.Max.Z:Box.Min.Z));
 }
 bool IntersectsScreenCircle(const FBox& Box,const FViewOcclusionProjection& View,FVector2D Center,float Radius,float MaxDepth)
 {
  if(Box.IsInsideOrOn(View.Eye)) return true;
  TArray<FVector> Points;Corners(Box,Points);double Near=DBL_MAX,Far=-DBL_MAX;FVector2D Min(DBL_MAX,DBL_MAX),Max(-DBL_MAX,-DBL_MAX);
  for(const auto& P:Points)
  {
   const double Z=View.Depth(P);Near=FMath::Min(Near,Z);Far=FMath::Max(Far,Z);FVector2D UV;
   if(View.Project(P,UV)) { Min.X=FMath::Min(Min.X,UV.X);Min.Y=FMath::Min(Min.Y,UV.Y);Max.X=FMath::Max(Max.X,UV.X);Max.Y=FMath::Max(Max.Y,UV.Y); }
  }
  if(Far<1. || Near>MaxDepth) return false;
  // Clip near-plane crossings rather than rejecting the box or treating every
  // large off-screen mesh as an occluder when it extends behind the camera.
  if(Near<1.) for(int32 I=0;I<8;++I) for(int32 Axis=0;Axis<3;++Axis)
  {
   const int32 J=I^(1<<Axis);if(J<I) continue;
   const double A=View.Depth(Points[I]),B=View.Depth(Points[J]);
   if((A<1.)==(B<1.)) continue;
   const FVector Clipped=FMath::Lerp(Points[I],Points[J],(1.01-A)/(B-A));FVector2D UV;
   if(View.Project(Clipped,UV)) { Min.X=FMath::Min(Min.X,UV.X);Min.Y=FMath::Min(Min.Y,UV.Y);Max.X=FMath::Max(Max.X,UV.X);Max.Y=FMath::Max(Max.Y,UV.Y); }
  }
  const FVector2D Closest(FMath::Clamp(Center.X,Min.X,Max.X),FMath::Clamp(Center.Y,Min.Y,Max.Y));
  return FVector2D((Closest.X-Center.X)*View.Aspect,Closest.Y-Center.Y).SizeSquared()<=FMath::Square(Radius);
 }
}

UBBSBViewOcclusionComponent::UBBSBViewOcclusionComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
 MaterialLibrary=TSoftObjectPtr<UBBSBViewOcclusionMaterials>(FSoftObjectPath(TEXT("/Game/BBSB/Visibility/DA_ViewOcclusion.DA_ViewOcclusion")));
}
void UBBSBViewOcclusionComponent::BeginPlay()
{
 Super::BeginPlay();Character=Cast<ABBSBCharacter>(GetOwner());Library=MaterialLibrary.LoadSynchronous();
 if(FMath::IsNearlyEqual(CircleRadius,.19f)) CircleRadius=.22f;
 if(FMath::IsNearlyEqual(CircleFeather,.025f)) CircleFeather=.07f;
 ScreenCircleRadius=CircleRadius;
 SpawnHandler=GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this,&UBBSBViewOcclusionComponent::PrepareCharacterCamera));
 for(TActorIterator<AActor> It(GetWorld());It;++It) PrepareCharacterCamera(*It);
}
UBBSBViewOcclusionComponent::~UBBSBViewOcclusionComponent()=default;
float UBBSBViewOcclusionComponent::OcclusionWeight(UMeshComponent* Mesh) const
{ for(const auto& State:States) if(State.Mesh.Get()==Mesh) return State.Weight;return 0; }
FString UBBSBViewOcclusionComponent::MaterialDiagnostics(UMeshComponent* Mesh) const
{
 if(!Mesh || !Mesh->GetMaterial(0))return TEXT("none");const auto* Material=Mesh->GetMaterial(0);const auto& Data=Mesh->GetCustomPrimitiveData().Data;
 return FString::Printf(TEXT("Material=%s; Blend=%d; Masked=%d; RootBlend=%d; Clip=%f; Data24=%f; Data25=%f; Data26=%f"),*Material->GetPathName(),int32(Material->GetBlendMode()),Material->IsMasked(),int32(Material->GetMaterial()->GetBlendMode()),Material->GetOpacityMaskClipValue(),Data.IsValidIndex(24)?Data[24]:-1.f,Data.IsValidIndex(25)?Data[25]:-1.f,Data.IsValidIndex(26)?Data[26]:-1.f);
}

void UBBSBViewOcclusionComponent::PrepareCharacterCamera(AActor* Actor)
{
 if(!bEnabled || !Character || KeepVisible(Actor,Character))return;
 const bool IsCharacter=OtherCharacter(Actor);
 if(IsCharacter ? !bCharacterFadeEnabled:!bEnvironmentMaskEnabled)return;
 // Camera avoidance is independent of reveal eligibility. An off-center post
 // must not push the lens into the hero just because that post stays opaque.
 TArray<UPrimitiveComponent*> Parts;Actor->GetComponents(Parts);
 for(auto* Part:Parts)
 {
  if(IsCharacter)
  {
   if(!CharacterCameraResponses.Contains(Part))CharacterCameraResponses.Add(Part,Part->GetCollisionResponseToChannel(ECC_Camera));
   Part->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
  }
  else if(Part->IsA<UMeshComponent>())IgnoreCamera(Part);
 }
}
bool UBBSBViewOcclusionComponent::CharacterTouchesLens(AActor* Actor,const FVector& Eye) const
{
 TArray<UMeshComponent*> Meshes;Actor->GetComponents(Meshes);
 for(auto* Mesh:Meshes)if(Mesh->IsVisible() && Mesh->Bounds.GetBox().ComputeSquaredDistanceToPoint(Eye)<=FMath::Square(CharacterLensRadius+18.f))return true;
 return false;
}
void UBBSBViewOcclusionComponent::UpdateCharacterProtection(const FVector& Eye)
{
 WantedCharacters.Reset();TSet<TWeakObjectPtr<AActor>> Protected;
 // Projectile counts must not increase the cost of this camera safety pass.
 TSet<AActor*> Characters;
 for(TActorIterator<APawn> It(GetWorld());It;++It)Characters.Add(*It);
 for(TObjectIterator<USkeletalMeshComponent> It;It;++It)
  if(It->GetWorld()==GetWorld() && It->GetOwner())Characters.Add(It->GetOwner());
 for(auto* Actor:Characters)
 {
  if(Actor!=Character && KeepVisible(Actor,Character))continue;
  PrepareCharacterCamera(Actor);if(!CharacterTouchesLens(Actor,Eye))continue;
  TArray<UMeshComponent*> Meshes;Actor->GetComponents(Meshes);bool Critical=false;
  for(auto* Mesh:Meshes)
  {
   if(!Mesh->IsVisible())continue;
   Critical|=Mesh->Bounds.GetBox().ComputeSquaredDistanceToPoint(Eye)<=FMath::Square(CharacterLensRadius+2.f);
   if(AddMesh(Mesh,true))WantedCharacters.Add(Mesh);
  }
  if(Critical)
  {
   if(!HiddenCharacters.Contains(Actor))HiddenCharacters.Add(Actor,Actor->IsHidden());
   Actor->SetActorHiddenInGame(true);Protected.Add(Actor);
  }
 }
 for(auto It=HiddenCharacters.CreateIterator();It;++It)if(!Protected.Contains(It.Key()))
 {if(auto* Actor=It.Key().Get())Actor->SetActorHiddenInGame(It.Value());It.RemoveCurrent();}
}
bool UBBSBViewOcclusionComponent::AddMesh(UMeshComponent* Mesh,bool bOtherCharacter)
{
 if(!IsValid(Mesh) || !Mesh->IsVisible() || Mesh->ComponentHasTag(TEXT("BBSBKeepVisible"))) return false;
 for(const auto& State:States) if(State.Mesh.Get()==Mesh) return true;
 FBBSBOccludingMesh State;State.Mesh=Mesh;State.bCharacter=bOtherCharacter;
 State.OriginalShadowInvalidation=uint8(Mesh->ShadowCacheInvalidationBehavior);State.bOriginalHidden=Mesh->bHiddenInGame;
 const auto& Data=Mesh->GetCustomPrimitiveData().Data;
 State.OriginalWeight=Data.IsValidIndex(WeightIndex) ? Data[WeightIndex]:0.f;State.OriginalKind=Data.IsValidIndex(KindIndex) ? Data[KindIndex]:0.f;State.OriginalInstanceKind=Data.IsValidIndex(InstanceKindIndex) ? Data[InstanceKindIndex]:0.f;
 bool Supported=false;
 for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
 {
  auto* Original=Mesh->GetMaterial(Slot);State.Originals.Add(Original);UMaterialInterface* Source=Original;
  while(auto* MID=Cast<UMaterialInstanceDynamic>(Source)) Source=MID->Parent;
  auto* Found=Library->Variants.Find(Source);
  // A new material instance inherits coverage from its parent automatically.
  while(!Found)
  {
   auto* Instance=Cast<UMaterialInstance>(Source);if(!Instance)break;Source=Instance->Parent;Found=Library->Variants.Find(Source);
  }
  UMaterialInterface* Replacement=Original;
  if(Found && *Found)
  {
   auto* Dynamic=UMaterialInstanceDynamic::Create(*Found,this);Dynamic->CopyMaterialUniformParameters(Original);
   Dynamic->SetScalarParameterValue(TEXT("BBSB_ViewOcclusionWeight"),0.f);
   Dynamic->SetScalarParameterValue(TEXT("BBSB_ViewOcclusionKind"),bOtherCharacter ? 1.f:0.f);
   Dynamic->SetScalarParameterValue(TEXT("BBSB_ViewInstanceKind"),Mesh->IsA<UInstancedStaticMeshComponent>() ? 1.f:0.f);
   Replacement=Dynamic;Supported=true;
  }
  State.Replacements.Add(Replacement);
 }
 State.bUnsupported=!Supported;
 if(auto* Instances=Cast<UInstancedStaticMeshComponent>(Mesh))
 {
  State.OriginalInstanceFloats=Instances->NumCustomDataFloats;State.OriginalInstanceData=Instances->PerInstanceSMCustomData;
  const int32 Count=Instances->GetInstanceCount(),Stride=FMath::Max(State.OriginalInstanceFloats,ViewOcclusion::InstanceSlot+1);
  Instances->SetNumCustomDataFloats(Stride);
  for(int32 I=0;I<Count;++I)
  {
   for(int32 J=0;J<State.OriginalInstanceFloats;++J)if(State.OriginalInstanceData.IsValidIndex(I*State.OriginalInstanceFloats+J))Instances->SetCustomDataValue(I,J,State.OriginalInstanceData[I*State.OriginalInstanceFloats+J],false);
   Instances->SetCustomDataValue(I,ViewOcclusion::InstanceSlot,0.f,false);
  }
  Mesh->SetCustomPrimitiveDataFloat(InstanceKindIndex,1.f);
 }
 else Mesh->SetCustomPrimitiveDataFloat(InstanceKindIndex,0.f);
 Mesh->ShadowCacheInvalidationBehavior=EShadowCacheInvalidationBehavior::Always;
 for(int32 Slot=0;Slot<State.Replacements.Num();++Slot) Mesh->SetMaterial(Slot,State.Replacements[Slot]);
 Mesh->SetCustomPrimitiveDataFloat(WeightIndex,0.f);Mesh->SetCustomPrimitiveDataFloat(KindIndex,bOtherCharacter ? 1.f:0.f);States.Add(MoveTemp(State));return true;
}
void UBBSBViewOcclusionComponent::IgnoreCamera(UPrimitiveComponent* Primitive)
{
 if(!IsValid(Primitive)) return;
 if(!CameraResponses.Contains(Primitive)) CameraResponses.Add(Primitive,Primitive->GetCollisionResponseToChannel(ECC_Camera));
 Primitive->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
}
void UBBSBViewOcclusionComponent::Scan(const FVector& Camera,const FVector& Focus)
{
 const double ScanStarted=FPlatformTime::Seconds();
 ON_SCOPE_EXIT { LastScanMilliseconds=float((FPlatformTime::Seconds()-ScanStarted)*1000.); };
 LastPlayerSamplingMilliseconds=0;PlayerSampleTriangles=0;
 WantedGeometry.Reset();WantedInstances.Reset();
 bool SamplesAttempted=false,SamplesReady=false;
 auto EnsureSamples=[&]()
 {
  if(!SamplesAttempted)
  {
   SamplesAttempted=true;const double Started=FPlatformTime::Seconds();
   SamplesReady=Queries->MakePlayerSamples(Character->GetMesh(),Camera,ReferenceRotation,ReferenceFOV,ScreenAspect);
   LastPlayerSamplingMilliseconds=float((FPlatformTime::Seconds()-Started)*1000.);
   PlayerSampleTriangles=Queries->PlayerTree.Indices.Num()/3;
  }
  return SamplesReady;
 };
 const FViewOcclusionProjection View(Camera,ReferenceRotation,ReferenceFOV,ScreenAspect);
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  auto* Actor=*It;if(KeepVisible(Actor,Character) || OtherCharacter(Actor))continue;PrepareCharacterCamera(Actor);
  TArray<UStaticMeshComponent*> Meshes;Actor->GetComponents(Meshes);
  for(auto* Mesh:Meshes)
  {
   if(!Mesh->IsVisible() || Mesh->ComponentHasTag(TEXT("BBSBKeepVisible")))continue;
   if(!IntersectsScreenCircle(Mesh->Bounds.GetBox(),View,ScreenCircleCenter,ScreenCircleRadius+CircleFeather,RevealDepth))continue;
   const auto* Tree=Queries->Tree(Mesh->GetStaticMesh(),Library);if(!Tree)continue;
   if(auto* Instances=Cast<UInstancedStaticMeshComponent>(Mesh))
   {
    TSet<int32> Hits;
    for(int32 I=0;I<Instances->GetInstanceCount();++I)
    {
     FTransform Transform;Instances->GetInstanceTransform(I,Transform,true);
     if(!IntersectsScreenCircle(Tree->Nodes[0].Box.TransformBy(Transform),View,ScreenCircleCenter,ScreenCircleRadius+CircleFeather,RevealDepth))continue;
     if(EnsureSamples() && Queries->Blocks(*Tree,Transform,Camera))Hits.Add(I);
    }
    if(Hits.Num() && AddMesh(Mesh,false)) { WantedGeometry.Add(Mesh);WantedInstances.Add(Mesh,MoveTemp(Hits)); }
   }
   else if(EnsureSamples() && Queries->Blocks(*Tree,Mesh->GetComponentTransform(),Camera) && AddMesh(Mesh,false))WantedGeometry.Add(Mesh);
  }
 }
}
void UBBSBViewOcclusionComponent::UpdateCollection(const FVector& Camera,const FRotator& Rotation,float FOV,int32 W,int32 H,const FVector& Focus)
{
 auto* MPC=GetWorld()->GetParameterCollectionInstance(Library->Collection);if(!MPC) return;
 ScreenAspect=float(W)/FMath::Max(1,H);
 ReferenceEye=Character->GetCameraBoom()->IsCollisionFixApplied() ? Character->GetCameraBoom()->GetUnfixedCameraPosition():Camera;
 ReferenceRotation=Rotation;ReferenceFOV=FOV;
 const FViewOcclusionProjection Actual(Camera,Rotation,FOV,ScreenAspect),Reference(ReferenceEye,Rotation,FOV,ScreenAspect);
 TArray<FVector> Points;Corners(Character->GetMesh()->Bounds.GetBox(),Points);
 auto Fit=[&](const FViewOcclusionProjection& P,FVector2D& Center,float& Radius,float& Depth)
 {
  FVector2D Min(DBL_MAX,DBL_MAX),Max(-DBL_MAX,-DBL_MAX);Depth=0.f;
  for(const auto& Point:Points)
  {
   FVector2D UV;if(!P.Project(Point,UV)) return false;
   Min.X=FMath::Min(Min.X,UV.X);Min.Y=FMath::Min(Min.Y,UV.Y);Max.X=FMath::Max(Max.X,UV.X);Max.Y=FMath::Max(Max.Y,UV.Y);
   Depth=FMath::Max(Depth,float(P.Depth(Point)));
  }
  Center=(Min+Max)*.5;Radius=FVector2D((Max.X-Min.X)*ScreenAspect,Max.Y-Min.Y).Size()*.5+.025f;Depth+=35.f;return true;
 };
 FVector2D Center;float Radius=CircleRadius,Depth=0.f;
 if(!Fit(Actual,Center,Radius,Depth))
 {
  if(!Fit(Reference,Center,Radius,Depth)) { Center=ScreenCircleCenter;Radius=CircleRadius;Depth=FMath::Max(100.,Actual.Depth(Focus)+130.); }
 }
 ScreenCircleCenter=Center;ScreenCircleRadius=FMath::Clamp(bFitPlayerSilhouette ? FMath::Max(CircleRadius,Radius):CircleRadius,.05f,.65f);RevealDepth=Depth;
 FVector2D ReferenceCenter;float ReferenceRadius,ReferenceDepth;
 const bool ReferenceValid=Fit(Reference,ReferenceCenter,ReferenceRadius,ReferenceDepth);
 ReferenceRevealDepth=ReferenceValid ? ReferenceDepth:Depth;
 ReferenceCircleCenter=ReferenceValid ? ReferenceCenter:Center;
 ReferenceCircleRadius=FMath::Clamp(bFitPlayerSilhouette && ReferenceValid ? FMath::Max(CircleRadius,ReferenceRadius):CircleRadius,.05f,.65f);
 MPC->SetVectorParameterValue(TEXT("OcclusionScreen"),FLinearColor(Center.X,Center.Y,ScreenAspect,ScreenCircleRadius));
 MPC->SetVectorParameterValue(TEXT("OcclusionSoft"),FLinearColor(CircleFeather,RevealDepth,20.f,0.f));
 // Expose the game camera basis for material authoring. The active shadow
 // variant fades only the selected occluder instances, keeping the window readable.
 const FRotationMatrix Basis(Rotation);
 const FVector Forward=Basis.GetUnitAxis(EAxis::X),Right=Basis.GetUnitAxis(EAxis::Y),Up=Basis.GetUnitAxis(EAxis::Z);
 MPC->SetVectorParameterValue(TEXT("OcclusionCamera"),FLinearColor(Camera.X,Camera.Y,Camera.Z,0));
 MPC->SetVectorParameterValue(TEXT("OcclusionForward"),FLinearColor(Forward.X,Forward.Y,Forward.Z,0));
 MPC->SetVectorParameterValue(TEXT("OcclusionRight"),FLinearColor(Right.X,Right.Y,Right.Z,0));
 MPC->SetVectorParameterValue(TEXT("OcclusionUp"),FLinearColor(Up.X,Up.Y,Up.Z,0));
 MPC->SetVectorParameterValue(TEXT("OcclusionView"),FLinearColor(Actual.Scale,0,0,0));
}
void UBBSBViewOcclusionComponent::RestoreMesh(FBBSBOccludingMesh& State)
{
 auto* Mesh=State.Mesh.Get();if(!Mesh) return;
 Mesh->ShadowCacheInvalidationBehavior=EShadowCacheInvalidationBehavior(State.OriginalShadowInvalidation);Mesh->SetHiddenInGame(State.bOriginalHidden);
 if(auto* Instances=Cast<UInstancedStaticMeshComponent>(Mesh))
 {
  Instances->SetNumCustomDataFloats(State.OriginalInstanceFloats);Instances->PerInstanceSMCustomData=State.OriginalInstanceData;
 }
 Mesh->SetCustomPrimitiveDataFloat(InstanceKindIndex,State.OriginalInstanceKind);Mesh->MarkRenderStateDirty();
 for(int32 Slot=0;Slot<State.Originals.Num();++Slot) if(Mesh->GetMaterial(Slot)==State.Replacements[Slot]) Mesh->SetMaterial(Slot,State.Originals[Slot]);
 Mesh->SetCustomPrimitiveDataFloat(WeightIndex,State.OriginalWeight);Mesh->SetCustomPrimitiveDataFloat(KindIndex,State.OriginalKind);
}
void UBBSBViewOcclusionComponent::RestoreGeometryVisibility()
{
 for(int32 I=States.Num()-1;I>=0;--I)if(!States[I].bCharacter) { RestoreMesh(States[I]);States.RemoveAtSwap(I); }
 WantedGeometry.Reset();WantedInstances.Reset();
 for(auto& Pair:CameraResponses) if(auto* P=Pair.Key.Get()) P->SetCollisionResponseToChannel(ECC_Camera,Pair.Value);CameraResponses.Reset();
 Queries.Reset();NextScanAt=0;GeometryOccluders=0;LastScanMilliseconds=LastPlayerSamplingMilliseconds=0;PlayerSampleTriangles=0;
}
void UBBSBViewOcclusionComponent::RestoreCharacterVisibility()
{
 for(int32 I=States.Num()-1;I>=0;--I)if(States[I].bCharacter) { RestoreMesh(States[I]);States.RemoveAtSwap(I); }
 WantedCharacters.Reset();
 for(auto& Pair:CharacterCameraResponses) if(auto* P=Pair.Key.Get()) P->SetCollisionResponseToChannel(ECC_Camera,Pair.Value);CharacterCameraResponses.Reset();
 for(auto& Pair:HiddenCharacters)if(auto* Actor=Pair.Key.Get())Actor->SetActorHiddenInGame(Pair.Value);HiddenCharacters.Reset();
 CharacterOccluders=0;
}
void UBBSBViewOcclusionComponent::RestoreVisibility()
{
 RestoreGeometryVisibility();RestoreCharacterVisibility();
 GeometryOccluders=CharacterOccluders=FallbackOccluders=0;
}
void UBBSBViewOcclusionComponent::EndPlay(const EEndPlayReason::Type Reason)
{ bSilhouetteEnding=true;UpdateFocusEdgePass();UpdateOccludedSilhouettes(false);GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandler);RestoreVisibility();Super::EndPlay(Reason); }
void UBBSBViewOcclusionComponent::TickComponent(float DT,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
 Super::TickComponent(DT,TickType,TickFunction);auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 auto* Camera=PC ? PC->PlayerCameraManager.Get():nullptr;
 UpdateFocusEdgePass();UpdateOccludedSilhouettes(IsOccludedSilhouetteActive());
 if(!Library || !Library->Collection || !Camera || !PC->IsLocalController()) { RestoreVisibility();return; }
 const FVector Eye=Camera->GetCameraLocation(),Focus=Character->GetActorLocation()+FVector(0,0,5);int32 W=0,H=0;PC->GetViewportSize(W,H);
 const bool Active=bEnabled && PC->GetViewTarget()==Character && W>0 && H>0;
 if(!Active) { RestoreVisibility();return; }
 if(bCharacterFadeEnabled)UpdateCharacterProtection(Eye);else RestoreCharacterVisibility();
 if(bEnvironmentMaskEnabled)
 {
  if(!Queries)
  {
   Queries=MakeShared<FBBSBViewOcclusionQueries>();NextScanAt=0;
   for(TActorIterator<AActor> It(GetWorld());It;++It)PrepareCharacterCamera(*It);
  }
  UpdateCollection(Eye,Camera->GetCameraRotation(),Camera->GetFOVAngle(),W,H,Focus);
  const double Now=GetWorld()->GetTimeSeconds();
  if(Now>=NextScanAt) { Scan(Eye,Focus);NextScanAt=Now+FMath::Max(.01f,ScanInterval); }
 }
 else if(Queries || !CameraResponses.IsEmpty() || !WantedGeometry.IsEmpty())RestoreGeometryVisibility();
 GeometryOccluders=CharacterOccluders=FallbackOccluders=0;
 for(int32 Index=States.Num()-1;Index>=0;--Index)
 {
  auto& State=States[Index];auto* Mesh=State.Mesh.Get();if(!Mesh) { States.RemoveAtSwap(Index);continue; }
  const bool Wanted=State.bCharacter ? WantedCharacters.Contains(Mesh):WantedGeometry.Contains(Mesh);
  if(auto* Instances=Cast<UInstancedStaticMeshComponent>(Mesh))
  {
   const auto* Set=WantedInstances.Find(Mesh);if(Set)for(int32 I:*Set)State.InstanceWeights.FindOrAdd(I);
   bool Changed=false;
   for(auto It=State.InstanceWeights.CreateIterator();It;++It)
   {
    const bool Hit=Set && Set->Contains(It.Key());const float Old=It.Value();
    It.Value()=FMath::FInterpConstantTo(Old,Hit ? 1.f:0.f,DT,1.f/FMath::Max(.02f,Hit ? FadeInSeconds:FadeOutSeconds));
    if(It.Value()!=Old) { Instances->SetCustomDataValue(It.Key(),ViewOcclusion::InstanceSlot,It.Value(),false);Changed=true; }
    if(!Hit && It.Value()<=0.f)It.RemoveCurrent();
   }
   if(Changed)Instances->MarkRenderInstancesDirty();State.Weight=State.InstanceWeights.Num() ? 1.f:0.f;
  }
  else State.Weight=FMath::FInterpConstantTo(State.Weight,Wanted ? 1.f:0.f,DT,1.f/FMath::Max(.02f,Wanted ? FadeInSeconds:FadeOutSeconds));
  Mesh->SetCustomPrimitiveDataFloat(WeightIndex,State.Weight);
  for(const auto& Material:State.Replacements)if(auto* Dynamic=Cast<UMaterialInstanceDynamic>(Material.Get()))Dynamic->SetScalarParameterValue(TEXT("BBSB_ViewOcclusionWeight"),State.Weight);
  // Unknown materials get a visibility fallback until the editor prepares them.
  // Critical near-plane character protection is actor-wide and independent of materials.
  if(State.bUnsupported) { ++FallbackOccluders;Mesh->SetHiddenInGame(State.bOriginalHidden || (Wanted && State.Weight>0.f)); }
  if(!Wanted && State.Weight<=0.f) { RestoreMesh(State);States.RemoveAtSwap(Index);continue; }
  if(State.bCharacter)++CharacterOccluders;else ++GeometryOccluders;
 }
}


void UBBSBViewOcclusionComponent::UpdateFocusEdgePass()
{
 auto* Camera=Character ? Character->GetFollowCamera():nullptr;
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 const bool Wanted=!bSilhouetteEnding && Camera && PC && PC->IsLocalController();
 if(Wanted && !BoundFocusEdgeMaterial)
 {
  if(auto* Material=FocusEdgeMaterial.LoadSynchronous())
  { Camera->AddOrUpdateBlendable(Material,1.f);BoundFocusEdgeMaterial=Material; }
 }
 else if(!Wanted && BoundFocusEdgeMaterial)
 {
  if(Camera)Camera->RemoveBlendable(BoundFocusEdgeMaterial);
  BoundFocusEdgeMaterial=nullptr;
 }
}

bool UBBSBViewOcclusionComponent::IsOccludedSilhouetteActive() const
{
 const auto* PC=Character ? Cast<APlayerController>(Character->GetController()):nullptr;
 return bEnabled && bOccludedSilhouettesEnabled && !bSilhouetteEnding && PC && PC->IsLocalController() && PC->GetViewTarget()==Character;
}
bool BBSBVisibilityStencil::Enabled(UWorld* World)
{
 const auto* LocalPawn=World ? UGameplayStatics::GetPlayerPawn(World,0):nullptr;
 const auto* Visibility=LocalPawn ? LocalPawn->FindComponentByClass<UBBSBViewOcclusionComponent>():nullptr;
 return Visibility && Visibility->IsOccludedSilhouetteActive();
}
void BBSBVisibilityStencil::UpdateHandOccluders(ACharacter* Character,bool Active)
{
 if(!Character || !Character->GetWorld())return;
 // These transient visuals start at depth=false/stencil=0. Update only on a
 // mode change or weapon creation, never by searching actors each frame.
 for(TActorIterator<AActor> It(Character->GetWorld());It;++It)
  if(It->GetOwner()==Character && It->GetRootComponent() &&
   (It->GetRootComponent()->GetFName()==TEXT("ReparatonRoot") || It->IsA<ABBSBBunniarCompanion>()))
  {
   const bool Buddy=It->IsA<ABBSBBunniarCompanion>();
   TInlineComponentArray<UMeshComponent*> Parts(*It);
   for(auto* Part:Parts)
   {
    const bool Highlighted=Buddy && (Part->CustomDepthStencilValue&127)==4;
    Part->SetRenderCustomDepth(Active || Highlighted);
    Part->SetCustomDepthStencilValue(Highlighted ? (Active ? 132:4):Active ? (Buddy ? 137:135):0);
   }
  }
}
void UBBSBViewOcclusionComponent::UpdateOccludedSilhouettes(bool Active)
{
 auto* Mesh=Character ? Character->GetMesh():nullptr;
 if(SilhouettePlayerMesh.IsValid() && (!Active || SilhouettePlayerMesh.Get()!=Mesh))
 {
  auto* Previous=SilhouettePlayerMesh.Get();Previous->SetRenderCustomDepth(bOriginalPlayerDepth);Previous->SetCustomDepthStencilValue(OriginalPlayerStencil);SilhouettePlayerMesh.Reset();
 }
 if(Active && Mesh)
 {
  if(!SilhouettePlayerMesh.IsValid())
  { SilhouettePlayerMesh=Mesh;bOriginalPlayerDepth=Mesh->bRenderCustomDepth;OriginalPlayerStencil=Mesh->CustomDepthStencilValue; }
  if(!Mesh->bRenderCustomDepth)Mesh->SetRenderCustomDepth(true);
  if(Mesh->CustomDepthStencilValue!=BBSBVisibilityStencil::Player)Mesh->SetCustomDepthStencilValue(BBSBVisibilityStencil::Player);
 }
 if(Active==bSilhouetteActive)return;
 bSilhouetteActive=Active;
 BBSBVisibilityStencil::UpdateHandOccluders(Character,Active);
 // One update per mode change; projectiles never poll this component each tick.
 for(TActorIterator<ABBSBInkProjectile> It(GetWorld());It;++It)
 {
  for(auto* Part:{static_cast<UMeshComponent*>(It->Body.Get()),static_cast<UMeshComponent*>(It->BeatVisual.Get())})
   if(Part) { Part->SetRenderCustomDepth(Active);Part->SetCustomDepthStencilValue(BBSBVisibilityStencil::Projectile); }
  if(auto* Shape=Cast<ABBSBFieldAttack>(*It))
   for(auto* Part:{Shape->Energy.Get(),Shape->Stones.Get()})
    if(Part) { Part->SetRenderCustomDepth(Active);Part->SetCustomDepthStencilValue(BBSBVisibilityStencil::Projectile); }
 }
 if(auto* Combat=Character ? Character->FindComponentByClass<UBBSBCombatComponent>():nullptr)
  if(Combat->HasTargetLock())Combat->FieldLockedTarget->SetSelected(true);
 auto* Camera=Character ? Character->GetFollowCamera():nullptr;if(!Camera)return;
 if(!Active)
 {
  if(bAddedSilhouetteBlendable && BoundSilhouetteMaterial)Camera->RemoveBlendable(BoundSilhouetteMaterial);
  bAddedSilhouetteBlendable=false;BoundSilhouetteMaterial=nullptr;return;
 }
 auto* Material=SilhouetteOutlineMaterial.LoadSynchronous();if(!Material)return;
 // Reuse the existing unbound outline pass. Generic levels get a camera fallback.
 bool Provided=false;
 for(TActorIterator<APostProcessVolume> It(GetWorld());It;++It)
  if(It->bEnabled && It->bUnbound && It->BlendWeight>0)
   for(const auto& Entry:It->Settings.WeightedBlendables.Array)
    if(Entry.Weight>0 && Entry.Object==Material)Provided=true;
 if(!Provided) { Camera->AddOrUpdateBlendable(Material,1.f);BoundSilhouetteMaterial=Material;bAddedSilhouetteBlendable=true; }
}
