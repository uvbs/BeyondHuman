// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include "Styling/SlateTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"

class SModoBridgeSettings : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SModoBridgeSettings) {}
 
	/*See private declaration of OwnerHUD below.*/
	SLATE_ARGUMENT(TSharedPtr<class SWindow>, ParentWindow)
 
	SLATE_END_ARGS()
 
public:
	////////////////////////////////////////////////////////////////////////////////////////////////////
	/////Needed for every widget
	/////Builds this widget and any of it's children
	void Construct(const FArguments& InArgs);
 
private:
	FReply OnPushAllClicked();
	FReply OnPushSelectedClicked();

	void OnAddressTextCommitted(const FText& CommentText, ETextCommit::Type CommitInfo);
	void OnPortTextCommitted(const FText& CommentText, ETextCommit::Type CommitInfo);

	ECheckBoxState IsServerEnabled() const;
	void OnServerEnabledChanged(ECheckBoxState NewCheckboxState);
	bool HandleServerOptionsAreEnabled() const;
	bool HandlePushOptionsAreEnabled() const;

	bool SetCurrentWorld();
	////////////////////////////////////////////////////////////////////////////////////////////////////
	/////Pointer to our parent HUD
	/////To make sure HUD's lifetime is controlled elsewhere, use "weak" ptr.
	/////HUD has "strong" pointer to Widget,
	/////circular ownership would prevent/break self-destruction of hud/widget (cause memory leak).
	TSharedPtr<class SWindow> _parentWindow;
	TSharedPtr<class SEditableTextBox> _addressTextBox;
	TSharedPtr<class SEditableTextBox> _portTextBox;
};
