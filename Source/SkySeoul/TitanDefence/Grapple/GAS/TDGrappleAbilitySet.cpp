#include "TitanDefence/Grapple/GAS/TDGrappleAbilitySet.h"
#include "TitanDefence/Grapple/GAS/TDGameplayAbility_Grapple.h"
#include "TitanDefence/Grapple/GAS/TDGrappleTags.h"

UTDGrappleAbilitySet::UTDGrappleAbilitySet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FAbilitySetData_GameplayAbility Entry;
	Entry.Ability = UTDGameplayAbility_Grapple::StaticClass();
	Entry.InputTag = TDGrappleTags::Input_Grapple;
	Entry.AbilityName = TEXT("Terrain Grapple");
	DefaultGrantedGameplayAbilities.Add(Entry);
}
