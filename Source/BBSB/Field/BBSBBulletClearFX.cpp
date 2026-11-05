#include "BBSBBulletClearFX.h"
#include "DynamicMeshBuilder.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

namespace
{
 struct FClearBurst { FVector Point;FLinearColor Tint;float T=0; };
 class FClearProxy final : public FPrimitiveSceneProxy
 {
 public:
  FClearProxy(const UPrimitiveComponent* Component,UMaterialInterface* Mat,TArray<FClearBurst>&& Data,float Radius)
   : FPrimitiveSceneProxy(Component),Material(Mat),Relevance(Mat->GetRelevance_Concurrent(GetScene().GetShaderPlatform())),Bursts(MoveTemp(Data)),Size(Radius) {}
  virtual SIZE_T GetTypeHash() const override { static size_t Type;return reinterpret_cast<size_t>(&Type); }
  virtual uint32 GetMemoryFootprint() const override { return sizeof(*this)+Bursts.GetAllocatedSize(); }
  virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
  { FPrimitiveViewRelevance R;R.bDrawRelevance=IsShown(View);R.bDynamicRelevance=true;R.bRenderInMainPass=ShouldRenderInMainPass();R.bShadowRelevance=false;Relevance.SetPrimitiveViewRelevance(R);return R; }
  virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,const FSceneViewFamily& Family,uint32 Map,FMeshElementCollector& Collector) const override
  {
   for(int32 V=0;V<Views.Num();++V)if(Map&(1u<<V))
   {
    FDynamicMeshBuilder Mesh(GetScene().GetFeatureLevel());const FVector Right=Views[V]->GetViewRight(),Up=Views[V]->GetViewUp();
    auto Quad=[&](const FVector& A,const FVector& B,const FVector& C,const FVector& D,FLinearColor Color)
    {
     const FColor Tint=Color.ToFColor(false);const int32 Base=Mesh.AddVertex(FVector3f(A),FVector2f(0,0),FVector3f(1,0,0),FVector3f(0,1,0),FVector3f(0,0,1),Tint);
     Mesh.AddVertex(FVector3f(B),FVector2f(0,1),FVector3f(1,0,0),FVector3f(0,1,0),FVector3f(0,0,1),Tint);
     Mesh.AddVertex(FVector3f(C),FVector2f(1,1),FVector3f(1,0,0),FVector3f(0,1,0),FVector3f(0,0,1),Tint);
     Mesh.AddVertex(FVector3f(D),FVector2f(1,0),FVector3f(1,0,0),FVector3f(0,1,0),FVector3f(0,0,1),Tint);
     Mesh.AddTriangle(Base,Base+1,Base+2);Mesh.AddTriangle(Base,Base+2,Base+3);
    };
    for(const auto& B:Bursts)
    {
     const float Fade=FMath::Square(1-B.T),Radius=Size*(.2f+.8f*FMath::Sqrt(B.T));
     FLinearColor Color=FMath::Lerp(B.Tint,FLinearColor(1.f,.96f,.83f),.65f);Color.A=.95f*Fade;
     auto Circle=[&](float Inner,float Outer,FLinearColor Tint)
     {
      for(int32 I=0;I<24;++I)
      { const float A=I*2*PI/24.f,C=(I+1)*2*PI/24.f;const FVector X=Right*FMath::Cos(A)+Up*FMath::Sin(A),Y=Right*FMath::Cos(C)+Up*FMath::Sin(C);
       Quad(B.Point+X*Inner,B.Point+X*Outer,B.Point+Y*Outer,B.Point+Y*Inner,Tint); }
     };
     Circle(Radius-4.f,Radius+4.f,FLinearColor(.23f,.14f,.29f,Fade*.6f));Circle(Radius-2.f,Radius+2.f,Color);
     for(int32 I=0;I<6;++I)
     {
      const float A=I*PI/3.f+B.T*1.4f;const FVector Along=Right*FMath::Cos(A)+Up*FMath::Sin(A),Across=Right*-FMath::Sin(A)+Up*FMath::Cos(A);
      const FVector P=B.Point+Along*(Radius*.8f);const float Length=Size*(1-B.T)*.4f,Width=Size*(1-B.T)*.065f;
      Quad(P-Along*Length,P+Across*Width,P+Along*Length,P-Across*Width,Color);
     }
     const float Flash=Size*.2f*FMath::Pow(1-B.T,3.f);Quad(B.Point-Right*Flash,B.Point+Up*Flash,B.Point+Right*Flash,B.Point-Up*Flash,FLinearColor(1.f,.98f,.9f,Fade));
    }
    Mesh.GetMesh(GetLocalToWorld(),Material->GetRenderProxy(),SDPG_World,true,false,V,Collector);
   }
  }
 private:
  UMaterialInterface* Material;FMaterialRelevance Relevance;TArray<FClearBurst> Bursts;float Size=60;
 };
}
UBBSBBulletClearFXComponent::UBBSBBulletClearFXComponent()
{
 PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostPhysics;PrimaryComponentTick.bStartWithTickEnabled=false;
 SetCollisionEnabled(ECollisionEnabled::NoCollision);SetGenerateOverlapEvents(false);SetReceivesDecals(false);CastShadow=false;bUseAsOccluder=false;SetTranslucentSortPriority(20);
}
void UBBSBBulletClearFXComponent::AddBurst(const FVector& Point,const FLinearColor& Tint)
{
 if(!GetWorld() || !EffectMaterial)return;
 const int32 Limit=FMath::Clamp(MaximumBursts,4,128);if(Bursts.Num()>=Limit)Bursts.RemoveAt(0,Bursts.Num()-Limit+1,EAllowShrinking::No);
 Bursts.Add({Point,Tint,GetWorld()->GetTimeSeconds()});LastBurstPoint=Point;++BurstsEmitted;SetComponentTickEnabled(true);UpdateBounds();MarkRenderStateDirty();
}
void UBBSBBulletClearFXComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(DT,Type,Function);const double Now=GetWorld()->GetTimeSeconds();
 Bursts.RemoveAll([&](const FBurst& B){return Now-B.Born>=FMath::Max(.1f,Lifetime);});UpdateBounds();MarkRenderStateDirty();
 if(Bursts.IsEmpty())SetComponentTickEnabled(false);
}
FBoxSphereBounds UBBSBBulletClearFXComponent::CalcBounds(const FTransform& LocalToWorld) const
{
 FBox Box(ForceInit);for(const auto& B:Bursts)Box+=B.Point;
 if(!Box.IsValid)Box=FBox(GetComponentLocation()-FVector(1),GetComponentLocation()+FVector(1));
 return FBoxSphereBounds(Box.ExpandBy(FMath::Max(10.f,BurstRadius)*1.5f));
}
FPrimitiveSceneProxy* UBBSBBulletClearFXComponent::CreateSceneProxy()
{
 if(!EffectMaterial || Bursts.IsEmpty())return nullptr;
 const double Now=GetWorld()->GetTimeSeconds();TArray<FClearBurst> Data;Data.Reserve(Bursts.Num());
 for(const auto& B:Bursts)Data.Add({B.Point,B.Tint,FMath::Clamp(float((Now-B.Born)/FMath::Max(.1f,Lifetime)),0.f,1.f)});
 return new FClearProxy(this,EffectMaterial,MoveTemp(Data),FMath::Max(10.f,BurstRadius));
}
