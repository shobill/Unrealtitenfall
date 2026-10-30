#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DeathMatch/Core/DMHealthTypes.h"
#include "DMHealthBarProviders.generated.h"

UINTERFACE(BlueprintType)
class SKYSEOUL_API UDMPublicHealthProvider : public UInterface
{
	GENERATED_BODY()
};

/** Supplies only health data that game/server policy has approved for this viewer and target. */
class SKYSEOUL_API IDMPublicHealthProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeathMatch|Public Health")
	bool GetPublicHealthSnapshot(const AActor* Viewer, const AActor* Target, FDMHealthSnapshot& OutSnapshot) const;
};

UINTERFACE(BlueprintType)
class SKYSEOUL_API UDMHealthBarAuthorizationProvider : public UInterface
{
	GENERATED_BODY()
};

/** Optional game-mode policy. It cannot request X-ray or all-character access. */
class SKYSEOUL_API IDMHealthBarAuthorizationProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeathMatch|Health Bar Policy")
	bool CanDisplayHealthBar(const AActor* Viewer, const AActor* Target) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DeathMatch|Health Bar Policy")
	bool CanRevealHealth(const AActor* Viewer, const AActor* Target) const;
};

