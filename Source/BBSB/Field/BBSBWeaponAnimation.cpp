#include "BBSBWeaponAnimation.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

const FBBSBWeaponAnimationClip* UBBSBWeaponAnimationSet::FindClip(EBBSBFieldGesture G,int32 Channel,int32 Combo) const
{
 const FBBSBWeaponAnimationClip* Best=nullptr; int32 BestScore=-1;
 for (const auto& Clip:Clips)
 {
  if (Clip.Gesture!=G || !Clip.Sequence || (Clip.MouseChannel>=0 && Clip.MouseChannel!=Channel) || (Clip.ComboStep>0 && Clip.ComboStep!=Combo)) continue;
  const int32 Score=(Clip.MouseChannel>=0 ? 2:0)+(Clip.ComboStep>0 ? 1:0);
  if (Score>BestScore) { Best=&Clip; BestScore=Score; }
 }
 return Best;
}

bool UBBSBWeaponAnimationSet::SampleTransform(const ULevelSequence* Sequence,FName Name,float Alpha,FTransform& Out)
{
 if (!Sequence || !Sequence->GetMovieScene()) return false;
 UMovieScene* Scene=Sequence->GetMovieScene(); const auto Range=Scene->GetPlaybackRange();
 if (!Range.HasLowerBound() || !Range.HasUpperBound()) return false;
 const double Frame=FMath::Lerp(double(Range.GetLowerBoundValue().Value),double(Range.GetUpperBoundValue().Value),double(FMath::Clamp(Alpha,0.f,1.f)));
 const FFrameTime Time=FFrameTime::FromDecimal(Frame);
 for (const FMovieSceneBinding& Binding:static_cast<const UMovieScene*>(Scene)->GetBindings())
 {
  const FMovieScenePossessable* Possessable=Scene->FindPossessable(Binding.GetObjectGuid());
  const FMovieSceneSpawnable* Spawnable=Scene->FindSpawnable(Binding.GetObjectGuid());
  const FString BindingName=Possessable ? Possessable->GetName() : Spawnable ? Spawnable->GetName() : FString();
  if (FName(*BindingName)!=Name) continue;
  for (UMovieSceneTrack* Track:Binding.GetTracks()) if (const auto* Transform=Cast<UMovieScene3DTransformTrack>(Track))
   for (UMovieSceneSection* Base:Transform->GetAllSections()) if (const auto* Section=Cast<UMovieScene3DTransformSection>(Base))
   {
    if (!Section->IsActive() || (!Section->GetRange().Contains(Time.FloorToFrame()) && Alpha<1.f)) continue;
    const auto Channels=Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
    if (Channels.Num()<9) continue;
    double V[9]={0,0,0,0,0,0,1,1,1};
    for (int32 I=0;I<9;++I) Channels[I]->Evaluate(Time,V[I]);
    Out=FTransform(FRotator(V[4],V[5],V[3]),FVector(V[0],V[1],V[2]),FVector(V[6],V[7],V[8])); return !Out.ContainsNaN();
   }
 }
 return false;
}

FTransform UBBSBWeaponAnimationSet::SampleClip(int32 Index,float Time) const
{
 FTransform Pose=FTransform::Identity;
 if (Clips.IsValidIndex(Index)) SampleTransform(Clips[Index].Sequence,TEXT("WeaponMotion"),Time,Pose);
 return Pose;
}

ABBSBWeaponAnimationPreview::ABBSBWeaponAnimationPreview()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StudioRoot")));
 CharacterBody=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterBody")); CharacterBody->SetupAttachment(RootComponent);
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> PlayerMesh(TEXT("/Game/Meshes/Characters/Reparado/Reparado.Reparado")); CharacterBody->SetSkeletalMesh(PlayerMesh.Object);
 CharacterBody->SetRelativeRotation(FRotator(0,-90,0)); CharacterBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Grip=CreateDefaultSubobject<USceneComponent>(TEXT("Grip")); Grip->SetupAttachment(CharacterBody,TEXT("HandGrip_R"));
 OtherGrip=CreateDefaultSubobject<USceneComponent>(TEXT("OtherGrip")); OtherGrip->SetupAttachment(CharacterBody,TEXT("HandGrip_L"));
 HitAnchor=CreateDefaultSubobject<USceneComponent>(TEXT("HitAnchor")); HitAnchor->SetupAttachment(RootComponent); HitAnchor->SetRelativeLocation(FVector(250,0,150));
 WeaponMotion=CreateDefaultSubobject<USceneComponent>(TEXT("WeaponMotion")); WeaponMotion->SetupAttachment(HitAnchor);
 WeaponModel=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponModel")); WeaponModel->SetupAttachment(WeaponMotion);
 Full=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Full")); Full->SetupAttachment(Grip);
 Upper=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Upper")); Upper->SetupAttachment(Grip);
 Lower=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lower")); Lower->SetupAttachment(Grip);
 OtherLower=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OtherLower")); OtherLower->SetupAttachment(OtherGrip);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> FullMesh(TEXT("/Game/Meshes/Weapons/Reparaton/Full/Reparaton_Full.Reparaton_Full")); Full->SetStaticMesh(FullMesh.Object);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> TopMesh(TEXT("/Game/Meshes/Weapons/Reparaton/Top/Reparaton_Top.Reparaton_Top")); Upper->SetStaticMesh(TopMesh.Object);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> BottomMesh(TEXT("/Game/Meshes/Weapons/Reparaton/Bottom/Reparaton_Bottom.Reparaton_Bottom")); Lower->SetStaticMesh(BottomMesh.Object); OtherLower->SetStaticMesh(BottomMesh.Object);
 for (UStaticMeshComponent* Part:{WeaponModel.Get(),Full.Get(),Upper.Get(),Lower.Get(),OtherLower.Get()}) Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABBSBWeaponAnimationPreview::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 Grip->AttachToComponent(CharacterBody,FAttachmentTransformRules::SnapToTargetNotIncludingScale,CharacterBody->DoesSocketExist(TEXT("HandGrip_R")) ? TEXT("HandGrip_R"):TEXT("hand_r"));
 OtherGrip->AttachToComponent(CharacterBody,FAttachmentTransformRules::SnapToTargetNotIncludingScale,CharacterBody->DoesSocketExist(TEXT("HandGrip_L")) ? TEXT("HandGrip_L"):TEXT("hand_l"));
 if (!AnimationSet) return;
 WeaponModel->SetStaticMesh(AnimationSet->WeaponMesh);
 if (AnimationSet->WeaponMesh) WeaponModel->SetRelativeLocation(-AnimationSet->WeaponMesh->GetBoundingBox().GetCenter());
 const bool Dual=AnimationSet->Weapon==EBBSBPrototypeWeapon::Sword || AnimationSet->Weapon==EBBSBPrototypeWeapon::Dagger || AnimationSet->Weapon==EBBSBPrototypeWeapon::Crescent;
 Full->SetVisibility(!AnimationSet->bSplitReparaton); Upper->SetVisibility(AnimationSet->bSplitReparaton); Lower->SetVisibility(AnimationSet->bSplitReparaton && !Dual); OtherLower->SetVisibility(Dual);
 PreviewForm();
}
void ABBSBWeaponAnimationPreview::PreviewForm()
{
 if (!AnimationSet) return;
 FTransform Pose;
 if (UBBSBWeaponAnimationSet::SampleTransform(AnimationSet->FormSequence,TEXT("Upper"),PreviewTime,Pose)) Upper->SetRelativeTransform(Pose);
 if (UBBSBWeaponAnimationSet::SampleTransform(AnimationSet->FormSequence,TEXT("Lower"),PreviewTime,Pose)) { Lower->SetRelativeTransform(Pose); OtherLower->SetRelativeTransform(Pose); }
}
void ABBSBWeaponAnimationPreview::PreviewAction()
{
 if (AnimationSet) WeaponMotion->SetRelativeTransform(AnimationSet->SampleClip(ClipIndex,PreviewTime));
}
