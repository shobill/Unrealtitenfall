#pragma once
#include "CoreMinimal.h"
#include "DMCombatDamageStimulus.h"

// Data-only threat snapshot for Knowledge/EQS consumers. Remembered actors are never queried for transforms.
struct FDMCombatThreatEntry
{
    TWeakObjectPtr<AActor> Actor;
    FVector KnownLocation = FVector::ZeroVector, KnownEyeLocation = FVector::ZeroVector;
    bool bCurrentlySensed = false, bVisible = false, bIsCurrentTarget = false, bIsFocusTarget = false;
    bool bAttackable = false, bFriendlyFireRisk = false, bRelevant = false, bPositionRelevant = false;
    float Distance = 0, LastSensedTime = 0, MemoryAge = 0, MemoryConfidence = 0;
    float ThreatScore = 0, TargetScore = 0, LastEvaluationTime = -100000;
    float CloseScore = 0, RearScore = 0, FacingScore = 0;
    float RecentDamageThreatBonus = 0, RecentDamageTargetBonus = 0;
    float MemoryExpiresAt = 0;
#if !UE_BUILD_SHIPPING
    FString DebugReason;
#endif
};

struct FDMCombatThreatSnapshot
{
    TArray<FDMCombatThreatEntry> Threats;
    TWeakObjectPtr<AActor> CurrentTarget;
    FVector ObserverLocation = FVector::ZeroVector;
    float BuiltTime = -100000;
    uint32 Revision = 0;
    int32 VisibleThreatCount = 0, RememberedThreatCount = 0, RelevantThreatCount = 0;
};
