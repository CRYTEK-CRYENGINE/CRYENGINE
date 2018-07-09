// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#ifndef __HUD_CONTROLLER_INPUT_ICONS_H__
#define __HUD_CONTROLLER_INPUT_ICONS_H__

enum eControllerInputTypeVisualization
{
	kCITV_none,
	kCITV_icon,
	kCITV_text
};

struct CControllerInputRenderInfo
{
	public:
	CControllerInputRenderInfo()
	{
		Clear();
	}

	void Clear();
	bool SetIcon(const char * text);
	bool SetText(const char * text);
	bool CreateForInput(const char * mapName, const char * inputName);
	void operator=(const CControllerInputRenderInfo & fromThis);
	
	ILINE eControllerInputTypeVisualization			GetType() const { return m_type; }
	ILINE const char *													GetText() const { return m_text; }
	ILINE const ITexture *											GetTexture() const { return m_texture; }

	private:
	eControllerInputTypeVisualization			m_type;
	char																	m_text[32];
	ITexture *                            m_texture;

	static ITexture* GetTexture(const char* name);
};

#endif	// __HUD_CONTROLLER_INPUT_ICONS_H__