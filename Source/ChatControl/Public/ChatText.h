#pragma once

#include "CoreMinimal.h"

/**
 * Two-language texts (English / Russian).
 * Language follows the game unless overridden in the settings ("auto", "en" or "ru").
 */
namespace ChatText
{
	CHATCONTROL_API void SetLanguage(const FString& Language);
	CHATCONTROL_API bool IsRussian();
	CHATCONTROL_API FText Tr(const TCHAR* English, const TCHAR* Russian);
}
