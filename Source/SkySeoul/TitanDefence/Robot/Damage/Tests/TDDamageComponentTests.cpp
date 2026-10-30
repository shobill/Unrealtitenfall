#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "TitanDefence/Robot/Damage/TDDamageComponentRegistry.h"
#include "TitanDefence/Robot/Damage/TDRobotPartDamageComponent.h"
#include "TitanDefence/Robot/Damage/TDSimpleDamageComponent.h"
#include "TitanDefence/Robot/Damage/Tests/TDDamageTestStateSource.h"
#include "TitanDefence/Robot/Destruction/TDRobotPartDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDSimpleDamageComponentTest,
	"SkySeoul.TitanDefence.Damage.SimpleResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDSimpleDamageComponentTest::RunTest(const FString& Parameters)
{
	UTDSimpleDamageComponent* Component = NewObject<UTDSimpleDamageComponent>();
	UTDDamageTestStateSource* State = NewObject<UTDDamageTestStateSource>();
	TestTrue(TEXT("A compatible body receiver is accepted"), Component->SetBodyDamageReceiver(State));

	Component->IncomingDamageMultiplier = 1.5f;
	TestTrue(TEXT("Valid generic damage resolves"),
		Component->ProcessGenericDamage(10.0f, nullptr, nullptr, nullptr));
	TestEqual(TEXT("State is called exactly once"), State->ApplyCallCount, 1);
	TestEqual(TEXT("Resolved multiplier is delivered to State"), State->TotalAppliedBodyDamage, 15.0f);

	State->bAcceptDamage = false;
	TestFalse(TEXT("State rejection is propagated"),
		Component->ProcessGenericDamage(10.0f, nullptr, nullptr, nullptr));
	TestEqual(TEXT("Rejected request still calls State only once"), State->ApplyCallCount, 2);
	TestEqual(TEXT("Rejecting State does not record body damage"), State->TotalAppliedBodyDamage, 15.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDDamageComponentRegistryTest,
	"SkySeoul.TitanDefence.Damage.SingleOwnerReceiver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDDamageComponentRegistryTest::RunTest(const FString& Parameters)
{
	AActor* Owner = GetMutableDefault<AActor>();
	UTDSimpleDamageComponent* First = NewObject<UTDSimpleDamageComponent>();
	UTDRobotPartDamageComponent* Second = NewObject<UTDRobotPartDamageComponent>();
	TestTrue(TEXT("The first damage component owns actor event routing"),
		TDDamageComponentRegistry::Register(Owner, First));
	AddExpectedError(TEXT("rejected an additional Titan Defence damage component"),
		EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A second damage component cannot duplicate event routing"),
		TDDamageComponentRegistry::Register(Owner, Second));
	TDDamageComponentRegistry::Unregister(Owner, First);
	TestTrue(TEXT("Routing ownership is released for a replacement component"),
		TDDamageComponentRegistry::Register(Owner, Second));
	TDDamageComponentRegistry::Unregister(Owner, Second);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDRobotPartDamageComponentTest,
	"SkySeoul.TitanDefence.Damage.RobotPartResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDRobotPartDamageComponentTest::RunTest(const FString& Parameters)
{
	const FGameplayTag PartTag = FGameplayTag::RequestGameplayTag(TEXT("Parts.Head"));
	UTDRobotPartDataAsset* DataAsset = NewObject<UTDRobotPartDataAsset>();
	FTDRobotPartDefinition& Definition = DataAsset->Parts.AddDefaulted_GetRef();
	Definition.PartTag = PartTag;
	Definition.BoneName = TEXT("damage_test_head");
	Definition.MaxHealth = 30.0f;

	UTDRobotPartDamageComponent* Component = NewObject<UTDRobotPartDamageComponent>();
	UTDDamageTestStateSource* State = NewObject<UTDDamageTestStateSource>();
	Component->PartDataAsset = DataAsset;
	Component->DefaultBodyDamageMultiplier = 0.5f;
	FTDRobotPartDamageRule& Rule = Component->PerPartDamageRules.AddDefaulted_GetRef();
	Rule.PartTag = PartTag;
	Rule.PartDamageMultiplier = 2.0f;
	Rule.BodyDamageMultiplier = 0.5f;
	Component->OnPartHealthDepleted.AddDynamic(State, &UTDDamageTestStateSource::ObservePartDepleted);
	Component->OnPartResolutionFailed.AddDynamic(State,
		&UTDDamageTestStateSource::ObservePartResolutionFailed);

	TestTrue(TEXT("A compatible body receiver is accepted"), Component->SetBodyDamageReceiver(State));
	TestTrue(TEXT("Part runtime state initializes from the data asset"), Component->InitializePartStates());
	TestEqual(TEXT("Initial part health comes from data asset"),
		Component->GetPartCurrentHealth(PartTag), 30.0f);

	FHitResult HitResult;
	HitResult.BoneName = Definition.BoneName;
	TestTrue(TEXT("Point damage resolves the exact authored bone"),
		Component->ProcessPointDamage(10.0f, HitResult, nullptr, nullptr, nullptr));
	TestEqual(TEXT("Part rule reduces runtime health"), Component->GetPartCurrentHealth(PartTag), 10.0f);
	TestEqual(TEXT("Body rule is delivered once"), State->TotalAppliedBodyDamage, 5.0f);
	TestEqual(TEXT("One State call for one attack"), State->ApplyCallCount, 1);

	TestTrue(TEXT("Second point damage depletes the part"),
		Component->ProcessPointDamage(10.0f, HitResult, nullptr, nullptr, nullptr));
	TestEqual(TEXT("Part health clamps to zero"), Component->GetPartCurrentHealth(PartTag), 0.0f);
	TestTrue(TEXT("Part reports depleted"), Component->IsPartHealthDepleted(PartTag));
	TestEqual(TEXT("Depletion event fires once"), State->PartDepletedCount, 1);

	TestTrue(TEXT("Damage can still route through an already depleted hit bone"),
		Component->ProcessPointDamage(10.0f, HitResult, nullptr, nullptr, nullptr));
	TestEqual(TEXT("Repeated damage does not duplicate depletion"), State->PartDepletedCount, 1);
	TestEqual(TEXT("Each attack produces one State call"), State->ApplyCallCount, 3);

	const float HealthBeforeGenericDamage = Component->GetPartCurrentHealth(PartTag);
	TestTrue(TEXT("Unresolved generic damage uses the default body-only policy"),
		Component->ProcessGenericDamage(10.0f, nullptr, nullptr, nullptr));
	TestEqual(TEXT("Generic damage does not select or mutate a part"),
		Component->GetPartCurrentHealth(PartTag), HealthBeforeGenericDamage);
	TestEqual(TEXT("Unresolved attack emits the resolution-failed event"),
		State->PartResolutionFailedCount, 1);
	TestEqual(TEXT("Generic body-only damage still calls State once"), State->ApplyCallCount, 4);
	return true;
}

#endif

