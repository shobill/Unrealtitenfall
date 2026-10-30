#pragma once

#include "CoreMinimal.h"
#include "Character/CharacterBase.h"
#include "Character/Abilities/Attributes/CharacterAttributeSetBase.h"
#include "DMHealthBarTestTypes.generated.h"

UCLASS()
class ADMHealthResolverTestCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	ADMHealthResolverTestCharacter(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
	}

	void SetTestAttributeSet(UCharacterAttributeSetBase* InAttributeSet)
	{
		AttributeSetBase = InAttributeSet;
	}
};

