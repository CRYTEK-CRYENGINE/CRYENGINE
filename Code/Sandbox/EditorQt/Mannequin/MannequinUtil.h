// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once


namespace MannequinDragDropHelpers
{
	inline UINT GetAnimationNameClipboardFormat()
	{
		static UINT nAnimationNameClipboardFormat = RegisterClipboardFormat("AnimationBrowserCopy");
		return nAnimationNameClipboardFormat;
	}

	inline UINT GetFragmentClipboardFormat()
	{
		static UINT nFragmentClipboardFormat = RegisterClipboardFormat("PreviewFragmentProperties");
		return nFragmentClipboardFormat;
	}
}
