#include "DeathMatchPlayerState.h"
#include "DeathMatch/Core/DeathMatchLog.h"
#include "Character/Abilities/CharacterAbilitySystemComponent.h"
#include "SkySeoul/DeathMatch/GameplayMessageStructure/DeathMatchMessageStructure.h"
#include "DeathMatch/GameplayMessageStructure/DeathMatchEventSubsystem.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "PartsCharacter/PartsCharacterBase.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"

ADeathMatchPlayerState::ADeathMatchPlayerState()
{
	CachedTeamID = FGenericTeamId::NoTeam;
}

void ADeathMatchPlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		if (!DeadTag.IsValid())
		{
			DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Dead")), false);
		}
		if (DeadTag.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(
				DeadTag,
				EGameplayTagEventType::NewOrRemoved
			).AddUObject(this, &ADeathMatchPlayerState::DeathTagChanged);
		}
	}
}

void ADeathMatchPlayerState::DeathTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (!HasAuthority())
	{
		return;
	}

	if (NewCount <= 0)
	{
		return;
	}

	AController* VictimController = GetOwner<AController>();

	// Parts death spectacle before match respawn destroys the pawn.
	if (ADeathMatchRobotAIController* RobotAI = Cast<ADeathMatchRobotAIController>(VictimController))
	{
		RobotAI->NotifyPossessedRobotDied();
	}
	else if (VictimController)
	{
		if (APartsCharacterBase* PartsPawn = Cast<APartsCharacterBase>(VictimController->GetPawn()))
		{
			PartsPawn->HandleOwnerDeathPartsExplosion();
		}
	}

	FDeathMessagePayload Payload;
	Payload.Victim = VictimController;
	Payload.VictimTeamId = GetGenericTeamId().GetId();

	const FGameplayTag DeathEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Death")), false);
	if (DeathEventTag.IsValid())
	{
		GetWorld()->GetSubsystem<UDeathMatchEventSubsystem>()->OnDeath.Broadcast(DeathEventTag, Payload);
	}
	else
	{
		UE_LOG(LogDM, Error, TEXT("Event.Death gameplay tag missing — add to DefaultGameplayTags.ini"));
	}
}

void ADeathMatchPlayerState::SetGenericTeamId(const FGenericTeamId& TeamID)
{
	CachedTeamID = TeamID;
}

FGenericTeamId ADeathMatchPlayerState::GetGenericTeamId() const
{
	return CachedTeamID;
}

ETeamAttitude::Type ADeathMatchPlayerState::GetTeamAttitudeTowards(const AActor& Other) const
{
	return DeathMatchTeamUtils::GetAttitudeTowards(GetGenericTeamId(), Other);
}
