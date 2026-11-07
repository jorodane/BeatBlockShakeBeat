#pragma once
#include "CoreMinimal.h"
#include "Engine/Font.h"
#include "Fonts/SlateFontInfo.h"
#include "UObject/StrongObjectPtr.h"

namespace BBSBHUDTypography
{
inline FSlateFontInfo NumberFont(float Pixels)
{
 // Canvas requires a UFont-backed font info. CoreStyle's file-only font info
 // works in Slate but has no Canvas font object, so its text is skipped.
 static TStrongObjectPtr<UFont> Font(LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/Roboto.Roboto")));
 return FSlateFontInfo(Font.Get(),FMath::Max(8,FMath::RoundToInt(Pixels)),TEXT("Bold"));
}
}
