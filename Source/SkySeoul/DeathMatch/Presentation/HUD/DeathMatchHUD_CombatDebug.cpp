#include "DeathMatchHUD.h"

#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "DeathMatch/AI/Combat/DMCombatDebug.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/AI/Movement/DMMovementSpeedPolicyComponent.h"
#include "DeathMatch/AI/Move/DMCombatMoveDirector.h"
#include "DeathMatch/Core/DMHealthResolver.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void ADeathMatchHUD::DrawAICombatDebug()
{
#if !UE_BUILD_SHIPPING
	const int32 Mode = DMCombatDebug::Mode();
	if (!Mode || !Canvas || !PlayerOwner || !GetWorld() || !GEngine || Mode >= 3) return;
	FVector Eye; FRotator View;
	PlayerOwner->GetPlayerViewPoint(Eye, View);
	for (TActorIterator<ADeathMatchRobotAIController> It(GetWorld()); It; ++It)
	{
		ADeathMatchRobotAIController* AI = *It;
		APawn* Pawn = AI->GetPawn();
		if (!Pawn || FVector::DistSquared(Eye, Pawn->GetActorLocation()) > FMath::Square(DMCombatDebug::Range())) continue;
		FVector2D Screen;
		const FVector Anchor = Pawn->GetActorLocation() + FVector(0, 0, 180.f);
		if (!PlayerOwner->ProjectWorldLocationToScreen(Anchor, Screen, true)) continue;
		const UDMCombatKnowledgeComponent* Knowledge = AI->GetCombatKnowledge();
		const UDMCombatFireControlComponent* Fire = AI->GetCombatFireControl();
		const UDMMovementSpeedPolicyComponent* Speed = AI->GetMovementSpeedPolicy();
		const UDMCombatMoveDirector* Move = AI->GetCombatMoveDirector();
		const FDMHealthSnapshot Health = DMHealth::Resolve(Pawn);
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("%s | HP %.0f%%"), *Pawn->GetName(), Health.bValid ? Health.HealthRatio * 100.f : 100.f));
		Lines.Add(FString::Printf(TEXT("Target: %s | Awareness: %s"),
			*GetNameSafe(Knowledge ? Knowledge->GetCombatTarget() : nullptr),
			Knowledge ? *StaticEnum<EDMThreatAwareness>()->GetNameStringByValue(static_cast<int64>(Knowledge->GetThreatAwareness())) : TEXT("None")));
		Lines.Add(FString::Printf(TEXT("Move: %s %s"), Move ? *StaticEnum<EDMMoveRequestOwner>()->GetNameStringByValue(static_cast<int64>(Move->GetActiveRequestOwner())) : TEXT("None"),
			Move ? *Move->GetActiveRequestGoal().ToCompactString() : TEXT("")));
		Lines.Add(FString::Printf(TEXT("Fire: %s | Failure: %s | Speed x%.2f"),
			Fire && Fire->IsFireActive() ? TEXT("Active") : TEXT("Idle"),
			Fire ? DMFireAttempt::Name(Fire->GetFireFailure()) : TEXT("Missing"), Speed ? Speed->GetAppliedMultiplier() : 1.f));
		const EDMClearShotFailure ClearShotFailure = Fire && Fire->HasClearShotObservation()
			? Fire->GetLastClearShotFailure() : EDMClearShotFailure::InvalidTarget;
		Lines.Add(FString::Printf(TEXT("ClearShot=%s | Failure=%s"),
			ClearShotFailure == EDMClearShotFailure::None ? TEXT("true") : TEXT("false"),
			Fire && Fire->HasClearShotObservation()
				? UDMCombatFireControlComponent::GetClearShotFailureName(ClearShotFailure) : TEXT("InvalidTarget")));
		float Width = 0.f, LineHeight = 12.f;
		for (const FString& Line : Lines) { float W, H; GetTextSize(Line, W, H, GEngine->GetSmallFont()); Width = FMath::Max(Width, W); LineHeight = FMath::Max(LineHeight, H); }
		const float X = FMath::Clamp(Screen.X - Width * .5f, 0.f, FMath::Max(0.f, Canvas->SizeX - Width - 8.f));
		const float Y = FMath::Clamp(Screen.Y - Lines.Num() * LineHeight - 8.f, 0.f, FMath::Max(0.f, Canvas->SizeY - Lines.Num() * LineHeight - 8.f));
		DrawRect(FLinearColor(0, 0, 0, .75f), X, Y, Width + 8.f, Lines.Num() * LineHeight + 8.f);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
			DrawText(Lines[Index], FLinearColor::White, X + 4.f, Y + 4.f + Index * LineHeight, GEngine->GetSmallFont());
	}
#endif
}
