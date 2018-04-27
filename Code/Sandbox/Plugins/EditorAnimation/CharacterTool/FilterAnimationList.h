// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/File/IFileChangeMonitor.h>
#include "../Shared/AnimationFilter.h"

namespace CharacterTool
{

// Maintains list of files that are used by AnimationFilter users to preview their filters.
class FilterAnimationList : public IFileChangeListener
{
public:
	FilterAnimationList();
	~FilterAnimationList();

	void                         Populate();

	const SAnimationFilterItems& Animations() const { return m_items; }
	int                          Revision() const   { return m_revision; }
protected:
	void                         OnFileChange(const char* filename, EChangeType eType) override;
private:
	void                         UpdateItem(const char* filename);
	int                          RemoveItem(const char* filename);

	SAnimationFilterItems m_items;
	int                   m_revision;
};

}

