// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TASKLIST_H__
#define __TASKLIST_H__

#include "IUIComponent.h"
#include <map>
#include <vector>
#include <string>

class TaskList : public IUIComponent
{
public:
	TaskList();

	void AddTask(const std::string& id, const std::string& description);
	void SetCurrentTask(const std::string& id);

	// IUIComponent
	virtual void CreateUI(void* window, int left, int top, int width, int height);
	virtual void Resize(void* window, int left, int top, int width, int height);
	virtual void DestroyUI(void* window);
	virtual void GetExtremeDimensions(void* window, int& minWidth, int& maxWidth, int& minHeight, int& maxHeight);

private:
	void SetColor();
	void SetText(const std::string& highlightedTask);

	std::map<std::string, int> m_idTaskMap;
	std::vector<std::pair<std::string, std::string> > m_tasks;
	void* m_edit;
};

#endif //__TASKLIST_H__
