#pragma once

#include "CoreMinimal.h"
#include "DeathMatch/AI/Combat/DMFireCycle.h"
#include "DMCombatFireTypes.generated.h"

/** Fire pattern policy only. It never decides whether combat should start. */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, meta=(DisplayName="Fire Action"))
class SKYSEOUL_API UDMCombatFireModule : public UObject
{
	GENERATED_BODY()
public:
	virtual void Reset();
	virtual bool IsReady(float Now) const;
	virtual void ShotFired(float Now);
	virtual FDMFirePatternSettings GetCycleSettings() const { return {}; }
	virtual bool AllowsMoveSpeed(double Speed) const { return true; }
	virtual float GetMaxFireMoveSpeed() const { return TNumericLimits<float>::Max(); }
	/** Compatibility observation only. Fire Control's FireCycle.NextShotTime is authoritative. */
	float GetNextShotAllowedTime() const { return NextShotTime; }
	/** Called only by Fire Control to mirror its authoritative FireCycle time. */
	void SetNextShotAllowedTime(float Time) { NextShotTime = Time; }
protected:
	float NextShotTime = 0.f;
};

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, meta=(DisplayName="Precision Single Fire"))
class SKYSEOUL_API UDMCombatFire_Precision : public UDMCombatFireModule
{
	GENERATED_BODY()
public:
	virtual void ShotFired(float Now) override;
	virtual FDMFirePatternSettings GetCycleSettings() const override
	{
		return {EDMAdaptiveFirePattern::Single, 1, 0.f, ShotPauseSeconds};
	}
	virtual bool AllowsMoveSpeed(double Speed) const override
	{
		return bAllowFireWhileMoving || Speed <= 1.0 + UE_KINDA_SMALL_NUMBER;
	}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta=(ClampMin="0")) float AimSeconds = .8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta=(ClampMin="0")) float ShotPauseSeconds = .8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire|Movement") bool bAllowFireWhileMoving = true;
};

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, meta=(DisplayName="Burst Fire"))
class SKYSEOUL_API UDMCombatFire_Burst : public UDMCombatFireModule
{
	GENERATED_BODY()
public:
	virtual void Reset() override;
	virtual void ShotFired(float Now) override;
	virtual FDMFirePatternSettings GetCycleSettings() const override
	{
		return {EDMAdaptiveFirePattern::Burst, FMath::Max(1, ShotsPerBurst), ShotInterval, BurstPause};
	}
	virtual bool AllowsMoveSpeed(double Speed) const override
	{
		return bAllowFireWhileMoving || Speed <= 1.0 + UE_KINDA_SMALL_NUMBER;
	}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire") bool bAllowFireWhileMoving = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta=(ClampMin="1")) int32 ShotsPerBurst = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta=(ClampMin="0.01")) float ShotInterval = .1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta=(ClampMin="0.01")) float BurstPause = .8f;
private:
	int32 Shots = 0;
};

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, meta=(DisplayName="Automatic Fire"))
class SKYSEOUL_API UDMCombatFire_Automatic : public UDMCombatFireModule
{
	GENERATED_BODY()
public:
	virtual void Reset() override { Super::Reset(); Shots = 0; }
	virtual void ShotFired(float Now) override;
	virtual FDMFirePatternSettings GetCycleSettings() const override
	{
		return {EDMAdaptiveFirePattern::Automatic, FMath::Max(0, MaxAutomaticShots), ShotInterval, AutomaticPause};
	}
	virtual bool AllowsMoveSpeed(double Speed) const override
	{
		return bAllowFireWhileMoving || Speed <= 1.0 + UE_KINDA_SMALL_NUMBER;
	}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta=(ClampMin="0")) int32 MaxAutomaticShots = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta=(ClampMin="0")) float AutomaticPause = .65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire") bool bAllowFireWhileMoving = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire", meta=(ClampMin="0.01")) float ShotInterval = .1f;
private:
	int32 Shots = 0;
};
