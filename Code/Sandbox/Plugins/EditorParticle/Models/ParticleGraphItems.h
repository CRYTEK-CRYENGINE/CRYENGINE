// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryParticleSystem/IParticlesPfx2.h>

#include <NodeGraph/ICryGraphEditor.h>
#include <NodeGraph/AbstractNodeItem.h>
#include <NodeGraph/AbstractPinItem.h>
#include <NodeGraph/AbstractConnectionItem.h>

#include <NodeGraph/PinWidget.h>

#include <CryExtension/CryGUID.h>

namespace CryGraphEditor {

class CNodeWidget;
class CNodeGraphViewModel;
class CNodeGraphView;

}

namespace CryParticleEditor {

class CParentPinItem;
class CChildPinItem;
class CFeaturePinItem;
class CFeatureItem;

typedef std::vector<CFeatureItem*> FeatureItemArray;

class CNodeItem : public CryGraphEditor::CAbstractNodeItem
{
	friend class CFlowGraphViewModel;

public:
	CNodeItem(pfx2::IParticleComponent& component, CryGraphEditor::CNodeGraphViewModel& viewModel);
	virtual ~CNodeItem();

	// CryGraphEditor::CAbstractNodeItem
	virtual void                                SetPosition(QPointF position) override;

	virtual CryGraphEditor::CNodeWidget*        CreateWidget(CryGraphEditor::CNodeGraphView& view) override;

	virtual QVariant                            GetId() const override;
	virtual bool                                HasId(QVariant id) const override;
	virtual QVariant                            GetTypeId() const override;

	virtual const CryGraphEditor::PinItemArray& GetPinItems() const override { return m_pins; };
	virtual QString                             GetName() const;
	virtual void                                SetName(const QString& name);

	virtual bool                                IsDeactivated() const override;
	virtual void                                SetDeactivated(bool isDeactivated) override;

	virtual void                                Serialize(Serialization::IArchive& archive) override;
	// ~CryGraphEditor::CAbstractNodeItem

	const char*               GetIdentifier() const         { return m_component.GetName(); }
	pfx2::IParticleComponent& GetComponentInterface() const { return m_component; }

	uint32                    GetIndex() const;
	CParentPinItem*           GetParentPinItem();
	CChildPinItem*            GetChildPinItem();

	const FeatureItemArray& GetFeatureItems() const { return m_features; }
	const size_t            GetNumFeatures() const  { return m_features.size(); }

	CFeatureItem*           AddFeature(uint32 index, const pfx2::SParticleFeatureParams& featureParams);
	CFeatureItem*           AddFeature(const pfx2::SParticleFeatureParams& featureParams);
	void                    RemoveFeature(uint32 index);

	bool                    MoveFeatureAtIndex(uint32 featureIndex, uint32 destIndex);

	void                    SetVisible(bool isVisible);
	bool                    IsVisible();

public:
	CCrySignal<void(CFeatureItem&)>  SignalFeatureAdded;
	CCrySignal<void(CFeatureItem&)>  SignalFeatureRemoved;
	CCrySignal<void(CFeatureItem&)>  SignalFeatureMoved;

	CCrySignal<void(bool isVisible)> SignalVisibleChanged;

private:
	pfx2::IParticleComponent&    m_component;
	CryGraphEditor::PinItemArray m_pins;
	FeatureItemArray             m_features;
};

inline CFeatureItem* CNodeItem::AddFeature(const pfx2::SParticleFeatureParams& featureParams)
{
	return AddFeature(m_component.GetNumFeatures(), featureParams);
}

class CConnectionItem;

enum class EPinType
{
	Unset,
	Parent,
	Child,
	Feature
};

enum ECustomItemType : int32
{
	eCustomItemType_Feature = CryGraphEditor::eItemType_UserType,
};

class CBasePinItem : public CryGraphEditor::CAbstractPinItem
{
public:
	typedef CryGraphEditor::CIconArray<CryGraphEditor::CPinWidget::Icon_Count> PinIconMap;

public:
	CBasePinItem(CNodeItem& node)
		: CryGraphEditor::CAbstractPinItem(node.GetViewModel())
		, m_nodeItem(node)
	{}

	virtual EPinType GetPinType() const { return EPinType::Unset; }

	// CryGraphEditor::CAbstractPinItem
	virtual CryGraphEditor::CAbstractNodeItem& GetNodeItem() const override { return static_cast<CryGraphEditor::CAbstractNodeItem&>(m_nodeItem); };
	// ~CryGraphEditor::CAbstractPinItem

protected:
	CNodeItem& m_nodeItem;
};

class CParentPinItem : public CBasePinItem
{
public:
	CParentPinItem(CNodeItem& node): CBasePinItem(node) {}

	// CBasePinItem
	virtual EPinType GetPinType() const { return EPinType::Parent; }

	virtual QString  GetName() const override { return QString("Parent"); }
	virtual QString  GetDescription() const override { return QString("Parent effect."); }
	virtual QString  GetTypeName() const override { return QString("Effect"); }

	virtual QVariant GetId() const override { return QVariant::fromValue(QString("Parent")); }
	virtual bool     HasId(QVariant id) const override { return QString("Parent") == id.value<QString>(); }

	virtual bool     IsInputPin() const override  { return true; }
	virtual bool     IsOutputPin() const override { return false; }

	virtual bool     CanConnect(const CryGraphEditor::CAbstractPinItem* pOtherPin) const override { return pOtherPin && pOtherPin->IsOutputPin() && !IsConnected(); }
	virtual bool     IsConnected() const override { return GetConnectionItems().size() > 0; }
	// ~CBasePinItem
};

class CChildPinItem : public CBasePinItem
{
public:
	CChildPinItem(CNodeItem& node): CBasePinItem(node) {}

	// CBasePinItem
	virtual EPinType GetPinType() const { return EPinType::Child; }

	virtual QString  GetName() const override { return QString("Children"); }
	virtual QString  GetDescription() const override { return QString("Child effects."); }
	virtual QString  GetTypeName() const override { return QString("Effect"); }

	virtual QVariant GetId() const override { return QVariant::fromValue(QString("Children")); }
	virtual bool     HasId(QVariant id) const override { return QString("Children") == id.value<QString>(); }

	virtual bool     IsInputPin() const override  { return false; }
	virtual bool     IsOutputPin() const override { return true; }

	virtual bool     CanConnect(const CryGraphEditor::CAbstractPinItem* pOtherPin) const override { return !pOtherPin || pOtherPin->IsInputPin(); }
	virtual bool     IsConnected() const override { return GetConnectionItems().size() > 0; }
	// ~CBasePinItem
};

class CFeaturePinItem : public CBasePinItem
{
public:
	CFeaturePinItem(CFeatureItem& feature);
	virtual ~CFeaturePinItem() {}

	// CryGraphEditor::CAbstractPinItem
	virtual EPinType GetPinType() const { return EPinType::Feature; }

	virtual QString  GetName() const override;
	virtual QString  GetDescription() const override { return GetName(); }
	virtual QString  GetTypeName() const override { return QString(); }

	virtual QVariant GetId() const override;
	virtual bool     HasId(QVariant id) const override;

	virtual bool     IsInputPin() const override  { return false; }
	virtual bool     IsOutputPin() const override { return true; }

	virtual bool     CanConnect(const CryGraphEditor::CAbstractPinItem* pOtherPin) const override;
	virtual bool     IsConnected() const override { return m_connections.size() > 0; }
	// ~CryGraphEditor::CAbstractPinItem

	CFeatureItem& GetFeatureItem() const { return m_featureItem; }

private:
	CFeatureItem& m_featureItem;
};

typedef std::vector<CFeaturePinItem*> FeaturePinItemArray;

class CFeatureItem : public CryGraphEditor::CAbstractNodeGraphViewModelItem
{
public:
	enum : int32 { Type = eCustomItemType_Feature };

public:
	CFeatureItem(pfx2::IParticleFeature& feature, CNodeItem& node, CryGraphEditor::CNodeGraphViewModel& viewModel);
	~CFeatureItem();

	// CryGraphEditor::CAbstractNodeGraphViewModelItem
	virtual int32 GetType() const override { return Type; }

	virtual bool  IsDeactivated() const override;
	virtual void  SetDeactivated(bool isDeactivated) override;

	virtual void  Serialize(Serialization::IArchive& archive) override;
	// ~CryGraphEditor::CAbstractNodeGraphViewModelItem

	QString                 GetGroupName() const;
	QString                 GetName() const;
	uint32                  GetIndex() const;

	pfx2::IParticleFeature& GetFeatureInterface() const { return m_featureInterface; }
	CNodeItem&              GetNodeItem() const         { return m_node; }
	CFeaturePinItem*        GetPinItem() const          { return m_pPin; }

	bool                    HasComponentConnector() const;
	QColor                  GetColor() const;

public:
	CCrySignal<void(bool)> SignalItemDeactivatedChanged;

private:
	CNodeItem&              m_node;
	pfx2::IParticleFeature& m_featureInterface;
	CFeaturePinItem*        m_pPin;
};

class CConnectionItem : public CryGraphEditor::CAbstractConnectionItem
{
public:
	CConnectionItem(CBasePinItem& sourcePin, CBasePinItem& targetPin, CryGraphEditor::CNodeGraphViewModel& viewModel);
	virtual ~CConnectionItem();

	// CryGraphEditor::CAbstractConnectionItem
	virtual CryGraphEditor::CConnectionWidget* CreateWidget(CryGraphEditor::CNodeGraphView& view) override;

	virtual CryGraphEditor::CAbstractPinItem&  GetSourcePinItem() const override { return m_sourcePin; }
	virtual CryGraphEditor::CAbstractPinItem&  GetTargetPinItem() const override { return m_targetPin; }

	virtual QVariant                           GetId() const override;
	virtual bool                               HasId(QVariant id) const override;
	// ~CryGraphEditor::CAbstractConnectionItem

	void OnConnectionRemoved();

private:
	CBasePinItem& m_sourcePin;
	CBasePinItem& m_targetPin;

	uint32        m_id;
};

}

