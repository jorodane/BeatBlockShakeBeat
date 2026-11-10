#include "Combat/BBSBCombat.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
void UBBSBCombatComponent::SetExternalRhythmGrab(bool Active) { bExternalRhythmGrab=Active;ClearParry(); }
bool UBBSBCombatComponent::ResolveGrabParry(int32 Mask,float ImpactAt,float Window)
{
 if(!bExternalRhythmGrab || bDead || !Character || Mask<=0 || Mask>15 || HeldParryMask()!=uint8(Mask))return false;
 for(int32 I=0;I<4;++I)if((Mask&(1<<I)) && FMath::Abs(Inputs[I].PressedAt-ImpactAt)>Window)return false;
 DefenseImpact(Character->GetActorLocation()+Character->GetActorForwardVector()*35,true);
 SetFeedback(TEXT("리듬 패링!"));return true;
}
