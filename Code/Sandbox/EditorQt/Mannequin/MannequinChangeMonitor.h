// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MannequinChangeMonitor_h__
#define __MannequinChangeMonitor_h__

#include <CrySystem/File/IFileChangeMonitor.h>

class CMannequinChangeMonitor : public IFileChangeListener
{
public:
	CMannequinChangeMonitor();
	~CMannequinChangeMonitor();

	virtual void OnFileChange(const char* sFilename, EChangeType eType) override;

	class CMannequinFileChangeWriter* m_pFileChangeWriter;

};

#endif //__MannequinChangeMonitor_h__

