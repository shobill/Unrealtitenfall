#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeathMatch/Core/DMHealthTypes.h"
#include "DMHealthBarWidget.generated.h"

class AActor;
class UProgressBar;
class UTextBlock;

/** C++ default health bar. Blueprint children may replace the visual tree and override the update event. */
UCLASS(Blueprintable)
class SKYSEOUL_API UDMHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Health Bar")
	void SetHealthDisplay(AActor* InTarget, const FDMHealthSnapshot& InSnapshot, const FDMHealthBarDisplayOptions& InOptions);

	UFUNCTION(BlueprintImplementableEvent, Category = "DeathMatch|Health Bar", meta = (DisplayName = "On Health Display Updated"))
	void BP_OnHealthDisplayUpdated(AActor* InTarget, const FDMHealthSnapshot& InSnapshot, const FDMHealthBarDisplayOptions& InOptions);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Health Bar")
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Health Bar")
	FDMHealthSnapshot HealthSnapshot;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Health Bar")
	FDMHealthBarDisplayOptions DisplayOptions;

private:
	void BuildDefaultWidgetTree();
	void RefreshDefaultWidgets();

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActorNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NumericHealthText;
};

