#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "BBSBViewOcclusion.generated.h"

class ABBSBCharacter;
class UMeshComponent;
class UPrimitiveComponent;
class UMaterialInterface;
class UMaterial;
class UMaterialParameterCollection;
class UStaticMesh;
struct FBBSBViewOcclusionQueries;

/** Render triangles baked in the editor; available in a packaged game without CPU mesh buffers. */
USTRUCT()
struct FBBSBViewOcclusionGeometry
{
 GENERATED_BODY()
 UPROPERTY() FGuid SourceGuid;
 UPROPERTY() TArray<FVector> Positions;
 UPROPERTY() TArray<int32> Indices;
};

/** Authoring copies retain every source material's textures, tint and static switches. */
UCLASS(BlueprintType)
class BBSB_API UBBSBViewOcclusionMaterials : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="View occlusion") TMap<TObjectPtr<UMaterialInterface>,TObjectPtr<UMaterialInterface>> Variants;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="View occlusion") TObjectPtr<UMaterialParameterCollection> Collection;
 UPROPERTY() TMap<TObjectPtr<UStaticMesh>,FBBSBViewOcclusionGeometry> Geometry;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="View occlusion|Editor") TArray<UMaterialInterface*> SceneMaterials();
 UFUNCTION(BlueprintCallable,CallInEditor,Category="View occlusion|Editor") int32 CacheLoadedGeometry();
 /** Clear opaque caches and imported constant overrides after wiring the opacity mask. */
 UFUNCTION(BlueprintCallable,CallInEditor,Category="View occlusion|Editor") bool PrepareMaskedMaterial(UMaterial* Material);
};

USTRUCT()
struct FBBSBOccludingMesh
{
 GENERATED_BODY()
 UPROPERTY() TWeakObjectPtr<UMeshComponent> Mesh;
 UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Originals;
 UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Replacements;
 float Weight=0,OriginalWeight=0,OriginalKind=0;
 bool bCharacter=false;
 uint8 OriginalShadowInvalidation=0;
 int32 OriginalInstanceFloats=0;
 TArray<float> OriginalInstanceData;
 TMap<int32,float> InstanceWeights;
 float OriginalInstanceKind=0;
 bool bUnsupported=false,bOriginalHidden=false;
};

/** Local-player visibility only: physics, aim traces, damage and other actors remain active. */
UCLASS(ClassGroup=(BBSB),meta=(BlueprintSpawnableComponent))
class BBSB_API UBBSBViewOcclusionComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UBBSBViewOcclusionComponent();
 virtual ~UBBSBViewOcclusionComponent() override;
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float DT,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion") bool bEnabled=true;
 /** Filled silhouettes for occluded player, projectile bodies and the locked enemy. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Occluded silhouettes") bool bOccludedSilhouettesEnabled=true;
 UPROPERTY(EditAnywhere,Category="Occluded silhouettes") TSoftObjectPtr<UMaterialInterface> SilhouetteOutlineMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Stylized/PostProcess/M_PP_FocusOutline.M_PP_FocusOutline")));
 /** Full-resolution Roberts Cross edge buffer, shared by visible and hidden focus contours. */
 UPROPERTY(EditAnywhere,Category="Occluded silhouettes") TSoftObjectPtr<UMaterialInterface> FocusEdgeMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Stylized/PostProcess/M_PP_FocusEdges.M_PP_FocusEdges")));
 UFUNCTION(BlueprintPure,Category="Occluded silhouettes") bool IsOccludedSilhouetteActive() const;
 /** Optional circular reveal for terrain and props; disabled by default. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion",meta=(EditCondition="bEnabled")) bool bEnvironmentMaskEnabled=false;
 /** Fade characters near the camera and hide them before the lens enters their mesh. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion",meta=(EditCondition="bEnabled")) bool bCharacterFadeEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion",meta=(ClampMin=".05",ClampMax=".4")) float CircleRadius=.22f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion",meta=(ClampMin=".001",ClampMax=".1")) float CircleFeather=.07f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion",meta=(ClampMin=".02",Units="s")) float FadeInSeconds=.14f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion",meta=(ClampMin=".02",Units="s")) float FadeOutSeconds=.24f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion",meta=(ClampMin=".01",Units="s")) float ScanInterval=.04f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion",meta=(ClampMin="1",ClampMax="30",Units="cm")) float CharacterLensRadius=12.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion") bool bFitPlayerSilhouette=true;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="View occlusion") FVector2D ScreenCircleCenter=FVector2D(.3,.68);
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="View occlusion") float ScreenCircleRadius=.22f;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="View occlusion") float RevealDepth=0.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="View occlusion") TSoftObjectPtr<UBBSBViewOcclusionMaterials> MaterialLibrary;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="View occlusion") int32 GeometryOccluders=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="View occlusion") int32 CharacterOccluders=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="View occlusion") int32 FallbackOccluders=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="View occlusion|Debug") float LastScanMilliseconds=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="View occlusion|Debug") float LastPlayerSamplingMilliseconds=0;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="View occlusion|Debug") int32 PlayerSampleTriangles=0;
 UFUNCTION(BlueprintPure,Category="View occlusion") float OcclusionWeight(UMeshComponent* Mesh) const;
 UFUNCTION(BlueprintPure,Category="View occlusion|Debug") FString MaterialDiagnostics(UMeshComponent* Mesh) const;
 UFUNCTION(BlueprintCallable,Category="View occlusion") void RestoreVisibility();
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Character;
 UPROPERTY(Transient) TObjectPtr<UBBSBViewOcclusionMaterials> Library;
 UPROPERTY(Transient) TArray<FBBSBOccludingMesh> States;
 TSet<TWeakObjectPtr<UMeshComponent>> WantedGeometry,WantedCharacters;
 TMap<TWeakObjectPtr<UMeshComponent>,TSet<int32>> WantedInstances;
 TMap<TWeakObjectPtr<AActor>,bool> HiddenCharacters;
 TSharedPtr<FBBSBViewOcclusionQueries> Queries;
 TMap<TWeakObjectPtr<UPrimitiveComponent>,TEnumAsByte<ECollisionResponse>> CameraResponses;
 TMap<TWeakObjectPtr<UPrimitiveComponent>,TEnumAsByte<ECollisionResponse>> CharacterCameraResponses;
 FDelegateHandle SpawnHandler;
 double NextScanAt=0;
 float ScreenAspect=1.f;
 FVector ReferenceEye=FVector::ZeroVector;
 FRotator ReferenceRotation=FRotator::ZeroRotator;
 float ReferenceFOV=90.f,ReferenceRevealDepth=0.f;
 FVector2D ReferenceCircleCenter=FVector2D(.3,.68);
 float ReferenceCircleRadius=.22f;
 void PrepareCharacterCamera(AActor* Actor);
 void UpdateCharacterProtection(const FVector& Eye);
 bool CharacterTouchesLens(AActor* Actor,const FVector& Eye) const;
 bool AddMesh(UMeshComponent* Mesh,bool bOtherCharacter);
 void Scan(const FVector& Camera,const FVector& Focus);
 void UpdateCollection(const FVector& Camera,const FRotator& Rotation,float FOV,int32 W,int32 H,const FVector& Focus);
 void RestoreMesh(FBBSBOccludingMesh& State);
 void RestoreGeometryVisibility();
 void RestoreCharacterVisibility();
 void IgnoreCamera(UPrimitiveComponent* Primitive);
 void UpdateOccludedSilhouettes(bool Active);
 void UpdateFocusEdgePass();
 TWeakObjectPtr<UMeshComponent> SilhouettePlayerMesh;
 bool bSilhouetteActive=false,bSilhouetteEnding=false,bOriginalPlayerDepth=false,bAddedSilhouetteBlendable=false;
 int32 OriginalPlayerStencil=0;
 UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BoundSilhouetteMaterial;
 UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BoundFocusEdgeMaterial;
};
