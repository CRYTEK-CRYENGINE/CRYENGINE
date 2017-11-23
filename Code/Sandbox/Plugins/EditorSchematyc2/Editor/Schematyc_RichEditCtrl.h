// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc2
{
	class CCustomRichEditCtrl : public CRichEditCtrl
	{
	public:

		virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);

		void SetText(const char* text);
		void AppendText(const char* text);
	};
}
