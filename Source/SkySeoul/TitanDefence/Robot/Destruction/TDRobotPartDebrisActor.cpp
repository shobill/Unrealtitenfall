#include "TitanDefence/Robot/Destruction/TDRobotPartDebrisActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

ATDRobotPartDebrisActor::ATDRobotPartDebrisActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	DebrisMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebrisMesh"));
	SetRootComponent(DebrisMeshComponent);

	DebrisMeshComponent->SetMobility(EComponentMobility::Movable);
	DebrisMeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
	DebrisMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	DebrisMeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	DebrisMeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	DebrisMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	DebrisMeshComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	DebrisMeshComponent->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	DebrisMeshComponent->SetGenerateOverlapEvents(false);
	DebrisMeshComponent->SetNotifyRigidBodyCollision(false);
	DebrisMeshComponent->SetCanEverAffectNavigation(false);
	DebrisMeshComponent->CanCharacterStepUpOn = ECB_No;
	DebrisMeshComponent->SetEnableGravity(true);
}

bool ATDRobotPartDebrisActor::InitializeDebris(
	UStaticMesh* InDebrisMesh,
	const FTDRobotPartSeparationSettings& SeparationSettings,
	AActor* SourceRobot,
	const FVector& HitLocation,
	const FVector& HitDirection,
	float ImpulseMultiplier)
{
	if (!IsValid(InDebrisMesh) || !IsValid(DebrisMeshComponent))
	{
		return false;
	}

	DebrisMeshComponent->SetStaticMesh(InDebrisMesh);

	RuntimePhysicalMaterial = NewObject<UPhysicalMaterial>(this);
	if (!RuntimePhysicalMaterial)
	{
		return false;
	}

	RuntimePhysicalMaterial->Friction = SeparationSettings.Friction;
	RuntimePhysicalMaterial->Restitution = SeparationSettings.Restitution;
	DebrisMeshComponent->SetPhysMaterialOverride(RuntimePhysicalMaterial);

	IgnoreSourceRobot(SourceRobot);
	DebrisMeshComponent->SetLinearDamping(SeparationSettings.LinearDamping);
	DebrisMeshComponent->SetAngularDamping(SeparationSettings.AngularDamping);
	DebrisMeshComponent->SetSimulatePhysics(true);
	DebrisMeshComponent->SetMassOverrideInKg(NAME_None, SeparationSettings.MassKg, true);
	DebrisMeshComponent->SetPhysicsMaxAngularVelocityInDegrees(
		SeparationSettings.MaxAngularVelocityDegrees, false);

	const bool bPhysicsApplied = DebrisMeshComponent->IsSimulatingPhysics();
	if (bPhysicsApplied && !HitDirection.IsNearlyZero() && SeparationSettings.SeparationImpulse > 0.0f)
	{
		const FVector ImpulseDirection =
			(HitDirection.GetSafeNormal() + FVector::UpVector * SeparationSettings.UpwardImpulseRatio)
			.GetSafeNormal();
		const FVector Impulse = ImpulseDirection
			* SeparationSettings.SeparationImpulse
			* FMath::Max(0.0f, ImpulseMultiplier);

		if (HitLocation.IsNearlyZero())
		{
			DebrisMeshComponent->AddImpulse(Impulse, NAME_None, false);
		}
		else
		{
			DebrisMeshComponent->AddImpulseAtLocation(Impulse, HitLocation, NAME_None);
		}
	}

	SetLifeSpan(SeparationSettings.DebrisLifetime);
	return bPhysicsApplied;
}

void ATDRobotPartDebrisActor::IgnoreSourceRobot(AActor* SourceRobot)
{
	if (!IsValid(SourceRobot) || !IsValid(DebrisMeshComponent))
	{
		return;
	}

	DebrisMeshComponent->IgnoreActorWhenMoving(SourceRobot, true);

	TInlineComponentArray<UPrimitiveComponent*> SourcePrimitives(SourceRobot);
	for (UPrimitiveComponent* SourcePrimitive : SourcePrimitives)
	{
		if (IsValid(SourcePrimitive))
		{
			SourcePrimitive->IgnoreActorWhenMoving(this, true);
		}
	}
}
