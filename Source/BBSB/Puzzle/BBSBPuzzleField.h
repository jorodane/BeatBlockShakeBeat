#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Caravan/BBSBBunniarSurface.h"
#include "Field/BBSBFieldGesture.h"
#include "BBSBPuzzleField.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USplineComponent;
class UMaterialInterface;
class UAudioComponent;
class USoundWaveProcedural;
class ABBSBCharacter;
class ALandscape;

UENUM(BlueprintType)
enum class EBBSBPuzzleKind : uint8
{
 BeatLatch, BeatRhythm, BeatOrder, HoldGate, HoldLift, ShakeLoose, ShakeBurst,
 FlickRight, FlickUp, DiveCue, DivePendulum, Ladder, Lever, Wire,
 Sequence, ChargeFlick, ShakeFlick, CarrySocket, Balance, Relay
};

/** Each station owns a different input rule and a physical result; shared recognizers remain authoritative. */
UCLASS()
class BBSB_API ABBSBPuzzleStation : public AActor, public IBBSBBunniarSurface
{
 GENERATED_BODY()
public:
 ABBSBPuzzleStation();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float DT) override;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle") EBBSBPuzzleKind Kind=EBBSBPuzzleKind::BeatLatch;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle") int32 Number=1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle") float BPM=80.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle") float TimingWindow=.18f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Puzzle") float DragDistance=160.f;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") bool bSolved=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") bool bHeld=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") bool bLoose=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") bool bCarrying=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") int32 Step=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") int32 Attempts=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") float Progress=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") FString Feedback;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") FVector2D Drag=FVector2D::ZeroVector;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") FVector2D PointerOrigin=FVector2D::ZeroVector;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") TObjectPtr<UStaticMeshComponent> Receiver;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") TObjectPtr<UStaticMeshComponent> Payload;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") TObjectPtr<UStaticMeshComponent> Socket;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") TObjectPtr<UStaticMeshComponent> Output;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") TObjectPtr<UStaticMeshComponent> Indicator;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Sign;
 UFUNCTION(BlueprintCallable, Category="Puzzle") void ResetPuzzle();
 UFUNCTION(BlueprintCallable,CallInEditor,Category="Puzzle") void RebuildPresentation() { Build(); }
 UFUNCTION(BlueprintPure, Category="Puzzle") FString Title() const;
 UFUNCTION(BlueprintPure, Category="Puzzle") FString Hint() const;
 UFUNCTION(BlueprintPure, Category="Puzzle") bool IsTimingOpen() const;
 UFUNCTION(BlueprintPure, Category="Puzzle") float CuePhase() const;
 UFUNCTION(BlueprintPure, Category="Puzzle") FVector SocketPoint() const;
 virtual void BunniarAttached_Implementation(ABBSBBunniarCompanion* Buddy,FVector Point,FVector Normal) override;
 virtual void BunniarDragged_Implementation(ABBSBBunniarCompanion* Buddy,FVector2D Delta,FVector2D Total) override;
 virtual void BunniarReleased_Implementation(ABBSBBunniarCompanion* Buddy) override;
 virtual void BunniarGesture_Implementation(ABBSBBunniarCompanion* Buddy,EBBSBFieldGesture Gesture,FVector2D Direction,float HeldSeconds) override;
private:
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 UPROPERTY(Transient) TObjectPtr<ABBSBBunniarCompanion> AttachedBuddy;
 FVector PayloadHome,OutputHome;
 FVector FlightFrom,FlightTo;
 FVector CarryRight=FVector::RightVector,CarryUp=FVector::UpVector;
 double HeldAt=0,LastBeatAt=-100,BurstAt=-100,FlightAt=-100,BalanceAt=-1;
 float OutputAlpha=0;
 bool bFlight=false,bFlightSuccess=false,bHoldSeen=false,bShakeSeen=false;
 int32 Pad=0;
 void Build();
 UStaticMeshComponent* Part(const FString& Name,FVector Location,FVector Size,FLinearColor Color,const TCHAR* Shape=TEXT("Cube"),bool Solid=false);
 void Solve(const FString& Message);
 void Fail(const FString& Message,bool ResetStep=true);
 void Launch(FVector2D Direction,bool Success);
 void SetWireVisual(int32 Segments);
};

/** The natural test field has a soft return boundary and no dependency on the caravan quest. */
UCLASS()
class BBSB_API ABBSBPuzzleField : public AActor
{
 GENERATED_BODY()
public:
 ABBSBPuzzleField();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void Tick(float DT) override;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field") FVector2D BoundaryRadii=FVector2D(7000,6400);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field") float ReturnHysteresis=.91f;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Field") bool bReturning=false;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Field") int32 ReturnCount=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Field") int32 SolvedCount=0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Field") TObjectPtr<ABBSBPuzzleStation> Focus;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Field") TArray<TObjectPtr<ABBSBPuzzleStation>> Stations;
 UFUNCTION(BlueprintCallable, Category="Field") void ResetNearest();
 UFUNCTION(BlueprintCallable, Category="Field") void ResetAll();
 UFUNCTION(BlueprintPure, Category="Field") bool HasRhythmCue() const { return RhythmAudio!=nullptr; }
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<UInputComponent> Controls;
 UPROPERTY(Transient) TObjectPtr<UAudioComponent> RhythmAudio;
 UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> RhythmClick;
 TWeakObjectPtr<ABBSBPuzzleStation> RhythmFocus;
 int32 LastRhythmBeat=MIN_int32;
 FVector SafeStart;
 bool bOwnsMoveIgnore=false;
 void ReleaseBoundary();
 void BindPlayer();
};

UCLASS()
class BBSB_API ABBSBPuzzleFieldHUD : public AHUD
{
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
};

/** Editor entry point creates genuine Landscape components with an imported heightmap. */
UCLASS()
class BBSB_API UBBSBPuzzleLandscapeTools : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure, Category="BBSB|Landscape") static float GroundHeight(float X,float Y);
 UFUNCTION(BlueprintPure, Category="BBSB|Landscape") static FVector StationPosition(int32 Index);
 UFUNCTION(BlueprintCallable, Category="BBSB|Landscape",meta=(WorldContext="WorldContextObject")) static ALandscape* CreateMeadow(UObject* WorldContextObject,UMaterialInterface* Material);
};
