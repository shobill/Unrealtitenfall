#include "DMCombatFire_Adaptive.h"
#include "DeathMatch/Weapons/DMWeaponActor.h"

namespace { float SafeDistance(float V) { return FMath::IsFinite(V) ? FMath::Max(0.f, V) : 0.f; } }
void UDMCombatFire_Adaptive::Reset()
{
    // Reset context and grouping, but never erase an already earned pause/cadence.
    SuccessfulShotsInPattern = RemainingBurstShots = 0;
    bPatternSelected = bGroupComplete = false;
    FireNotifyReceived = FireCycleNotifyReceived = false;
    Target.Reset(); Weapon.Reset(); CycleState = EDMFireCycleState::Idle;
    ++CycleId;
}
void UDMCombatFire_Adaptive::ResolveBoundaries(float Effective, float Maximum)
{
    CachedEffectiveRange = SafeDistance(Effective);
    CachedMaxFireRange = FMath::Max(CachedEffectiveRange, SafeDistance(Maximum));
    if (RangeMode == EDMFirePatternRangeMode::UseWeaponRangeRatios)
    {
        const float A = FMath::Clamp(SafeDistance(AutomaticRangeRatio), 0.f, 1.f);
        const float B = FMath::Clamp(SafeDistance(BurstRangeRatio), A, 1.f);
        AutomaticBoundary = CachedEffectiveRange * A;
        BurstBoundary = CachedEffectiveRange * B;
        SingleBoundary = CachedMaxFireRange;
    }
    else
    {
        SingleBoundary = SingleMaxDistance <= 0 ? CachedMaxFireRange : FMath::Min(SafeDistance(SingleMaxDistance), CachedMaxFireRange);
        AutomaticBoundary = FMath::Min(SafeDistance(AutomaticMaxDistance), SingleBoundary);
        BurstBoundary = FMath::Clamp(SafeDistance(BurstMaxDistance), AutomaticBoundary, SingleBoundary);
        if (!bWarnedInvalidRange && (AutomaticBoundary != AutomaticMaxDistance || BurstBoundary != BurstMaxDistance || (SingleMaxDistance > 0 && SingleBoundary != SingleMaxDistance)))
        {
            bWarnedInvalidRange = true;
            UE_LOG(LogDM, Warning, TEXT("Adaptive Fire %s: custom distances clamped to %.1f / %.1f / %.1f"), *GetName(), AutomaticBoundary, BurstBoundary, SingleBoundary);
        }
    }
}
void UDMCombatFire_Adaptive::SelectPattern(float Distance, bool bForce)
{
    if (!bForce && bPatternSelected && CurrentPattern == EDMAdaptiveFirePattern::Burst && SuccessfulShotsInPattern > 0 && !bGroupComplete) return;
    EDMAdaptiveFirePattern Next = Distance <= AutomaticBoundary ? EDMAdaptiveFirePattern::Automatic : Distance <= BurstBoundary ? EDMAdaptiveFirePattern::Burst : EDMAdaptiveFirePattern::Single;
    const float H = SafeDistance(PatternDistanceHysteresis);
    if (!bForce && bPatternSelected)
    {
        if (CurrentPattern == EDMAdaptiveFirePattern::Automatic && Distance <= AutomaticBoundary + H) Next = CurrentPattern;
        if (CurrentPattern == EDMAdaptiveFirePattern::Burst && Distance > AutomaticBoundary - H && Distance <= BurstBoundary + H) Next = CurrentPattern;
        if (CurrentPattern == EDMAdaptiveFirePattern::Single && Distance > BurstBoundary - H) Next = CurrentPattern;
    }
    if (bForce || !bPatternSelected || Next != CurrentPattern || bGroupComplete) SuccessfulShotsInPattern = 0;
    CurrentPattern = Next; PatternSelectionDistance = Distance; bPatternSelected = true; bGroupComplete = false;
    RemainingBurstShots = CurrentPattern == EDMAdaptiveFirePattern::Burst ? FMath::Max(1, BurstShotCount) - SuccessfulShotsInPattern : 0;
}
bool UDMCombatFire_Adaptive::Prepare(AActor* NewTarget, ADMWeaponActor* NewWeapon, float Distance)
{
    if (!IsValid(NewWeapon)) { LastFailureReason = EDMFireAttemptFailure::MissingWeapon; return false; }
    const bool Changed = Target.Get() != NewTarget || Weapon.Get() != NewWeapon;
    Target = NewTarget; Weapon = NewWeapon;
    ResolveBoundaries(NewWeapon->GetEffectiveRange(), NewWeapon->GetMaxFireRange());
    SelectPattern(Distance, Changed);
    return InRange(Distance);
}
void UDMCombatFire_Adaptive::BeginCycle()
{
    ++CycleId; FireNotifyReceived = FireCycleNotifyReceived = false;
    CycleState = EDMFireCycleState::WaitingForFireNotify;
    LastFailureReason = EDMFireAttemptFailure::None;
}
FDMFirePatternSettings UDMCombatFire_Adaptive::GetCycleSettings() const
{
    switch (CurrentPattern)
    {
    case EDMAdaptiveFirePattern::Burst:
        return {CurrentPattern, FMath::Max(1, BurstShotCount), BurstShotInterval, BurstPause};
    case EDMAdaptiveFirePattern::Automatic:
        return {CurrentPattern, FMath::Max(0, MaxAutomaticShots), AutomaticShotInterval, AutomaticPause};
    default:
        return {EDMAdaptiveFirePattern::Single, 1, 0.f, SingleShotPause};
    }
}
void UDMCombatFire_Adaptive::ObserveCycle(const FDMFireCycleRuntime& State)
{
    // Blueprint read-only observations; Fire Control remains the runtime owner.
    CurrentPattern = State.Settings.Pattern;
    SuccessfulShotsInPattern = State.SuccessfulShotsInPattern;
    RemainingBurstShots = State.RemainingBurstShots;
    CycleId = State.CycleId;
    CycleState = State.CycleState;
    FireNotifyReceived = State.FireNotifyReceived;
    FireCycleNotifyReceived = State.FireCycleNotifyReceived;
    bGroupComplete = State.bGroupComplete;
    LastFailureReason = State.LastFailureReason;
    NextShotTime = State.NextShotTime;
    if (bGroupComplete && CurrentPattern == EDMAdaptiveFirePattern::Burst)
        NextBurstAllowedTime = NextShotTime;
}
void UDMCombatFire_Adaptive::ShotFired(float Now)
{
    // Native entry point uses the same accounting as Fire Control.
    FDMFireCycleRuntime State;
    State.Settings = GetCycleSettings();
    State.SuccessfulShotsInPattern = SuccessfulShotsInPattern;
    State.CycleId = CycleId;
    State.FireNotifyReceived = FireNotifyReceived;
    State.FireCycleNotifyReceived = FireCycleNotifyReceived;
    State.RecordSuccessfulShot(Now);
    ObserveCycle(State);
}
