// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>
#include <IAudioConnection.h>
#include <ACETypes.h>
#include <CrySystem/XML/IXml.h>
#include "ACETypes.h"
#include <CrySerialization/Forward.h>

namespace ACE
{

class CATLControlsModel;
class IAudioSystemItem;

struct SRawConnectionData
{
	SRawConnectionData(XmlNodeRef node, bool bIsValid)
		: xmlNode(node)
		, bValid(bIsValid) {}

	XmlNodeRef xmlNode;
	bool       bValid; // indicates if the connection is valid for the currently loaded middle-ware
};

typedef std::vector<SRawConnectionData> XMLNodeList;

class CATLControl
{
	friend class CAudioControlsLoader;
	friend class CAudioControlsWriter;
	friend class CUndoControlModified;

public:
	CATLControl();
	CATLControl(const string& controlName, CID nID, EACEControlType eType, CATLControlsModel* pModel);
	~CATLControl();

	CID             GetId() const;

	EACEControlType GetType() const;

	string          GetName() const;
	void            SetName(const string& name);

	Scope           GetScope() const;
	void            SetScope(Scope scope);

	bool            IsAutoLoad() const;
	void            SetAutoLoad(bool bAutoLoad);

	CATLControl*    GetParent() const { return m_pParent; }
	void            SetParent(CATLControl* pParent);

	size_t          ChildCount() const                      { return m_children.size(); }
	CATLControl*    GetChild(uint index) const              { return m_children[index]; }
	void            AddChild(CATLControl* pChildControl)    { m_children.push_back(pChildControl); }
	void            RemoveChild(CATLControl* pChildControl) { m_children.erase(std::remove(m_children.begin(), m_children.end(), pChildControl), m_children.end()); }

	size_t          GetConnectionCount();
	void            AddConnection(ConnectionPtr pConnection);
	void            RemoveConnection(ConnectionPtr pConnection);
	void            RemoveConnection(IAudioSystemItem* pAudioSystemControl);
	void            ClearConnections();
	ConnectionPtr   GetConnectionAt(int index);
	ConnectionPtr   GetConnection(CID id);
	ConnectionPtr   GetConnection(IAudioSystemItem* pAudioSystemControl);
	void            ReloadConnections();
	void            LoadConnectionFromXML(XmlNodeRef xmlNode, int platformIndex = -1);

	void            SignalControlAboutToBeModified();
	void            SignalControlModified();
	void            SignalConnectionAdded(IAudioSystemItem* pMiddlewareControl);
	void            SignalConnectionRemoved(IAudioSystemItem* pMiddlewareControl);

	void            Serialize(Serialization::IArchive& ar);

private:
	void SetId(CID id);
	void SetType(EACEControlType type);

	CID                        m_id;
	EACEControlType            m_type;
	string                     m_name;
	Scope                      m_scope;
	std::vector<ConnectionPtr> m_connectedControls;
	std::vector<CATLControl*>  m_children;
	CATLControl*               m_pParent;
	bool                       m_bAutoLoad;
	bool                       m_bModified;

	CATLControlsModel*         m_pModel;

	// All the raw connection nodes. Used for reloading the data when switching middleware.
	void         AddRawXMLConnection(XmlNodeRef xmlNode, bool bValid, int platformIndex = -1);
	XMLNodeList& GetRawXMLConnections(int platformIndex = -1);
	std::map<int, XMLNodeList> m_connectionNodes;
};

typedef std::shared_ptr<CATLControl> ATLControlPtr;
}
