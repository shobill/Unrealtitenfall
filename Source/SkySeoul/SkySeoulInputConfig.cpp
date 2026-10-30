// Fill out your copyright notice in the Description page of Project Settings.


#include "SkySeoulInputConfig.h"

USkySeoulInputConfig::USkySeoulInputConfig(const FObjectInitializer& ObjectInitializer)
{
}

const UInputAction* USkySeoulInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag) const
{
	for (const FSkySeoulInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && (Action.InputTag == InputTag))
		{
			return Action.InputAction;
		}
	}
	return nullptr;
}