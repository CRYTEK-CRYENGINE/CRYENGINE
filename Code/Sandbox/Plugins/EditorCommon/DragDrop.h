// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include <QMimeData>
#include <QStringList>
#include <QDrag.h>

//! Helper class to deal with passing custom data during Drag&Drop operations. Internally using mime data.
//! Usage: 
//! * If starting a drag, create a new CDragDropData and use it as you would QMimeData
//!		* For dragging within the application, prefer using SetCustomData/GetCustomData where possible
//!		* If the data cannot be kept valid during the time of the drag, data must be serialized into a QByteArray
//! * If handling a drag, use CDragDropData::FromDragDropEvent or CDragDropData::FromMimeDataSafe to retrive the CDragDropData
//!		* Always retrieve data from the paired accessor with which it has been set. 
//! //////////////////////////////////////////////////////////////////////////
//! * Here is an example of how to pass internal types, pointers, or arrays
//! 
//!  QDrag* drag = new QDrag(this);
//!  CDragDropData* data = new CDragDropData();
//!
//!  std::vector<InternalType*> internalTypeArray = ...; //MUST stay in scope during drag->exec() so data will remain valid
//!  data->SetCustomData<std::vector<InternalType*>>("InternalType", internalTypeArray);
//!  
//!  drag->setMimeData(mimeData);
//!  Qt::DropAction dropAction = drag->exec(); //Blocking call, data will remain valid
//! //////////////////////////////////////////////////////////////////////////
//! * Here is an example of how to retrieve internal data from the sample above
//!
//!  const CDragDropData* data = CDragDropData::FromDragDropEvent(event);
//!  
//!  if(data->HasCustomData("InternalType")) //always test if a certain type is present
//!  {
//!		const auto& internalTypeArrayRef = data->GetCustomData<std::vector<InternalType*>>("InternalType");
//!		... //Handle drag or drop event as usual
//!  }
//!
class EDITOR_COMMON_API CDragDropData : public QMimeData
{
	Q_OBJECT
public:
	CDragDropData() : QMimeData() {}

	//! Helper to get the CDragDropData from any drag&drop event (they all inherit from QDropEvent)
	static const CDragDropData* FromDragDropEvent(const QDropEvent* event);

	//! Helper method to create the CDragDropData from the QMimeData pointer provided by Qt, will fail if mimeData is not of CDragDropData type
	static const CDragDropData* FromMimeDataSafe(const QMimeData* mimeData);

	//! Helper method to create the CDragDropData from the QMimeData pointer provided by Qt
	static const CDragDropData* FromMimeData(const QMimeData* mimeData);

	//Only use the following for internal data types, basic types are supported by QMimeData
	
	//! Retrieves QByteArray data for a given key. Do not use if data was set with SetCustomData<T> as it is dangerous
	QByteArray GetCustomData(const char* type) const;

	//! Sets a QByteArray data for a given key.
	void       SetCustomData(const char* type, const QByteArray& data);

	//! Returns true if mime data contains custom data for a given key
	bool       HasCustomData(const char* type) const;

	//! Sets a custom data type in the mime data. Type will be saved as a binary copy, therefore it must be kept valid during the drag. For temporary variables, keep in scope.
	template<typename T>
	void SetCustomData(const char* type, const T& data);

	//! Retrieves a reference to the custom data from the mime data. Note that the object is going to be a memcpy of the original object passet to SetCustomData
	template<typename T>
	const T& GetCustomData(const char* type) const;

	//! Helper to check if mime data contains file paths (from file system, not internal file paths)
	bool HasFilePaths() const;

	//! Retrieves file paths (from file system, not internal file paths)
	QStringList GetFilePaths() const;

	//!Mime format : application/crytek;type=<type_name_token>
	//!Using crytek to be able to copy paste between all crytek applications
	//!Using type for actual differentiation within the application
	static QString GetMimeFormatForType(const char* type);

	// Tooltip.
	//! Show tracking tooltip to contextualize drag action. pWidget is the widget that is handling that drag action
	static void ShowDragText(QWidget* pWidget, const QString& what);
	//! Show tracking pixmap to contextualize drag action. pWidget is the widget that is handling that drag action
	static void ShowDragPixmap(QWidget* pWidget, const QPixmap& what, const QPoint& pixmapCursorOffset = QPoint());
	//! Clears the tooltip text for this widget
	static void ClearDragTooltip(QWidget* pWidget);

	//! Helper to create a QDrag and start a drag with the dragData as parameter
	static void StartDrag(QObject* pDragSource, Qt::DropActions supportedActions, QMimeData* pMimeData, const QPixmap* pPixmap = nullptr, const QPoint& pixmapCursorOffset = QPoint());
};

template<typename T>
void CDragDropData::SetCustomData(const char* type, const T& data)
{
	QByteArray byteArray((char*)&data, sizeof(T));
	SetCustomData(type, byteArray);
}

template<typename T>
const T& CDragDropData::GetCustomData(const char* type) const
{
	QByteArray data = GetCustomData(type);
	return *reinterpret_cast<const T*>(data.data());
}




