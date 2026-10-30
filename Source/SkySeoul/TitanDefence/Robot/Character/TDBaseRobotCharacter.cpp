#include "TitanDefence/Robot/Character/TDBaseRobotCharacter.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "TitanDefence/Robot/Controller/TDBaseRobotAIController.h"
#include "TitanDefence/Robot/Core/TDRobotLog.h"
#include "TitanDefence/Robot/GAS/TDRobotAbilitySystemComponent.h"
#include "TitanDefence/Robot/GAS/TDRobotAttributeSet.h"
#include "TitanDefence/Robot/State/TDRobotStateComponent.h"

ATDBaseRobotCharacter::ATDBaseRobotCharacter()
{
	AIControllerClass = ATDBaseRobotAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	RobotAbilitySystemComponent = CreateDefaultSubobject<UTDRobotAbilitySystemComponent>(
		TEXT("RobotAbilitySystem"));
	RobotAttributeSet = CreateDefaultSubobject<UTDRobotAttributeSet>(TEXT("RobotAttributes"));
	RobotStateComponent = CreateDefaultSubobject<UTDRobotStateComponent>(TEXT("RobotState"));
}

UAbilitySystemComponent* ATDBaseRobotCharacter::GetAbilitySystemComponent() const
{
	return RobotAbilitySystemComponent;
}

void ATDBaseRobotCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Robot '%s' possessed by '%s' (class=%s, authority=%s)."),
		*GetNameSafe(this), *GetNameSafe(NewController),
		*GetNameSafe(NewController ? NewController->GetClass() : nullptr),
		HasAuthority() ? TEXT("true") : TEXT("false"));
	InitializeRobotStateSystem();
}

void ATDBaseRobotCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	InitializeRobotStateSystem();
}

float ATDBaseRobotCharacter::TakeDamage(
	float DamageAmount,
	FDamageEvent const& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	const TCHAR* EventType = TEXT("Generic");
	FName HitBoneName = NAME_None;
	FVector HitLocation = FVector::ZeroVector;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		EventType = TEXT("Point");
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		HitBoneName = PointEvent.HitInfo.BoneName;
		HitLocation = PointEvent.HitInfo.ImpactPoint;
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		EventType = TEXT("Radial");
		const FRadialDamageEvent& RadialEvent = static_cast<const FRadialDamageEvent&>(DamageEvent);
		if (!RadialEvent.ComponentHits.IsEmpty())
		{
			HitBoneName = RadialEvent.ComponentHits[0].BoneName;
			HitLocation = RadialEvent.ComponentHits[0].ImpactPoint;
		}
	}

	UE_LOG(LogTDRobotDamage, Warning,
		TEXT("Robot TakeDamage ENTER: robot='%s' event=%s requested=%.2f canBeDamaged=%s authority=%s instigator='%s' causer='%s' bone='%s' hit=%s."),
		*GetNameSafe(this), EventType, DamageAmount, CanBeDamaged() ? TEXT("true") : TEXT("false"),
		HasAuthority() ? TEXT("true") : TEXT("false"), *GetNameSafe(EventInstigator),
		*GetNameSafe(DamageCauser), *HitBoneName.ToString(), *HitLocation.ToCompactString());

	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	UE_LOG(LogTDRobotDamage, Warning,
		TEXT("Robot TakeDamage EXIT: robot='%s' event=%s requested=%.2f returned=%.2f anyDamageBound=%s pointDamageBound=%s radialDamageBound=%s."),
		*GetNameSafe(this), EventType, DamageAmount, AppliedDamage,
		OnTakeAnyDamage.IsBound() ? TEXT("true") : TEXT("false"),
		OnTakePointDamage.IsBound() ? TEXT("true") : TEXT("false"),
		OnTakeRadialDamage.IsBound() ? TEXT("true") : TEXT("false"));
	return AppliedDamage;
}

void ATDBaseRobotCharacter::BeginPlay()
{
	Super::BeginPlay();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const USkeletalMeshComponent* SkeletalMesh = GetMesh();
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Robot '%s' BeginPlay: location=%s controller='%s' autoMove=%s initialTarget='%s' acceptanceRadius=%.1f."),
		*GetNameSafe(this), *GetActorLocation().ToCompactString(), *GetNameSafe(GetController()),
		bAutoMoveOnBeginPlay ? TEXT("true") : TEXT("false"),
		*GetNameSafe(InitialMoveTargetActor), TargetAcceptanceRadius);
	UE_LOG(LogTDRobotMovement, Warning,
		TEXT("Robot movement collision diagnostics: robot='%s' actorScale=%s actorCollision=%s capsuleRadius=%.2f capsuleHalfHeight=%.2f capsuleProfile='%s' capsuleCollision=%d capsuleNavRelevant=%s mesh='%s' meshProfile='%s' meshCollision=%d meshNavRelevant=%s."),
		*GetNameSafe(this), *GetActorScale3D().ToCompactString(),
		GetActorEnableCollision() ? TEXT("enabled") : TEXT("disabled"),
		Capsule ? Capsule->GetUnscaledCapsuleRadius() : 0.0f,
		Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 0.0f,
		Capsule ? *Capsule->GetCollisionProfileName().ToString() : TEXT("None"),
		Capsule ? static_cast<int32>(Capsule->GetCollisionEnabled()) : -1,
		Capsule && Capsule->CanEverAffectNavigation() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr),
		SkeletalMesh ? *SkeletalMesh->GetCollisionProfileName().ToString() : TEXT("None"),
		SkeletalMesh ? static_cast<int32>(SkeletalMesh->GetCollisionEnabled()) : -1,
		SkeletalMesh && SkeletalMesh->CanEverAffectNavigation() ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTDRobotDamage, Warning,
		TEXT("Robot damage collision diagnostics: robot='%s' canBeDamaged=%s authority=%s capsuleQuery=%s meshQuery=%s meshHasPhysicsAsset=%s anyDamageBound=%s pointDamageBound=%s radialDamageBound=%s."),
		*GetNameSafe(this), CanBeDamaged() ? TEXT("true") : TEXT("false"),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		Capsule && Capsule->IsQueryCollisionEnabled() ? TEXT("true") : TEXT("false"),
		SkeletalMesh && SkeletalMesh->IsQueryCollisionEnabled() ? TEXT("true") : TEXT("false"),
		SkeletalMesh && SkeletalMesh->GetPhysicsAsset() ? TEXT("true") : TEXT("false"),
		OnTakeAnyDamage.IsBound() ? TEXT("true") : TEXT("false"),
		OnTakePointDamage.IsBound() ? TEXT("true") : TEXT("false"),
		OnTakeRadialDamage.IsBound() ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTDRobotDamage, Warning,
		TEXT("Robot collision channel responses: robot='%s' capsule[Visibility=%d Camera=%d WorldStatic=%d WorldDynamic=%d Pawn=%d] mesh[Visibility=%d Camera=%d WorldStatic=%d WorldDynamic=%d Pawn=%d]. Response values: Ignore=0 Overlap=1 Block=2."),
		*GetNameSafe(this),
		Capsule ? static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_Visibility)) : -1,
		Capsule ? static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_Camera)) : -1,
		Capsule ? static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_WorldStatic)) : -1,
		Capsule ? static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_WorldDynamic)) : -1,
		Capsule ? static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_Pawn)) : -1,
		SkeletalMesh ? static_cast<int32>(SkeletalMesh->GetCollisionResponseToChannel(ECC_Visibility)) : -1,
		SkeletalMesh ? static_cast<int32>(SkeletalMesh->GetCollisionResponseToChannel(ECC_Camera)) : -1,
		SkeletalMesh ? static_cast<int32>(SkeletalMesh->GetCollisionResponseToChannel(ECC_WorldStatic)) : -1,
		SkeletalMesh ? static_cast<int32>(SkeletalMesh->GetCollisionResponseToChannel(ECC_WorldDynamic)) : -1,
		SkeletalMesh ? static_cast<int32>(SkeletalMesh->GetCollisionResponseToChannel(ECC_Pawn)) : -1);
	InitializeRobotStateSystem();
	if (ATDBaseRobotAIController* RobotController = GetTDRobotController())
	{
		RobotController->NotifyRobotBeginPlay(this);
	}
}

bool ATDBaseRobotCharacter::InitializeRobotStateSystem()
{
	if (!IsValid(RobotAbilitySystemComponent)
		|| !IsValid(RobotAttributeSet)
		|| !IsValid(RobotStateComponent))
	{
		UE_LOG(LogTDRobot, Error,
			TEXT("Titan Defence robot '%s' is missing a required native State subobject."),
			*GetNameSafe(this));
		return false;
	}

	if (RobotAbilitySystemComponent->GetOwnerActor() != this
		|| RobotAbilitySystemComponent->GetAvatarActor() != this)
	{
		RobotAbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	if (!RobotStateComponent->InitializeStateSystem(
		RobotAbilitySystemComponent,
		RobotAttributeSet))
	{
		return false;
	}

	if (!bRobotStateReadyNotified)
	{
		bRobotStateReadyNotified = true;
		BP_OnRobotStateReady(RobotStateComponent, RobotAbilitySystemComponent);
	}
	return true;
}

bool ATDBaseRobotCharacter::StartMovingToInitialTarget()
{
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Robot '%s' StartMovingToInitialTarget: target='%s'."),
		*GetNameSafe(this), *GetNameSafe(InitialMoveTargetActor));
	return StartMovingToActor(InitialMoveTargetActor);
}

bool ATDBaseRobotCharacter::StartMovingToActor(AActor* NewTargetActor)
{
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Robot '%s' StartMovingToActor: target='%s' controller='%s'."),
		*GetNameSafe(this), *GetNameSafe(NewTargetActor), *GetNameSafe(GetController()));
	SetCurrentMoveTargetActor(NewTargetActor);

	// A null target means that no move command is configured. This is not a failure.
	if (NewTargetActor == nullptr)
	{
		UE_LOG(LogTDRobotMovement, Warning,
			TEXT("Robot '%s' has no movement target. Assign Initial Move Target Actor or pass a valid actor."),
			*GetNameSafe(this));
		if (ATDBaseRobotAIController* RobotController = GetTDRobotController())
		{
			return RobotController->ClearMoveTarget();
		}

		TransitionMovementState(ETDRobotMovementState::Idle);
		return true;
	}

	if (!IsValid(NewTargetActor))
	{
		UE_LOG(LogTDRobotMovement, Warning,
			TEXT("Robot '%s' rejected invalid movement target '%s'."),
			*GetNameSafe(this), *GetNameSafe(NewTargetActor));
		SetCurrentMoveTargetActor(nullptr);
		TransitionMovementState(
			ETDRobotMovementState::Failed,
			ETDRobotMovementEndReason::InvalidTarget);
		return false;
	}

	ATDBaseRobotAIController* RobotController = GetTDRobotController();
	if (!RobotController)
	{
		UE_LOG(LogTDRobotMovement, Warning,
			TEXT("Robot '%s' cannot move: controller '%s' is not TDBaseRobotAIController."),
			*GetNameSafe(this), *GetNameSafe(GetController()));
		TransitionMovementState(
			ETDRobotMovementState::Failed,
			ETDRobotMovementEndReason::MissingController);
		return false;
	}

	return RobotController->RequestMoveToActor(NewTargetActor, TargetAcceptanceRadius);
}

void ATDBaseRobotCharacter::CancelCurrentMovement()
{
	if (ATDBaseRobotAIController* RobotController = GetTDRobotController())
	{
		RobotController->CancelCurrentMove();
		return;
	}

	SetCurrentMoveTargetActor(nullptr);
	TransitionMovementState(
		ETDRobotMovementState::Idle,
		ETDRobotMovementEndReason::Cancelled);
}

bool ATDBaseRobotCharacter::RequestMovementInterruption(
	ETDRobotMovementState InterruptionState,
	ETDRobotMovementEndReason EndReason)
{
	if (InterruptionState != ETDRobotMovementState::Blocked
		&& InterruptionState != ETDRobotMovementState::Interrupted
		&& InterruptionState != ETDRobotMovementState::Disabled)
	{
		UE_LOG(LogTDRobot, Warning,
			TEXT("Robot %s rejected unsupported interruption state %d."),
			*GetNameSafe(this), static_cast<int32>(InterruptionState));
		return false;
	}

	if (ATDBaseRobotAIController* RobotController = GetTDRobotController())
	{
		RobotController->InterruptCurrentMove(InterruptionState, EndReason);
	}
	else
	{
		TransitionMovementState(InterruptionState, EndReason);
	}

	return true;
}

ATDBaseRobotAIController* ATDBaseRobotCharacter::GetTDRobotController() const
{
	return Cast<ATDBaseRobotAIController>(GetController());
}

void ATDBaseRobotCharacter::SetCurrentMoveTargetActor(AActor* NewTargetActor)
{
	CurrentMoveTargetActor = NewTargetActor;
}

void ATDBaseRobotCharacter::TransitionMovementState(
	ETDRobotMovementState NewState,
	ETDRobotMovementEndReason EndReason)
{
	LastMovementEndReason = EndReason;

	if (MovementState == NewState)
	{
		return;
	}

	const ETDRobotMovementState PreviousState = MovementState;
	MovementState = NewState;
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Robot '%s' movement state: %s -> %s, reason=%s, target='%s'."),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(PreviousState),
		*UEnum::GetValueAsString(MovementState),
		*UEnum::GetValueAsString(LastMovementEndReason),
		*GetNameSafe(CurrentMoveTargetActor));
	OnMovementStateChanged.Broadcast(
		PreviousState,
		MovementState,
		LastMovementEndReason,
		CurrentMoveTargetActor);
}

