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
	class CAudioAssetsManager;
	class IAudioSystemItem;

	struct SRawConnectionData
	{
		SRawConnectionData(XmlNodeRef node, bool bIsValid)
			: xmlNode(node)
			, bValid(bIsValid) {}

		XmlNodeRef xmlNode;
		bool       bValid; // indicates if the connection is valid for the currently loaded middle-ware
	};

using XMLNodeList = std::vector<SRawConnectionData>;

	class IAudioAsset
	{
	public:
		IAudioAsset(const string& name) : m_name(name) {}
		virtual EItemType GetType() const { return eItemType_Invalid; }

		IAudioAsset*      GetParent() const { return m_pParent; }
		void              SetParent(IAudioAsset* pParent);

		size_t            ChildCount() const { return m_children.size(); }
		IAudioAsset*      GetChild(size_t const index) const { return m_children[index]; }
		void              AddChild(IAudioAsset* pChildControl);
		void              RemoveChild(IAudioAsset* pChildControl);

		string            GetName() const { return m_name; }
		virtual void      SetName(const string& name) { m_name = name; }

		virtual bool      IsModified() const = 0;
		virtual void      SetModified(bool const bModified, bool const bForce = false) = 0;

		void              SetPlaceholderConnection(bool const bIsPlaceholder) { m_bHasPlaceholderConnection = bIsPlaceholder; }
		bool              HasPlaceholderConnection() const { return m_bHasPlaceholderConnection; }

	protected:
		IAudioAsset*              m_pParent = nullptr;
		std::vector<IAudioAsset*> m_children;
		string                    m_name;
		bool                      m_bHasPlaceholderConnection = false;
	};

	class CAudioLibrary : public IAudioAsset
	{

public:
	CAudioLibrary(const string& name) : IAudioAsset(name) {}
	virtual EItemType GetType() const override    { return eItemType_Library; }
	bool              IsModified() const override { return m_bModified; }
	virtual void      SetModified(bool const bModified, bool const bForce = false) override;

	private:
		bool m_bModified = false;

	};

	class CAudioFolder : public IAudioAsset
	{

public:
	CAudioFolder(const string& name) : IAudioAsset(name) {}
	virtual EItemType GetType() const override { return eItemType_Folder; }
	bool              IsModified() const override;
	virtual void      SetModified(bool const bModified, bool const bForce = false) override;
	};

	class CAudioControl : public IAudioAsset
	{
		friend class CAudioControlsLoader;
		friend class CAudioControlsWriter;
		friend class CUndoControlModified;

public:
	CAudioControl() = default;
	CAudioControl(const string& controlName, CID id, EItemType type);
	~CAudioControl();

		CID           GetId() const;
		EItemType     GetType() const override { return m_type; }

		virtual void  SetName(const string& name) override;

		Scope         GetScope() const;
		void          SetScope(Scope scope);

		bool          IsAutoLoad() const;
		void          SetAutoLoad(bool bAutoLoad);

		float         GetRadius() const { return m_radius; }
		void          SetRadius(float const radius) { m_radius = radius; }

		float         GetOcclusionFadeOutDistance() const { return m_occlusionFadeOutDistance; }
		void          SetOcclusionFadeOutDistance(float fadeOutArea);

		size_t        GetConnectionCount();
		void          AddConnection(ConnectionPtr pConnection);
		void          RemoveConnection(ConnectionPtr pConnection);
		void          RemoveConnection(IAudioSystemItem* pAudioSystemControl);
		void          ClearConnections();
		ConnectionPtr GetConnectionAt(int index);
		ConnectionPtr GetConnection(CID id);
		ConnectionPtr GetConnection(IAudioSystemItem* pAudioSystemControl);
		void          ReloadConnections();
		void          LoadConnectionFromXML(XmlNodeRef xmlNode, int platformIndex = -1);

		void          MatchRadiusToAttenuation();
		bool          IsMatchRadiusToAttenuationEnabled() const { return m_bMatchRadiusAndAttenuation; }
		void          SetMatchRadiusToAttenuationEnabled(bool bEnabled) { m_bMatchRadiusAndAttenuation = bEnabled; }

		void          Serialize(Serialization::IArchive& ar);

		virtual bool  IsModified() const override;
		virtual void  SetModified(bool const bModified, bool const bForce = false) override;

	private:

		void SignalControlAboutToBeModified();
		void SignalControlModified();
		void SignalConnectionAdded(IAudioSystemItem* pMiddlewareControl);
		void SignalConnectionRemoved(IAudioSystemItem* pMiddlewareControl);
		void SignalConnectionModified();

		CID                        m_id = ACE_INVALID_ID;
		EItemType                  m_type = eItemType_Trigger;
		Scope                      m_scope = 0;
		std::vector<ConnectionPtr> m_connectedControls;
		float                      m_radius = 0.0f;
		float                      m_occlusionFadeOutDistance = 0.0f;
		bool                       m_bAutoLoad = true;
		bool                       m_bMatchRadiusAndAttenuation = true;

		// All the raw connection nodes. Used for reloading the data when switching middleware.
		void         AddRawXMLConnection(XmlNodeRef xmlNode, bool bValid, int platformIndex = -1);
		XMLNodeList& GetRawXMLConnections(int platformIndex = -1);
		std::map<int, XMLNodeList> m_connectionNodes;

	bool                       m_modifiedSignalEnabled = true;
	};
} // namespace ACE
