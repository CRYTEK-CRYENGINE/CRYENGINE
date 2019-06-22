// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ProxyModels\ItemModelAttribute.h"

class CAsset;
class CAttributeFilter;

namespace Attributes
{

class CDependenciesOperatorBase : public IAttributeFilterOperator
{
public:
	virtual QWidget*             CreateEditWidget(std::shared_ptr<CAttributeFilter> pFilter, const QStringList* pAttributeValues) override;
	virtual void                 UpdateWidget(QWidget* widget, const QVariant& value) override;
	virtual std::pair<bool, int> GetUsageInfo(const CAsset& asset, const string& pathToTest) const = 0;

};

class CUsedBy : public CDependenciesOperatorBase
{
public:
	virtual QString              GetName() override { return QWidget::tr("used by"); }
	virtual bool                 Match(const QVariant& value, const QVariant& filterValue) override;
	virtual std::pair<bool, int> GetUsageInfo(const CAsset& asset, const string& pathToTest) const override;
};

class CUse : public CDependenciesOperatorBase
{
public:
	virtual QString              GetName() override { return QWidget::tr("that use"); }
	virtual bool                 Match(const QVariant& value, const QVariant& filterValue) override;
	virtual std::pair<bool, int> GetUsageInfo(const CAsset& asset, const string& pathToTest) const override;
};

class CDependenciesAttribute : public CItemModelAttribute
{
public:
	CDependenciesAttribute();
};

extern CDependenciesAttribute s_dependenciesAttribute;

}
