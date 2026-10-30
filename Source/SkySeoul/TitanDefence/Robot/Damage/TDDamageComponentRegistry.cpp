#include "TitanDefence/Robot/Damage/TDDamageComponentRegistry.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "TitanDefence/Robot/Core/TDRobotLog.h"

namespace
{
	TMap<TWeakObjectPtr<AActor>, TSet<TWeakObjectPtr<UActorComponent>>> RegisteredComponents;
}

bool TDDamageComponentRegistry::Register(AActor* Owner, UActorComponent* Component)
{
	if (!IsValid(Owner) || !IsValid(Component))
	{
		return false;
	}

	for (auto Iterator = RegisteredComponents.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator.Key().IsValid())
		{
			Iterator.RemoveCurrent();
			continue;
		}

		for (auto ComponentIterator = Iterator.Value().CreateIterator(); ComponentIterator; ++ComponentIterator)
		{
			if (!ComponentIterator->IsValid())
			{
				ComponentIterator.RemoveCurrent();
			}
		}
	}

	TSet<TWeakObjectPtr<UActorComponent>>& Components = RegisteredComponents.FindOrAdd(Owner);
	if (Components.Contains(Component))
	{
		return true;
	}
	if (!Components.IsEmpty())
	{
		UE_LOG(LogTDRobot, Warning,
			TEXT("Actor '%s' rejected an additional Titan Defence damage component. "
				"Use either TDSimpleDamageComponent or TDRobotPartDamageComponent, not both; "
				"only the first component will bind damage events."),
			*GetNameSafe(Owner));
		return false;
	}

	Components.Add(Component);
	return true;
}

void TDDamageComponentRegistry::Unregister(AActor* Owner, UActorComponent* Component)
{
	if (!Owner || !Component)
	{
		return;
	}

	if (TSet<TWeakObjectPtr<UActorComponent>>* Components = RegisteredComponents.Find(Owner))
	{
		Components->Remove(Component);
		if (Components->IsEmpty())
		{
			RegisteredComponents.Remove(Owner);
		}
	}
}

