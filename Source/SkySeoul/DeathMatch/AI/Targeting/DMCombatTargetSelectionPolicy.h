#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DMCombatTargetSelectionPolicy.generated.h"

class UDMCombatKnowledgeComponent;
struct FDMRememberedThreat;

/** Reusable target scoring policy. Knowledge supplies evidence; the BT task decides when to run it. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SKYSEOUL_API UDMCombatTargetSelectionPolicy : public UObject
{
	GENERATED_BODY()

public:
	AActor* SelectTarget(const UDMCombatKnowledgeComponent& Knowledge, const APawn* Observer) const;
	float ScoreCandidate(const FDMRememberedThreat& Candidate, const AActor* CurrentTarget, const APawn* Observer) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target Selection") float VisibleBonus = 1000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target Selection") float CurrentTargetBonus = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target Selection") float DistancePenaltyPerMeter = 1.f;
};
