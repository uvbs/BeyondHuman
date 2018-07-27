// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include "Styling/SlateTypes.h"

class ModoBridgeStyle
{
public:

	static void Initialize();
	static void Shutdown();

	static TSharedPtr< class ISlateStyle > Get();

private:

	static FString InContent( const FString& RelativePath, const ANSICHAR* Extension );
	static TSharedPtr< class FSlateStyleSet > StyleSet;
};