#pragma once
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace BBSBCombatVisuals
{
	inline FCollisionObjectQueryParams GeometryTypes()
	{
		FCollisionObjectQueryParams Types; Types.AddObjectTypesToQuery(ECC_WorldStatic); Types.AddObjectTypesToQuery(ECC_WorldDynamic); return Types;
	}
	inline void Tint(UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (!Mesh) return;
		UMaterialInstanceDynamic* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
		if (!MID)
		{
			if (UMaterialInterface* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))) Mesh->SetMaterial(0,Base);
			MID=Mesh->CreateAndSetMaterialInstanceDynamic(0);
		}
		if (MID) { MID->SetVectorParameterValue(TEXT("Color"),Color); MID->SetScalarParameterValue(TEXT("Roughness"),0.95f); }
	}
	inline bool Ground(UWorld* World, const FVector& Point, FVector& Out)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit,Point+FVector(0,0,600),Point-FVector(0,0,1600),GeometryTypes(),FCollisionQueryParams(SCENE_QUERY_STAT(BBSBStageGround)))) return false;
		if (Hit.ImpactNormal.Z<0.82f) return false;
		Out=Hit.ImpactPoint; return true;
	}
	inline UStaticMeshComponent* Ink(AActor* Owner, FName Name)
	{
		UStaticMeshComponent* Mesh=NewObject<UStaticMeshComponent>(Owner,Name);
		Owner->AddInstanceComponent(Mesh);
		Mesh->SetupAttachment(Owner->GetRootComponent());
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->RegisterComponent();
		Tint(Mesh,FLinearColor(0.035f,0.025f,0.045f));
		return Mesh;
	}
	inline const TCHAR* DirectionName(int32 Index)
	{
		static const TCHAR* Names[]={TEXT("UP"),TEXT("RIGHT"),TEXT("DOWN"),TEXT("LEFT")};
		return Names[FMath::Clamp(Index,0,3)];
	}
}
