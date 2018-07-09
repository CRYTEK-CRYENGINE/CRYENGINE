#pragma once

#include "IPlatformAchievement.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CAchivement
				: public IAchievement
			{
			public:
				CAchivement(const char* name, bool bAchieved);
				virtual ~CAchivement() = default;

				// IAchievement
				virtual const char* GetName() const override { return m_name.c_str(); }
				virtual bool IsCompleted() const override { return m_bAchieved; }

				virtual bool Reset() override;
				virtual bool Achieve() override;
				// ~IAchievement

			protected:
				string m_name;
				bool m_bAchieved;
			};
		}
	}
}