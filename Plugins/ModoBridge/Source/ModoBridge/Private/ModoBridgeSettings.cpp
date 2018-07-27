// Copyright Foundry, 2017. All rights reserved.

#include "ModoBridgeSettings.h"
#include "EditorStyleSet.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SBox.h"

#include "ModoBridge.h"
#include "ModoBridgeHandler.h"
#include "UnrealClient.h"

#define LOCTEXT_NAMESPACE "SModoBridgeSettings"

FString gAddressText = "127.0.0.1";
FString gPortText = "12000";
ECheckBoxState gEnableServer = ECheckBoxState::Unchecked;

ECheckBoxState SModoBridgeSettings::IsServerEnabled() const
{
	auto handler = ModoBridgeHandler::activeInstance();
	if (handler->isServerEnabled())
	{
		gEnableServer = ECheckBoxState::Checked;
	}
	else
	{
		gEnableServer = ECheckBoxState::Unchecked;
	}

	return gEnableServer;
}

bool SModoBridgeSettings::SetCurrentWorld()
{
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (DesiredLevel == nullptr)
	{
		return false;
	}

	auto handler = ModoBridgeHandler::activeInstance();
	handler->setWorld(DesiredLevel->OwningWorld);

	return true;
}

FReply SModoBridgeSettings::OnPushAllClicked()
{
	SetCurrentWorld();
	auto handler = ModoBridgeHandler::activeInstance();
	handler->pushAll();

	return FReply::Handled();
}

FReply SModoBridgeSettings::OnPushSelectedClicked()
{
	SetCurrentWorld();
	auto handler = ModoBridgeHandler::activeInstance();
	handler->pushSelected();

	return FReply::Handled();
}

void SModoBridgeSettings::OnAddressTextCommitted(const FText& CommentText, ETextCommit::Type CommitInfo)
{
	gAddressText = CommentText.ToString();
}

void SModoBridgeSettings::OnPortTextCommitted(const FText& CommentText, ETextCommit::Type CommitInfo)
{
	gPortText = CommentText.ToString();
}

void SModoBridgeSettings::OnServerEnabledChanged(ECheckBoxState NewCheckboxState)
{
	SetCurrentWorld();

	if (NewCheckboxState == ECheckBoxState::Checked)
	{
		auto handler = ModoBridgeHandler::activeInstance();
		FString portText = _portTextBox->GetText().ToString();
		FString addressText = _addressTextBox->GetText().ToString();
		int32 port = std::stoi(TCHAR_TO_UTF8(*portText), nullptr);

		if (port < 0 || port > 65535)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT(LOCTEXT_NAMESPACE, "PORT_INVALID", "Port is out of range [0, 65535]"));
		}
		else
		{
			handler->enableServer(addressText, port);
		}
	}
	else
	{
		auto handler = ModoBridgeHandler::activeInstance();
		handler->disableServer();
	}

	gEnableServer = NewCheckboxState;
}

bool SModoBridgeSettings::HandleServerOptionsAreEnabled() const
{
	return IsServerEnabled() == ECheckBoxState::Unchecked;
}

bool SModoBridgeSettings::HandlePushOptionsAreEnabled() const
{
	auto peerController = PeerController::activeInstance();
	return !peerController->isPushing() && (IsServerEnabled() == ECheckBoxState::Checked);
}

void SModoBridgeSettings::Construct(const FArguments& InArgs)
{
	_parentWindow = InArgs._ParentWindow;

	// UE4 will convert the color to sRGB for dislay, 0.015^0.4545 = 0.148 is close to default BG
	static auto labelBackgroundColor(FLinearColor(0.014, 0.014, 0.014, 1.0));
	static auto labelBackgroundBrush(FEditorStyle::GetBrush("WhiteBrush"));

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		[

			SNew(SCheckBox)
			.IsChecked(this, &SModoBridgeSettings::IsServerEnabled)
			.OnCheckStateChanged(this, &SModoBridgeSettings::OnServerEnabledChanged)
			[
				SNew(STextBlock)
				.Text(FText::FromString("Connect to Server"))
			]
		
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			
			SNew(SGridPanel)
			.FillColumn(0, 2.f)
			.FillColumn(1, 1.f)

			// Address input
			+ SGridPanel::Slot(0, 0)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(_addressTextBox, SEditableTextBox)
					.Text(FText::FromString(gAddressText))
					.IsEnabled(this, &SModoBridgeSettings::HandleServerOptionsAreEnabled)
					.OnTextCommitted(this, &SModoBridgeSettings::OnAddressTextCommitted)
				]
			]

			// Address label
			+ SGridPanel::Slot(0, 1)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HeightOverride(3)
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("FilePath.GroupIndicator"))
						.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.5f))
						.Padding(FMargin(150.f, 0.f))
					]
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.Padding(5.f)
					.BorderImage(labelBackgroundBrush)
					.BorderBackgroundColor(labelBackgroundColor)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Address", "Address"))
					]
				]
			]

			// Port input
			+ SGridPanel::Slot(1, 0)
			.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
			.VAlign(VAlign_Center)
			[
				SAssignNew(_portTextBox, SEditableTextBox)
				.Text(FText::FromString(gPortText))
				.IsEnabled(this, &SModoBridgeSettings::HandleServerOptionsAreEnabled)
				.Padding(FMargin(5.f, 3.f))
				.OnTextCommitted(this, &SModoBridgeSettings::OnPortTextCommitted)
			]

			// Port label
			+ SGridPanel::Slot(1, 1)
			.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.VAlign(VAlign_Center)
				[
				SNew(SBox)
					.HeightOverride(3)
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("FilePath.GroupIndicator"))
						.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.5f))
						.Padding(FMargin(75.f, 0.f))
					]
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
					.Padding(5.f)
					.BorderImage(labelBackgroundBrush)
					.BorderBackgroundColor(labelBackgroundColor)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Port", "Port"))
					]
				]
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		[
			SNew(SGridPanel)
			+ SGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
				.ForegroundColor(FLinearColor::White)
				.ContentPadding(FMargin(6, 2))
				.OnClicked(this, &SModoBridgeSettings::OnPushAllClicked)
				.IsEnabled(this, &SModoBridgeSettings::HandlePushOptionsAreEnabled)
				.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
				.Text(LOCTEXT("PushAll", "Push All"))
			]
			+ SGridPanel::Slot(1, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Empty4", "    "))
			]
			+ SGridPanel::Slot(2, 0)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
				.ForegroundColor(FLinearColor::White)
				.ContentPadding(FMargin(6, 2))
				.IsEnabled(this, &SModoBridgeSettings::HandlePushOptionsAreEnabled)
				.OnClicked(this, &SModoBridgeSettings::OnPushSelectedClicked)
				.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
				.Text(LOCTEXT("PushSelected", "Push Selected"))
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
