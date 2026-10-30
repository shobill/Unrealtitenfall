#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "DMCombatEQSContexts.generated.h"

UCLASS(meta=(DisplayName="DM Focusing Target"))
class SKYSEOUL_API UDMEnvQueryContext_FocusingTarget : public UEnvQueryContext
{
    GENERATED_BODY()
public: virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UCLASS(meta=(DisplayName="DM Visible Enemies"))
class SKYSEOUL_API UDMEnvQueryContext_VisibleEnemies : public UEnvQueryContext
{
    GENERATED_BODY()
public: virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UCLASS(meta=(DisplayName="DM Remembered Enemy Locations"))
class SKYSEOUL_API UDMEnvQueryContext_RememberedEnemyLocations : public UEnvQueryContext
{
    GENERATED_BODY()
public: virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UCLASS(meta=(DisplayName="DM Last Known Threat Location"))
class SKYSEOUL_API UDMEnvQueryContext_LastKnownThreatLocation : public UEnvQueryContext
{
    GENERATED_BODY()
public: virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UCLASS(meta=(DisplayName="DM Damage Origin"))
class SKYSEOUL_API UDMEnvQueryContext_DamageOrigin : public UEnvQueryContext
{
    GENERATED_BODY()
public: virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UCLASS(meta=(DisplayName="DM Friendly Actors"))
class SKYSEOUL_API UDMEnvQueryContext_FriendlyActors : public UEnvQueryContext
{
    GENERATED_BODY()
public: virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
