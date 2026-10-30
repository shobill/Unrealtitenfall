#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

namespace PartsHitCollision
{
    // Project HitTrace. Both sides may override in BP, but must agree.
    constexpr ECollisionChannel DefaultWeaponTraceChannel = ECC_GameTraceChannel1;
    inline const FName DetachedDebrisTag(TEXT("Parts.DetachedDebris"));
}
