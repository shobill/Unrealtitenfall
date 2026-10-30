#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DeathMatch/AI/BT/DMEQSParameters.h"
#include "DeathMatch/AI/BT/DMBTExecutionServices.h"
#include "DeathMatch/AI/BT/DMBTDataNodes.h"
#include "DeathMatch/AI/Combat/DMCombatDamageStimulus.h"
#include "DeathMatch/AI/Combat/DMCombatPositionRegistry.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/AI/Movement/DMMovementSpeedPolicyComponent.h"
#include "DeathMatch/AI/Move/DMCombatMoveDirector.h"
#include "DeathMatch/AI/Execution/DMActionIntentComponent.h"
#include "DeathMatch/AI/Execution/DMCombatFocusComponent.h"
#include "DeathMatch/AI/State/DMCombatStatusComponent.h"
#include "DeathMatch/AI/Targeting/DMCombatTargetSelectionPolicy.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "DeathMatch/Tests/DMFireControlTestComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTDecorator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatKnowledgeDamageTest,
	"SkySeoul.DeathMatch.BTFoundation.Knowledge.DamageOrigin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatKnowledgeDamageTest::RunTest(const FString&)
{
	UDMCombatKnowledgeComponent* Knowledge = NewObject<UDMCombatKnowledgeComponent>();
	FDMCombatDamageStimulus Stimulus;
	Stimulus.bHasReliableOrigin = true;
	Stimulus.DamageOrigin = FVector(10, 20, 30);
	Stimulus.DamageDirection = FVector::ForwardVector;
	Stimulus.DamageTime = 42.f;
	Stimulus.AwarenessType = EDMDamageAwarenessType::Surprise;
	Knowledge->NotifyDamageReceived(Stimulus);
	TestEqual(TEXT("Damage origin stored"), Knowledge->GetDamageOrigin(), Stimulus.DamageOrigin);
	TestEqual(TEXT("Damage direction stored"), Knowledge->GetDamageDirection(), Stimulus.DamageDirection);
	TestEqual(TEXT("Damage time stored"), Knowledge->GetLastDamageTime(), 42.f);
	TestTrue(TEXT("Surprise classification stored"), Knowledge->GetLastDamageAwarenessType() == EDMDamageAwarenessType::Surprise);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMMovementSpeedPolicyTest,
	"SkySeoul.DeathMatch.BTFoundation.SpeedPolicy.MinimumWins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMMovementSpeedPolicyTest::RunTest(const FString&)
{
	TMap<EDMMovementSpeedSource, float> Values;
	Values.Add(EDMMovementSpeedSource::Roam, .8f);
	Values.Add(EDMMovementSpeedSource::ADS, .55f);
	Values.Add(EDMMovementSpeedSource::Firing, .7f);
	EDMMovementSpeedSource Limiter = EDMMovementSpeedSource::ExternalStatus;
	TestEqual(TEXT("Minimum multiplier applied"), UDMMovementSpeedPolicyComponent::CalculateAppliedMultiplier(Values, &Limiter), .55f);
	TestTrue(TEXT("ADS reported as limiter"), Limiter == EDMMovementSpeedSource::ADS);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMActionIntentLifecycleTest,
	"SkySeoul.DeathMatch.BTFoundation.ActionIntent.BranchLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMActionIntentLifecycleTest::RunTest(const FString&)
{
	UDMActionIntentComponent* Intent = NewObject<UDMActionIntentComponent>();
	const uint64 Attack = Intent->BeginIntentRequest(EDMActionIntent::Attack);
	TestTrue(TEXT("Attack intent is active"), Intent->GetActiveIntent() == EDMActionIntent::Attack);
	TestTrue(TEXT("Fire Control policy allows Attack"), UDMCombatFireControlComponent::IntentAllowsFire(Intent->GetActiveIntent()));
	const uint64 Roam = Intent->BeginIntentRequest(EDMActionIntent::Roam);
	TestTrue(TEXT("Newer Roam cannot preempt an active Attack branch"),
		Intent->GetActiveIntent() == EDMActionIntent::Attack);
	const uint64 Reposition = Intent->BeginIntentRequest(EDMActionIntent::FastReposition);
	TestTrue(TEXT("Newer FastReposition cannot preempt an active Attack branch"),
		Intent->GetActiveIntent() == EDMActionIntent::Attack);
	const uint64 Hide = Intent->BeginIntentRequest(EDMActionIntent::Hide);
	TestTrue(TEXT("Higher-priority Hide can preempt Attack"), Intent->GetActiveIntent() == EDMActionIntent::Hide);
	TestFalse(TEXT("Fire Control policy blocks Hide"), UDMCombatFireControlComponent::IntentAllowsFire(Intent->GetActiveIntent()));
	Intent->EndIntentRequest(Hide);
	TestTrue(TEXT("Ending Hide restores Attack"), Intent->GetActiveIntent() == EDMActionIntent::Attack);
	Intent->EndIntentRequest(Attack);
	TestTrue(TEXT("Ending Attack restores the newest lower-priority request"),
		Intent->GetActiveIntent() == EDMActionIntent::FastReposition);
	Intent->EndIntentRequest(Reposition);
	Intent->EndIntentRequest(Roam);
	TestTrue(TEXT("Ending final branch restores None"), Intent->GetActiveIntent() == EDMActionIntent::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMStateTransitionOwnershipTest,
	"SkySeoul.DeathMatch.BTFoundation.StateTransition.RequestOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMStateTransitionOwnershipTest::RunTest(const FString&)
{
	UDMActionIntentComponent* Intent = NewObject<UDMActionIntentComponent>();
	const uint64 Roam = Intent->BeginIntentRequest(EDMActionIntent::Roam);
	TestTrue(TEXT("Roam intent exposes the distinct Root behavior state"),
		Intent->GetActiveBehaviorState() == EDMBehaviorState::Roam);
	const uint64 Attack = Intent->BeginIntentRequest(EDMActionIntent::Attack);
	TestTrue(TEXT("Combat preempts Roam at the Root-state layer"),
		Intent->GetActiveBehaviorState() == EDMBehaviorState::Combat);
	TestTrue(TEXT("Combat keeps its Attack sub-intent"), Intent->GetActiveIntent() == EDMActionIntent::Attack);
	Intent->EndIntentRequest(Roam);
	Intent->EndIntentRequest(Roam);
	TestEqual(TEXT("Repeated old-branch cleanup does not remove the new request"), Intent->GetIntentRequestCount(), 1);
	TestTrue(TEXT("Attack survives repeated Roam cleanup"), Intent->GetActiveIntent() == EDMActionIntent::Attack);

	Intent->EndIntentRequest(Attack);
	const uint64 Observe = Intent->BeginIntentRequest(EDMActionIntent::Observe);
	TestTrue(TEXT("Combat to LostTarget selects the Observe Root state"),
		Intent->GetActiveBehaviorState() == EDMBehaviorState::LostTarget);
	const uint64 ReacquiredAttack = Intent->BeginIntentRequest(EDMActionIntent::Attack);
	Intent->EndIntentRequest(Observe);
	TestTrue(TEXT("LostTarget to Combat restores Attack"),
		Intent->GetActiveBehaviorState() == EDMBehaviorState::Combat);

	const uint64 ReturnRoam = Intent->BeginIntentRequest(EDMActionIntent::Roam);
	Intent->EndIntentRequest(ReacquiredAttack);
	TestTrue(TEXT("Complete Combat exit leaves only Roam"),
		Intent->GetActiveBehaviorState() == EDMBehaviorState::Roam);

	const uint64 FinalAttack = Intent->BeginIntentRequest(EDMActionIntent::Attack);
	Intent->EndIntentRequest(ReturnRoam);
	const uint64 Hide = Intent->BeginIntentRequest(EDMActionIntent::Hide);
	TestTrue(TEXT("Hide preempts Combat"), Intent->GetActiveBehaviorState() == EDMBehaviorState::Hide);
	Intent->EndIntentRequest(Hide);
	TestTrue(TEXT("Ending Hide restores the existing Combat request"),
		Intent->GetActiveBehaviorState() == EDMBehaviorState::Combat);
	Intent->EndIntentRequest(FinalAttack);
	TestTrue(TEXT("Ending the last request leaves no active Root state"),
		Intent->GetActiveBehaviorState() == EDMBehaviorState::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMMoveStateContractTest,
	"SkySeoul.DeathMatch.BTFoundation.StateTransition.MoveOwnerContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMMoveStateContractTest::RunTest(const FString&)
{
	TestTrue(TEXT("Roam allows only StrategicRoam"), UDMCombatMoveDirector::IsOwnerAllowedForState(
		EDMBehaviorState::Roam, EDMMoveRequestOwner::StrategicRoam));
	TestTrue(TEXT("LostTarget allows only Investigate"), UDMCombatMoveDirector::IsOwnerAllowedForState(
		EDMBehaviorState::LostTarget, EDMMoveRequestOwner::Investigate));
	TestTrue(TEXT("Combat allows only Combat"), UDMCombatMoveDirector::IsOwnerAllowedForState(
		EDMBehaviorState::Combat, EDMMoveRequestOwner::Combat));
	TestTrue(TEXT("Retreat allows only Retreat"), UDMCombatMoveDirector::IsOwnerAllowedForState(
		EDMBehaviorState::Retreat, EDMMoveRequestOwner::Retreat));
	TestTrue(TEXT("Hide allows only EmergencyHide"), UDMCombatMoveDirector::IsOwnerAllowedForState(
		EDMBehaviorState::Hide, EDMMoveRequestOwner::EmergencyHide));
	TestFalse(TEXT("Combat rejects a stale StrategicRoam request"), UDMCombatMoveDirector::IsOwnerAllowedForState(
		EDMBehaviorState::Combat, EDMMoveRequestOwner::StrategicRoam));
	TestFalse(TEXT("Dead permits no move request"), UDMCombatMoveDirector::IsOwnerAllowedForState(
		EDMBehaviorState::Dead, EDMMoveRequestOwner::Combat));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMDeadTransitionCleanupTest,
	"SkySeoul.DeathMatch.BTFoundation.StateTransition.DeadClearsExecutionState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMDeadTransitionCleanupTest::RunTest(const FString&)
{
	ADeathMatchRobotAIController* AI = NewObject<ADeathMatchRobotAIController>();
	UDMActionIntentComponent* Intent = AI->GetActionIntentComponent();
	UDMCombatFireControlComponent* Fire = AI->GetCombatFireControl();
	UDMCombatFocusComponent* Focus = AI->GetCombatFocusComponent();
	Intent->BeginIntentRequest(EDMActionIntent::Attack);
	Fire->BeginFireRequest();
	Focus->BeginLocationFocus(FVector(100.f, 0.f, 0.f));

	AI->NotifyPossessedRobotDied();
	TestEqual(TEXT("Dead clears every Action Intent request"), Intent->GetIntentRequestCount(), 0);
	TestEqual(TEXT("Dead clears every Fire request"), Fire->GetFireRequestCount(), 0);
	TestEqual(TEXT("Dead clears every Focus request"), Focus->GetFocusRequestCount(), 0);
	TestTrue(TEXT("Dead leaves no active move owner"),
		AI->GetCombatMoveDirector()->GetActiveRequestOwner() == EDMMoveRequestOwner::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMActionIntentADSPolicyTest,
	"SkySeoul.DeathMatch.BTFoundation.ActionIntent.ADSPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMActionIntentADSPolicyTest::RunTest(const FString&)
{
	FDMActionIntentExecutionSettings NoMovingADS;
	NoMovingADS.bUseADSWhileMoving = false;
	TestTrue(TEXT("Attack requires ADS while stationary even when moving ADS is disabled"),
		UDMActionIntentComponent::ShouldUseADS(EDMActionIntent::Attack, false, &NoMovingADS));
	TestFalse(TEXT("Moving ADS override only disables ADS during movement"),
		UDMActionIntentComponent::ShouldUseADS(EDMActionIntent::Attack, true, &NoMovingADS));
	TestTrue(TEXT("Attack defaults to ADS while moving"),
		UDMActionIntentComponent::ShouldUseADS(EDMActionIntent::Attack, true));
	TestTrue(TEXT("AttackMove keeps its stationary ADS requirement"),
		UDMActionIntentComponent::ShouldUseADS(EDMActionIntent::AttackMove, false, &NoMovingADS));
	TestTrue(TEXT("CombatCover keeps its stationary ADS requirement"),
		UDMActionIntentComponent::ShouldUseADS(EDMActionIntent::CombatCover, false, &NoMovingADS));
	TestTrue(TEXT("Retreat keeps its stationary ADS requirement"),
		UDMActionIntentComponent::ShouldUseADS(EDMActionIntent::Retreat, false, &NoMovingADS));
	TestFalse(TEXT("Retreat defaults to no ADS while moving"),
		UDMActionIntentComponent::ShouldUseADS(EDMActionIntent::Retreat, true));
	TestFalse(TEXT("FastReposition never requests ADS"),
		UDMActionIntentComponent::ShouldUseADS(EDMActionIntent::FastReposition, false));
	TestTrue(TEXT("Attack permits moving fire by default"),
		UDMCombatFireControlComponent::IntentAllowsMovingFire(EDMActionIntent::Attack));
	TestFalse(TEXT("Retreat does not permit moving fire by default"),
		UDMCombatFireControlComponent::IntentAllowsMovingFire(EDMActionIntent::Retreat));
	TestFalse(TEXT("FastReposition does not permit moving fire"),
		UDMCombatFireControlComponent::IntentAllowsMovingFire(EDMActionIntent::FastReposition));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMActionIntentExecutionSettingsTest,
	"SkySeoul.DeathMatch.BTFoundation.ActionIntent.ExecutionSettingsLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMActionIntentExecutionSettingsTest::RunTest(const FString&)
{
	ADeathMatchRobotAIController* AI = NewObject<ADeathMatchRobotAIController>();
	UDMActionIntentComponent* Intent = AI->GetActionIntentComponent();
	UDMCombatFireControlComponent* Fire = AI->GetCombatFireControl();
	FDMActionIntentExecutionSettings Previous;
	Previous.bUseADSWhileMoving = true;
	Previous.bAllowFireWhileMoving = true;
	const uint64 PreviousRequest = Intent->BeginIntentRequest(EDMActionIntent::Attack, Previous);
	TestTrue(TEXT("Profile ADS policy reaches active ActionIntent settings"),
		Intent->GetActiveExecutionSettings().bUseADSWhileMoving);
	TestTrue(TEXT("Profile moving-fire policy reaches Fire Control"), Fire->IsMovingFireAllowedByAction());

	FDMActionIntentExecutionSettings Branch;
	Branch.bUseADSWhileMoving = false;
	Branch.bAllowFireWhileMoving = false;
	const uint64 BranchRequest = Intent->BeginIntentRequest(EDMActionIntent::AttackMove, Branch);
	TestFalse(TEXT("Newest branch overrides ADS"), Intent->GetActiveExecutionSettings().bUseADSWhileMoving);
	TestFalse(TEXT("Newest branch overrides moving fire"), Fire->IsMovingFireAllowedByAction());
	Intent->EndIntentRequest(BranchRequest);
	TestTrue(TEXT("Branch abort restores previous ADS policy"),
		Intent->GetActiveExecutionSettings().bUseADSWhileMoving);
	TestTrue(TEXT("Branch abort restores previous moving-fire policy"), Fire->IsMovingFireAllowedByAction());
	Intent->EndIntentRequest(PreviousRequest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMActionIntentExecutionPriorityTest,
	"SkySeoul.DeathMatch.BTFoundation.ActionIntent.ExecutionSettingsPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMActionIntentExecutionPriorityTest::RunTest(const FString&)
{
	FDMActionIntentExecutionSettings Fixed;
	Fixed.bUseADSWhileMoving = false;
	Fixed.bAllowFireWhileMoving = true;
	bool bHasOverride = false;
	FDMActionIntentExecutionSettings Resolved = UBTService_DM_ActionIntent::ChooseExecutionSettings(
		false, Fixed, true, true, false, bHasOverride);
	TestTrue(TEXT("Blackboard settings create an execution override"), bHasOverride);
	TestTrue(TEXT("Blackboard ADS value is selected"), Resolved.bUseADSWhileMoving);
	TestFalse(TEXT("Blackboard moving-fire value is selected"), Resolved.bAllowFireWhileMoving);
	Resolved = UBTService_DM_ActionIntent::ChooseExecutionSettings(
		true, Fixed, true, true, false, bHasOverride);
	TestFalse(TEXT("Explicit Service ADS override wins"), Resolved.bUseADSWhileMoving);
	TestTrue(TEXT("Explicit Service moving-fire override wins"), Resolved.bAllowFireWhileMoving);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMFireRequestOwnershipTest,
	"SkySeoul.DeathMatch.BTFoundation.Fire.MaintainFireOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMFireRequestOwnershipTest::RunTest(const FString&)
{
	UDMFireControlTestComponent* Fire = NewObject<UDMFireControlTestComponent>();
	uint64 ServiceRequest = 0;

	// The first attempt deliberately fails as it would before Attack Intent/ADS settles.
	UBTService_DM_MaintainFire::BeginOrRefreshOwnedRequest(Fire, ServiceRequest);
	const uint64 OriginalRequest = ServiceRequest;
	TestTrue(TEXT("Service entry creates an owned request"), OriginalRequest != 0);
	TestEqual(TEXT("Exactly one request exists per service instance"), Fire->GetFireRequestCount(), 1);
	TestEqual(TEXT("Initial fire attempt was made"), Fire->StartCallCount, 1);
	TestFalse(TEXT("A transient start failure does not discard the request"), Fire->IsFireActive());

	UBTService_DM_MaintainFire::BeginOrRefreshOwnedRequest(Fire, ServiceRequest);
	TestEqual(TEXT("Repeated relevance does not allocate another request"), ServiceRequest, OriginalRequest);
	TestEqual(TEXT("Repeated relevance still owns one request"), Fire->GetFireRequestCount(), 1);
	TestEqual(TEXT("Inactive fire retries with the same request"), Fire->StartCallCount, 2);

	Fire->bFireActive = true;
	UBTService_DM_MaintainFire::BeginOrRefreshOwnedRequest(Fire, ServiceRequest);
	TestEqual(TEXT("Active Fire Ability is not activated again"), Fire->StartCallCount, 2);
	TestFalse(TEXT("Refresh policy rejects duplicate activation while active"),
		UDMCombatFireControlComponent::ShouldAttemptFireForRequest(true, true));

	Fire->bFireActive = false;
	Fire->bStartSucceeds = true;
	Fire->FireCycle.NextShotTime = 10.f;
	UBTService_DM_MaintainFire::BeginOrRefreshOwnedRequest(Fire, ServiceRequest);
	UBTService_DM_MaintainFire::BeginOrRefreshOwnedRequest(Fire, ServiceRequest);
	TestEqual(TEXT("Cooldown ticks do not request Ability activation"), Fire->StartCallCount, 2);
	TestFalse(TEXT("Timing gate rejects an otherwise owned inactive request"),
		UDMCombatFireControlComponent::ShouldAttemptFireForRequest(true, false, false));
	Fire->FireCycle.NextShotTime = 0.f;
	UBTService_DM_MaintainFire::BeginOrRefreshOwnedRequest(Fire, ServiceRequest);
	TestEqual(TEXT("Recovered conditions retry the same request"), ServiceRequest, OriginalRequest);
	TestEqual(TEXT("Recovered conditions perform one new activation"), Fire->StartCallCount, 3);
	TestTrue(TEXT("Recovered retry activates fire"), Fire->IsFireActive());

	const uint64 OtherRequest = Fire->BeginFireRequest();
	TestEqual(TEXT("A second owner can coexist"), Fire->GetFireRequestCount(), 2);
	UBTService_DM_MaintainFire::EndOwnedRequest(Fire, ServiceRequest);
	TestEqual(TEXT("Service cease clears its memory id"), ServiceRequest, static_cast<uint64>(0));
	TestEqual(TEXT("Service cease removes only its own request"), Fire->GetFireRequestCount(), 1);
	TestEqual(TEXT("Ending a non-final request does not stop fire"), Fire->StopCallCount, 0);
	TestTrue(TEXT("Other valid request keeps Fire Ability active"), Fire->IsFireActive());

	Fire->EndFireRequest(OtherRequest);
	TestEqual(TEXT("Last request removal clears the ledger"), Fire->GetFireRequestCount(), 0);
	TestEqual(TEXT("Only the last request stops fire"), Fire->StopCallCount, 1);
	TestFalse(TEXT("No fire remains active without an owner"), Fire->IsFireActive());
	return true;
}

namespace
{
const UBTCompositeNode* FindAttackBranch(const UBTCompositeNode* Node)
{
	if (!Node) return nullptr;
	for (const UBTService* Service : Node->Services)
	{
		const UBTService_DM_ActionIntent* Intent = Cast<UBTService_DM_ActionIntent>(Service);
		if (Intent && Intent->ActionIntent == EDMActionIntent::Attack)
		{
			return Node;
		}
	}
	for (const FBTCompositeChild& Child : Node->Children)
	{
		if (const UBTCompositeNode* Found = FindAttackBranch(Child.ChildComposite)) return Found;
	}
	return nullptr;
}

int32 CountTaskClass(const UBTCompositeNode* Node, const FString& ClassName)
{
	if (!Node) return 0;
	int32 Count = 0;
	for (const FBTCompositeChild& Child : Node->Children)
	{
		Count += Child.ChildTask && Child.ChildTask->GetClass()->GetName() == ClassName ? 1 : 0;
		Count += CountTaskClass(Child.ChildComposite, ClassName);
	}
	return Count;
}

void GatherDecorators(const UBTCompositeNode* Node, TArray<const UBTDecorator*>& Out)
{
	if (!Node) return;
	for (const FBTCompositeChild& Child : Node->Children)
	{
		for (const UBTDecorator* Decorator : Child.Decorators)
		{
			if (Decorator) Out.Add(Decorator);
		}
		GatherDecorators(Child.ChildComposite, Out);
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMRootStateAbortContractTest,
	"SkySeoul.DeathMatch.BTFoundation.StateTransition.RootAbortContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMRootStateAbortContractTest::RunTest(const FString&)
{
	const UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr,
		TEXT("/Game/DeathMatch/AI/BT/BT_main.BT_main"));
	if (!TestNotNull(TEXT("BT_main loads"), Tree)
		|| !TestNotNull(TEXT("BT_main has a runtime root"), Tree ? Tree->RootNode.Get() : nullptr))
	{
		return false;
	}
	TArray<const UBTDecorator*> Decorators;
	GatherDecorators(Tree->RootNode, Decorators);
	int32 StateDecoratorCount = 0;
	int32 DeadDecoratorCount = 0;
	for (const UBTDecorator* Decorator : Decorators)
	{
		if (!Decorator->IsA<UBTDecorator_DM_ReactiveStateCondition>()) continue;
		++StateDecoratorCount;
		DeadDecoratorCount += Decorator->IsA<UBTDecorator_DM_IsDead>() ? 1 : 0;
		TestTrue(*FString::Printf(TEXT("%s actively aborts affected branches"), *Decorator->GetName()),
			Decorator->GetFlowAbortMode() == EBTFlowAbortMode::Both);
	}
	TestTrue(TEXT("Root has reactive state decorators"), StateDecoratorCount >= 5);
	TestEqual(TEXT("Root has one Dead state gate"), DeadDecoratorCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMFireOwnershipBTAssetTest,
	"SkySeoul.DeathMatch.BTFoundation.Fire.BTCombatSingleExecutor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMFireOwnershipBTAssetTest::RunTest(const FString&)
{
	const UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr,
		TEXT("/Game/DeathMatch/AI/BT/BT_Combat.BT_Combat"));
	if (!TestNotNull(TEXT("BT_Combat loads"), Tree) ||
		!TestNotNull(TEXT("BT_Combat has a runtime root"), Tree ? Tree->RootNode.Get() : nullptr))
	{
		return false;
	}
	TestEqual(TEXT("BT_Combat has no DM Start Fire task"),
		CountTaskClass(Tree->RootNode, TEXT("BTTask_DM_StartFire")), 0);
	TestEqual(TEXT("BT_Combat has no legacy DM Activate Ability task"),
		CountTaskClass(Tree->RootNode, TEXT("BTTask_DM_ActivateAbility")), 0);
	TestEqual(TEXT("BT_Combat has no legacy DM Stop Fire task"),
		CountTaskClass(Tree->RootNode, TEXT("BTTask_DM_StopFire")), 0);

	const UBTCompositeNode* Attack = FindAttackBranch(Tree->RootNode);
	if (!TestNotNull(TEXT("Attack branch is identified by its Action Intent service"), Attack))
	{
		return false;
	}
	int32 IntentServices = 0;
	int32 FocusServices = 0;
	int32 FireServices = 0;
	for (const UBTService* Service : Attack->Services)
	{
		IntentServices += Cast<UBTService_DM_ActionIntent>(Service) != nullptr ? 1 : 0;
		FocusServices += Cast<UBTService_DM_MaintainFocus>(Service) != nullptr ? 1 : 0;
		FireServices += Cast<UBTService_DM_MaintainFire>(Service) != nullptr ? 1 : 0;
	}
	TestEqual(TEXT("Attack branch owns one Action Intent service"), IntentServices, 1);
	TestEqual(TEXT("Attack branch owns one Maintain Focus service"), FocusServices, 1);
	TestEqual(TEXT("Attack branch owns one Maintain Fire service"), FireServices, 1);
	TestTrue(TEXT("Maintain Fire is a service and cannot fail the Attack sequence"),
		UBTService_DM_MaintainFire::StaticClass()->IsChildOf(UBTService::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMFocusRequestLifecycleTest,
	"SkySeoul.DeathMatch.BTFoundation.Focus.BranchAbortClearsRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMFocusRequestLifecycleTest::RunTest(const FString&)
{
	UDMCombatFocusComponent* Focus = NewObject<UDMCombatFocusComponent>();
	const uint64 Request = Focus->BeginLocationFocus(FVector(10.f, 20.f, 30.f));
	TestTrue(TEXT("Maintain Focus owns a request"), Focus->HasFocusRequests());
	const uint64 NewRequest = Focus->BeginLocationFocus(FVector(40.f, 50.f, 60.f));
	Focus->EndFocusRequest(Request);
	Focus->EndFocusRequest(Request);
	TestEqual(TEXT("Repeated old Focus cleanup preserves the new branch request"), Focus->GetFocusRequestCount(), 1);
	Focus->EndFocusRequest(NewRequest);
	TestFalse(TEXT("Branch end removes the owned focus request"), Focus->HasFocusRequests());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTargetSelectionScoringTest,
	"SkySeoul.DeathMatch.BTFoundation.TargetSelection.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMTargetSelectionScoringTest::RunTest(const FString&)
{
	UDMCombatTargetSelectionPolicy* Policy = NewObject<UDMCombatTargetSelectionPolicy>();
	AActor* Current = NewObject<AActor>();
	AActor* Other = NewObject<AActor>();
	FDMRememberedThreat Visible;
	Visible.Actor = Other;
	Visible.bVisible = true;
	FDMRememberedThreat Remembered;
	Remembered.Actor = Current;
	TestTrue(TEXT("Visible evidence outranks remembered retention by default"),
		Policy->ScoreCandidate(Visible, Current, nullptr) > Policy->ScoreCandidate(Remembered, Current, nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMPositionReservationRuleTest,
	"SkySeoul.DeathMatch.BTFoundation.PositionReservation.ControllerOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMPositionReservationRuleTest::RunTest(const FString&)
{
	TestTrue(TEXT("Nearby 2D reservations overlap"), UDMCombatPositionRegistry::Overlaps(FVector::ZeroVector, FVector(100, 0, 0), 150.f));
	TestFalse(TEXT("Separated reservations do not overlap"), UDMCombatPositionRegistry::Overlaps(FVector::ZeroVector, FVector(300, 0, 0), 150.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMBTControllerCompositionTest,
	"SkySeoul.DeathMatch.BTFoundation.Controller.Components",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMBTControllerCompositionTest::RunTest(const FString&)
{
	ADeathMatchRobotAIController* AI = NewObject<ADeathMatchRobotAIController>();
	TestNotNull(TEXT("Knowledge component exists"), AI->GetCombatKnowledge());
	TestNotNull(TEXT("Fire control component exists"), AI->GetCombatFireControl());
	TestNotNull(TEXT("Speed policy component exists"), AI->GetMovementSpeedPolicy());
	TestNotNull(TEXT("Action intent component exists"), AI->GetActionIntentComponent());
	TestNotNull(TEXT("Focus component exists"), AI->GetCombatFocusComponent());
	TestNotNull(TEXT("Combat status component exists"), AI->GetCombatStatus());
	TestNotNull(TEXT("Target selection policy exists"), AI->GetTargetSelectionPolicy());
	TestNotNull(TEXT("Move director exists"), AI->GetCombatMoveDirector());
	for (UActorComponent* Component : AI->GetComponents())
	{
		const FString Name = Component ? Component->GetClass()->GetName() : FString();
		TestFalse(TEXT("No Situation Framework component"), Name.Contains(TEXT("SituationFramework")));
		TestFalse(TEXT("No Combat Tactics component"), Name.Contains(TEXT("CombatTactics")));
		TestFalse(TEXT("No Move Goal Provider component"), Name.Contains(TEXT("MoveGoalProvider")));
	}
	TestEqual(TEXT("EQS parameter is centralized"), DMEQSParameters::WeaponEffectiveRange, FName(TEXT("WeaponEffectiveRange")));
	return true;
}

#endif
