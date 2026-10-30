#include "ChatText.h"
#include "Internationalization/Culture.h"

namespace
{
	FString LanguageOverride = TEXT("auto");
}

void ChatText::SetLanguage(const FString& Language)
{
	LanguageOverride = Language.TrimStartAndEnd().ToLower();
}

bool ChatText::IsRussian()
{
	if (LanguageOverride == TEXT("ru"))
	{
		return true;
	}
	if (LanguageOverride == TEXT("en"))
	{
		return false;
	}
	return FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName() == TEXT("ru");
}

FText ChatText::Tr(const TCHAR* English, const TCHAR* Russian)
{
	return FText::FromString(IsRussian() ? Russian : English);
}
