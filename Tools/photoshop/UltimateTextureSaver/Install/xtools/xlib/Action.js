//
// Action.jsx
// This script defines missing classes from the ActionManager API
//
// $Id: Action.js,v 1.18 2014/01/05 22:57:16 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//-include "xlib/stdlib.js"
//-include "xlib/PSConstants.js"

var action_js = true;

//=========================== ActionsPaletteFile ==============================

ActionsPaletteFile = function() {
  var self = this;

  self.actionsPalette = null; // ActionsPalette
  self.version = 0x10;        // word
  self.file = null;           // File
};

ActionsPaletteFile.prototype.typename = "ActionsPaletteFile";
ActionsPaletteFile.prototype.getActionsPalette = function() {
  return this.actionsPalette;
}
ActionsPaletteFile.prototype.getFile = function() {
  return this.file;
}
ActionsPaletteFile.prototype.getVersion = function() {
  return this.version;
}

//=============================== ActionFile ==================================

ActionFile = function() {
  var self = this;

  self.actionSet = null; // ActionSet
  self.file = null;      // File
};

ActionFile.prototype.typename = "ActionFile";

ActionFile.prototype.getActionSet = function() { return this.actionSet; };
ActionFile.prototype.getFile      = function() { return this.file; };

ActionFile.prototype.setActionSet = function(obj) {
  var self = this;
  obj.parent = self;
  self.actionSet = obj;
};


//=========================== ActionsPalette ==================================

ActionsPalette = function() {
  var self = this;

  self.parent = null;
  self.name = app.name;
  self.count = 0;
  self.version = 0x10;
  self.actionSets = [];
};
ActionsPalette.prototype.typename = "ActionsPalette";

ActionsPalette.prototype.getName    = function() { return this.name; };
ActionsPalette.prototype.getCount   = function() { return this.count; };
ActionsPalette.prototype.getVersion = function() { return this.version; };
ActionsPalette.prototype.getParent  = function() { return this.parent; };

ActionsPalette.prototype.getNames = function() {
  var self = this;
  var names = [];

  for (var i = 0; i < self.actionSets.length; i++) {
    var as = self.actionSets[i];
    names.push(as.name);
  }
  return names;
};

ActionsPalette.prototype.getByName = function(name) {
  var self = this;
  for (var i = 0; i < self.actionSets.length; i++) {
    var as = self.actionSets[i];
    if (as.name == name) {
      return as;
    }
  }
  return undefined;
};
ActionsPalette.prototype.byIndex = function(index) {
  var self = this;
  return self.actionSets[index];
};

ActionsPaletteIterator = function(state) {
  this.state = state;
};
ActionsPaletteIterator.prototype.exec = function(actionSet) {
};

//================================ ActionSet ==================================

ActionSet = function() {
  var self = this;

  self.parent   = null;
  self.version  = 0x10;        // version
  self.name     = '';          // unicode
  self.expanded = false;       // boolean/byte
  self.count    = 0;           // int16
  self.actions  = [];          // Actions
  return self;
};

ActionSet.prototype.typename = "ActionSet";

ActionSet.prototype.getVersion  = function(act) { return this.version; };
ActionSet.prototype.getName     = function(act) { return this.name; };
ActionSet.prototype.getExpanded = function(act) { return this.expanded; };
ActionSet.prototype.getCount    = function(act) { return this.count; };

ActionSet.prototype.getNames = function() {
  var self = this;
  var names = [];

  for (var i = 0; i < self.actions.length; i++) {
    var act = self.actions[i];
    names.push(act.name);
  }
  return names;
};
ActionSet.prototype.getByName = function(name) {
  var self = this;
  for (var i = 0; i < self.actions.length; i++) {
    var act = self.actions[i];
    if (act.name == name) {
      return act;
    }
  }
  return undefined;
};
ActionSet.prototype.byIndex = function(index) {
  var self = this;
  return self.actions[index];
};
ActionSet.prototype.add = function(obj) {
  var self = this;
  obj.parent = self;
  self.actions.push(obj);
  self.count = self.actions.length;
};

//================================ Action =====================================
Action = function() {
  var self = this;

  self.index = 0;              // int16  This is really the function key!
  self.shiftKey = false;       // boolean/byte
  self.commandKey = false;     // boolean/byte
  self.colorIndex = 0;         // int16
  self.name = '';              // unicode
  self.expanded = false;       // boolean/byte
  self.count = 0;              // word
  self.actionItems = [];       // ActionItems

  return self;
};
Action.prototype.typename = "Action";

Action.prototype.getIndex      = function() { return this.index; };
Action.prototype.getShiftKey   = function() { return this.shiftKey; };
Action.prototype.getCommandKey = function() { return this.commandKey; };
Action.prototype.getColorIndex = function() { return this.colorIndex; };
Action.prototype.getName       = function() { return this.name; };
Action.prototype.getExpanded   = function() { return this.expanded; };
Action.prototype.getCount      = function() { return this.count; };

Action.prototype.byIndex = function(index) {
  var self = this;
  return self.actionItems[index];
};

Action.prototype.add = function(obj) {
  var self = this;
  obj.parent = self;
  self.actionItems.push(obj);
  self.count = self.actionItems.length;
};


// useful for debugging
ActionDescriptor.prototype.listKeys = function() {
  var self = this;
  var str = '';
  for (var i = 0; i < self.count; i++) {
    var key = self.getKey(i);
    str += PSConstants.reverseNameLookup(key) + ":" +
      self.getType(key).toString() + "\r\n";
  }
  return str;
};

//============================== ActionItem ===================================
ActionItem = function() {
  var self = this;

  self.parent = null;
  self.expanded = false;       // boolean/byte
  self.enabled = true;         // boolean/byte
  self.withDialog = false;     // boolean/byte
  self.dialogOptions = 0;      // byte
  self.identifier = '';        // string [4] TEXT/long
  self.event = '';             // ascii
  self.itemID = 0;             // word
  self.name = '';              // ascii
  self.hasDescriptor = false;  // flag (-1 == true)
  self.descriptor = null;      // ActionDescriptor
  return self;
};

ActionItem.prototype.typename = "ActionItem";

ActionItem.TEXT_ID = 'TEXT';
ActionItem.LONG_ID = 'long';

ActionItem.prototype.getExpanded        = function() { return this.expanded; };
ActionItem.prototype.getEnabled         = function() { return this.enabled; };
ActionItem.prototype.getWithDialog      = function() { return this.withDialog; };
ActionItem.prototype.getDialogOptions   = function() { return this.dialogOptions; };
ActionItem.prototype.getIdentifier      = function() { return this.identifier; };
ActionItem.prototype.getEvent           = function() { return this.event; };
ActionItem.prototype.getItemID          = function() { return this.itemID; };
ActionItem.prototype.getName            = function() { return this.name; };
ActionItem.prototype.containsDescriptor = function() { return this.hasDescriptor; };
ActionItem.prototype.getDescriptor      = function() { return this.descriptor; };

ActionItem.prototype.setEvent = function(str) {
  var self = this;

  self.event = str;
  self.identifier = ActionItem.TEXT_ID;
};
ActionItem.prototype.setItemID = function(id) {
  var self = this;

  self.itemID = id;
  self.identifier = ActionItem.LONG_ID;
};

ActionItem.prototype.setDescriptor = function(desc) {
  var self = this;
  self.descriptor = desc;
  self.hasDescriptor = (!!desc);
};

"Actions.js";
// EOF

