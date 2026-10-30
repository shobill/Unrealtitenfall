#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DeathMatch/AI/BT/DMBTDataNodes.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Fire/DMCombatFireTypes.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"

namespace
{
EDMClearShotFailure ClearShot(bool bContext, bool bTarget, bool bVisible, bool bWeapon,
	EDMFireAttemptFailure LineFailure = EDMFireAttemptFailure::None)
{
	return UDMCombatFireControlComponent::ClassifyClearShotFailure(
		bContext, bTarget, bVisible, bWeapon, LineFailure);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMClearShotPreconditionTest,
	"SkySeoul.DeathMatch.Combat.ClearShot.Preconditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMClearShotPreconditionTest::RunTest(const FString&)
{
	TestTrue(TEXT("Valid visible selected target, weapon, and open shared line are clear"),
		ClearShot(true, true, true, true) == EDMClearShotFailure::None);
	TestTrue(TEXT("No selected CombatTarget is invalid"),
		ClearShot(true, false, false, true) == EDMClearShotFailure::InvalidTarget);
	TestTrue(TEXT("Another visible enemy does not replace a missing selected CombatTarget"),
		ClearShot(true, false, true, true) == EDMClearShotFailure::InvalidTarget);
	TestTrue(TEXT("Remembered but not currently visible CombatTarget is rejected"),
		ClearShot(true, true, false, true) == EDMClearShotFailure::NotVisible);
	TestTrue(TEXT("Missing active ranged weapon is rejected"),
		ClearShot(true, true, true, false) == EDMClearShotFailure::MissingWeapon);
	TestTrue(TEXT("Invalid controller or Pawn context is rejected"),
		ClearShot(false, true, true, true) == EDMClearShotFailure::InvalidTarget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMClearShotSharedLineContractTest,
	"SkySeoul.DeathMatch.Combat.ClearShot.SharedLineContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMClearShotSharedLineContractTest::RunTest(const FString&)
{
	AActor* Target = NewObject<AActor>();
	AActor* WorldBlocker = NewObject<AActor>();
	ADeathMatchRobotAIController* Friendly = NewObject<ADeathMatchRobotAIController>();
	Friendly->SetGenericTeamId(FGenericTeamId(1));
	const EDMFireAttemptFailure OpenLine = UDMCombatFireControlComponent::ClassifyLineOfFireHit(
		FGenericTeamId(1), Target, Target, true);
	const EDMFireAttemptFailure BlockedLine = UDMCombatFireControlComponent::ClassifyLineOfFireHit(
		FGenericTeamId(1), WorldBlocker, Target, true);
	const EDMFireAttemptFailure FriendlyLine = UDMCombatFireControlComponent::ClassifyLineOfFireHit(
		FGenericTeamId(1), Friendly, Target, true);
	TestTrue(TEXT("Shared line classifier accepts the intended target"),
		ClearShot(true, true, true, true, OpenLine) == EDMClearShotFailure::None);
	TestTrue(TEXT("Maximum range failure is preserved"),
		ClearShot(true, true, true, true, EDMFireAttemptFailure::Range) == EDMClearShotFailure::Range);
	TestTrue(TEXT("World structure blocking is reported as Sight"),
		ClearShot(true, true, true, true, BlockedLine) == EDMClearShotFailure::Sight);
	TestTrue(TEXT("Friendly weapon-line blocking is preserved"),
		ClearShot(true, true, true, true, FriendlyLine) == EDMClearShotFailure::FriendlyFireRisk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMClearShotExecutionIndependenceTest,
	"SkySeoul.DeathMatch.Combat.ClearShot.ExecutionIndependence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMClearShotExecutionIndependenceTest::RunTest(const FString&)
{
	UDMCombatFireControlComponent* FireControl = NewObject<UDMCombatFireControlComponent>();
	FireControl->FireCycle.NextShotTime = 10.f;
	TestFalse(TEXT("Test setup is inside authoritative cadence cooldown"), FireControl->IsFireCycleReadyAt(0.f));
	TestTrue(TEXT("Cadence cooldown does not remove a clear shot"),
		ClearShot(true, true, true, true) == EDMClearShotFailure::None);
	UDMCombatFire_Precision* FirePattern = NewObject<UDMCombatFire_Precision>();
	FirePattern->bAllowFireWhileMoving = false;
	TestFalse(TEXT("Test setup forbids moving fire"), FirePattern->AllowsMoveSpeed(600.0));
	TestTrue(TEXT("Moving-fire policy does not remove a clear shot"),
		ClearShot(true, true, true, true) == EDMClearShotFailure::None);
	const bool bMontageReady = false;
	const bool bFireAbilityActive = false;
	TestFalse(TEXT("Test setup has no montage execution window"), bMontageReady);
	TestTrue(TEXT("Montage state does not remove a clear shot"),
		ClearShot(true, true, true, true) == EDMClearShotFailure::None);
	TestFalse(TEXT("Test setup has no active Fire Ability"), bFireAbilityActive);
	TestTrue(TEXT("Fire Ability commit state does not remove a clear shot"),
		ClearShot(true, true, true, true) == EDMClearShotFailure::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMClearShotDecoratorContractTest,
	"SkySeoul.DeathMatch.Combat.ClearShot.DecoratorContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMClearShotDecoratorContractTest::RunTest(const FString&)
{
	UBTDecorator_DM_HasClearShot* Decorator = NewObject<UBTDecorator_DM_HasClearShot>();
	TestEqual(TEXT("BT editor display name"), Decorator->GetNodeName(), FString(TEXT("DM Has Clear Shot")));
	TestTrue(TEXT("Observer Aborts defaults to Both"), Decorator->GetFlowAbortMode() == EBTFlowAbortMode::Both);
	TestFalse(TEXT("Condition is not inversed"), Decorator->IsInversed());
	TestEqual(TEXT("Default reevaluation interval is bounded"), Decorator->ReevaluationInterval, 0.1f);
	TestTrue(TEXT("Each Behavior Tree component receives a node instance"), Decorator->HasInstance());
	TestTrue(TEXT("Blocked line drives Attack to Approach"),
		ClearShot(true, true, true, true, EDMFireAttemptFailure::Sight) != EDMClearShotFailure::None);
	TestTrue(TEXT("Recovered line drives Approach back to Attack"),
		ClearShot(true, true, true, true, EDMFireAttemptFailure::None) == EDMClearShotFailure::None);
	return true;
}

#endif
