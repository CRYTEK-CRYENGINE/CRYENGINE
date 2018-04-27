// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/ItemCollection.h>

namespace CryParticleEditor {

class CNodeItem;

class CParticleGraphModel;

class CClipboardItemCollection : public CryGraphEditor::CItemCollection
{
private:
	struct SConnection
	{
		int32 sourceNodeIndex;
		int32 sourceFeatureIndex;
		int32 targetNodeIndex;

		SConnection()
			: sourceNodeIndex(-1)
			, sourceFeatureIndex(-1)
			, targetNodeIndex(-1)
		{}

		void Serialize(Serialization::IArchive& archive);
	};

	struct SNode
	{
		float  positionX;
		float  positionY;

		string dataBuffer;

		SNode()
			: positionX(.0f)
			, positionY(.0f)
		{}

		void Serialize(Serialization::IArchive& archive);
	};

	struct SFeature
	{
		string groupName;
		string featureName;
		string dataBuffer;

		void   Serialize(Serialization::IArchive& archive);
	};

	typedef std::map<CNodeItem*, int32 /* index */> NodeIndexByInstance;
	typedef std::vector<CNodeItem*>                 NodesByIndex;

public:
	CClipboardItemCollection(CParticleGraphModel& model);

	virtual void Serialize(Serialization::IArchive& archive) override;

private:
	CParticleGraphModel& m_model;
};

}

