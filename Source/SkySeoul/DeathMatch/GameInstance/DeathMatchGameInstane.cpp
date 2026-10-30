// Fill out your copyright notice in the Description page of Project Settings.


#include "DeathMatchGameInstane.h"

void UDeathMatchGameInstane::SetPayloadCharacter(TSubclassOf<ACharacter> InCharacter)
{
	PayloadCharacter = InCharacter;
}

void UDeathMatchGameInstane::SetPayloadController(TSubclassOf<AController> InController)
{
	PayloadController = InController;
}

TSubclassOf<ACharacter> UDeathMatchGameInstane::GetPayloadCharacter() const
{
	if (PayloadCharacter)
	{
		return PayloadCharacter.Get();
	}
	return nullptr;
}

TSubclassOf<AController> UDeathMatchGameInstane::GetPayloadController() const
{
	if (PayloadController)
	{
		return PayloadController.Get();
	}
	return nullptr;
}
