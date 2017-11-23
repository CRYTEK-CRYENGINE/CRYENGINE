// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ImplControls.h"

#include <CrySystem/XML/IXml.h>
#include <SystemTypes.h>

namespace ACE
{
namespace Fmod
{
class CProjectLoader final
{
public:

	CProjectLoader(string const& projectPath, string const& soundbanksPath, CImplItem& root);

private:

	CImplItem* CreateItem(EImpltemType const type, CImplItem* const pParent, string const& name);
	CID        GetId(EImpltemType const type, string const& name, CImplItem* const pParent) const;
	CImplItem* GetControl(CID const id) const;
	string     GetPathName(CImplItem const* const pImplItem) const;
	string     GetTypeName(EImpltemType const type) const;

	void       LoadBanks(string const& folderPath);
	void       ParseFolder(string const& folderPath);
	void       ParseFile(string const& filepath);

	CImplItem* GetContainer(string const& id, EImpltemType const type);
	CImplItem* LoadContainer(XmlNodeRef const pNode, EImpltemType const type, string const& relationshipParamName);
	CImplItem* LoadFolder(XmlNodeRef const pNode);
	CImplItem* LoadGroup(XmlNodeRef const pNode);

	CImplItem* LoadItem(XmlNodeRef const pNode, EImpltemType const type);
	CImplItem* LoadEvent(XmlNodeRef const pNode);
	CImplItem* LoadSnapshot(XmlNodeRef const pNode);
	CImplItem* LoadReturn(XmlNodeRef const pNode);
	CImplItem* LoadParameter(XmlNodeRef const pNode, EImpltemType const type, CImplItem& parentEvent);

	typedef std::map<CID, CImplItem*> ControlsCache;

	CImplItem&                   m_root;
	ControlsCache                m_controlsCache;
	std::map<string, CImplItem*> m_containerIdMap;
	string                       m_projectPath;
};
} // namespace Fmod
} // namespace ACE
