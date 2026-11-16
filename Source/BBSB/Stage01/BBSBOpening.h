#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"
#include "Blueprint/UserWidget.h"
#include "BBSBOpening.generated.h"
class ABBSBCharacter;class ABBSBStage01Director;class ABBSBBunniarCompanion;class UAnimSequence;class ULevelSequence;class ACameraActor;class UInputComponent;class UStaticMeshComponent;class UAudioComponent;class UAnimInstance;

UENUM(BlueprintType)
enum class EBBSBOpeningBeat : uint8 { Writing, Tap, Bandage, Stand, Complete };
/** All offsets use the desk's world-aligned frame, in centimetres. Clips are ordinary editable animations. */
UCLASS(BlueprintType)
class BBSB_API UBBSBOpeningPresentation : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Writing;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Tap;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Bandage;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Stand;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<ULevelSequence> CameraSequence;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector SeatOffset=FVector(40,-95,89);
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float FacingYaw=90;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector BunniarOffset=FVector(10,-60,98);
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float BunniarYaw=180;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector CameraOffset=FVector(270,-210,155);
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector CameraLookAt=FVector(15,-48,95);
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraFOV=46;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector StandExitOffset=FVector(95,-165,89);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin=".2")) float CameraReturnSeconds=.85f;
};

/** Noninteractive, outlined skip hint; remains independent of the hidden gameplay HUD. */
UCLASS()
class BBSB_API UBBSBOpeningSkipPrompt : public UUserWidget
{
 GENERATED_BODY()
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
};

UCLASS()
class BBSB_API ABBSBStage01Opening : public AActor
{
 GENERATED_BODY()
public:
 ABBSBStage01Opening();
 virtual void Tick(float DT) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UFUNCTION(BlueprintCallable) bool Start(ABBSBStage01Director* Story,ABBSBCharacter* Character);
 UFUNCTION(BlueprintCallable) void Squeak();
 UFUNCTION(BlueprintCallable) void Skip();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bActive=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) EBBSBOpeningBeat Beat=EBBSBOpeningBeat::Writing;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Squeaks=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bBandageApplied=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FVector DeskOrigin;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBBSBOpeningPresentation> Presentation;
 UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly) TObjectPtr<ACameraActor> SceneCamera;
private:
 UPROPERTY(Transient) TObjectPtr<ABBSBCharacter> Player;
 UPROPERTY(Transient) TObjectPtr<ABBSBStage01Director> Director;
 UPROPERTY(Transient) TObjectPtr<ABBSBBunniarCompanion> Buddy;
 UPROPERTY(Transient) TObjectPtr<UInputComponent> Controls;
 UPROPERTY(Transient) TObjectPtr<UBBSBOpeningSkipPrompt> SkipPrompt;
 UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Pen;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Props;
 UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Sounds;
 UPROPERTY(Transient) TSubclassOf<UAnimInstance> SavedAnimClass;
 TWeakObjectPtr<AActor> PreviousView;
 FTransform SavedMeshTransform;
 bool bSavedHUD=true,bSavedCursor=false,bSavedGaze=true,bSavedPostProcess=false,bSavedEquipmentBlock=false;
 bool bOwnMoveIgnore=false,bOwnLookIgnore=false,bStartedTransition=false,bWritingReady=false;
 uint8 SavedAnimationMode=0,SavedRootMotionMode=0;
 bool bSavedRootMotionMode=false;
 double BeatStarted=0,NextScribble=0;
 uint8 TapCueCount=0;
 void ChangeBeat(EBBSBOpeningBeat Next);
 void Finish(bool Completed);
 void UpdateBuddy(float Age);
 void UpdateCamera(float Age);
 void PlaySoundCue(int Kind);
 UStaticMeshComponent* Prop(FName Name,FVector Position,FVector Scale,const TCHAR* Material,const TCHAR* Shape=TEXT("/Engine/BasicShapes/Cube.Cube"));
};
