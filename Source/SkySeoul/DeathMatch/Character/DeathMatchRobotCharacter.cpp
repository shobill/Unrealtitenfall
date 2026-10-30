#include "DeathMatchRobotCharacter.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/Abilities/DMGameplayAbility_Fire.h"

#include "DeathMatch/Weapons/DMWeaponLoadoutComponent.h"
#include "DeathMatch/Weapons/DMAimZoomComponent.h"
#include "DeathMatch/Character/DMAimTargetComponent.h"
#include "DeathMatch/AIModules/DMPerceptionStimuliSourceComponent.h"
#include "DeathMatch/Core/DeathMatchLog.h"
#include "DeathMatch/Navigation/DMNavigationSafety.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/DamageEvents.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
FDMCombatDamageStimulus MakeDamageStimulus(AController* Instigator, AActor* Causer, const FVector& Hit, FName Part)
{
    FDMCombatDamageStimulus S;
    S.DamageCauser=Causer; S.AttackerController=Instigator; S.HitLocation=Hit; S.HitPartName=Part;
    AActor* Attacker=IsValid(Instigator)?Instigator->GetPawn():nullptr;
    // Weapons/projectiles may supply only a causer. Resolve a pawn/controller, never invent hostility.
    if (!Attacker && IsValid(Causer))
    {
        if (auto* Pawn=Cast<APawn>(Causer)) Attacker=Pawn;
        else if (auto* Controller=Cast<AController>(Causer)) Attacker=Controller->GetPawn();
        else Attacker=Causer->GetInstigator();
    }
    AActor* Owner=IsValid(Causer)?Causer->GetOwner():nullptr;
    for (int32 Depth=0; !Attacker && IsValid(Owner) && Depth<4; ++Depth,Owner=Owner->GetOwner())
    {
        if (auto* Pawn=Cast<APawn>(Owner)) Attacker=Pawn;
        else if (auto* Controller=Cast<AController>(Owner)) Attacker=Controller->GetPawn();
    }
    S.Attacker=Attacker; S.bHasReliableAttacker=IsValid(Attacker);
    if (S.bHasReliableAttacker)
    {
        if (auto* Pawn=Cast<APawn>(Attacker)) S.AttackerController=Pawn->GetController();
        S.DamageOrigin=Attacker->GetActorLocation(); S.bHasReliableOrigin=true;
        S.DamageDirection=(Hit-S.DamageOrigin).GetSafeNormal();
    }
    return S;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GDMRecheckNavigationCommand(
	TEXT("dm.Nav.Recheck"),
	TEXT("Synchronize and re-run DeathMatch robot capsule/NavAgent/NavMesh diagnostics."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) return;
		for (TActorIterator<ADeathMatchRobotCharacter> It(World); It; ++It)
		{
			It->SynchronizeNavAgentWithCapsule();
			It->RecheckNavigationCompatibility(true);
		}
	}));
#endif
}

float ADeathMatchRobotCharacter::TakeDamage(float DamageAmount, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer)
{
    if (IsDead || !FMath::IsFinite(DamageAmount) || DamageAmount<=0) return 0;
    const float Before=CurHP;
    auto S=MakeDamageStimulus(EventInstigator,Causer,GetActorLocation(),NAME_None);
    if (Event.IsOfType(FPointDamageEvent::ClassID))
    {
        const auto& Point=static_cast<const FPointDamageEvent&>(Event);
        S.HitLocation=Point.HitInfo.ImpactPoint; S.HitPartName=Point.HitInfo.BoneName; S.DamageDirection=Point.ShotDirection;
    }
    else if (Event.IsOfType(FRadialDamageEvent::ClassID))
    {
        const auto& Radial=static_cast<const FRadialDamageEvent&>(Event);
        S.DamageDirection=(GetActorLocation()-Radial.Origin).GetSafeNormal();
    }
    const float Applied=Super::TakeDamage(DamageAmount,Event,EventInstigator,Causer);
    // Parts router publishes one combined event after its body call returns.
    auto* Manager=GetPartsDamageManager();
    if (!IsDead && (!Manager || !Manager->IsRoutingDamage()))
    {
        S.DamageAmount=FMath::Clamp(Before-CurHP,0.f,FMath::Max(0.f,Applied));
        if (auto* Knowledge=GetController()?GetController()->FindComponentByClass<UDMCombatKnowledgeComponent>():nullptr) Knowledge->NotifyDamageReceived(S);
    }
    return Applied;
}

void ADeathMatchRobotCharacter::OnPartsDamageApplied(const FPartsHitDamageResult& Result, AController* EventInstigator, AActor* Causer, const FVector& HitLocation, FName Part)
{
    if (IsDead || !Result.bApplied) return;
    auto S=MakeDamageStimulus(EventInstigator,Causer,HitLocation,Part);
    // Both health pools lost HP, but this remains one Knowledge damage event.
    S.DamageAmount=Result.BodyDamageApplied+Result.PartDamageApplied;
    if (auto* Knowledge=GetController()?GetController()->FindComponentByClass<UDMCombatKnowledgeComponent>():nullptr) Knowledge->NotifyDamageReceived(S);
}

ADeathMatchRobotCharacter::ADeathMatchRobotCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->RotationRate = FRotator(0.0f, 480.0f, 0.0f);
		MoveComp->bRequestedMoveUseAcceleration = true;
		MoveComp->SetUpdateNavAgentWithOwnersCollisions(true);
		MoveComp->bUseRVOAvoidance = true;
		MoveComp->AvoidanceConsiderationRadius = 500.0f;
		MoveComp->AvoidanceWeight = 0.6f;
	}

	WeaponLoadout = CreateDefaultSubobject<UDMWeaponLoadoutComponent>(TEXT("WeaponLoadout"));
	PerceptionStimuliSource = CreateDefaultSubobject<UDMPerceptionStimuliSourceComponent>(TEXT("PerceptionStimuliSource"));

	AimTarget = CreateDefaultSubobject<UDMAimTargetComponent>(TEXT("AimTarget"));
	// Capsule/actor space — mesh is usually yaw -90, so mesh-local sensors skew left/into floor.
	AimTarget->SetupAttachment(RootComponent);

	AimZoom = CreateDefaultSubobject<UDMAimZoomComponent>(TEXT("AimZoom"));
	AimZoom->bApplyCameraZoom = false;
	AimZoom->bSetPlayerActionZoomState = false;
}

void ADeathMatchRobotCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	SynchronizeNavAgentWithCapsule();
	RecheckNavigationCompatibility(true);

	EnsureBodyHealth();

	if (WeaponLoadout)
	{
		WeaponLoadout->RefreshActiveWeaponAbilities();
	}
}

void ADeathMatchRobotCharacter::BeginPlay()
{
	Super::BeginPlay();
	SynchronizeNavAgentWithCapsule();
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		LivingPawnCollisionResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		bCapturedLivingPawnCollisionResponse = true;
	}
#if !UE_BUILD_SHIPPING
	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSystem->OnNavigationGenerationFinishedDelegate.AddDynamic(this, &ADeathMatchRobotCharacter::HandleNavigationGenerationFinished);
	}
	RecheckNavigationCompatibility(false);
#endif
	EnsureBodyHealth();
    if (auto* Manager=GetPartsDamageManager()) Manager->OnAppliedDamage.AddUObject(this,&ADeathMatchRobotCharacter::OnPartsDamageApplied);
}

void ADeathMatchRobotCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_BUILD_SHIPPING
	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSystem->OnNavigationGenerationFinishedDelegate.RemoveDynamic(this, &ADeathMatchRobotCharacter::HandleNavigationGenerationFinished);
	}
#endif
	Super::EndPlay(EndPlayReason);
}

void ADeathMatchRobotCharacter::SynchronizeNavAgentWithCapsule()
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Capsule || !Movement)
	{
		return;
	}
	Movement->SetUpdateNavAgentWithOwnersCollisions(true);
	Movement->UpdateNavAgent(*this);
	const FDMNavDimensions CapsuleSize = DMNavigationSafety::FromCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	Movement->NavAgentProps.AgentRadius = CapsuleSize.Radius;
	Movement->NavAgentProps.AgentHeight = CapsuleSize.Height;
}

void ADeathMatchRobotCharacter::RecheckNavigationCompatibility(bool bForce)
{
#if !UE_BUILD_SHIPPING
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Capsule || !Movement || !NavSystem)
	{
		return;
	}
	ANavigationData* NavData = NavSystem->GetNavDataForProps(Movement->GetNavAgentPropertiesRef(), GetActorLocation());
	if (!NavData || (!bForce && LastDiagnosedNavData.Get() == NavData))
	{
		return;
	}
	LastDiagnosedNavData = NavData;
	const FDMNavDimensions CapsuleSize = DMNavigationSafety::FromCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	const FNavAgentProperties& AgentProps = Movement->GetNavAgentPropertiesRef();
	const FNavDataConfig& NavConfig = NavData->GetConfig();
	const FDMNavDimensions CharacterAgent{AgentProps.AgentRadius, AgentProps.AgentHeight};
	const FDMNavDimensions NavMeshAgent{NavConfig.AgentRadius, NavConfig.AgentHeight};
	float NavStepHeight = 0.0f;
	if (const ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData))
	{
		NavStepHeight = Recast->GetAgentMaxStepHeight(ENavigationDataResolution::Default);
	}
	const FDMNavMismatch Mismatch = DMNavigationSafety::EvaluateMismatch(
		CapsuleSize, CharacterAgent, NavMeshAgent, Movement->MaxStepHeight, NavStepHeight);
	if (Mismatch.HasWarning())
	{
		UE_LOG(LogDM, Warning,
			TEXT("DM Nav Warning: %s Capsule(R=%.1f H=%.1f), NavAgent(R=%.1f H=%.1f), NavMesh=%s(R=%.1f H=%.1f Step=%.1f), CharacterStep=%.1f. capsuleMismatch=%d navMeshTooSmall=%d stepMismatch=%d wrongNavData=%d"),
			*GetName(), CapsuleSize.Radius, CapsuleSize.Height, CharacterAgent.Radius, CharacterAgent.Height,
			*GetNameSafe(NavData), NavMeshAgent.Radius, NavMeshAgent.Height, NavStepHeight, Movement->MaxStepHeight,
			Mismatch.bCharacterAgentMismatch, Mismatch.bNavMeshTooSmall, Mismatch.bStepHeightMismatch, Mismatch.bWrongNavData);
	}
	else
	{
		UE_LOG(LogDM, Log,
			TEXT("DM Nav: %s Capsule/NavAgent/NavMesh compatible R=%.1f H=%.1f NavData=%s Step=%.1f/%.1f"),
			*GetName(), CapsuleSize.Radius, CapsuleSize.Height, *GetNameSafe(NavData), Movement->MaxStepHeight, NavStepHeight);
	}
#endif
}

void ADeathMatchRobotCharacter::HandleNavigationGenerationFinished(ANavigationData* NavData)
{
#if !UE_BUILD_SHIPPING
	if (NavData == LastDiagnosedNavData.Get())
	{
		LastDiagnosedNavData.Reset();
		RecheckNavigationCompatibility(false);
	}
#endif
}

void ADeathMatchRobotCharacter::SetDeathNavigationState(bool bDead)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetAvoidanceEnabled(!bDead);
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		if (!bCapturedLivingPawnCollisionResponse)
		{
			LivingPawnCollisionResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
			bCapturedLivingPawnCollisionResponse = true;
		}
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, bDead ? ECR_Ignore : LivingPawnCollisionResponse.GetValue());
	}
}

void ADeathMatchRobotCharacter::EnsureBodyHealth()
{
	if (MaxHP <= 0.0f)
	{
		MaxHP = 100.0f;
	}
	if (CurHP <= 0.0f || CurHP > MaxHP)
	{
		CurHP = MaxHP;
	}
	IsDead = false;
}

bool ADeathMatchRobotCharacter::TryActivateDeathMatchAbility(FGameplayTag AbilityTag)
{
	if (!AbilityTag.IsValid())
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return false;
	}

	FGameplayTagContainer AbilityQueryTags;
	AbilityQueryTags.AddTag(AbilityTag);
	const bool bFire = AbilityTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Ability.DeathMatch.Fire")));
    auto* Tactics = GetController() ? GetController()->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr;
    // BT can tick while a montage is active. Do not create/consume another request.
    if (bFire)
        for (const auto& Spec : ASC->GetActivatableAbilities())
			if (Spec.IsActive() && Spec.Ability && Spec.Ability->IsA<UDMGameplayAbility_Fire>()) return false;
    const uint64 Id = bFire && Tactics ? Tactics->EnsureFireAttempt() : 0;
    if (bFire) DMFireAttempt::Log(this, Id, TEXT("AbilityActivationRequested"));
    const bool bActivated = ASC->TryActivateAbilitiesByTag(AbilityQueryTags, true);
	if (bFire && !bActivated) DMFireAttempt::Log(this, Id, TEXT("AbilityActivationRejected"), TEXT("Inspect CanActivate gate / granted Fire specs"));
    if (bFire && Tactics) Tactics->ConsumeFireAttempt();
    return bActivated;
}

bool ADeathMatchRobotCharacter::TryActivateDeathMatchFire()
{
	const FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(FName("Ability.DeathMatch.Fire"), false);
	return TryActivateDeathMatchAbility(FireTag);
}


