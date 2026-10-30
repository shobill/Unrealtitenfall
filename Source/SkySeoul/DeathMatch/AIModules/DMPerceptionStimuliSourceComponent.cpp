#include "DMPerceptionStimuliSourceComponent.h"

#include "Perception/AISense_Sight.h"

UDMPerceptionStimuliSourceComponent::UDMPerceptionStimuliSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegisterAsSource = true;
	RegisterAsSourceForSenses.Add(UAISense_Sight::StaticClass());
}
