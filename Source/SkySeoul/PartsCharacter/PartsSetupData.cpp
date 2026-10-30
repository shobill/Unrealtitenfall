#include "PartsSetupData.h"

namespace PartsSetupDataPrivate
{
	static FGameplayTag Tag(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	}

	static FPartsDefinition MakeRoot(
		const TCHAR* Part,
		float MaxHP,
		FName FollowBone,
		EPartsDestroyArmEffect ArmEffect,
		bool bImpairHead)
	{
		FPartsDefinition Def;
		Def.PartTag = Tag(Part);
		Def.MaxHP = MaxHP;
		Def.FollowBoneName = FollowBone;
		Def.HitBoneAliases.Add(FollowBone);
		Def.HideBonesOnAttach.Add(FollowBone);
		Def.AttachRelativeTransform = FTransform::Identity;
		Def.bAttachTransformIsBoneLocal = false;
		Def.ArmEffectOnDestroy = ArmEffect;
		Def.bImpairHeadOnDestroy = bImpairHead;
		Def.bSeparable = true;
		return Def;
	}

	static FPartsDefinition MakeChild(
		const TCHAR* Part,
		const TCHAR* Parent,
		float MaxHP,
		FName HitBone,
		EPartsDestroyArmEffect ArmEffect)
	{
		FPartsDefinition Def;
		Def.PartTag = Tag(Part);
		Def.ParentTag = Tag(Parent);
		Def.MaxHP = MaxHP;
		Def.FollowBoneName = HitBone; // attach + hit bone on owner skeleton (ParentTag = cascade only)
		Def.HitBoneAliases.Add(HitBone);
		Def.HideBonesOnAttach.Add(HitBone);
		Def.AttachRelativeTransform = FTransform::Identity;
		Def.bAttachTransformIsBoneLocal = false;
		Def.ArmEffectOnDestroy = ArmEffect;
		Def.bImpairHeadOnDestroy = false;
		Def.bSeparable = true;
		return Def;
	}
}

void UPartsSetupData::FillDefaultDeathMatchParts(TArray<FPartsDefinition>& OutParts)
{
	using namespace PartsSetupDataPrivate;

	OutParts.Reset();
	OutParts.Add(MakeRoot(TEXT("Parts.Head"), 40.f, TEXT("head"), EPartsDestroyArmEffect::None, true));
	OutParts.Add(MakeRoot(TEXT("Parts.Arm.L.Upper"), 45.f, TEXT("upperarm_l"), EPartsDestroyArmEffect::DisableLeft, false));
	OutParts.Add(MakeChild(TEXT("Parts.Arm.L.Lower"), TEXT("Parts.Arm.L.Upper"), 35.f, TEXT("lowerarm_l"), EPartsDestroyArmEffect::WeakenLeft));
	{
		FPartsDefinition Hand = MakeChild(TEXT("Parts.Arm.L.Hand"), TEXT("Parts.Arm.L.Lower"), 25.f, TEXT("hand_l"), EPartsDestroyArmEffect::WeakenLeft);
		// Do not hide hand_l on attach — HideBone would also kill finger bones. Hand SM covers palm; SK fingers stay.
		Hand.HideBonesOnAttach.Reset();
		Hand.bHideDescendantBonesOnDestroy = true;
		Hand.HideTreeRootBone = FName(TEXT("hand_l"));
		OutParts.Add(Hand);
	}
	OutParts.Add(MakeRoot(TEXT("Parts.Arm.R.Upper"), 45.f, TEXT("upperarm_r"), EPartsDestroyArmEffect::DisableRight, false));
	OutParts.Add(MakeChild(TEXT("Parts.Arm.R.Lower"), TEXT("Parts.Arm.R.Upper"), 35.f, TEXT("lowerarm_r"), EPartsDestroyArmEffect::WeakenRight));
	{
		FPartsDefinition Hand = MakeChild(TEXT("Parts.Arm.R.Hand"), TEXT("Parts.Arm.R.Lower"), 25.f, TEXT("hand_r"), EPartsDestroyArmEffect::WeakenRight);
		Hand.HideBonesOnAttach.Reset();
		Hand.bHideDescendantBonesOnDestroy = true;
		Hand.HideTreeRootBone = FName(TEXT("hand_r"));
		OutParts.Add(Hand);
	}
}
