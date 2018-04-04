//
// PresetsManager
//
// var mgr = new PresetsManager()
// var brushes = mgr.getNames(PresetType.BRUSHES);
//
//
//
//
//
// $Id: PresetsManager.jsx,v 1.16 2011/07/16 20:11:48 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//--include "xlib/Stdlib.js"

//
// PresetType
//   This is an enumeration of the preset types available
//
PresetType = function(name, toolID, classID) {
  this.sym = toolID;
  this.name = name;
  this.id = xTID(toolID);
  this.classID = xTID(classID);
  this.toString = function() { return this.name; };
};
PresetType.BRUSHES        = new PresetType("Brushes", 'Brsh', 'Brsh');
PresetType.COLORS         = new PresetType("Colors", 'Clr ', 'Clr ');
PresetType.GRADIENTS      = new PresetType("Gradients", 'Grdn', 'Grad');
PresetType.STYLES         = new PresetType("Styles", 'StyC', 'Styl');
PresetType.PATTERNS       = new PresetType("Patterns", 'PttR', 'Ptrn');
PresetType.SHAPING_CURVES = new PresetType("Shaping Curves", 'ShpC', 'Shp ');
PresetType.CUSTOM_SHAPES  = new PresetType("Custom Shapes", 'customShape',
                                           'customShape');
PresetType.TOOL_PRESETS   = new PresetType("Tool Presets", 'toolPreset',
                                           'toolPreset');


//
//
// PresetsManager
//   This class is a container for all of the information that we can gather
//   about presets in Photoshop.
//
PresetsManager = function() {
  var self = this;

//   self.brushes       = [];
//   self.colors        = [];
//   self.gradients     = [];
//   self.styles        = [];
//   self.patterns      = [];
//   self.shapingCurves = [];
//   self.customShapes  = [];
//   self.toolPresets   = [];

  self.manager = null;
};
PresetsManager.prototype.typename = "PresetsManager";

//
// PresetsManager.getPresetsManager
//   Retrieves the PresetsManager object/list from Photoshop
//
PresetsManager.prototype.getPresetsManager = function() {
  var self = this;

  if (!self.manager) {
    var classApplication = cTID('capp');
    var typeOrdinal      = cTID('Ordn');
    var enumTarget       = cTID('Trgt');

    var ref = new ActionReference();
    ref.putEnumerated(classApplication, typeOrdinal, enumTarget);

    var appDesc = app.executeActionGet(ref);
    self.manager = appDesc.getList(sTID('presetManager'));
  }

  return self.manager;
};

PresetsManager.getManager = function(presetType) {
  var mgr = new PresetsManager();
  return new PTM(mgr, presetType);
};

PresetsManager.prototype.resetManager = function() {
  var self = this;
  self.manager = undefined;
};

PresetsManager.prototype.getCount = function(presetType) {
  if (presetType == undefined) {
    Error.runtimeError(2, "presetType");   // undefined
  }
  if (!(presetType instanceof PresetType)) {
    Error.runtimeError(19, "presetType");  // bad argument type
  }

  var self = this;

  var names = [];
  var key = presetType.id;
  var mgr = self.getPresetsManager();
  var max = mgr.count;

  for (var i = 0; i < max; i++) {
    var objType = mgr.getObjectType(i);
    if (objType == key) {
      break;
    }
  }

  if (i == max) {
    return -1;
  }
  var preset = mgr.getObjectValue(i);
  var list = preset.getList(cTID('Nm  '));
  return list.count;
};

//
//
PresetsManager.prototype.getNames = function(presetType) {
  if (presetType == undefined) {
    Error.runtimeError(2, "presetType");   // undefined
  }
  if (!(presetType instanceof PresetType)) {
    Error.runtimeError(19, "presetType");  // bad argument type
  }

  var self = this;

  var names = [];
  var key = presetType.id;
  var mgr = self.getPresetsManager();
  var max = mgr.count;

  for (var i = 0; i < max; i++) {
    var objType = mgr.getObjectType(i);
    if (objType == key) {
      break;
    }
  }

  if (i != max) {
    var preset = mgr.getObjectValue(i);
    var list = preset.getList(cTID('Nm  '));
    var max = list.count;
    for (var i = 0; i < max; i++) {
      var str = list.getString(i);
      names.push(str);
    }
    self[key] = names;
  }

  return names;
};

PresetsManager.prototype.resetPreset = function(presetType) {
  if (presetType == undefined) {
    Error.runtimeError(2, "presetType");   // undefined
  }
  if (!(presetType instanceof PresetType)) {
    Error.runtimeError(19, "presetType");  // bad argument type
  }

  var self = this;
  var ref = new ActionReference();
  ref.putProperty(cTID('Prpr'), presetType.classID);
  ref.putEnumerated(cTID('capp'), cTID('Ordn'), cTID('Trgt'));

  var desc = new ActionDescriptor();
  desc.putReference(cTID('null'), ref);

  executeAction(cTID('Rset'), desc, DialogModes.NO);
};

PresetsManager.prototype.saveAllPresets = function(presetType, fptr) {
  if (presetType == undefined) {
    Error.runtimeError(2, "presetType");   // undefined
  }
  if (fptr == undefined) {
    Error.runtimeError(2, "fptr");         // undefined
  }
  if (!(presetType instanceof PresetType)) {
    Error.runtimeError(19, "presetType");  // bad argument type
  }

  var self = this;

  var file = Stdlib.convertFptr(fptr);
  var desc = new ActionDescriptor();
  desc.putPath( cTID('null'), file);

  var ref = new ActionReference();
  ref.putProperty(cTID('Prpr'), presetType.classID);
  ref.putEnumerated(cTID('capp'), cTID('Ordn'), cTID('Trgt'));
  desc.putReference(cTID('T   '), ref);

  executeAction(cTID('setd'), desc, DialogModes.NO);
};

PresetsManager.prototype.loadPresets = function(presetType, fptr, append) {
  if (presetType == undefined) {
    Error.runtimeError(2, "presetType");   // undefined
  }
  if (fptr == undefined) {
    Error.runtimeError(2, "fptr");         // undefined
  }
  if (!(presetType instanceof PresetType)) {
    Error.runtimeError(19, "presetType");  // bad argument type
  }

  var self = this;

  var file = Stdlib.convertFptr(fptr);

  var ref = new ActionReference();
  ref.putProperty(cTID('Prpr'), presetType.classID);
  ref.putEnumerated(cTID('capp'), cTID('Ordn'), cTID('Trgt'));

  var desc = new ActionDescriptor();
  desc.putReference(cTID('null'), ref);
  desc.putPath(cTID('T   '), file);

  if (Boolean(append)) {
    desc.putBoolean(cTID('Appe'), true);
  }

  executeAction(cTID('setd'), desc, DialogModes.NO);
  self.resetManager();
};

PresetsManager.prototype.appendPresets = function(presetType, fptr) {
  this.loadPresets(presetType, fptr, true);
};

PresetsManager.prototype.getIndexOfElement = function(presetType, name) {
  if (presetType == undefined) {
    Error.runtimeError(2, "presetType");   // undefined
  }
  if (name == undefined) {
    Error.runtimeError(2, "name");         // undefined
  }
  if (!(presetType instanceof PresetType)) {
    Error.runtimeError(19, "presetType");  // bad argument type
  }

  var self = this;

  var names = self.getNames(presetType);
  for (var i = 0; i < names.length; i++) {
    if (names[i] == name) {
      return i+1;
    }
  }
  return -1;
};
PresetsManager.prototype.deleteElementAt = function(presetType, index) {
  if (presetType == undefined) {
    Error.runtimeError(2, "presetType");   // undefined
  }
  if (index == undefined) {
    Error.runtimeError(2, "index");       // undefined
  }
  if (!(presetType instanceof PresetType)) {
    Error.runtimeError(19, "presetType");  // bad argument type
  }

  var self = this;
  self.resetManager();

  var ref = new ActionReference();
  ref.putIndex(presetType.classID, index);

  var list = new ActionList();
  list.putReference(ref);

  var desc = new ActionDescriptor();
  desc.putList(cTID('null'), list);

  executeAction(cTID('Dlt '), desc, DialogModes.NO);
  self.resetManager();
};

PresetsManager.prototype.deleteElement = function(presetType, name) {
  var self = this;

  var idx = self.getIndexOfElement(presetType, name);

  if (idx == -1) {
    Error.runtimeError(9001, "Preset name '" + name + "' not found.");
  }

  self.deleteElementAt(presetType, idx);
};
PresetsManager.prototype.renameByIndex = function(presetType, index, name) {
  if (presetType == undefined) {
    Error.runtimeError(2, "presetType");   // undefined
  }
  if (index == undefined) {
    Error.runtimeError(2, "index");       // undefined
  }
  if (name == undefined) {
    Error.runtimeError(2, "name");        // undefined
  }
  if (!(presetType instanceof PresetType)) {
    Error.runtimeError(19, "presetType");  // bad argument type
  }

  var self = this;
  self.resetManager();
  var ref = new ActionReference();
  ref.putIndex(presetType.classID, index);

  var desc = new ActionDescriptor();
  desc.putReference(cTID('null'), ref);
  desc.putString(cTID('T   '), name);

  executeAction(cTID('Rnm '), desc, DialogModes.NO );
  self.resetManager();
};
PresetsManager.prototype.renameElement = function(presetType, oldName, newName) {
  var self = this;

  var idx = self.getIndexOfElement(oldName);

  if (idx == -1) {
    Error.runtimeError(9001, "Preset name '" + name + "' not found.");
  }

  self.renameByIndex(idx, newName);
};

PresetsManager.prototype.saveElementByIndex = function(presetType, index, fptr) {
  if (presetType == undefined) {
    Error.runtimeError(2, "presetType");   // undefined
  }
  if (index == undefined) {
    Error.runtimeError(2, "index");        // undefined
  }
  if (fptr == undefined) {
    Error.runtimeError(2, "fptr");         // undefined
  }
  if (!(presetType instanceof PresetType)) {
    Error.runtimeError(19, "presetType");  // bad argument type
  }

  var self = this;

  var file = Stdlib.convertFptr(fptr);

  if (file.exists) {
    file.remove();
  }

  var desc = new ActionDescriptor();
  desc.putPath(cTID('null'), file);
  var list = new ActionList();
  var ref = new ActionReference();
  ref.putIndex(presetType.classID, index);
  list.putReference(ref);
  desc.putList(cTID('T   '), list);
  executeAction(cTID('setd'), desc, DialogModes.NO );
};

PresetsManager.prototype.saveElement = function(presetType, name, fptr) {
  var self = this;

  var idx = self.getIndexOfElement(presetType, name);

  if (idx == -1) {
    Error.runtimeError(9001, "Preset name '" + name + "' not found.");
  }

  self.saveElementByIndex(presetType, idx, fptr);
};

// XXX add code here.... This is broken... fix later
PresetsManager.prototype.newElement = function(presetType, name, obj,
                                               interactive) {
  var desc = new ActionDescriptor();
  var ref = new ActionReference();
  ref.putClass( presetType.classID );
  desc.putReference( cTID('null'), ref );

  switch (presetType) {
    case PresetType.STYLES: {
      desc.putString(cTID('Nm  '), name);

      var lref = new ActionReference();
      lref.putEnumerated(cTID('Lyr '), cTID('Ordn'), cTID('Trgt'));
      desc.putReference(cTID('Usng'), lref);
      desc.putBoolean(sTID('blendOptions'), false);
      desc.putBoolean(cTID('Lefx'), true);
      break;
    };
    case PresetType.COLORS: {
      var color = obj;
      var swdesc = new ActionDescriptor();

      swdesc.putString(cTID('Nm  '), name);
      var cdesc = new ActionDescriptor();
      cdesc.putDouble(cTID('Rd  '), color.rgb.red);
      cdesc.putDouble(cTID('Grn '), color.rgb.green);
      cdesc.putDouble(cTID('Bl  '), color.rgb.blue);
      swdesc.putObject( cTID('Clr '), cTID('RGBC'), cdesc);
      desc.putObject(cTID('Usng'), cTID('Clrs'), swdesc);
    };
    case PresetType.PATTERNS: {
      var ref = new ActionReference();
        ref.putProperty(cTID('Prpr'), cTID('fsel'));
        ref.putEnumerated(cTID('Dcmn'), cTID('Ordn'), cTID('Trgt'));
        desc.putReference(cTID('Usng'), ref);
    };

    case PresetType.BRUSHES:
    case PresetType.CUSTOM_SHAPE:
    case PresetType.TOOL_PRESETS: {
      desc.putString(cTID('Nm  '), name);
      var ref = new ActionReference();
      ref.putProperty(cTID('Prpr'), cTID('CrnT'));
      ref.putEnumerated(cTID('capp'), cTID('Ordn'), cTID('Trgt'));
      desc.putReference(cTID('Usng'), ref);
    };

    case PresetType.SHAPING_CURVES:
    case PresetType.GRADIENTS:
    default: break;
  }
  var mode = (interactive ? DialogModes.ALL : DialogModes.NO);

  var xdesc = executeAction(cTID('Mk  '), desc, mode);
  return xdesc;
};


PresetsManager.prototype.populateDropdownList = function(presetType, ddlist,
                                                         none) {
  var self = this;
  var names = self.getNames(presetType);

  if (none) {
    ddlist.add("item", "None");
  }
  for (var i = 0; i < names.length; i++) {
    ddlist.add("item", names[i]);
  }
};

/*
//  //@include "xlib/PresetsManager.jsx"
//  PresetsManager.placeShape(doc, 'Cat Print', undefined, true);
//
*/
PresetsManager.placeShape = function(doc, name, bnds) {
  if (bnds) {
    if (bnds[0] instanceof UnitValue) {
      // assume we have an array of pixels...
      bnds[0].type = "px";
      var b = [];
      b.push(bnds[0].value);
      b.push(bnds[1].value);
      b.push(bnds[2].value);
      b.push(bnds[3].value);
      bnds = b;
    }
  } else {
    var ru = app.preferences.rulerUnits;
    app.preferences.rulerUnits = Units.PIXELS;
    bnds = [0,0,0,0];
    bnds[2] = doc.width.value / 4;
    bnds[3] = doc.height.value / 4;
    app.preferences.rulerUnits = ru;
  }

  function _ftn() {
    var desc = new ActionDescriptor();
    var ref = new ActionReference();
    ref.putClass(sTID('contentLayer'));
    desc.putReference(cTID('null'), ref);
    var tdesc = new ActionDescriptor();
    tdesc.putClass(cTID('Type'), sTID('solidColorLayer'));
    var sdesc = new ActionDescriptor();
    sdesc.putString(cTID('Nm  '), name);
    if (bnds) {
      sdesc.putUnitDouble(cTID('Left'), cTID('#Pxl'), bnds[0]);
      sdesc.putUnitDouble(cTID('Top '), cTID('#Pxl'), bnds[1]);
      sdesc.putUnitDouble(cTID('Rght'), cTID('#Pxl'), bnds[2]);
      sdesc.putUnitDouble(cTID('Btom'), cTID('#Pxl'), bnds[3]);
    }
    tdesc.putObject(cTID('Shp '), sTID('customShape'), sdesc);
    desc.putObject(cTID('Usng'), sTID('contentLayer'), tdesc);
    executeAction(cTID('Mk  '), desc, DialogModes.NO);
  }

  return Stdlib.wrapLC(doc, _ftn);
};


//
// PresetTypeManager
//
PTM = function(mgr, presetType) {
  var self = this;
  if (mgr) {
    self.mgr = mgr;
  } else {
    self.mgr = new PresetsManager();
  }
  self.presetType = presetType;
};

PTM.prototype.reset = function() {
  this.mgr.resetManager();
};
PTM.prototype.getCount = function() {
  return this.mgr.getCount(this.presetType);
};
PTM.prototype.getNames = function() {
  return this.mgr.getNames(this.presetType);
};
PTM.prototype.resetPreset = function() {
  return this.mgr.resetPreset(this.presetType);
};
PTM.prototype.saveAllPresets = function(fptr) {
  return this.mgr.saveAllPresets(this.presetType, fptr);
};
PTM.prototype.loadPresets = function(fptr, append) {
  return this.mgr.loadPresets(this.presetType, fptr, append);
};
PTM.prototype.appendPresets = function(fptr) {
  return this.mgr.appendPresets(this.presetType, fptr);
};
PTM.prototype.getIndexOfElement = function(name) {
  return this.mgr.getIndexOfElement(this.presetType, name);
};
PTM.prototype.deleteElementAt = function(index) {
  return this.mgr.deleteElementAt(this.presetType, index);
};
PTM.prototype.deleteElement = function(name) {
  return this.mgr.deleteElement(this.presetType, name);
};
PTM.prototype.renameByIndex = function(index, name) {
  return this.mgr.renameByIndex(this.presetType, index, name);
};
PTM.prototype.renameElement = function(oldName, newName) {
  return this.mgr.renameElement(this.presetType, oldName, newName);
};
PTM.prototype.saveElementByIndex = function(index, fptr) {
  return this.mgr.saveElementByIndex(this.presetType, index, fptr);
};
PTM.prototype.saveElement = function(name, fptr) {
  return this.mgr.saveElement(this.presetType, name, fptr);
};
PTM.prototype.newElement = function(name, obj, interactive) {
  return this.mgr.newElement(this.presetType, name, obj, interactive);
};
PTM.prototype.populateDropdownList = function(ddlist, none) {
  return this.mgr.populateDropdownList(this.presetType, ddlist, none);
};

// _newStyle = function(name) {
//   var desc = new ActionDescriptor();
//   var ref = new ActionReference();
//   ref.putClass( cTID('Styl') );
//   desc.putReference( cTID('null'), ref );
//   desc.putString( cTID('Nm  '), name );
//   var lref = new ActionReference();
//   lref.putEnumerated(cTID('Lyr '), cTID('Ordn'), cTID('Trgt') );
//   desc.putReference( cTID('Usng'), lref );
//   desc.putBoolean( sTID('blendOptions'), false );
//   desc.putBoolean( cTID('Lefx'), true );
//   executeAction( cTID('Mk  '), desc, DialogModes.NO );
// };

// // _styleToHex();
// _styleToHex = function() {
//   debugger;
//   var styleMgr = new PTM(new PresetsManager(), PresetType.STYLES);

//   var sname = "__TEMP_STYLE__";

//   //var file = new File(Folder.temp + "/" + sname + ".asl");
//   var file = new File("/c/work/tmp/" + sname + ".asl");
//   if (file.exists) {
//     file.remove();
//   }

//   var idx = styleMgr.getIndexOfElement(sname);
//   if (idx != -1) {
//     styleMgr.deleteElementAt(idx);
//   }

//   var bin = '';
//   try {
//     _newStyle(sname);
//     styleMgr.saveElement(sname, file);
//     //styleMgr.deleteElement(sname);

//     var bin = Stdlib.readFromFile(file, 'BINARY');
//     alert(file.length);
//     //file.remove();

//   } catch (e) {
//     throw e;
//   }

//   return  Stdlib.binToHex(bin);
// };

"PresetsManager.jsx";
// EOF
