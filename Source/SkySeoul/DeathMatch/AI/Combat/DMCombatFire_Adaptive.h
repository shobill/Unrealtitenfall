#pragma once
#include "CoreMinimal.h"
#include "DeathMatch/AI/Fire/DMCombatFireTypes.h"
#include "DeathMatch/Abilities/DMFireAttempt.h"
#include "DMCombatFire_Adaptive.generated.h"

class ADMWeaponActor;
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, meta=(DisplayName="Distance Adaptive Fire"))
class SKYSEOUL_API UDMCombatFire_Adaptive : public UDMCombatFireModule
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Range") EDMFirePatternRangeMode RangeMode = EDMFirePatternRangeMode::UseWeaponRangeRatios;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Range") float AutomaticRangeRatio = .40f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Range") float BurstRangeRatio = .85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Range") float AutomaticMaxDistance = 800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Range") float BurstMaxDistance = 1700.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Range") float SingleMaxDistance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Range", meta=(ClampMin="0")) float PatternDistanceHysteresis = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Automatic", meta=(ClampMin="0.01")) float AutomaticShotInterval = .12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Automatic", meta=(ClampMin="0")) int32 MaxAutomaticShots = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Automatic", meta=(ClampMin="0")) float AutomaticPause = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Burst", meta=(ClampMin="1")) int32 BurstShotCount = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Burst", meta=(ClampMin="0.01")) float BurstShotInterval = .16f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Burst", meta=(ClampMin="0")) float BurstPause = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Single", meta=(ClampMin="0")) float SingleShotPause = .90f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Movement", meta=(ToolTip="False permits stationary fire only (1 cm/s tolerance); Movement Speed Policy owns desired speed.")) bool bAllowFireWhileMoving = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Combat|Legacy", meta=(DeprecatedProperty, DeprecationMessage="Use Movement Speed Policy Firing multiplier.")) float MaxFireMoveSpeed = 600.f;

    // Read-only BP observations synchronized from Fire Control runtime.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire|State") EDMAdaptiveFirePattern CurrentPattern = EDMAdaptiveFirePattern::Single;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire|State") EDMFireCycleState CycleState = EDMFireCycleState::Idle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire|State") float PatternSelectionDistance = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire|State") int32 SuccessfulShotsInPattern = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire|State") int32 RemainingBurstShots = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire|State") int32 CycleId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire|State") float NextBurstAllowedTime = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire|State") bool FireNotifyReceived = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fire|State") bool FireCycleNotifyReceived = false;
    float AutomaticBoundary = 0, BurstBoundary = 0, SingleBoundary = 0;
    float CachedEffectiveRange = 0, CachedMaxFireRange = 0;
    EDMFireAttemptFailure LastFailureReason = EDMFireAttemptFailure::None;
    TWeakObjectPtr<AActor> Target;
    TWeakObjectPtr<ADMWeaponActor> Weapon;
    bool bPatternSelected = false;
    bool bGroupComplete = false;
    bool bWarnedInvalidRange = false;
    virtual FDMFirePatternSettings GetCycleSettings() const override;
    void ObserveCycle(const FDMFireCycleRuntime& State);
    virtual void Reset() override;
    virtual void ShotFired(float Now) override;
    void ResolveBoundaries(float Effective, float Maximum);
    void SelectPattern(float Distance, bool bForce = false);
    bool Prepare(AActor* NewTarget, ADMWeaponActor* NewWeapon, float Distance);
    bool InRange(float Distance) const { return FMath::IsFinite(Distance) && Distance >= 0 && Distance <= SingleBoundary; }
    virtual bool AllowsMoveSpeed(double Speed) const override { return bAllowFireWhileMoving || Speed <= 1.0 + UE_KINDA_SMALL_NUMBER; }
    virtual float GetMaxFireMoveSpeed() const override { return bAllowFireWhileMoving ? FMath::Max(0.f, MaxFireMoveSpeed) : 1.f; }
    float GetNextShotAllowedTime() const { return NextShotTime; }
    bool ShouldContinue() const { return CurrentPattern != EDMAdaptiveFirePattern::Single && !bGroupComplete; }
    void BeginCycle();
};
