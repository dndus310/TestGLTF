// Attribute Sample - WBP_AttributeList: VerticalBox list + AddAttribute button

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AttributeSampleTypes.h"
#include "AttributeListWidget.generated.h"

class UButton;
class UVerticalBox;
class UAttributeListWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAttributeItemEvent, UAttributeAssetItem* /*Item*/);

/** Binds one list button to its item (UButton::OnClicked has no parameters). */
UCLASS()
class UAttributeListEntryProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UAttributeAssetItem> Item;

	TWeakObjectPtr<UAttributeListWidget> Owner;

	UFUNCTION()
	void HandleClicked();
};

UCLASS()
class DWO_PROJ_01_API UAttributeListWidget : public UUserWidget
{
	GENERATED_BODY()

	/** Editor tool that writes BuildDefaultLayout() into the Widget Blueprint designer tree. */
	friend class FAttributeWidgetDesignerExporter;

public:
	UFUNCTION(BlueprintCallable, Category = "Attribute|UI")
	void SetItems(const TArray<UAttributeAssetItem*>& Items);

	void HandleEntryClicked(UAttributeAssetItem* Item);

	/** Item click -> SpawnAttribute */
	FOnAttributeItemEvent OnItemClicked;

	/** AddAttribute button */
	FSimpleMulticastDelegate OnAddAttributeClicked;

protected:
	virtual void NativeOnInitialized() override;

	void BuildDefaultLayout();

	UFUNCTION()
	void HandleAddAttributeClicked();

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> List;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> AddAttribute;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAttributeListEntryProxy>> EntryProxies;
};
