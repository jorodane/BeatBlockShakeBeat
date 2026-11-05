#pragma once
#include "Engine/DamageEvents.h"

/** Ongoing status damage cannot roll another critical from the original hit. */
struct FBBSBStatusDamageEvent : FDamageEvent
{
 static constexpr int32 ClassID=0x42534201;
 virtual int32 GetTypeID() const override { return ClassID; }
 virtual bool IsOfType(int32 ID) const override { return ID==ClassID || FDamageEvent::IsOfType(ID); }
};
