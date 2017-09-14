// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/IConsole.h>

#if defined(ENABLE_AUDIO_LOGGING)
	#include <CrySystem/ISystem.h>
#endif // ENABLE_AUDIO_LOGGING

namespace CryAudio
{
enum class ELogType : EnumFlagsType
{
	None,
	Comment,
	Warning,
	Error,
	Always,
};

enum class ELoggingOptions : EnumFlagsType
{
	None,
	Errors   = BIT(6), // a
	Warnings = BIT(7), // b
	Comments = BIT(8), // c
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ELoggingOptions);

/**
 * A simpler logger wrapper that adds and audio tag and a timestamp
 */
class CLogger final
{
public:

	/** @cond */
	CLogger() = default;
	CLogger(CLogger const&) = delete;
	CLogger& operator=(CLogger const&) = delete;
	/** @endcond */

	/**
	 * Log a message
	 * @param type        - log message type (eALT_COMMENT, eALT_WARNING, eALT_ERROR or eALT_ALWAYS)
	 * @param szFormat, ... - printf-style format string and its arguments
	 */
	static void Log(ELogType const type, char const* const szFormat, ...)
	{
#if defined(ENABLE_AUDIO_LOGGING)
		if (szFormat && szFormat[0] && gEnv->pLog->GetVerbosityLevel() != -1)
		{
			CRY_PROFILE_REGION(PROFILE_AUDIO, "CLogger::Log");

			char sBuffer[256];
			va_list ArgList;
			va_start(ArgList, szFormat);
			cry_vsprintf(sBuffer, szFormat, ArgList);
			va_end(ArgList);

			float const currentTime = gEnv->pTimer->GetAsyncCurTime();
			ICVar* const pCVar = gEnv->pConsole->GetCVar("s_AudioLoggingOptions");

			if (pCVar != nullptr)
			{
				ELoggingOptions const loggingOptions = static_cast<ELoggingOptions>(pCVar->GetIVal());

				switch (type)
				{
				case ELogType::Warning:
					{
						if ((loggingOptions& ELoggingOptions::Warnings) > 0)
						{
							gEnv->pSystem->Warning(VALIDATOR_MODULE_AUDIO, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, nullptr, "<Audio> <%.3f>: %s", currentTime, sBuffer);
						}

						break;
					}
				case ELogType::Error:
					{
						if ((loggingOptions& ELoggingOptions::Errors) > 0)
						{
							gEnv->pSystem->Warning(VALIDATOR_MODULE_AUDIO, VALIDATOR_ERROR, VALIDATOR_FLAG_AUDIO, nullptr, "<Audio> <%.3f>: %s", currentTime, sBuffer);
						}

						break;
					}
				case ELogType::Comment:
					{
						if ((gEnv->pLog != nullptr) && (gEnv->pLog->GetVerbosityLevel() >= 4) && ((loggingOptions& ELoggingOptions::Comments) > 0))
						{
							CryLogAlways("<Audio> <%.3f>: %s", currentTime, sBuffer);
						}

						break;
					}
				case ELogType::Always:
					{
						CryLogAlways("<Audio> <%.3f>: %s", currentTime, sBuffer);

						break;
					}
				default:
					{
						CRY_ASSERT(false);

						break;
					}
				}
			}
		}
#endif // ENABLE_AUDIO_LOGGING
	}
};
} // namespace CryAudio
