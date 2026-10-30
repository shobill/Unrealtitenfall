#pragma once

#include "CoreMinimal.h"
#include "TDRobotMovementState.generated.h"

/** High-level state of a Titan Defence robot's single movement command. */
UENUM(BlueprintType)
enum class ETDRobotMovementState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Requesting UMETA(DisplayName = "Requesting"),
	Moving UMETA(DisplayName = "Moving"),
	Arrived UMETA(DisplayName = "Arrived"),
	Blocked UMETA(DisplayName = "Blocked"),
	Interrupted UMETA(DisplayName = "Interrupted"),
	Disabled UMETA(DisplayName = "Disabled"),
	Failed UMETA(DisplayName = "Failed"),
	WaitingForNavigation UMETA(DisplayName = "Waiting For Navigation")
};

/** Why the most recent movement command ended or could not be started. */
UENUM(BlueprintType)
enum class ETDRobotMovementEndReason : uint8
{
	None UMETA(DisplayName = "None"),
	Arrived UMETA(DisplayName = "Arrived"),
	Cancelled UMETA(DisplayName = "Cancelled"),
	InvalidTarget UMETA(DisplayName = "Invalid Target"),
	MissingController UMETA(DisplayName = "Missing Controller"),
	RequestRejected UMETA(DisplayName = "Request Rejected"),
	NoPath UMETA(DisplayName = "No Path"),
	Aborted UMETA(DisplayName = "Aborted"),
	NavigationDataNotReady UMETA(DisplayName = "Navigation Data Not Ready"),
	NavigationWaitTimedOut UMETA(DisplayName = "Navigation Wait Timed Out")
};

