#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DeathMatch/Core/DMHealthResolver.h"
#include "DeathMatch/Presentation/HealthBars/DMHealthBarsRuntimeComponent.h"
#include "DeathMatch/Debug/DMDebugObserverPawn.h"
#include "DeathMatch/Character/DeathMatchRobotCharacter.h"
#include "DeathMatch/Tests/DMHealthBarTestTypes.h"
#include "Character/Abilities/Attributes/CharacterAttributeSetBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMHealthSnapshotValueTest, "SkySeoul.DeathMatch.HealthBars.SnapshotValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMHealthSnapshotValueTest::RunTest(const FString& Parameters)
{
	const FDMHealthSnapshot Half = DMHealth::MakeSnapshot(50.0f, 100.0f, true, EDMHealthSource::BodyHP);
	TestTrue(TEXT("Valid body snapshot"), Half.bValid);
	TestEqual(TEXT("Body source"), Half.HealthSource, EDMHealthSource::BodyHP);
	TestEqual(TEXT("Half ratio"), Half.HealthRatio, 0.5f);
	TestTrue(TEXT("Positive existing state is alive"), Half.bAlive);

	const FDMHealthSnapshot Overheal = DMHealth::MakeSnapshot(150.0f, 100.0f, true, EDMHealthSource::GAS);
	TestEqual(TEXT("Current clamps to max"), Overheal.CurrentHealth, 100.0f);
	TestEqual(TEXT("Ratio clamps to one"), Overheal.HealthRatio, 1.0f);

	const FDMHealthSnapshot Negative = DMHealth::MakeSnapshot(-10.0f, 100.0f, true, EDMHealthSource::BodyHP);
	TestEqual(TEXT("Current clamps to zero"), Negative.CurrentHealth, 0.0f);
	TestFalse(TEXT("Zero health is dead"), Negative.bAlive);
	TestFalse(TEXT("Existing death state is respected"), DMHealth::MakeSnapshot(100.0f, 100.0f, false, EDMHealthSource::BodyHP).bAlive);

	for (const float InvalidMax : { 0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
	{
		const FDMHealthSnapshot Invalid = DMHealth::MakeSnapshot(50.0f, InvalidMax, true, EDMHealthSource::BodyHP);
		TestFalse(TEXT("Invalid max rejected"), Invalid.bValid);
		TestEqual(TEXT("Invalid ratio safely defaults to one"), Invalid.HealthRatio, 1.0f);
		TestEqual(TEXT("Invalid source"), Invalid.HealthSource, EDMHealthSource::Invalid);
	}
	TestFalse(TEXT("Invalid current rejected"), DMHealth::MakeSnapshot(std::numeric_limits<float>::quiet_NaN(), 100.0f, true, EDMHealthSource::GAS).bValid);
	TestFalse(TEXT("Explicit invalid source rejected"), DMHealth::MakeSnapshot(100.0f, 100.0f, true, EDMHealthSource::Invalid).bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMHealthResolverActorTest, "SkySeoul.DeathMatch.HealthBars.ResolverActors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMHealthResolverActorTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Initialization;
	Initialization.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DMHealthResolverActorTest"), nullptr, true, ERHIFeatureLevel::Num, &Initialization);
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}

	ADeathMatchRobotCharacter* Robot = World->SpawnActor<ADeathMatchRobotCharacter>();
	Robot->CurHP = 35.0f;
	Robot->MaxHP = 100.0f;
	Robot->IsDead = false;
	FDMHealthSnapshot Snapshot = DMHealth::Resolve(Robot);
	TestTrue(TEXT("DeathMatch parts health is valid"), Snapshot.bValid);
	TestEqual(TEXT("DeathMatch parts prefer body HP"), Snapshot.HealthSource, EDMHealthSource::BodyHP);
	TestEqual(TEXT("DeathMatch body ratio"), Snapshot.HealthRatio, 0.35f);
	Robot->IsDead = true;
	TestFalse(TEXT("Existing DeathMatch death state wins over positive HP"), DMHealth::Resolve(Robot).bAlive);

	ADMHealthResolverTestCharacter* Generic = World->SpawnActor<ADMHealthResolverTestCharacter>();
	UCharacterAttributeSetBase* Attributes = NewObject<UCharacterAttributeSetBase>(Generic);
	Attributes->SetHealth(25.0f);
	Attributes->SetMaxHealth(50.0f);
	Generic->SetTestAttributeSet(Attributes);
	Generic->CurHP = 90.0f;
	Generic->MaxHP = 100.0f;
	Snapshot = DMHealth::Resolve(Generic);
	TestEqual(TEXT("Generic character prefers valid GAS"), Snapshot.HealthSource, EDMHealthSource::GAS);
	TestEqual(TEXT("GAS ratio"), Snapshot.HealthRatio, 0.5f);
	Attributes->SetMaxHealth(0.0f);
	Snapshot = DMHealth::Resolve(Generic);
	TestEqual(TEXT("Invalid GAS max falls back to body"), Snapshot.HealthSource, EDMHealthSource::BodyHP);
	TestEqual(TEXT("Fallback body ratio"), Snapshot.HealthRatio, 0.9f);

	const FVector ExpectedAnchor = Generic->GetActorLocation() + Generic->GetActorUpVector()
		* (Generic->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 30.0f);
	TestTrue(TEXT("Character anchor uses capsule top"), DMHealthBars::ResolveAnchorLocation(Generic, NAME_None, 30.0f).Equals(ExpectedAnchor));

	ADMDebugObserverPawn* Observer = World->SpawnActor<ADMDebugObserverPawn>();
	TestFalse(TEXT("Observer cannot be damaged"), Observer->CanBeDamaged());
	TestEqual(TEXT("Observer damage is always zero"), Observer->TakeDamage(100.0f, FDamageEvent(), nullptr, nullptr), 0.0f);
	TestFalse(TEXT("Observer does not replicate"), Observer->GetIsReplicated());
	TestFalse(TEXT("Observer collision is disabled"), Observer->GetActorEnableCollision());
	TestTrue(TEXT("Observer has no combat team"), Observer->GetGenericTeamId() == FGenericTeamId::NoTeam);

	World->DestroyWorld(false);
	return true;
}

#endif

