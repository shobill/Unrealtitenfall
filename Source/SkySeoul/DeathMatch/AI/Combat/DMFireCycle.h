#pragma once
#include "CoreMinimal.h"
#include "DeathMatch/Abilities/DMFireAttempt.h"
#include "DMFireCycle.generated.h"

// Keep reflected names and ordinal values for existing Blueprint data.
UENUM(BlueprintType)
enum class EDMFireMode : uint8 { UseExistingModule, Precision, Burst, Automatic, Adaptive };
UENUM(BlueprintType)
enum class EDMAdaptiveFirePattern : uint8 { Single, Burst, Automatic };
UENUM(BlueprintType)
enum class EDMFirePatternRangeMode : uint8 { UseWeaponRangeRatios, UseCustomDistances };
UENUM(BlueprintType)
enum class EDMFireCycleState : uint8 { Idle, Starting, WaitingForFireNotify, ShotResolved, WaitingForCycleNotify, CycleQueued, BurstPause, Ending };

/** Value returned by a Fire Action; contains no montage, timer or controller ownership. */
struct FDMFirePatternSettings
{
    EDMAdaptiveFirePattern Pattern = EDMAdaptiveFirePattern::Single;
    int32 ShotLimit = 1; // Zero means unlimited Automatic.
    float ShotInterval = 0.f;
    float Pause = 0.f;
};

/** Fire Control owns this runtime state. Fire Ability is its only cycle executor. */
struct FDMFireCycleRuntime
{
    FDMFirePatternSettings Settings;
    EDMFireCycleState CycleState = EDMFireCycleState::Idle;
    int32 CycleId = 0;
    int32 SuccessfulShotsInPattern = 0;
    int32 SuccessfulShotsInActivation = 0;
    int32 RemainingBurstShots = 0;
    bool FireNotifyReceived = false;
    bool FireCycleNotifyReceived = false;
    bool bGroupComplete = false;
    float NextShotTime = 0.f;
    EDMFireAttemptFailure LastFailureReason = EDMFireAttemptFailure::None;

    void Select(const FDMFirePatternSettings& NewSettings, bool bNewActivation)
    {
        if (bNewActivation || Settings.Pattern != NewSettings.Pattern)
            SuccessfulShotsInPattern = 0;
        if (bNewActivation) SuccessfulShotsInActivation = 0;
        Settings = NewSettings;
        bGroupComplete = false;
        RemainingBurstShots = Settings.Pattern == EDMAdaptiveFirePattern::Burst
            ? FMath::Max(0, FMath::Max(1, Settings.ShotLimit) - SuccessfulShotsInPattern) : 0;
    }
    void BeginCycle()
    {
        ++CycleId;
        FireNotifyReceived = FireCycleNotifyReceived = false;
        LastFailureReason = EDMFireAttemptFailure::None;
        CycleState = EDMFireCycleState::WaitingForFireNotify;
    }
    void RecordSuccessfulShot(float Now)
    {
        ++SuccessfulShotsInPattern;
        ++SuccessfulShotsInActivation;
        bGroupComplete = Settings.Pattern == EDMAdaptiveFirePattern::Single
            || (Settings.ShotLimit > 0 && SuccessfulShotsInPattern >= Settings.ShotLimit);
        RemainingBurstShots = Settings.Pattern == EDMAdaptiveFirePattern::Burst
            ? FMath::Max(0, Settings.ShotLimit - SuccessfulShotsInPattern) : 0;
        const float Interval = FMath::IsFinite(Settings.ShotInterval) ? FMath::Max(0.f, Settings.ShotInterval) : 0.f;
        const float Pause = FMath::IsFinite(Settings.Pause) ? FMath::Max(0.f, Settings.Pause) : 0.f;
        NextShotTime = Now + (bGroupComplete ? FMath::Max(Interval, Pause) : FMath::Max(.01f, Interval));
        CycleState = EDMFireCycleState::WaitingForCycleNotify;
    }
    bool ShouldContinue() const { return Settings.Pattern != EDMAdaptiveFirePattern::Single && !bGroupComplete; }
    float GetNextShotAllowedTime() const { return NextShotTime; }
    float GetRemainingShotDelay(float Now) const { return FMath::Max(0.f, NextShotTime - Now); }
    bool IsShotReady(float Now) const { return Now + UE_KINDA_SMALL_NUMBER >= NextShotTime; }
};
