#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "DMTacticalAreaTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable, HideDropdown)
class ADMTacticalAreaTestPawn : public APawn, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override { TeamId = NewTeamId; }
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }

private:
	FGenericTeamId TeamId = FGenericTeamId::NoTeam;
};
