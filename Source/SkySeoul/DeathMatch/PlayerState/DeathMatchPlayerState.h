// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SkySeoul/ProjectPlayerState.h"
#include "GenericTeamAgentInterface.h"
#include "DeathMatchPlayerState.generated.h"

/**
 * 
 */

UCLASS()
class SKYSEOUL_API ADeathMatchPlayerState : public AProjectPlayerState, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ADeathMatchPlayerState();

private:
	/** Team this PlayerState belongs to (NoTeam until AssignTeamIfNone). */
	FGenericTeamId CachedTeamID = FGenericTeamId::NoTeam;

protected:
	virtual void BeginPlay() override;

	virtual void DeathTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
public:
	virtual void SetGenericTeamId(const FGenericTeamId& TeamID) override;

	/** Retrieve team identifier in form of FGenericTeamId */
	virtual FGenericTeamId GetGenericTeamId() const override;

	/** Retrieved owner attitude toward given Other object */
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
};
