#pragma once

struct TreeConfig
{
	TreeConfig();

	static TreeConfig defaultConfig;

	bool fullRowContainers;
	bool immediateUpdate;
	bool showContainerIndices;
	bool filterWhenType;
	float valueColumnWidth;
	int filter;
	int tabSize;
	int sliderUpdateDelay;
	int defaultRowHeight;

	int expandLevels;
	bool undoEnabled;
	bool fullUndo;
	bool multiSelection;
	// Whether we'll allow actions / buttons in the tree to be used
	bool enableActions;
};

