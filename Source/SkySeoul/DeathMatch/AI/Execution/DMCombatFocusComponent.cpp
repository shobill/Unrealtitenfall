#include "DMCombatFocusComponent.h"

#include "AIController.h"

UDMCombatFocusComponent::UDMCombatFocusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

uint64 UDMCombatFocusComponent::BeginActorFocus(AActor* Actor)
{
	const uint64 Id = NextRequestId++;
	FFocusRequest& Request = FocusRequests.Add(Id);
	Request.Actor = Actor;
	Request.bActor = true;
	Request.Serial = NextSerial++;
	ApplyWinningFocus();
	return Id;
}

uint64 UDMCombatFocusComponent::BeginLocationFocus(const FVector& Location)
{
	const uint64 Id = NextRequestId++;
	FFocusRequest& Request = FocusRequests.Add(Id);
	Request.Location = Location;
	Request.Serial = NextSerial++;
	ApplyWinningFocus();
	return Id;
}

void UDMCombatFocusComponent::UpdateActorFocus(uint64 RequestId, AActor* Actor)
{
	if (FFocusRequest* Request = FocusRequests.Find(RequestId))
	{
		Request->Actor = Actor;
		Request->bActor = true;
		ApplyWinningFocus();
	}
}

void UDMCombatFocusComponent::UpdateLocationFocus(uint64 RequestId, const FVector& Location)
{
	if (FFocusRequest* Request = FocusRequests.Find(RequestId))
	{
		Request->Actor.Reset();
		Request->Location = Location;
		Request->bActor = false;
		ApplyWinningFocus();
	}
}

void UDMCombatFocusComponent::EndFocusRequest(uint64 RequestId)
{
	if (RequestId != 0 && FocusRequests.Remove(RequestId) > 0)
	{
		ApplyWinningFocus();
	}
}

void UDMCombatFocusComponent::ResetFocusRequests()
{
	FocusRequests.Reset();
	if (AAIController* AI = Cast<AAIController>(GetOwner()))
	{
		AI->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

void UDMCombatFocusComponent::ApplyWinningFocus()
{
	AAIController* AI = Cast<AAIController>(GetOwner());
	if (!AI)
	{
		return;
	}
	const FFocusRequest* Winner = nullptr;
	for (const TPair<uint64, FFocusRequest>& Pair : FocusRequests)
	{
		if (!Winner || Pair.Value.Serial > Winner->Serial)
		{
			Winner = &Pair.Value;
		}
	}
	AI->ClearFocus(EAIFocusPriority::Gameplay);
	if (!Winner)
	{
		return;
	}
	if (Winner->bActor && Winner->Actor.IsValid())
	{
		AI->SetFocus(Winner->Actor.Get(), EAIFocusPriority::Gameplay);
	}
	else if (!Winner->bActor)
	{
		AI->SetFocalPoint(Winner->Location, EAIFocusPriority::Gameplay);
	}
}
