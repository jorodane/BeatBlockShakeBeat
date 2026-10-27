#include "BBSBBladeTrail.h"
#include "DynamicMeshBuilder.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

namespace
{
 struct FTrailPoint { FVector3f Root,Tip;FColor Color; };
 class FBladeTrailProxy final : public FPrimitiveSceneProxy
 {
 public:
  FBladeTrailProxy(const UPrimitiveComponent* Component,UMaterialInterface* InMaterial,TArray<FTrailPoint>&& InPoints)
   : FPrimitiveSceneProxy(Component),Material(InMaterial),Relevance(InMaterial->GetRelevance_Concurrent(GetScene().GetShaderPlatform())),Points(MoveTemp(InPoints)) {}
  virtual SIZE_T GetTypeHash() const override { static size_t Type;return reinterpret_cast<size_t>(&Type); }
  virtual uint32 GetMemoryFootprint() const override { return sizeof(*this)+Points.GetAllocatedSize(); }
  virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
  {
   FPrimitiveViewRelevance R;R.bDrawRelevance=IsShown(View);R.bDynamicRelevance=true;R.bRenderInMainPass=ShouldRenderInMainPass();R.bShadowRelevance=false;Relevance.SetPrimitiveViewRelevance(R);return R;
  }
  virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,const FSceneViewFamily& Family,uint32 VisibilityMap,FMeshElementCollector& Collector) const override
  {
   if(Points.Num()<2) return;
   for(int32 ViewIndex=0;ViewIndex<Views.Num();++ViewIndex) if(VisibilityMap&(1u<<ViewIndex))
   {
    FDynamicMeshBuilder Mesh(GetScene().GetFeatureLevel());
    for(int32 I=0;I<Points.Num();++I)
    {
     const auto& P=Points[I];const float U=float(I)/FMath::Max(1,Points.Num()-1);
     Mesh.AddVertex(P.Root,FVector2f(U,0),FVector3f(1,0,0),FVector3f(0,1,0),FVector3f(0,0,1),P.Color);
     Mesh.AddVertex(P.Tip,FVector2f(U,1),FVector3f(1,0,0),FVector3f(0,1,0),FVector3f(0,0,1),P.Color);
     if(I>0) { const int32 A=(I-1)*2,B=I*2;Mesh.AddTriangle(A,A+1,B);Mesh.AddTriangle(A+1,B+1,B); }
    }
    Mesh.GetMesh(GetLocalToWorld(),Material->GetRenderProxy(),SDPG_World,true,false,ViewIndex,Collector);
   }
  }
 private:
  UMaterialInterface* Material;
  FMaterialRelevance Relevance;
  TArray<FTrailPoint> Points;
 };
}

UBBSBBladeTrailComponent::UBBSBBladeTrailComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostPhysics;
 SetCollisionEnabled(ECollisionEnabled::NoCollision);SetGenerateOverlapEvents(false);SetReceivesDecals(false);CastShadow=false;bUseAsOccluder=false;
}
void UBBSBBladeTrailComponent::AddBladeSample(const FVector& Root,const FVector& Tip)
{
 if(bFinished || !GetWorld()) return;
 if(!Samples.IsEmpty() && Samples.Last().Root.Equals(Root,.01) && Samples.Last().Tip.Equals(Tip,.01)) return;
 Samples.Add({Root,Tip,GetWorld()->GetTimeSeconds()});
 if(Samples.Num()>128) Samples.RemoveAt(0,Samples.Num()-128,EAllowShrinking::No);
 UpdateBounds();MarkRenderTransformDirty();MarkRenderStateDirty();
}
void UBBSBBladeTrailComponent::Finish() { bFinished=true; }
void UBBSBBladeTrailComponent::TickComponent(float DT,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
 Super::TickComponent(DT,TickType,TickFunction);
 const double Now=GetWorld()->GetTimeSeconds();Samples.RemoveAll([&](const FSample& P){ return Now-P.Born>=Lifetime; });
 if(bFinished && Samples.IsEmpty()) { DestroyComponent();return; }
 UpdateBounds();MarkRenderTransformDirty();MarkRenderStateDirty();
}
FBoxSphereBounds UBBSBBladeTrailComponent::CalcBounds(const FTransform& LocalToWorld) const
{
 FBox Box(ForceInit);for(const auto& P:Samples) { Box+=P.Root;Box+=P.Tip; }
 if(!Box.IsValid) Box=FBox(GetComponentLocation()-FVector(1),GetComponentLocation()+FVector(1));
 return FBoxSphereBounds(Box.ExpandBy(2));
}
FPrimitiveSceneProxy* UBBSBBladeTrailComponent::CreateSceneProxy()
{
 if(!TrailMaterial || Samples.Num()<2) return nullptr;
 const double Now=GetWorld()->GetTimeSeconds();const FTransform WorldToLocal=GetComponentTransform().Inverse();TArray<FTrailPoint> Points;Points.Reserve(Samples.Num());
 for(const auto& P:Samples)
 {
  FLinearColor C=Color;const float Fade=FMath::Clamp(1.f-float((Now-P.Born)/FMath::Max(.03f,Lifetime)),0.f,1.f);C.A*=Fade*Fade;
  Points.Add({FVector3f(WorldToLocal.TransformPosition(P.Root)),FVector3f(WorldToLocal.TransformPosition(P.Tip)),C.ToFColor(false)});
 }
 return new FBladeTrailProxy(this,TrailMaterial,MoveTemp(Points));
}
