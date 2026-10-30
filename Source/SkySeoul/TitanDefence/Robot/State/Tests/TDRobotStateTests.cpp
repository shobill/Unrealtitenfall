#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "TitanDefence/Robot/Controller/TDBaseRobotAIController.h"
#include "TitanDefence/Robot/Damage/TDBodyDamageReceiver.h"
#include "TitanDefence/Robot/Damage/TDRobotPartDamageComponent.h"
#include "TitanDefence/Robot/Damage/TDSimpleDamageComponent.h"
#include "TitanDefence/Robot/Destruction/TDRobotPartDataAsset.h"
#include "TitanDefence/Robot/Destruction/TDRobotPartSeparationComponent.h"
#include "TitanDefence/Robot/GAS/TDRobotAbilitySystemComponent.h"
#include "TitanDefence/Robot/GAS/TDRobotAttributeSet.h"
#include "TitanDefence/Robot/GAS/TDRobotBodyDamageExecutionCalculation.h"
#include "TitanDefence/Robot/GAS/TDRobotGameplayTags.h"
#include "TitanDefence/Robot/State/TDRobotStateComponent.h"
#include "TitanDefence/Robot/State/Tests/TDRobotStateTestTypes.h"

namespace
{
	UWorld* CreateRobotStateTestWorld(const FName Name)
	{
		UWorld::InitializationValues Initialization;
		Initialization
			.AllowAudioPlayback(false)
			.CreatePhysicsScene(true)
			.CreateNavigation(false)
			.CreateAISystem(false)
			.ShouldSimulatePhysics(false);
		return UWorld::CreateWorld(
			EWorldType::Game, false, Name, nullptr, true,
			ERHIFeatureLevel::Num, &Initialization);
	}

	template <typename T>
	int32 CountComponents(const AActor* Actor)
	{
		TArray<T*> Components;
		Actor->GetComponents<T>(Components);
		return Components.Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTDRobotStateContractTest,
	"SkySeoul.TitanDefence.Robot.State.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDRobotStateContractTest::RunTest(const FString& Parameters)
{
	const ATDBaseRobotAIController* ControllerCDO = GetDefault<ATDBaseRobotAIController>();
	TestFalse(TEXT("Titan AI does not request a PlayerState"), ControllerCDO->bWantsPlayerState != 0);
	TestTrue(TEXT("Robot State implements the body receiver contract"),
		UTDRobotStateComponent::StaticClass()->ImplementsInterface(UTDBodyDamageReceiver::StaticClass()));
	TestFalse(TEXT("Robot State is not Blueprint-spawnable"),
		UTDRobotStateComponent::StaticClass()->HasMetaData(TEXT("BlueprintSpawnableComponent")));
	TestEqual(TEXT("Unbound external GE damage defaults to body damage"),
		GetDefault<UTDRobotStateComponent>()->ExternalDamageFallbackPolicy,
		ETDRobotExternalDamageFallbackPolicy::ApplyToBody);
	TestFalse(TEXT("Production body-damage execution is concrete"),
		UTDRobotBodyDamageExecutionCalculation::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestFalse(TEXT("Production body-damage execution is visible in GE class pickers"),
		UTDRobotBodyDamageExecutionCalculation::StaticClass()->HasMetaData(TEXT("HideDropdown")));

	const UTDRobotAbilitySystemComponent* ASCCDO = GetDefault<UTDRobotAbilitySystemComponent>();
	TestEqual(TEXT("Robot ASC uses Minimal replication"), ASCCDO->ReplicationMode,
		EGameplayEffectReplicationMode::Minimal);
	TestFalse(TEXT("Robot ASC does not tick"), ASCCDO->PrimaryComponentTick.bCanEverTick);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTDRobotStateCharacterInitializationTest,
	"SkySeoul.TitanDefence.Robot.State.CharacterInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDRobotStateCharacterInitializationTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateRobotStateTestWorld(TEXT("TDRobotStateCharacterInitializationTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}

	AAIController* Controller = World->SpawnActor<AAIController>();
	ATDRobotStateTestCharacter* Robot = World->SpawnActor<ATDRobotStateTestCharacter>();
	Controller->Possess(Robot);

	UTDRobotStateComponent* State = Robot->GetRobotStateComponent();
	UTDRobotAbilitySystemComponent* ASC = Robot->GetRobotAbilitySystemComponent();
	TestEqual(TEXT("Character owns exactly one robot State component"),
		CountComponents<UTDRobotStateComponent>(Robot), 1);
	TestEqual(TEXT("Character owns exactly one robot ASC"),
		CountComponents<UTDRobotAbilitySystemComponent>(Robot), 1);
	TestEqual(TEXT("Character owns exactly one robot AttributeSet subobject"),
		ASC->GetSpawnedAttributes().FilterByPredicate(
			[](const UAttributeSet* Set) { return Set->IsA<UTDRobotAttributeSet>(); }).Num(), 1);
	TestTrue(TEXT("State initializes from explicit Character injection"), State->IsStateInitialized());
	TestTrue(TEXT("Character ability-system interface returns its robot ASC"),
		Robot->GetAbilitySystemComponent() == ASC);
	TestTrue(TEXT("ASC Owner Actor is the Character"), ASC->GetOwnerActor() == Robot);
	TestTrue(TEXT("ASC Avatar Actor is the Character"), ASC->GetAvatarActor() == Robot);
	TestEqual(TEXT("Initial GE sets Health"), State->GetHealth(), 100.0f);
	TestEqual(TEXT("Initial GE sets MaxHealth"), State->GetMaxHealth(), 100.0f);
	TestNull(TEXT("Character does not hardcode a part-damage component"),
		Robot->FindComponentByClass<UTDRobotPartDamageComponent>());
	TestNull(TEXT("Character does not hardcode a separation component"),
		Robot->FindComponentByClass<UTDRobotPartSeparationComponent>());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTDRobotStateGameplayEffectDamageTest,
	"SkySeoul.TitanDefence.Robot.State.GameplayEffectDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDRobotStateGameplayEffectDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateRobotStateTestWorld(TEXT("TDRobotStateGameplayEffectDamageTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}

	AAIController* Controller = World->SpawnActor<AAIController>();
	ATDRobotStateTestCharacter* Robot = World->SpawnActor<ATDRobotStateTestCharacter>();
	Controller->Possess(Robot);
	UTDRobotStateComponent* State = Robot->GetRobotStateComponent();
	UTDRobotStateTestObserver* Observer = NewObject<UTDRobotStateTestObserver>(Robot);
	State->OnBodyDeath.AddDynamic(Observer, &UTDRobotStateTestObserver::ObserveDeath);

	FTDDamageResult Result;
	TestTrue(TEXT("Positive resolved damage is applied through a GE"),
		State->ApplyResolvedBodyDamage(25.0f, Result));
	TestEqual(TEXT("One damage GE reduces Health exactly once"), State->GetHealth(), 75.0f);
	TestTrue(TEXT("Robot AttributeSet inherits the shared character AttributeSet contract"),
		State->GetRobotAttributeSet()->IsA<UCharacterAttributeSetBase>());
	TestFalse(TEXT("Non-positive damage is rejected"),
		State->ApplyResolvedBodyDamage(0.0f, Result));
	TestEqual(TEXT("Rejected damage does not mutate Health"), State->GetHealth(), 75.0f);

	TestTrue(TEXT("Lethal damage is accepted"), State->ApplyResolvedBodyDamage(100.0f, Result));
	TestEqual(TEXT("Lethal damage clamps Health to zero"), State->GetHealth(), 0.0f);
	TestTrue(TEXT("Death applies State.Dead"),
		Robot->GetRobotAbilitySystemComponent()->HasMatchingGameplayTag(
			TDRobotGameplayTags::State_Dead));
	TestEqual(TEXT("Death event is emitted once"), Observer->DeathCount, 1);
	TestFalse(TEXT("Damage after death is rejected"),
		State->ApplyResolvedBodyDamage(10.0f, Result));
	TestEqual(TEXT("Rejected post-death damage does not repeat death"), Observer->DeathCount, 1);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTDRobotExternalGameplayEffectDamageTest,
	"SkySeoul.TitanDefence.Robot.State.ExternalGameplayEffectDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDRobotExternalGameplayEffectDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateRobotStateTestWorld(TEXT("TDRobotExternalGameplayEffectDamageTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}

	AAIController* SourceController = World->SpawnActor<AAIController>();
	AAIController* TargetController = World->SpawnActor<AAIController>();
	ATDRobotStateTestCharacter* Source = World->SpawnActor<ATDRobotStateTestCharacter>();
	ATDRobotStateTestCharacter* Target = World->SpawnActor<ATDRobotStateTestCharacter>();
	SourceController->Possess(Source);
	TargetController->Possess(Target);

	UTDRobotAbilitySystemComponent* SourceASC = Source->GetRobotAbilitySystemComponent();
	UTDRobotAbilitySystemComponent* TargetASC = Target->GetRobotAbilitySystemComponent();
	UTDRobotStateComponent* TargetState = Target->GetRobotStateComponent();
	UTDRobotStateTestObserver* Observer = NewObject<UTDRobotStateTestObserver>(Target);
	TargetState->OnExternalGameplayEffectDamageReceived.AddDynamic(
		Observer, &UTDRobotStateTestObserver::ObserveExternalDamage);

	FHitResult HitResult;
	HitResult.BoneName = TEXT("external_damage_head");
	HitResult.ImpactPoint = FVector(10.0f, 20.0f, 30.0f);
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddHitResult(HitResult, true);
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
		UTDRobotExternalHealthDamageTestEffect::StaticClass(), 1.0f, Context);
	TestTrue(TEXT("External damage spec is created"), Spec.IsValid() && Spec.Data.IsValid());
	if (!Spec.IsValid() || !Spec.Data.IsValid())
	{
		World->DestroyWorld(false);
		return false;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	TestTrue(TEXT("External GE application completes"), AppliedHandle.WasSuccessfullyApplied());
	TestEqual(TEXT("External Health modifier is blocked immediately"), TargetState->GetHealth(), 100.0f);
	TestEqual(TEXT("External damage event is not re-entrant"), Observer->ExternalDamageCount, 0);

	World->GetTimerManager().Tick(0.01f);
	TestEqual(TEXT("External damage event fires exactly once on the next tick"),
		Observer->ExternalDamageCount, 1);
	TestEqual(TEXT("External request preserves normalized damage"),
		Observer->LastExternalDamage.Damage, 25.0f);
	TestEqual(TEXT("External request preserves the hit bone"),
		Observer->LastExternalDamage.HitBoneName, HitResult.BoneName);
	TestTrue(TEXT("External request preserves its HitResult"),
		Observer->LastExternalDamage.bHasValidHitResult);
	TestTrue(TEXT("External request preserves its source actor"),
		Observer->LastExternalDamage.SourceActor == Source);
	TestTrue(TEXT("External request preserves its GE class"),
		Observer->LastExternalDamage.GameplayEffectClass ==
		UTDRobotExternalHealthDamageTestEffect::StaticClass());
	TestEqual(TEXT("A bound event leaves body resolution to its receiver"),
		TargetState->GetHealth(), 100.0f);

	FTDDamageResult DamageResult;
	TestTrue(TEXT("Internal BodyDamage GE is not intercepted"),
		TargetState->ApplyResolvedBodyDamage(25.0f, DamageResult));
	TestEqual(TEXT("Data.Damage 25 reduces shared Health by exactly 25"),
		TargetState->GetHealth(), 75.0f);
	TestEqual(TEXT("Internal BodyDamage GE does not emit another external event"),
		Observer->ExternalDamageCount, 1);
	World->GetTimerManager().Tick(0.01f);
	TestEqual(TEXT("The queued external event is not duplicated"),
		Observer->ExternalDamageCount, 1);

	AAIController* FallbackController = World->SpawnActor<AAIController>();
	ATDRobotStateTestCharacter* FallbackTarget = World->SpawnActor<ATDRobotStateTestCharacter>();
	FallbackController->Possess(FallbackTarget);
	UTDRobotAbilitySystemComponent* FallbackASC = FallbackTarget->GetRobotAbilitySystemComponent();
	FGameplayEffectContextHandle FallbackContext = SourceASC->MakeEffectContext();
	FGameplayEffectSpecHandle FallbackSpec = SourceASC->MakeOutgoingSpec(
		UTDRobotExternalHealthDamageTestEffect::StaticClass(), 1.0f, FallbackContext);
	if (!TestTrue(TEXT("Fallback external damage spec is created"),
		FallbackSpec.IsValid() && FallbackSpec.Data.IsValid()))
	{
		World->DestroyWorld(false);
		return false;
	}
	SourceASC->ApplyGameplayEffectSpecToTarget(*FallbackSpec.Data.Get(), FallbackASC);
	TestEqual(TEXT("Fallback also blocks immediate Health loss"),
		FallbackTarget->GetRobotStateComponent()->GetHealth(), 100.0f);
	World->GetTimerManager().Tick(0.01f);
	TestEqual(TEXT("Default no-listener policy applies body damage exactly once"),
		FallbackTarget->GetRobotStateComponent()->GetHealth(), 75.0f);
	World->GetTimerManager().Tick(0.01f);
	TestEqual(TEXT("One external GE cannot reduce Health twice"),
		FallbackTarget->GetRobotStateComponent()->GetHealth(), 75.0f);

	AAIController* PartTargetController = World->SpawnActor<AAIController>();
	ATDRobotStateTestCharacter* PartTarget = World->SpawnActor<ATDRobotStateTestCharacter>();
	PartTargetController->Possess(PartTarget);
	const FGameplayTag HeadPartTag = FGameplayTag::RequestGameplayTag(TEXT("Parts.Head"));
	UTDRobotPartDataAsset* PartData = NewObject<UTDRobotPartDataAsset>(PartTarget);
	FTDRobotPartDefinition& HeadDefinition = PartData->Parts.AddDefaulted_GetRef();
	HeadDefinition.PartTag = HeadPartTag;
	HeadDefinition.BoneName = HitResult.BoneName;
	HeadDefinition.MaxHealth = 100.0f;
	UTDRobotPartDamageComponent* PartDamage =
		NewObject<UTDRobotPartDamageComponent>(PartTarget);
	PartDamage->PartDataAsset = PartData;
	FTDRobotPartDamageRule& HeadRule = PartDamage->PerPartDamageRules.AddDefaulted_GetRef();
	HeadRule.PartTag = HeadPartTag;
	HeadRule.PartDamageMultiplier = 1.0f;
	HeadRule.BodyDamageMultiplier = 0.5f;
	TestTrue(TEXT("External GE pipeline initializes part state"),
		PartDamage->InitializePartStates());
	TestTrue(TEXT("External GE pipeline installs the body receiver"),
		PartDamage->SetBodyDamageReceiver(PartTarget->GetRobotStateComponent()));
	UTDRobotStateTestObserver* PartForwarder = NewObject<UTDRobotStateTestObserver>(PartTarget);
	PartForwarder->PartDamageComponent = PartDamage;
	PartTarget->GetRobotStateComponent()->OnExternalGameplayEffectDamageReceived.AddDynamic(
		PartForwarder, &UTDRobotStateTestObserver::ForwardExternalDamage);
	FGameplayEffectContextHandle PartContext = SourceASC->MakeEffectContext();
	PartContext.AddHitResult(HitResult, true);
	FGameplayEffectSpecHandle PartSpec = SourceASC->MakeOutgoingSpec(
		UTDRobotExternalHealthDamageTestEffect::StaticClass(), 1.0f, PartContext);
	if (!TestTrue(TEXT("Part-pipeline external damage spec is created"),
		PartSpec.IsValid() && PartSpec.Data.IsValid()))
	{
		World->DestroyWorld(false);
		return false;
	}
	SourceASC->ApplyGameplayEffectSpecToTarget(
		*PartSpec.Data.Get(), PartTarget->GetRobotAbilitySystemComponent());
	TestEqual(TEXT("External GE cannot bypass part resolution"),
		PartTarget->GetRobotStateComponent()->GetHealth(), 100.0f);
	World->GetTimerManager().Tick(0.01f);
	TestEqual(TEXT("Part pipeline receives one deferred external GE event"),
		PartForwarder->ExternalDamageCount, 1);
	TestEqual(TEXT("Part body multiplier alone determines final Health loss"),
		PartTarget->GetRobotStateComponent()->GetHealth(), 87.5f);
	TestEqual(TEXT("Part damage uses the preserved hit bone"),
		PartDamage->GetPartCurrentHealth(HeadPartTag), 75.0f);
	World->GetTimerManager().Tick(0.01f);
	TestEqual(TEXT("Part pipeline does not apply the external GE twice"),
		PartTarget->GetRobotStateComponent()->GetHealth(), 87.5f);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTDRobotNavigationWaitQueueTest,
	"SkySeoul.TitanDefence.Robot.Movement.NavigationWaitQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDRobotNavigationWaitQueueTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateRobotStateTestWorld(TEXT("TDRobotNavigationWaitQueueTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}

	ATDBaseRobotAIController* Controller = World->SpawnActor<ATDBaseRobotAIController>();
	ATDRobotStateTestCharacter* Robot = World->SpawnActor<ATDRobotStateTestCharacter>();
	AActor* Target = World->SpawnActor<AActor>();
	Controller->Possess(Robot);

	TestTrue(TEXT("A move without NavData is accepted into the wait queue"),
		Robot->StartMovingToActor(Target));
	TestEqual(TEXT("Robot reports navigation wait instead of NoPath"),
		Robot->GetMovementState(), ETDRobotMovementState::WaitingForNavigation);
	TestEqual(TEXT("Wait reason identifies unavailable NavData"),
		Robot->GetLastMovementEndReason(),
		ETDRobotMovementEndReason::NavigationDataNotReady);

	Robot->CancelCurrentMovement();
	TestEqual(TEXT("Cancelling clears a queued navigation request"),
		Robot->GetMovementState(), ETDRobotMovementState::Idle);
	TestNull(TEXT("Cancelling clears the queued target"),
		Robot->GetCurrentMoveTargetActor());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTDRobotBeginPlayMovementOrderingTest,
	"SkySeoul.TitanDefence.Robot.Movement.BeginPlayOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDRobotBeginPlayMovementOrderingTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateRobotStateTestWorld(TEXT("TDRobotBeginPlayMovementOrderingTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}

	ATDBaseRobotAIController* Controller = World->SpawnActor<ATDBaseRobotAIController>();
	ATDRobotStateTestCharacter* Robot = World->SpawnActor<ATDRobotStateTestCharacter>();
	AActor* Target = World->SpawnActor<AActor>();
	Robot->ConfigureAutomaticMoveForTest(Target);
	Controller->Possess(Robot);

	TestFalse(TEXT("PossessedBy does not masquerade as BeginPlay"), Robot->HasActorBegunPlay());
	TestEqual(TEXT("PossessedBy does not start automatic movement"),
		Robot->GetMovementState(), ETDRobotMovementState::Idle);
	TestNull(TEXT("PossessedBy does not install the initial move target"),
		Robot->GetCurrentMoveTargetActor());

	Robot->DispatchBeginPlay();
	TestTrue(TEXT("The robot has now completed real BeginPlay"), Robot->HasActorBegunPlay());
	TestEqual(TEXT("BeginPlay schedules rather than synchronously starts movement"),
		Robot->GetMovementState(), ETDRobotMovementState::Idle);
	World->GetTimerManager().Tick(0.01f);
	TestEqual(TEXT("The next tick requests movement and waits when NavData is unavailable"),
		Robot->GetMovementState(), ETDRobotMovementState::WaitingForNavigation);

	Robot->CancelCurrentMovement();
	Controller->UnPossess();
	ATDBaseRobotAIController* RepossessingController =
		World->SpawnActor<ATDBaseRobotAIController>();
	RepossessingController->Possess(Robot);
	TestEqual(TEXT("Re-possess after BeginPlay still defers until the next tick"),
		Robot->GetMovementState(), ETDRobotMovementState::Idle);
	World->GetTimerManager().Tick(0.01f);
	TestEqual(TEXT("Re-possess after BeginPlay schedules automatic movement"),
		Robot->GetMovementState(), ETDRobotMovementState::WaitingForNavigation);

	Robot->CancelCurrentMovement();
	RepossessingController->UnPossess();
	World->GetTimerManager().Tick(0.01f);
	TestEqual(TEXT("UnPossess clears deferred and navigation timers"),
		Robot->GetMovementState(), ETDRobotMovementState::Idle);

	ATDBaseRobotAIController* EndingController = World->SpawnActor<ATDBaseRobotAIController>();
	EndingController->Possess(Robot);
	EndingController->Destroy();
	World->GetTimerManager().Tick(0.01f);
	TestEqual(TEXT("Controller EndPlay clears its deferred movement timer"),
		Robot->GetMovementState(), ETDRobotMovementState::Idle);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTDRobotStatePartDamageRollbackTest,
	"SkySeoul.TitanDefence.Robot.State.MissingDamageEffectRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDRobotStatePartDamageRollbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreateRobotStateTestWorld(TEXT("TDRobotStatePartDamageRollbackTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}

	AAIController* Controller = World->SpawnActor<AAIController>();
	ATDRobotMissingDamageEffectTestCharacter* Robot =
		World->SpawnActor<ATDRobotMissingDamageEffectTestCharacter>();
	Controller->Possess(Robot);

	const FGameplayTag PartTag = FGameplayTag::RequestGameplayTag(TEXT("Parts.Head"));
	UTDRobotPartDataAsset* DataAsset = NewObject<UTDRobotPartDataAsset>(Robot);
	FTDRobotPartDefinition& Definition = DataAsset->Parts.AddDefaulted_GetRef();
	Definition.PartTag = PartTag;
	Definition.BoneName = TEXT("damage_test_head");
	Definition.MaxHealth = 30.0f;

	UTDRobotPartDamageComponent* PartDamage = NewObject<UTDRobotPartDamageComponent>(Robot);
	PartDamage->PartDataAsset = DataAsset;
	TestTrue(TEXT("Part State initializes"), PartDamage->InitializePartStates());
	TestTrue(TEXT("Part resolver accepts the explicit body receiver"),
		PartDamage->SetBodyDamageReceiver(Robot->GetRobotStateComponent()));

	FHitResult Hit;
	Hit.BoneName = Definition.BoneName;
	TestFalse(TEXT("Missing body-damage GE rejects the attack"),
		PartDamage->ProcessPointDamage(10.0f, Hit, nullptr, nullptr, nullptr));
	TestEqual(TEXT("Rejected body damage leaves body Health unchanged"),
		Robot->GetRobotStateComponent()->GetHealth(), 100.0f);
	TestEqual(TEXT("Rejected body damage rolls part Health back"),
		PartDamage->GetPartCurrentHealth(PartTag), 30.0f);

	World->DestroyWorld(false);
	return true;
}

#endif

