#pragma once
#include "CoreMinimal.h"

class AController;

enum class EDMDamageAwarenessType : uint8 { Expected, Surprise, Unknown };

// Event-time evidence only. No owning references and no perception/animation side effects.
struct FDMCombatDamageStimulus
{
    TWeakObjectPtr<AActor> Attacker, DamageCauser;
    TWeakObjectPtr<AController> AttackerController;
    FVector DamageOrigin = FVector::ZeroVector, DamageDirection = FVector::ZeroVector, HitLocation = FVector::ZeroVector;
    FName HitPartName;
    float DamageAmount = 0, DamageTime = -100000;
    bool bHasReliableAttacker = false, bHasReliableOrigin = false;
    EDMDamageAwarenessType AwarenessType = EDMDamageAwarenessType::Unknown;
};

struct FDMDamageThreatRecord
{
    TWeakObjectPtr<AActor> Attacker;
    float RecentDamageAmount = 0, RecentDamageRatio = 0, LastDamageTime = -100000, FirstDamageTimeInWindow = 0;
    float LastSurpriseDamageTime = -100000;
    float LastKnownDamageOriginTime = -100000;
    int32 RecentHitCount = 0;
    EDMDamageAwarenessType LastDamageAwarenessType = EDMDamageAwarenessType::Unknown;
    FVector LastKnownDamageOrigin = FVector::ZeroVector;
    bool bHasReliableOrigin = false;
};

