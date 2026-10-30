#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DeathMatch/Abilities/DMGameplayAbility_Fire.h"
#include "DeathMatch/AI/Combat/DMCombatFire_Adaptive.h"
#include "DeathMatch/AI/Fire/DMCombatFireTypes.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Execution/DMActionIntentTypes.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "DeathMatch/Weapons/DMWeaponActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMFirePatternPolicyTest,
	"SkySeoul.DeathMatch.Combat.FireControl.Patterns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMFirePatternPolicyTest::RunTest(const FString&)
{
	UDMCombatFire_Precision* Single = NewObject<UDMCombatFire_Precision>();
	UDMCombatFire_Burst* Burst = NewObject<UDMCombatFire_Burst>();
	UDMCombatFire_Automatic* Automatic = NewObject<UDMCombatFire_Automatic>();
	Burst->ShotsPerBurst = 4;
	Automatic->MaxAutomaticShots = 0;
	TestTrue(TEXT("Precision maps to Single"), Single->GetCycleSettings().Pattern == EDMAdaptiveFirePattern::Single);
	TestEqual(TEXT("Single has one successful shot"), Single->GetCycleSettings().ShotLimit, 1);
	TestTrue(TEXT("Burst maps to Burst"), Burst->GetCycleSettings().Pattern == EDMAdaptiveFirePattern::Burst);
	TestEqual(TEXT("Burst preserves configured shot count"), Burst->GetCycleSettings().ShotLimit, 4);
	TestTrue(TEXT("Automatic maps to Automatic"), Automatic->GetCycleSettings().Pattern == EDMAdaptiveFirePattern::Automatic);
	TestEqual(TEXT("Zero automatic limit means continuous"), Automatic->GetCycleSettings().ShotLimit, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMFireCycleCadenceAuthorityTest,
	"SkySeoul.DeathMatch.Combat.FireControl.CadenceAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMFireCycleCadenceAuthorityTest::RunTest(const FString&)
{
	FDMFireCycleRuntime Single;
	Single.Select({EDMAdaptiveFirePattern::Single, 1, 0.f, .8f}, true);
	Single.BeginCycle();
	TestEqual(TEXT("Ability/cycle start does not spend successful-shot cadence"), Single.NextShotTime, 0.f);
	Single.RecordSuccessfulShot(5.f);
	TestTrue(TEXT("Single commits its pause only after success"), FMath::IsNearlyEqual(Single.NextShotTime, 5.8f, .001f));

	FDMFireCycleRuntime Burst;
	Burst.Select({EDMAdaptiveFirePattern::Burst, 3, .1f, .8f}, true);
	Burst.RecordSuccessfulShot(1.f);
	TestTrue(TEXT("Burst first shot uses interval"), FMath::IsNearlyEqual(Burst.NextShotTime, 1.1f, .001f));
	Burst.RecordSuccessfulShot(1.1f);
	TestTrue(TEXT("Burst middle shot uses interval"), FMath::IsNearlyEqual(Burst.NextShotTime, 1.2f, .001f));
	Burst.RecordSuccessfulShot(1.2f);
	TestTrue(TEXT("Burst final shot uses group pause"), FMath::IsNearlyEqual(Burst.NextShotTime, 2.f, .001f));

	FDMFireCycleRuntime Automatic;
	Automatic.Select({EDMAdaptiveFirePattern::Automatic, 2, .12f, .65f}, true);
	Automatic.RecordSuccessfulShot(2.f);
	TestTrue(TEXT("Automatic inner shot uses interval"), FMath::IsNearlyEqual(Automatic.NextShotTime, 2.12f, .001f));
	Automatic.RecordSuccessfulShot(2.12f);
	TestTrue(TEXT("Bounded Automatic final shot uses pause"), FMath::IsNearlyEqual(Automatic.NextShotTime, 2.77f, .001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMFireRetryTimingTest,
	"SkySeoul.DeathMatch.Combat.FireControl.RetryTimingIndependent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMFireRetryTimingTest::RunTest(const FString&)
{
	UDMCombatFireControlComponent* Fire = NewObject<UDMCombatFireControlComponent>();
	Fire->FireCycle.NextShotTime = 10.f;
	Fire->RecordFailedFireAttemptAt(EDMFireAttemptFailure::Sight, 2.f);
	TestEqual(TEXT("Failure retry does not overwrite successful-shot cadence"), Fire->GetNextShotAllowedTime(), 10.f);
	TestTrue(TEXT("Sight retry is tracked separately"), FMath::IsNearlyEqual(Fire->GetRetryAfterTime(), 2.1f));
	TestFalse(TEXT("Successful-shot cadence still blocks after retry expires"), Fire->IsFireCycleReadyAt(2.2f));

	Fire->FireCycle.NextShotTime = 0.f;
	Fire->RecordFailedFireAttemptAt(EDMFireAttemptFailure::FriendlyFireRisk, 4.f);
	TestFalse(TEXT("Failure retry can block while successful-shot cadence is ready"), Fire->IsFireCycleReadyAt(4.05f));
	TestTrue(TEXT("Failure retry expiry restores readiness"), Fire->IsFireCycleReadyAt(4.11f));

	UDMCombatFireControlComponent* Other = NewObject<UDMCombatFireControlComponent>();
	TestTrue(TEXT("Another AI keeps independent timing state"), Other->IsFireCycleReadyAt(4.05f));
	TestEqual(TEXT("Another AI successful-shot time is unchanged"), Other->GetNextShotAllowedTime(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMAdaptiveFireRangeTest,
	"SkySeoul.DeathMatch.Combat.FireControl.AdaptiveRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMAdaptiveFireRangeTest::RunTest(const FString&)
{
	UDMCombatFire_Adaptive* Adaptive = NewObject<UDMCombatFire_Adaptive>();
	Adaptive->ResolveBoundaries(3000.f, 5000.f);
	Adaptive->SelectPattern(1000.f, true);
	TestTrue(TEXT("Near range selects Automatic"), Adaptive->CurrentPattern == EDMAdaptiveFirePattern::Automatic);
	Adaptive->SelectPattern(2000.f, true);
	TestTrue(TEXT("Mid range selects Burst"), Adaptive->CurrentPattern == EDMAdaptiveFirePattern::Burst);
	Adaptive->SelectPattern(4000.f, true);
	TestTrue(TEXT("Far valid range selects Single"), Adaptive->CurrentPattern == EDMAdaptiveFirePattern::Single);
	TestTrue(TEXT("Maximum range is accepted"), Adaptive->InRange(5000.f));
	TestFalse(TEXT("Beyond maximum range is rejected"), Adaptive->InRange(5000.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMMovingFirePolicyTest,
	"SkySeoul.DeathMatch.Combat.FireControl.MovingFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMMovingFirePolicyTest::RunTest(const FString&)
{
	UDMCombatFire_Automatic* Automatic = NewObject<UDMCombatFire_Automatic>();
	Automatic->bAllowFireWhileMoving = false;
	TestTrue(TEXT("Stationary tolerance is allowed"), Automatic->AllowsMoveSpeed(1.0));
	TestFalse(TEXT("Movement is blocked when disabled"), Automatic->AllowsMoveSpeed(100.0));
	Automatic->bAllowFireWhileMoving = true;
	TestTrue(TEXT("Movement is allowed when enabled"), Automatic->AllowsMoveSpeed(600.0));
	TestTrue(TEXT("Attack defaults to moving fire enabled"),
		UDMCombatFireControlComponent::IntentAllowsMovingFire(EDMActionIntent::Attack));
	TestFalse(TEXT("Hide defaults to moving fire disabled"),
		UDMCombatFireControlComponent::IntentAllowsMovingFire(EDMActionIntent::Hide));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMWeaponRangeContractTest,
	"SkySeoul.DeathMatch.Combat.FireControl.WeaponRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMWeaponRangeContractTest::RunTest(const FString&)
{
	ADMWeaponActor* Weapon = NewObject<ADMWeaponActor>();
	TestTrue(TEXT("Effective range is positive"), Weapon->GetEffectiveRange() > 0.f);
	TestTrue(TEXT("Maximum range never falls below effective range"),
		Weapon->GetMaxFireRange() >= Weapon->GetEffectiveRange());
	Weapon->EffectiveRange = 2750.f;
	Weapon->MaxFireRange = 0.f;
	TestEqual(TEXT("Zero configured maximum preserves effective range as the maximum"),
		Weapon->GetMaxFireRange(), Weapon->GetEffectiveRange());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMFireRangeSingleAuthorityTest,
	"SkySeoul.DeathMatch.Combat.FireControl.RangeSingleAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMFireRangeSingleAuthorityTest::RunTest(const FString&)
{
	const FVector Muzzle(100.f, -20.f, 40.f);
	const FVector ExactEnd = Muzzle + FVector(5000.f, 0.f, 0.f);
	const FDMFireRangeEvaluation Exact = UDMCombatFireControlComponent::EvaluateFireRange(
		Muzzle, ExactEnd, 5000.f);
	TestTrue(TEXT("Exact maximum range is accepted"), Exact.bAllowed);
	TestEqual(TEXT("Range distance is measured from the supplied fire origin"), Exact.Distance, 5000.f);

	const FDMFireRangeEvaluation Tolerated = UDMCombatFireControlComponent::EvaluateFireRange(
		Muzzle, ExactEnd + FVector(UDMCombatFireControlComponent::FireRangeToleranceCm, 0.f, 0.f), 5000.f);
	const FDMFireRangeEvaluation Outside = UDMCombatFireControlComponent::EvaluateFireRange(
		Muzzle, ExactEnd + FVector(UDMCombatFireControlComponent::FireRangeToleranceCm + 1.f, 0.f, 0.f), 5000.f);
	TestTrue(TEXT("The centralized centimeter tolerance is accepted"), Tolerated.bAllowed);
	TestFalse(TEXT("A distance beyond the centralized tolerance is rejected"), Outside.bAllowed);

	AActor* Target = NewObject<AActor>();
	const FDMFireRangeEvaluation AimOffset = UDMCombatFireControlComponent::ResolveFireRange(
		nullptr, Target, FVector(0.f, 0.f, -40.f), 100.f);
	TestEqual(TEXT("Fallback aim location differs from the target actor origin"), AimOffset.End.Z, 60.0);
	TestTrue(TEXT("Resolved aim offset still uses the same exact-boundary contract"), AimOffset.bAllowed);

	const FDMFireRangeEvaluation BeforeNotify = UDMCombatFireControlComponent::EvaluateFireRange(
		FVector::ZeroVector, FVector(4990.f, 0.f, 0.f), 5000.f);
	const FDMFireRangeEvaluation AtNotify = UDMCombatFireControlComponent::EvaluateFireRange(
		FVector::ZeroVector, FVector(5100.f, 0.f, 0.f), 5000.f);
	TestTrue(TEXT("Target is initially inside range"), BeforeNotify.bAllowed);
	TestFalse(TEXT("Re-evaluation rejects a target that moved outside before notify"), AtNotify.bAllowed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMFireAbilityRangeRoutingTest,
	"SkySeoul.DeathMatch.Combat.FireControl.AbilityRangeRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMFireAbilityRangeRoutingTest::RunTest(const FString&)
{
	UDMCombatFireControlComponent* FireControl = NewObject<UDMCombatFireControlComponent>();
	TestTrue(TEXT("Enabled Fire Control with a module owns Ability range safety"),
		UDMGameplayAbility_Fire::UsesAuthoritativeFireControl(FireControl, false));
	FireControl->bEnabled = false;
	TestFalse(TEXT("Disabled Fire Control keeps the player/legacy range gate"),
		UDMGameplayAbility_Fire::UsesAuthoritativeFireControl(FireControl, false));
	TestFalse(TEXT("No Fire Control keeps the player/legacy range gate"),
		UDMGameplayAbility_Fire::UsesAuthoritativeFireControl(nullptr, false));
	TestTrue(TEXT("An already active AI fire cycle remains authoritative"),
		UDMGameplayAbility_Fire::UsesAuthoritativeFireControl(nullptr, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMFriendlyFirePolicyTest,
	"SkySeoul.DeathMatch.Combat.FireControl.FriendlyFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMFriendlyFirePolicyTest::RunTest(const FString&)
{
	ADeathMatchRobotAIController* Friendly = NewObject<ADeathMatchRobotAIController>();
	ADeathMatchRobotAIController* Enemy = NewObject<ADeathMatchRobotAIController>();
	AActor* IntendedTarget = NewObject<AActor>();
	Friendly->SetGenericTeamId(FGenericTeamId(1));
	Enemy->SetGenericTeamId(FGenericTeamId(2));
	TestTrue(TEXT("Friendly line blocker is rejected"),
		UDMCombatFireControlComponent::IsFriendlyFireBlocker(FGenericTeamId(1), Friendly, IntendedTarget));
	TestFalse(TEXT("Enemy line blocker is not classified as friendly fire"),
		UDMCombatFireControlComponent::IsFriendlyFireBlocker(FGenericTeamId(1), Enemy, IntendedTarget));
	TestFalse(TEXT("Intended target is never classified as a blocker"),
		UDMCombatFireControlComponent::IsFriendlyFireBlocker(FGenericTeamId(1), IntendedTarget, IntendedTarget));
	return true;
}

#endif
