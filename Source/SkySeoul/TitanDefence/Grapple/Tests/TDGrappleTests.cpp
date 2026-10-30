#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TitanDefence/Grapple/TDGrappleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "CableComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDGrappleTerrainTest, "SkySeoul.TitanDefense.Grapple.TerrainOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDGrappleTerrainTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues IV = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TDGrappleTest"),
		nullptr, true, ERHIFeatureLevel::Num, &IV);
	AActor* Owner = World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Owner);
	Owner->SetRootComponent(Root);
	Root->RegisterComponent();
	UTDGrappleComponent* Grapple = NewObject<UTDGrappleComponent>(Owner);
	Grapple->RegisterComponent();
	Owner->DispatchBeginPlay();
	TestNotNull(TEXT("Cable created at BeginPlay"), Grapple->Cable.Get());
	auto AddBox = [World](AActor* Actor, float X, ECollisionChannel ObjectType, EComponentMobility::Type Mobility)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(50));
		Box->SetMobility(Mobility);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ObjectType);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetWorldLocation(FVector(X, 0, 0));
		Box->RegisterComponent();
		return Box;
	};
	AActor* Wall = World->SpawnActor<AActor>();
	AddBox(Wall, 1000.f, ECC_WorldStatic, EComponentMobility::Static);
	const FVector Start = FVector::ZeroVector;
	auto TryConnect = [Grapple, Start](const FVector& Direction)
	{
		Grapple->ReleaseGrapple();
		FHitResult Hit;
		return Grapple->FindTerrainTarget(Start, Direction, Hit) && Grapple->AttachToTerrain(Hit);
	};
	TestTrue(TEXT("Static terrain accepts grapple"), TryConnect(FVector::ForwardVector));
	TestTrue(TEXT("Hit point is saved on the wall"), Grapple->GetAnchorWorldLocation().Equals(FVector(950, 0, 0), 1.f));
	if (Grapple->Cable)
	{
		TestTrue(TEXT("Cable visible while connected"), Grapple->Cable->IsVisible());
		TestTrue(TEXT("Cable local endpoint converts to the hit position"),
			Grapple->Cable->GetComponentTransform().TransformPosition(Grapple->Cable->EndLocation)
			.Equals(Grapple->GetAnchorWorldLocation(), 1.f));
	}
	Grapple->ReleaseGrapple();
	TestFalse(TEXT("Release clears state"), Grapple->IsGrappling());
	if (Grapple->Cable) TestFalse(TEXT("Cable hidden after release"), Grapple->Cable->IsVisible());
	Grapple->MaxRange = 500;
	TestFalse(TEXT("Out of range terrain rejected"), TryConnect(FVector::ForwardVector));
	Grapple->MaxRange = 3000;
	APawn* Titan = World->SpawnActor<APawn>();
	UBoxComponent* TitanBox = AddBox(Titan, 400.f, ECC_WorldStatic, EComponentMobility::Static);
	TestFalse(TEXT("Pawn rejected even with WorldStatic collision; wall behind remains blocked"),
		TryConnect(FVector::ForwardVector));
	TitanBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AActor* MovingObject = World->SpawnActor<AActor>();
	UBoxComponent* MovingBox = AddBox(MovingObject, 500.f, ECC_WorldDynamic, EComponentMobility::Movable);
	TestFalse(TEXT("Dynamic obstacle rejected"), TryConnect(FVector::ForwardVector));
	MovingBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Terrain can reconnect"), TryConnect(FVector::ForwardVector));
	TestFalse(TEXT("Miss clears previous connection"), TryConnect(FVector::UpVector));
	TestFalse(TEXT("Miss leaves no grapple"), Grapple->IsGrappling());
	World->DestroyWorld(false);
	return true;
}
#endif
