#pragma once

class AActor;
class UActorComponent;

namespace TDDamageComponentRegistry
{
	/** Returns false when another damage component already owns the actor's damage events. */
	bool Register(AActor* Owner, UActorComponent* Component);
	void Unregister(AActor* Owner, UActorComponent* Component);
}

