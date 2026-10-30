#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMCombatFocusComponent.generated.h"

/** Owns branch-scoped focus requests and restores the previous request when a branch ends. */
UCLASS(ClassGroup=(DeathMatch), meta=(BlueprintSpawnableComponent))
class SKYSEOUL_API UDMCombatFocusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDMCombatFocusComponent();

	uint64 BeginActorFocus(AActor* Actor);
	uint64 BeginLocationFocus(const FVector& Location);
	void UpdateActorFocus(uint64 RequestId, AActor* Actor);
	void UpdateLocationFocus(uint64 RequestId, const FVector& Location);
	void EndFocusRequest(uint64 RequestId);
	void ResetFocusRequests();
	bool HasFocusRequests() const { return !FocusRequests.IsEmpty(); }
	int32 GetFocusRequestCount() const { return FocusRequests.Num(); }

private:
	struct FFocusRequest
	{
		TWeakObjectPtr<AActor> Actor;
		FVector Location = FVector::ZeroVector;
		uint64 Serial = 0;
		bool bActor = false;
	};

	void ApplyWinningFocus();
	TMap<uint64, FFocusRequest> FocusRequests;
	uint64 NextRequestId = 1;
	uint64 NextSerial = 1;
};
