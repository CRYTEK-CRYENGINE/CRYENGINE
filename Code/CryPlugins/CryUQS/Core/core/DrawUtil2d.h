// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CDrawUtil2d
		//
		//===================================================================================

		class CDrawUtil2d
		{
		public:
			static void           DrawLabel(int row, const ColorF& color, const char* szFormat, ...);
			static void           DrawLabel(float xPos, int row, const ColorF& color, const char* szFormat, ...);
			static float          GetRowSize();
			static float          GetIndentSize();

		private:
			static void           DoDrawLabel(float xPos, float yPos, const ColorF& color, const char* szFormat, va_list args);

		private:
			static const float    s_rowSize;
			static const float    s_fontSize;
			static const float    s_indentSize;
		};

	}
}
