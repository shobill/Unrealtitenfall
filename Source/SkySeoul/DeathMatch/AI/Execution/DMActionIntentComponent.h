#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMActionIntentTypes.h"
#include "DMActionIntentComponent.generated.h"

/**
 * Converts high-level BT intent requests into ADS, fire permission and movement-speed policy.
 * Requests are scoped to BT branch lifetime. Safety-critical combat intents outrank overlapping
 * non-combat requests; activation order breaks ties within the same priority.
 */
UCLASS(ClassGroup=(DeathMatch), meta=(BlueprintSpawnableComponent))
class SKYSEOUL_API UDMActionIntentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDMActionIntentComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	uint64 BeginIntentRequest(EDMActionIntent Intent);
	uint64 BeginIntentRequest(EDMActionIntent Intent, const FDMActionIntentExecutionSettings& ExecutionSettings);
	void UpdateIntentRequest(uint64 RequestId, EDMActionIntent Intent);
	void UpdateIntentRequest(uint64 RequestId, EDMActionIntent Intent,
		const FDMActionIntentExecutionSettings& ExecutionSettings);
	void EndIntentRequest(uint64 RequestId);
	void ResetIntentRequests();

	UFUNCTION(BlueprintPure, Category="DeathMatch|Action Intent")
	EDMActionIntent GetActiveIntent() const { return ActiveIntent; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Action Intent")
	EDMBehaviorState GetActiveBehaviorState() const { return ActiveBehaviorState; }
	int32 GetIntentRequestCount() const { return IntentRequests.Num(); }
	bool HasActiveExecutionOverride() const { return bHasActiveExecutionOverride; }
	const FDMActionIntentExecutionSettings& GetActiveExecutionSettings() const { return ActiveExecutionSettings; }
	static bool IntentRequiresADS(EDMActionIntent Intent);
	static bool IntentUsesADSWhileMovingByDefault(EDMActionIntent Intent);
	static bool ShouldUseADS(EDMActionIntent Intent, bool bIsMoving,
		const FDMActionIntentExecutionSettings* ExecutionSettings = nullptr);
	static int32 GetIntentPriority(EDMActionIntent Intent);
	static EDMBehaviorState GetBehaviorStateForIntent(EDMActionIntent Intent);

private:
	struct FIntentRequest
	{
		EDMActionIntent Intent = EDMActionIntent::None;
		FDMActionIntentExecutionSettings ExecutionSettings;
		bool bHasExecutionOverride = false;
		uint64 Serial = 0;
	};

	void ApplyWinningIntent();
	void ApplyIntent(EDMActionIntent Intent, const FDMActionIntentExecutionSettings* ExecutionSettings = nullptr);
	void TransitionBehaviorState(EDMBehaviorState NewState);
	void RefreshADSState();

	TMap<uint64, FIntentRequest> IntentRequests;
	uint64 NextRequestId = 1;
	uint64 NextSerial = 1;

	UPROPERTY(Transient)
	EDMActionIntent ActiveIntent = EDMActionIntent::None;
	UPROPERTY(Transient)
	EDMBehaviorState ActiveBehaviorState = EDMBehaviorState::None;
	FDMActionIntentExecutionSettings ActiveExecutionSettings;
	bool bHasActiveExecutionOverride = false;

};
