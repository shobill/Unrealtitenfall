#pragma once

#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DMFireControlTestComponent.generated.h"

/** Deterministic request-lifecycle probe; it does not grant or activate a Gameplay Ability. */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class UDMFireControlTestComponent : public UDMCombatFireControlComponent
{
	GENERATED_BODY()

public:
	bool bFireActive = false;
	bool bStartSucceeds = false;
	int32 StartCallCount = 0;
	int32 StopCallCount = 0;

	virtual bool StartFire() override
	{
		++StartCallCount;
		if (bStartSucceeds)
		{
			bFireActive = true;
		}
		return bStartSucceeds;
	}

	virtual void StopFire() override
	{
		++StopCallCount;
		bFireActive = false;
	}

	virtual bool IsFireActive() const override { return bFireActive; }
};
