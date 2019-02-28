// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <memory>
#include "TreeConfig.h"
#include <vector>
#include <CrySerialization/yasli/Config.h>
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/Pointers.h>
#include "Rect.h"
#include "sigslot.h"
using std::vector;

class PropertyTreeModel;
class PropertyTreeModel;
class PropertyRow;
typedef vector<yasli::SharedPtr<PropertyRow> > PropertyRows;
class PropertyTreeOperator;
struct PropertyRowMenuHandler;
class Entry;

namespace yasli { class Object; struct Context; }


namespace property_tree{ 
class InplaceWidget;
struct Color;
class QDrawContext;

enum Modifier;
class IMenu;
struct IUIFacade; 
struct KeyEvent;
}
using property_tree::IUIFacade;
using property_tree::KeyEvent;
class ValidatorBlock;
struct PropertyTreeStyle;
class PROPERTY_TREE_API PropertyTree
{
public:
	// Used to attach an object to a PropertyTree widget. Attached object should implement
	// Serialize method. Example of usage:
	//
	//   struct MyType
	//   {
	//     void serialize(yasli::Archive& ar);
	//   };
	//   MyType object;
	//
	//   propertyTree->attach(yasli::Serializer(object));
	// 
	// Attached object will be serialized through PropertyOArchive to populate the tree. On
	// every input made to the tree attached object will be deserialized through
	// PropertyIArchive and serialized back again through PropertyOArchive to make sure that
	// tree content is up-to-date.
	// Property archives can be identified by calling ar.IsEdit(). 
	// Serializer stores a pointer to an actual object that should either outlive property tree
	// or be detached on destruction.
	void attach(const yasli::Serializer& serializer);
	// This form attaches an array of Serializers-s. This is used to edit multiple objects
	// simultaneously. Only shared properties will be shown (i.e.  intersection of all
	// properties). Properties with different values will be shown as "..." or a gray
	// checkbox.
	bool attach(const yasli::Serializers& serializers);
	void attach(const yasli::Object& object);
	// Used for two-trees setup. In this case master tree acts as an outliner, that shows
	// top level of the data (either by using setOutlineMode or setting different filter).
	// Attached, slave tree then shows properties ot the item selected in the main tree.
	// Temporary structures (e.g. created on the stack) should not be used in this mode
	// (except for decorators), as this may cause access to deallocated object when
	// selecting it in the tree.
	virtual void attachPropertyTree(PropertyTree* propertyTree);
	void detachPropertyTree();
	void setAutoHideAttachedPropertyTree(bool autoHide);
	// Effectively clears the tree.
	void detach();
	bool attached() const { return !attached_.empty(); }
	// This methods returns array of Serializers for all selected properties.
	// This is useful for manual implementation of attachPropertyTree behavior
	// with type filtering or special logic.
	void getSelectionSerializers(yasli::Serializers* serializers);
	bool getSelectedObject(yasli::Object* object);

	// Forces serialization of attached object to update properties.  Can be used to update
	// property tree when attached object was changed for some reason.
	void revert();
	// Same as revert(), except that it will but interrupt editing or mouse action in
	// progress.
	void revertNoninterrupting();
	// Forces deserialization of attached objects from property items. 
	void apply(bool continuousUpdate = false);
	// Useful to apply edit boxes that are being edited at the moment.
	// May be needed when click on toolbar button doesn't steal the focus, leaving input
	// data effectively not saved.
	void applyInplaceEditor();

	int revertObjects(vector<void*> objectAddresses);
	bool revertObject(void* objectAddress);

	// Reduces width of the tree by removing expansion arrow/plus on the first level of the
	// tree (first level is always expanded).
	void setCompact(bool compact);
	bool compact() const;
	// Puts checkboxes into two columns when possible.
	void setPackCheckboxes(bool pack);
	bool packCheckboxes() const;
	// Changes distance between rows, multiplier of row height.
	void setRowSpacing(float rowSpacing);
	float rowSpacing() const;
	// Sets default width of the value column, 0..1 (relative to widget width)
	void setValueColumnWidth(float valueColumnWidth);
	float valueColumnWidth() const;
	// Set number of levels to be expanded by default. Note that this function should be
	// invoked before first call to attach attach() to have an effect.
	void setExpandLevels(int levels);
	// Can be used to control if container(array) elements have numbered labels.
	void setShowContainerIndices(bool showContainerIndices) { config_.showContainerIndices = showContainerIndices; }
	bool showContainerIndices() const{ return config_.showContainerIndices; }
	// Limits the rate at which sliders emit change signal.
	void setSliderUpdateDelay(int delayMS) { config_.sliderUpdateDelay = delayMS; }

	// Can be used to disable internal undo.
	void setUndoEnabled(bool enabled, bool full = false);
	// This can be used to disable automatic serialization after each deserialization call.
	// May be useful to prevent double-revert when signalChange is connected to some
	// external data model, which fires an event that reverts tree automatically.
	void setAutoRevert(bool autoRevert) { autoRevert_ = autoRevert; }
	bool multiSelectable() const { return attachedPropertyTree_ != 0; }

	// When set filtering is started just by typing in the property tree
	void setFilterWhenType(bool filterWhenType) {	config_.filterWhenType = filterWhenType; }
	
	// Outline mode hides content of the elements of the container (excepted for
	// inlined/pulled-up properties). Can be used together with second property
	// tree through attachPropertyTree.
	void setOutlineMode(bool outlineMode) { outlineMode_ = outlineMode; }
	bool outlineMode() const{ return outlineMode_; }
	// Hide selection when widget is out of focus. Disables selection for parent of inline items.
	void setHideSelection(bool hideSelection) { hideSelection_ = hideSelection; }
	bool hideSelection() const{ return hideSelection_; }
	// Can be used to disable selection of multiple properties at the same time.
	void setMultiSelection(bool multiSelection) { config_.multiSelection = multiSelection; }
	bool multiSelection() const{ return config_.multiSelection; }

	// Sets head of the context-list. Can be used to pass additional data to nested decorators.
	void setArchiveContext(yasli::Context* lastContext);
	// Sets archive filter. Filter is a bit mask stored within archive that can be used to
	// affect behavior of serialization. For example one can have two trees that shows
	// different portions of the same object.
	void setFilter(int filter) { config_.filter = filter; }

	// Can be used to select once serialized object(s) in the tree.
	bool selectByAddress(const void*, bool keepSelectionIfChildSelected = false);
	bool selectByAddresses(const void* const* addresses, size_t addressCount, bool keepSelectionIfChildSelected);
	bool selectByAddresses(const vector<const void*>& addresses, bool keepSelectionIfChildSelected);
	// Can be used to query information about selection in the tree.
	bool setSelectedRow(PropertyRow* row);
	PropertyRow* selectedRow();
	int selectedRowCount() const;
	PropertyRow* selectedRowByIndex(int index);

	// Reports if serialized data contains warnings. Warnings are reported
	// through Archive::warning() method.
	bool containsWarnings() const;
	// Reports if serialized data contains errors. Errors are reported
	// through Archive::error() method.
	bool containsErrors() const;
	void focusFirstError();

	void ensureVisible(PropertyRow* row, bool update = true, bool considerChildren = true);
	void expandRow(PropertyRow* row, bool expanded = true, bool updateHeights = true);
	void expandAll();
	void collapseAll();
	// PropertyTreeStyle used to customize visual appearance of the property tree
	const PropertyTreeStyle& treeStyle() const{ return *style_; }
	void setTreeStyle(const PropertyTreeStyle& style);
	static void setDefaultTreeStyle(const PropertyTreeStyle& style);
	static const PropertyTreeStyle& defaultTreeStyle() { return defaultTreeStyle_; }
	const TreeConfig& config() const{ return config_; }

	Point treeSize() const;

	virtual void YASLI_SERIALIZE_METHOD(yasli::Archive& ar);

protected:
	PropertyTree(IUIFacade* ui);
	~PropertyTree();

public:
	// internal use
	int leftBorder() const { return leftBorder_; }
	int rightBorder() const { return rightBorder_; }

	bool spawnWidget(PropertyRow* row, bool ignoreReadOnly);
	void expandParents(PropertyRow* row);
	void expandChildren(PropertyRow*);
	void collapseChildren(PropertyRow*);

	void setFullRowMode(bool fullRowMode);
	bool fullRowMode() const;
	void setFullRowContainers(bool fullRowContainers) { config_.fullRowContainers = fullRowContainers; repaint(); }
	bool fullRowContainers() const { return config_.fullRowContainers; }

	void setActionsEnabled(bool enable) { config_.enableActions = enable; repaint(); }
	bool actionsEnabled() const { return config_.enableActions; }

	int _defaultRowHeight() const { return defaultRowHeight_; }
	int getPropertySplitterPos() const { return propertySplitterPos_; }
	void setPropertySplitterPos(int pos);
	Rect getPropertySplitterRect() const;
	PropertyTreeModel* model() { return model_.get(); }
	const PropertyTreeModel* model() const { return model_.get(); }

	void onRowSelected(const std::vector<PropertyRow*>& row, bool addSelection, bool adjustCursorPos);
	void onAttachedTreeChanged();
	const ValidatorBlock* _validatorBlock() const { return validatorBlock_.get(); }
	IUIFacade* ui() const { return ui_; }
	virtual void _cancelWidget();
	virtual bool _isDragged(const PropertyRow* row) const = 0;
	bool _isCapturedRow(const PropertyRow* row) const;


	PropertyRow* _pressedRow() const { return pressedRow_; }
	void _setPressedRow(PropertyRow* row) { pressedRow_ = row; }
	virtual bool hasFocusOrInplaceHasFocus() const = 0;
	void addMenuHandler(PropertyRowMenuHandler* handler);
	void clearMenuHandlers();
	Point _toWidget(Point point) const;
	virtual void repaint() = 0;
	virtual void updateHeights(bool recalculateTextSize = false) = 0;
	virtual void defocusInplaceEditor() = 0;

	struct PROPERTY_TREE_API RowFilter {
		enum Type {
			NAME_VALUE,
			NAME,
			VALUE,
			TYPE,
			NUM_TYPES
		};

		yasli::string start[NUM_TYPES];
		bool tillEnd[NUM_TYPES];
		std::vector<yasli::string> substrings[NUM_TYPES];

		void parse(const char* filter);
		bool match(const char* text, Type type, size_t* matchStart, size_t* matchEnd) const;
		bool typeRelevant(Type type) const{
			return !start[type].empty() || !substrings[type].empty();
		}

		RowFilter()
		{
			for (int i = 0; i < NUM_TYPES; ++i)
				tillEnd[i] = false;
		}
	};

protected:
	virtual void onAboutToSerialize(yasli::Archive& ar) = 0;
	virtual void onSerialized(yasli::Archive& ar) = 0;
	virtual void onChanged() = 0;
	virtual void onContinuousChange() = 0;
	virtual void onSelected() = 0;
	virtual void onPushUndo() = 0;

	virtual void copyRow(PropertyRow* row) = 0;
	virtual void pasteRow(PropertyRow* row) = 0;
	virtual bool canBePasted(PropertyRow* destination) = 0;
	virtual bool canBePasted(const char* destinationType) = 0;
	PropertyRow* rowByPoint(const Point& point);
	void onRowMenuDecompose(PropertyRow* row);
	bool toggleRow(PropertyRow* row);

	Point pointToRootSpace(const Point& pointInWindowSpace) const;
	Point pointFromRootSpace(const Point& point) const;
	virtual bool updateScrollBar() = 0;
	virtual void interruptDrag() = 0;
	virtual void _arrangeChildren() = 0;
	virtual void startFilter(const char* text) = 0;
	virtual void resetFilter() = 0;

	void updateValidatorIcons();
	void applyValidation();
	void jumpToNextHiddenValidatorIssue(bool isError, PropertyRow* start);

	bool onContextMenu(PropertyRow* row, property_tree::IMenu& menu);

	bool onRowKeyDown(PropertyRow* row, const KeyEvent* ev);
	// points here are specified in root-row space
	bool onRowLMBDown(PropertyRow* row, const Rect& rowRect, Point point, bool controlPressed, bool shiftPressed);
	void onRowLMBUp(PropertyRow* row, const Rect& rowRect, Point point);
	void onRowRMBDown(PropertyRow* row, const Rect& rowRect, Point point);
	void onRowMouseMove(PropertyRow* row, const Rect& rowRect, Point point, property_tree::Modifier modifiers);
	void onMouseStill();

	void setWidget(property_tree::InplaceWidget* widget, PropertyRow* widgetRow);

	void updateAttachedPropertyTree(bool revert);

	void onModelUpdated(const PropertyRows& rows, bool needApply);
	void onModelPushUndo(PropertyTreeOperator* op, bool* handled);

private:
	PropertyTree(const PropertyTree&);
	PropertyTree& operator=(const PropertyTree&);
protected:
	std::auto_ptr<PropertyTreeModel> model_;
	std::auto_ptr<property_tree::InplaceWidget> widget_; // in-place widget
	PropertyRow* widgetRow_;
	vector<PropertyRowMenuHandler*> menuHandlers_;
	IUIFacade* ui_;

	typedef vector<yasli::Object> Objects;
	Objects attached_;
	PropertyTree* attachedPropertyTree_;
	RowFilter rowFilter_;
	yasli::Context* archiveContext_;

	int defaultRowHeight_;
	int leftBorder_;
	int rightBorder_;
	int propertySplitterPos_;
	Point size_;
	Point offset_;
	Rect area_;

	int cursorX_;
	bool filterMode_;
	yasli::SharedPtr<PropertyRow> lastSelectedRow_;
	Point pressPoint_;
	Point pressDelta_;
	bool pointerMovedSincePress_;
	Point lastStillPosition_;
	PropertyRow* mouseOverRow_;
	PropertyRow* capturedRow_;
	PropertyRow* pressedRow_;
	TreeConfig config_;
	static PropertyTreeStyle defaultTreeStyle_;
	std::auto_ptr<PropertyTreeStyle> style_;
	std::auto_ptr<ValidatorBlock> validatorBlock_;

	bool autoRevert_;

	bool dragCheckMode_;
	bool dragCheckValue_;
	bool autoHideAttachedPropertyTree_;
	int zoomLevel_;
	bool outlineMode_;
	bool hideSelection_;
	bool splitterDragging_;

	friend class DragWindow;
	friend class property_tree::QDrawContext;
	friend struct FilterVisitor;
	friend struct PropertyTreeMenuHandler;
	friend struct ContainerMenuHandler;
	friend class PropertyTreeModel;
};

