#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Field/BBSBFieldGesture.h"
#include "BBSBBunniarSurface.generated.h"

class ABBSBBunniarCompanion;

/** Optional puzzle callbacks. Ordinary collidable surfaces need no interface to receive Bunniar. */
UINTERFACE(BlueprintType)
class BBSB_API UBBSBBunniarSurface : public UInterface { GENERATED_BODY() };
class BBSB_API IBBSBBunniarSurface
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Bunniar") void BunniarAttached(ABBSBBunniarCompanion* Bunniar,FVector WorldPoint,FVector WorldNormal);
 /** Actual visible arrival; logical attachment retains the existing puzzle press timing. */
 UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Bunniar") void BunniarArrived(ABBSBBunniarCompanion* Bunniar);
 UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Bunniar") void BunniarDragged(ABBSBBunniarCompanion* Bunniar,FVector2D DeltaPixels,FVector2D TotalPixels);
 UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Bunniar") void BunniarReleased(ABBSBBunniarCompanion* Bunniar);
 /** Direction is normalized screen space (right/down positive); seconds come from the real held channel. */
 UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Bunniar") void BunniarGesture(ABBSBBunniarCompanion* Bunniar,EBBSBFieldGesture Gesture,FVector2D Direction,float HeldSeconds);
};
