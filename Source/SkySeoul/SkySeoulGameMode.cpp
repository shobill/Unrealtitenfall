// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkySeoulGameMode.h"
#include "Character/SkySeoulCharacter.h"
#include "UObject/ConstructorHelpers.h"

ASkySeoulGameMode::ASkySeoulGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
