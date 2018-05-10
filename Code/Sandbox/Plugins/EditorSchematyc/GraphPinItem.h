// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/AbstractNodeItem.h>
#include <NodeGraph/AbstractPinItem.h>

#include <QString>
#include <QVariant>

class CDataTypeItem;

namespace CryGraphEditor {

class CPinWidget;
class CNodeWidget;
class CNodeGraphView;

}

namespace CrySchematycEditor {

enum EPinFlag : uint32
{
	None   = 0,
	Input  = 1 << 0,
	Output = 1 << 1,
};

class CNodeItem;

// Note: This is part of a workaround because pin IDs in Schematc are not unique atm.
class CPinId
{
public:
	enum class EType : uint8
	{
		Unset  = 0,
		Input  = EPinFlag::Input,
		Output = EPinFlag::Output
	};

	CPinId()
		: m_type(EType::Unset)
	{}

	CPinId(Schematyc::CUniqueId portId, EType type)
		: m_portId(portId)
		, m_type(type)
	{}

	const Schematyc::CUniqueId& GetPortId()  const { return m_portId; }
	bool                        IsInput() const    { return (m_type == EType::Input); }
	bool                        IsOutput() const   { return (m_type == EType::Output); }

	//
	bool operator==(const CPinId& other) const
	{
		return (m_type == other.m_type && m_portId == other.m_portId);
	}

	bool operator!=(const CPinId& other) const
	{
		return (m_type != other.m_type || m_portId != other.m_portId);
	}

private:
	Schematyc::CUniqueId m_portId;
	EType                m_type;
};
// ~Note

enum class EPinType : int8
{
	Unset = 0,
	Execution,
	Data,
	Signal
};

class CPinItem : public CryGraphEditor::CAbstractPinItem
{
public:
	CPinItem(uint32 index, uint32 flags, CNodeItem& nodeItem, CryGraphEditor::CNodeGraphViewModel& model);
	virtual ~CPinItem();

	// CryGraphEditor::CAbstractPinItem
	virtual CryGraphEditor::CPinWidget*        CreateWidget(CryGraphEditor::CNodeWidget& nodeWidget, CryGraphEditor::CNodeGraphView& view) override;
	virtual const char*                        GetStyleId() const { return m_styleId.c_str(); }
	virtual CryGraphEditor::CAbstractNodeItem& GetNodeItem() const override;

	virtual QString                            GetName() const override        { return m_name; };
	virtual QString                            GetDescription() const override { return QString(); }
	virtual QString                            GetTypeName() const override;

	virtual QVariant                           GetId() const override;
	virtual bool                               HasId(QVariant id) const override;

	virtual bool                               IsInputPin() const  { return m_flags & EPinFlag::Input; }
	virtual bool                               IsOutputPin() const { return m_flags & EPinFlag::Output; }

	virtual bool                               CanConnect(const CryGraphEditor::CAbstractPinItem* pOtherPin) const;
	// ~CryGraphEditor::CAbstractPinItem

	const CDataTypeItem& GetDataTypeItem() const { return *m_pDataTypeItem; }
	uint32               GetPinIndex() const     { return m_index; }
	void                 UpdateWithNewIndex(uint32 index);

	Schematyc::CUniqueId GetPortId() const;
	EPinType             GetPinType() const { return m_pinType; }

	CPinId               GetPinId() const;

protected:
	CNodeItem&           m_nodeItem;
	const CDataTypeItem* m_pDataTypeItem;
	QString              m_name;
	string               m_styleId;

	CPinId               m_id;
	uint32               m_flags;
	uint16               m_index;
	EPinType             m_pinType;

};

inline CPinId CPinItem::GetPinId() const
{
	return CPinId(GetPortId(), IsInputPin() ? CPinId::EType::Input : CPinId::EType::Output);
}

}

Q_DECLARE_METATYPE(CrySchematycEditor::CPinId);

