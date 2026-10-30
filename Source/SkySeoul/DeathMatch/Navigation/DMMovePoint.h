#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DMMovePoint.generated.h"

/**
 * Place in level for AI roam / move goals (optional minimap marker later).
 * Selection logic lives in BT/ability; this actor is data only.
 */
UCLASS(Blueprintable)
class SKYSEOUL_API ADMMovePoint : public AActor
{
	GENERATED_BODY()

public:
	ADMMovePoint();

	UFUNCTION(BlueprintPure, Category = "DeathMatch|MovePoint")
	FVector GetMoveLocation() const { return GetActorLocation(); }

	/** Optional filter (team lane, zone id). 255 = any. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|MovePoint")
	uint8 ZoneId = 255;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|MovePoint")
	bool bEnabled = true;

	/** Higher = more likely to be chosen (before runtime penalties). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|MovePoint", meta = (ClampMin = "0.01"))
	float BaseWeight = 1.0f;

	/** Radius for future "pick near this point" logic. 0 = use exact location. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|MovePoint", meta = (ClampMin = "0.0"))
	float ArrivalRadius = 0.0f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "DeathMatch|MovePoint")
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "DeathMatch|MovePoint")
	TObjectPtr<UBillboardComponent> EditorSprite;
#endif
};
