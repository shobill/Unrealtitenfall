#include "DeathMatchHUD.h"

#include "DeathMatch/Presentation/Widgets/DeathMatchMatchHUDWidget.h"
#include "DeathMatch/Presentation/Widgets/DeathMatchScoreboardWidget.h"
#include "DeathMatch/Presentation/Widgets/DeathMatchRoundBannerWidget.h"
#include "DeathMatch/Presentation/Widgets/DeathMatchMatchResultWidget.h"
#include "DeathMatch/Character/DeathMatchPlayerCharacter.h"
#include "DeathMatch/Weapons/DMAimZoomComponent.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

ADeathMatchHUD::ADeathMatchHUD()
{
}

void ADeathMatchHUD::BeginPlay()
{
	Super::BeginPlay();

	CreateWidgets();
	TryBindGameState();
}

void ADeathMatchHUD::DrawHUD()
{
	Super::DrawHUD();
	DrawAICombatDebug();

	if (!Canvas)
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	DrawCombatHUDOverlay();
	DrawCrosshairHUD();
}

AGS_DeathMatch* ADeathMatchHUD::ResolveGameState() const
{
	if (AGS_DeathMatch* Cached = CachedGameState.Get())
	{
		return Cached;
	}
	if (const UWorld* World = GetWorld())
	{
		return World->GetGameState<AGS_DeathMatch>();
	}
	return nullptr;
}

void ADeathMatchHUD::DrawCombatHUDOverlay()
{
	if (!bDrawCombatHUD || !Canvas || !GEngine)
	{
		return;
	}

	if (bHideCombatHUDWhenMatchWidgetPresent && MatchHUDWidget)
	{
		return;
	}

	const AGS_DeathMatch* GS = ResolveGameState();
	if (!GS)
	{
		return;
	}

	UFont* LargeFont = GEngine->GetLargeFont();
	UFont* SmallFont = GEngine->GetSmallFont();
	if (!LargeFont || !SmallFont)
	{
		return;
	}

	const float Remaining = GS->GetRemainingTime();
	const int32 Minutes = FMath::Max(0, FMath::FloorToInt(Remaining / 60.0f));
	const int32 Seconds = FMath::Max(0, FMath::FloorToInt(FMath::Fmod(Remaining, 60.0f)));

	const int32 Team0Score = GS->GetTeamScore(0);
	const int32 Team1Score = GS->GetTeamScore(1);
	const int32 Team0Rounds = GS->GetTeamRoundScore(0);
	const int32 Team1Rounds = GS->GetTeamRoundScore(1);

	FString RoundLabel;
	switch (GS->GetRoundState())
	{
	case EDeathMatchRoundState::WaitingToStart: RoundLabel = TEXT("WAITING"); break;
	case EDeathMatchRoundState::InProgress:     RoundLabel = TEXT("LIVE"); break;
	case EDeathMatchRoundState::RoundOver:      RoundLabel = TEXT("ROUND OVER"); break;
	case EDeathMatchRoundState::MatchOver:      RoundLabel = TEXT("MATCH OVER"); break;
	default: break;
	}

	const FString TimerText = FString::Printf(TEXT("%d:%02d"), Minutes, Seconds);
	const FString ScoreText = FString::Printf(
		TEXT("T0 %d  |  %d T1    Rounds %d-%d    %s"),
		Team0Score,
		Team1Score,
		Team0Rounds,
		Team1Rounds,
		*RoundLabel);

	const FColor Color = CombatHUDTextColor.ToFColor(true);
	const float Scale = 1.5f;

	float TimerW = 0.0f;
	float TimerH = 0.0f;
	GetTextSize(TimerText, TimerW, TimerH, LargeFont, Scale);
	DrawText(TimerText, Color, (Canvas->SizeX - TimerW) * 0.5f, 18.0f, LargeFont, Scale);

	float ScoreW = 0.0f;
	float ScoreH = 0.0f;
	GetTextSize(ScoreText, ScoreW, ScoreH, SmallFont, 1.25f);
	DrawText(ScoreText, Color, (Canvas->SizeX - ScoreW) * 0.5f, 18.0f + TimerH + 8.0f, SmallFont, 1.25f);
}

void ADeathMatchHUD::DrawCrosshairHUD()
{
	if (!bDrawCrosshair || !Canvas)
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	float Gap = CrosshairGap;
	if (const ADeathMatchPlayerCharacter* DMChar = Cast<ADeathMatchPlayerCharacter>(PC->GetPawn()))
	{
		if (const UDMAimZoomComponent* AimZoom = DMChar->GetAimZoom())
		{
			if (bCrosshairOnlyWhileAiming && !AimZoom->IsAiming())
			{
				return;
			}

			const float AimAlpha = AimZoom->GetAimAlpha();
			Gap = FMath::Lerp(CrosshairGap, CrosshairGap * CrosshairAimGapScale, AimAlpha);
		}
		else if (bCrosshairOnlyWhileAiming)
		{
			return;
		}
	}
	else if (bCrosshairOnlyWhileAiming)
	{
		return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const FColor Color = CrosshairColor.ToFColor(true);

	DrawRect(Color, CX - 1.0f, CY - 1.0f, 2.0f, 2.0f);
	DrawRect(Color, CX - Gap - CrosshairSize, CY - CrosshairThickness * 0.5f, CrosshairSize, CrosshairThickness);
	DrawRect(Color, CX + Gap, CY - CrosshairThickness * 0.5f, CrosshairSize, CrosshairThickness);
	DrawRect(Color, CX - CrosshairThickness * 0.5f, CY - Gap - CrosshairSize, CrosshairThickness, CrosshairSize);
	DrawRect(Color, CX - CrosshairThickness * 0.5f, CY + Gap, CrosshairThickness, CrosshairSize);
}

void ADeathMatchHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGameState();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GameStateBindRetryHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ADeathMatchHUD::CreateWidgets()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	auto CreateAndAdd = [PC](TSubclassOf<UUserWidget> WidgetClass, int32 ZOrder) -> UUserWidget*
	{
		if (!WidgetClass)
		{
			return nullptr;
		}

		UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
		if (Widget)
		{
			Widget->AddToViewport(ZOrder);
		}
		return Widget;
	};

	MatchHUDWidget = Cast<UDeathMatchMatchHUDWidget>(CreateAndAdd(MatchHUDClass, 0));
	RoundBannerWidget = Cast<UDeathMatchRoundBannerWidget>(CreateAndAdd(RoundBannerClass, 10));
	ScoreboardWidget = Cast<UDeathMatchScoreboardWidget>(CreateAndAdd(ScoreboardClass, 20));
	MatchResultWidget = Cast<UDeathMatchMatchResultWidget>(CreateAndAdd(MatchResultClass, 30));
}

void ADeathMatchHUD::TryBindGameState()
{
	if (bBoundToGameState)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AGS_DeathMatch* GS = World->GetGameState<AGS_DeathMatch>();
	if (!GS)
	{
		World->GetTimerManager().SetTimer(
			GameStateBindRetryHandle,
			this,
			&ADeathMatchHUD::TryBindGameState,
			0.1f,
			false
		);
		return;
	}

	CachedGameState = GS;
	GS->OnRoundStateChanged.AddDynamic(this, &ADeathMatchHUD::OnRoundStateChanged);
	bBoundToGameState = true;

	ApplyRoundState(GS->GetRoundState());
}

void ADeathMatchHUD::UnbindGameState()
{
	if (!bBoundToGameState)
	{
		return;
	}

	if (AGS_DeathMatch* GS = CachedGameState.Get())
	{
		GS->OnRoundStateChanged.RemoveDynamic(this, &ADeathMatchHUD::OnRoundStateChanged);
	}

	bBoundToGameState = false;
	CachedGameState.Reset();
}

void ADeathMatchHUD::OnRoundStateChanged(EDeathMatchRoundState NewState)
{
	ApplyRoundState(NewState);
}

void ADeathMatchHUD::ApplyRoundState(EDeathMatchRoundState NewState)
{
	const bool bWaiting = (NewState == EDeathMatchRoundState::WaitingToStart);
	const bool bRoundOver = (NewState == EDeathMatchRoundState::RoundOver);
	const bool bMatchOver = (NewState == EDeathMatchRoundState::MatchOver);

	if (RoundBannerWidget)
	{
		RoundBannerWidget->SetBannerVisible(bWaiting || bRoundOver);
	}

	if (MatchResultWidget)
	{
		MatchResultWidget->SetResultVisible(bMatchOver);
	}

	if (bMatchOver)
	{
		SetScoreboardVisible(true);
	}
}

void ADeathMatchHUD::ToggleScoreboard()
{
	SetScoreboardVisible(!bScoreboardVisible);
}

void ADeathMatchHUD::SetScoreboardVisible(bool bVisible)
{
	bScoreboardVisible = bVisible;

	if (ScoreboardWidget)
	{
		ScoreboardWidget->SetScoreboardVisible(bVisible);
	}
}
