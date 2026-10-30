#pragma once
#include "CoreMinimal.h"
#include "DeathMatch/Core/DeathMatchLog.h"
#include "Engine/World.h"

enum class EDMFireAttemptFailure : uint8
{
    None, Context, Movement, TargetChanged, NotSensed, CannotShoot, Sight,
    Range, AimSettle, Cadence, Probability, MoveSpeed, Facing, AimZoom,
    InvalidTarget, InvalidAvatar, Parts, ExecuteFire, Commit, Montage, NoNotifyReceived,
    MissingWeapon, TargetDead, AvatarDead, FriendlyFireRisk, WeaponChanged, MissingFireCycleNotify, Cancelled
};

namespace DMFireAttempt
{
    inline uint64 NextId() { static uint64 Id = 0; return ++Id; }
    inline const TCHAR* Name(EDMFireAttemptFailure Reason)
    {
        switch (Reason)
        {
#define DM_REASON(Value) case EDMFireAttemptFailure::Value: return TEXT(#Value);
            DM_REASON(None) DM_REASON(Context) DM_REASON(Movement) DM_REASON(TargetChanged)
            DM_REASON(NotSensed) DM_REASON(CannotShoot) DM_REASON(Sight) DM_REASON(Range)
            DM_REASON(AimSettle) DM_REASON(Cadence) DM_REASON(Probability) DM_REASON(MoveSpeed)
            DM_REASON(Facing) DM_REASON(AimZoom) DM_REASON(InvalidTarget) DM_REASON(InvalidAvatar)
            DM_REASON(Parts) DM_REASON(ExecuteFire) DM_REASON(Commit) DM_REASON(Montage) DM_REASON(NoNotifyReceived)
            DM_REASON(MissingWeapon) DM_REASON(TargetDead) DM_REASON(AvatarDead) DM_REASON(FriendlyFireRisk) DM_REASON(WeaponChanged) DM_REASON(MissingFireCycleNotify) DM_REASON(Cancelled)
#undef DM_REASON
        }
        return TEXT("Unknown");
    }
    inline void Log(const UObject* Owner, uint64 Id, const TCHAR* Event, const FString& Detail = FString())
    {
        UE_CLOG(DMIsCombatDiagEnabled(), LogDM, Log, TEXT("DM FireAttempt id=%llu actor=%s t=%.3f event=%s %s"),
            Id, *GetNameSafe(Owner), Owner && Owner->GetWorld() ? Owner->GetWorld()->GetTimeSeconds() : 0., Event, *Detail);
    }
}

// Guard at the call site so Shipping/disabled diagnostics never format arguments.
#if UE_BUILD_SHIPPING
#define DM_FIRE_LOG(...) do {} while (false)
#else
#define DM_FIRE_LOG(...) do { if (DMIsCombatDiagEnabled()) { DMFireAttempt::Log(__VA_ARGS__); } } while (false)
#endif
