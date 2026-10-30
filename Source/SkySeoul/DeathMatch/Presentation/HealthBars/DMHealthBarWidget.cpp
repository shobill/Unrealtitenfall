#include "DMHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	FLinearColor ResolveHealthColor(const FDMHealthSnapshot& Snapshot)
	{
		if (!Snapshot.bValid)
		{
			return FLinearColor(0.35f, 0.35f, 0.35f, 1.0f);
		}
		if (Snapshot.HealthRatio >= 0.60f)
		{
			return FLinearColor(0.08f, 0.80f, 0.18f, 1.0f);
		}
		if (Snapshot.HealthRatio >= 0.30f)
		{
			return FLinearColor(0.95f, 0.78f, 0.05f, 1.0f);
		}
		return FLinearColor(0.90f, 0.08f, 0.05f, 1.0f);
	}

	const TCHAR* HealthSourceName(EDMHealthSource Source)
	{
		switch (Source)
		{
		case EDMHealthSource::BodyHP: return TEXT("BodyHP");
		case EDMHealthSource::GAS: return TEXT("GAS");
		case EDMHealthSource::PublicData: return TEXT("Public");
		default: return TEXT("Invalid");
		}
	}
}

void UDMHealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultWidgetTree();
	RefreshDefaultWidgets();
}

void UDMHealthBarWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* RootColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HealthBarRoot"));
	WidgetTree->RootWidget = RootColumn;

	ActorNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActorNameText"));
	ActorNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ActorNameText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	ActorNameText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* NameSlot = RootColumn->AddChildToVerticalBox(ActorNameText))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		NameSlot->SetHorizontalAlignment(HAlign_Fill);
		NameSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	UOverlay* BarOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HealthBarLane"));
	if (UVerticalBoxSlot* BarSlot = RootColumn->AddChildToVerticalBox(BarOverlay))
	{
		BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BarSlot->SetHorizontalAlignment(HAlign_Fill);
		BarSlot->SetVerticalAlignment(VAlign_Fill);
	}

	HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
	HealthProgressBar->SetPercent(1.0f);
	HealthProgressBar->SetFillColorAndOpacity(ResolveHealthColor(FDMHealthSnapshot()));
	if (UOverlaySlot* ProgressSlot = BarOverlay->AddChildToOverlay(HealthProgressBar))
	{
		ProgressSlot->SetHorizontalAlignment(HAlign_Fill);
		ProgressSlot->SetVerticalAlignment(VAlign_Fill);
	}

	NumericHealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NumericHealthText"));
	NumericHealthText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NumericHealthText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	NumericHealthText->SetJustification(ETextJustify::Right);
	if (UOverlaySlot* NumericSlot = BarOverlay->AddChildToOverlay(NumericHealthText))
	{
		NumericSlot->SetHorizontalAlignment(HAlign_Fill);
		NumericSlot->SetVerticalAlignment(VAlign_Center);
		NumericSlot->SetPadding(FMargin(4.0f, 0.0f));
	}
}

void UDMHealthBarWidget::SetHealthDisplay(AActor* InTarget, const FDMHealthSnapshot& InSnapshot, const FDMHealthBarDisplayOptions& InOptions)
{
	TargetActor = InTarget;
	HealthSnapshot = InSnapshot;
	DisplayOptions = InOptions;
	RefreshDefaultWidgets();
	BP_OnHealthDisplayUpdated(InTarget, InSnapshot, InOptions);
}

void UDMHealthBarWidget::RefreshDefaultWidgets()
{
	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(HealthSnapshot.bValid ? HealthSnapshot.HealthRatio : 1.0f);
		HealthProgressBar->SetFillColorAndOpacity(ResolveHealthColor(HealthSnapshot));
	}

	if (ActorNameText)
	{
		FString Label;
		if (DisplayOptions.bShowActorName && TargetActor.IsValid())
		{
			Label = TargetActor->GetActorNameOrLabel();
		}
#if !UE_BUILD_SHIPPING
		if (DisplayOptions.bShowHealthSource)
		{
			Label += FString::Printf(TEXT(" [%s]"), HealthSourceName(HealthSnapshot.HealthSource));
		}
#endif
		ActorNameText->SetText(FText::FromString(Label));
		ActorNameText->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (NumericHealthText)
	{
		const FString Numeric = DisplayOptions.bShowNumericHealth && HealthSnapshot.bValid
			? FString::Printf(TEXT("%.0f/%.0f"), HealthSnapshot.CurrentHealth, HealthSnapshot.MaxHealth)
			: FString();
		NumericHealthText->SetText(FText::FromString(Numeric));
		NumericHealthText->SetVisibility(Numeric.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

