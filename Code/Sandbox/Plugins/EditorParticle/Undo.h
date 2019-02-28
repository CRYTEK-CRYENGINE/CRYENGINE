// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/NodeGraphUndo.h>

#include <QPointF>
#include <QObject>

namespace pfx2 {

struct SParticleFeatureParams;

}

namespace CryParticleEditor {

class CFeatureItem;

class CUndoFeatureCreate : public CryGraphEditor::CUndoObject
{
public:
	CUndoFeatureCreate(CFeatureItem& feature, const string& description = string());

protected:
	// IUndoObject
	virtual void Undo(bool bUndo) override;
	virtual void Redo() override;
	// ~UndoObject

private:
	QVariant                            m_nodeId;
	const pfx2::SParticleFeatureParams& m_params;
	DynArray<char>                      m_featureData;
	uint32                              m_featureIndex;
};

class CUndoFeatureRemove : public CryGraphEditor::CUndoObject
{
public:
	CUndoFeatureRemove(CFeatureItem& feature, const string& description = string());

protected:
	// IUndoObject
	virtual void Undo(bool bUndo) override;
	virtual void Redo() override;
	// ~UndoObject

private:
	QVariant                            m_nodeId;
	const pfx2::SParticleFeatureParams& m_params;
	DynArray<char>                      m_featureData;
	uint32                              m_featureIndex;
};

class CUndoFeatureMove : public CryGraphEditor::CUndoObject
{
public:
	CUndoFeatureMove(CFeatureItem& feature, uint32 oldIndex, const string& description = string());

protected:
	// IUndoObject
	virtual void Undo(bool bUndo) override;
	virtual void Redo() override;
	// ~UndoObject

private:
	QVariant m_nodeId;
	uint32   m_oldFeatureIndex;
	uint32   m_newFeatureIndex;
};

}

