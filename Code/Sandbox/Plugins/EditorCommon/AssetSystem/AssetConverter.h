#pragma once

#include "Asset.h"
#include "DragDrop.h"
#include <IEditorClassFactory.h>

class CAssetBrowser;

//! Information about the conversion that the converter might need
struct EDITOR_COMMON_API SAssetConverterConversionInfo
{
	//! The location where the asset is being dropped
	string         folderPath;
	//! The Asset Browser executing the conversion
	CAssetBrowser* pOwnerBrowser;
};

//! An asset converter converts data coming from other editors to a format that the asset browser can use(aka Assets).
//! Asset Converters are initialized by the Asset Manager at init.
class EDITOR_COMMON_API CAssetConverter : public IClassDesc
{
public:
	virtual ESystemClassID SystemClassID() override final;
	//! If this Converter can convert the current mime data
	virtual bool           CanConvert(const QMimeData& data) = 0;
	//! The actual conversion operation
	virtual void           Convert(const QMimeData& data, const SAssetConverterConversionInfo& info) = 0;
	//! Returns a readable string representing the result of the conversion operation
	virtual string         ConversionInfo(const QMimeData& data) = 0;
	virtual ~CAssetConverter() {}
};

#define DECLARE_ASSET_CONVERTER_DESC(type)                    \
	virtual const char* ClassName() override { return # type; } \
	virtual void* CreateObject() override { return nullptr; }

#define REGISTER_ASSET_CONVERTER(type) REGISTER_CLASS_DESC(type)