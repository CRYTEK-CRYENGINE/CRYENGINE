//
// Styles
//   This file contains a variety of functions for working with styles and,
//   in particular, layer styles.
//
// Functions:
//   getLayerStyleDescriptor(doc, layer) does work although it may need more
//                                       extensive testing.
//   setLayerStyleDescriptor(doc, layer, dec) does NOT yet work. See note below
//
//   newStyle(name)
//   loadStyles(file)
//   saveAllStyles(file)
//   saveStyle(file, index)
//   saveCurrentStyle(file)
//   deleteStyle(index)
//   getPresetManager()
//   getPresets()
//
// $Id: Styles.js,v 1.15 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

Styles = function() {};
Styles.TEMP_NAME = "$$$TempStyle";
Styles.TEMP_FOLDER = Folder.temp;  // Folder.temp;

/* new layer style */
Styles.newStyle = function(name) {
  function _ftn() {
    var desc = new ActionDescriptor();
    var ref = new ActionReference();
    ref.putClass( PSKey.Style );
    desc.putReference( cTID('null'), ref );
    desc.putString( PSKey.Name, name );
    var lref = new ActionReference();
    lref.putEnumerated( PSClass.Layer, PSType.Ordinal, PSEnum.Target );
    desc.putReference( PSKey.Using, lref );
    desc.putBoolean( PSString.blendOptions, true );
    desc.putBoolean( PSClass.LayerEffects, true );
    executeAction( PSEvent.Make, desc, DialogModes.NO );
  }

  _ftn();
};

/* load styles file */
Styles.loadStyles = function(file) {
  function _ftn() {
    var desc = new ActionDescriptor();
    var ref = new ActionReference();
    ref.putProperty( PSClass.Property, PSKey.Style );
    ref.putEnumerated( PSClass.Application, PSType.Ordinal, PSEnum.Target );
    desc.putReference( cTID('null'), ref );
    desc.putPath( PSKey.To, file);
    desc.putBoolean( PSKey.Append, true);
    executeAction( PSEvent.Set, desc, DialogModes.NO );
  }

  _ftn();
};

/* save all styles */
Styles.saveAllStyles = function(file) {
  function _ftn() {
    var desc125 = new ActionDescriptor();
    desc125.putPath( cTID('null'), file);
    var ref128 = new ActionReference();
    ref128.putProperty( PSClass.Property, PSKey.Style );
    ref128.putEnumerated( PSClass.Application, PSType.Ordinal, PSEnum.Target );
    desc125.putReference( PSKey.To, ref128 );
    executeAction( PSEvent.Set, desc125, DialogModes.NO );
  }

  _ftn();
};

/* save style to file */
Styles.saveStyle = function(file, index) {

  if (file.exists) {
    file.remove();
  }

  function _ftn() {
    var desc = new ActionDescriptor();
    desc.putPath( cTID('null'), file);
    var list = new ActionList();
    var ref = new ActionReference();
    ref.putIndex( PSKey.Style, index + 1);
    list.putReference( ref );
    desc.putList( PSKey.To, list );
    executeAction( PSEvent.Set, desc, DialogModes.NO );
  }

  _ftn();
};

Styles.saveCurrentStyle = function(file) {
  var idx = Styles.getPresets().length;

  Styles.newStyle(Styles.TEMP_NAME);
  Styles.saveStyle(file, idx);
  Styles.deleteStyle(idx);
};

/* delete style by index */
Styles.deleteStyle = function(index) {
  function _ftn() {
    var desc = new ActionDescriptor();
    var list = new ActionList();
    var ref = new ActionReference();
    ref.putIndex( PSKey.Style, index + 1);
    list.putReference( ref );
    desc.putList( cTID('null'), list );
    executeAction( PSEvent.Delete, desc, DialogModes.NO );
  }

  _ftn();
};

Styles.getPresetManager = function() {
  var classApplication = cTID('capp');
  var typeOrdinal      = cTID('Ordn');
  var enumTarget       = cTID('Trgt');

  var ref = new ActionReference();
  ref.putEnumerated(classApplication, typeOrdinal, enumTarget);

  var appDesc = app.executeActionGet(ref);
  return appDesc.getList(sTID('presetManager'));
};

Styles.getPresets = function() {
  var styleKey = cTID('StyC');
  var names = [];

  var mgr = Styles.getPresetManager();
  var max = mgr.count;

  for (var i = 0; i < max; i++) {
    var objType = mgr.getObjectType(i);
    if (objType == styleKey) {
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
  }

  return names;
};

Styles.loadTypeIDs = function () {
  var needsDefs = true;
  var lvl = $.level;
  $level = 0;
  try {
    PSClass;
    needsDefs = false;
  } catch (e) {
  }
  $.level = lvl;

  cTID = function(s) { return app.charIDToTypeID(s); };
  sTID = function(s) { return app.stringIDToTypeID(s); };
  xTID = function(s) {
    if (s.constructor == Number) {
      return s;
    }
    if (s.constructor == String) {
      if (s.length != 4) return sTID(s);
      try { return cTID(s); } catch (e) { return sTID(s); }
    }
    throw "Bad typeid: " + s;
  };

  if (needsDefs) {
    PSClass  = function() {};
    PSEnum   = function() {};
    PSEvent  = function() {};
    PSForm   = function() {};
    PSKey    = function() {};
    PSType   = function() {};
    PSUnit   = function() {};
    PSString = function() {};
  }

  PSClass.Application = cTID('capp');
  PSClass.Layer = cTID('Lyr ');
  PSClass.LayerEffects = cTID('Lefx');
  PSClass.Property = cTID('Prpr');
  PSClass.Document = cTID('Dcmn');
  PSClass.FileInfo = cTID("FlIn");
  PSEnum.Scale = cTID('Scl ');
  PSEnum.Target = cTID('Trgt');
  PSEvent.Delete = cTID('Dlt ');
  PSEvent.Make = cTID('Mk  ');
  PSEvent.Set = cTID('setd');
  PSKey.Append = cTID('Appe');
  PSKey.FileInfo = cTID("FlIn");
  PSKey.LayerEffects = cTID("Lefx");
  PSKey.Name = cTID('Nm  ');
  PSKey.Style = cTID('Styl');
  PSKey.To = cTID('T   ');
  PSKey.Using = cTID('Usng');
  PSType.Ordinal = cTID('Ordn');
  PSUnit.Percent = cTID('#Prc');

  PSString.blendOptions = sTID('blendOptions');
  PSString.Null = cTID('null');
};

Styles.loadTypeIDs();

Styles.throwError = function(e) {
  throw e;
};
Styles.readFromFile = function(fptr) {
  var file = Styles.convertFptr(fptr);
  file.open("r") || Styles.throwError("Unable to open input file \"" +
                                      file + "\".\r" + file.error);
  file.encoding = 'BINARY';
  var str = '';
  str = file.read(file.length);
  file.close();
  return str;
};
Styles.writeToFile = function(fptr, str) {
  var file = Styles.convertFptr(fptr);
  file.open("w") || Styles.throwError("Unable to open output file \"" +
                                      file + "\".\r" + file.error);
  file.encoding = 'BINARY';
  file.write(str);
  file.close();
};
Styles.convertFptr = function(fptr) {
  var f;
  if (fptr.constructor == String) {
    f = File(fptr);
  } else if (fptr instanceof File || fptr instanceof Folder) {
    f = fptr;
  } else {
    throw "Bad file \"" + fptr + "\" specified.";
  }
  return f;
};

//
// Styles.loadDescriptor
//   This function loads information from an Layer Styles file. This only works
//   if there is a single style in the file.
//   The input parameter can either be a File or a string representation of
//      a file.
//
//   The descriptor returned has two elements:
//      FileInfo ('FlIn') contains styles palette information about the style,
//         specifically its Name and Identifier.
//      LayerEffects contains the actually Layer Style information. This is
//         the block of information that you would normally see in the
//         ScriptingListener log file whenever you add or change a layer style.
//         This descriptor can be used to set the layer style of a layer.
//
Styles.loadFileDescriptor = function(fptr) {
  var str = Styles.readFromFile(fptr);

  if (str.substring(2, 6) != "8BSL") {
    throw "File is not a Styles file.";
  }

  // trim of the file header stuff
  str = str.slice(0x14);

  // There may be extra stuff at the beginning of the file,
  // like other non-standard object definitions (like patterns).
  // This is the simplest check (for the AD format number) for that.
  if (str.charCodeAt(3) != 0x10) {

    // Now, search for a marker that would only occur in the Layer Styles Def
    var idx = str.search(/Nm  TEXT/);
    if (idx == -1) {
      throw "Layer Style not found in file";
    }
    // The beginning of the AD def is 26 characters before "Nm  TEXT"
    str = str.slice(idx - 26);
  }

  // Read in the first descriptor
  var desc = new ActionDescriptor();
  desc.fromStream(str);

  // Create a StylesFile descriptor for return
  var sfdesc = new ActionDescriptor();

  // The first AD has some styles file info, actually just the Name and ID
  sfdesc.putObject(PSClass.FileInfo, PSKey.FileInfo, desc);

  // Skip past the first AD definition
  str = str.slice(desc.toStream().length);

  // And read in the next
  desc = new ActionDescriptor();
  desc.fromStream(str);

  // if the layer style doesn't really contain anything,
  // there won't be a descriptor for it, so we don't place
  // anything in the return descriptor
  if (desc.hasKey(PSKey.LayerEffects)) {
    var lefx = desc.getObjectValue(PSKey.LayerEffects);

    // Now store the LayerEffects AD
    sfdesc.putObject(PSKey.LayerEffects, PSClass.LayerEffects, lefx);
  }

  if (desc.hasKey(PSString.blendOptions)) {
    var blndOpts = desc.getObjectValue(PSString.blendOptions);
    sfdesc.putObject(PSString.blendOptions, PSString.blendOptions, blndOpts);
  }

  // And return our manufactured AD
  return sfdesc;
};

//
// Styles.getLayerStyleDescriptor
//   Returns the LayerStyles descriptor for the specified doc/layer
//
//   This is implemented by
//      1) creating a new named style based on the current layer
//      2) saving the new style to a file
//      3) removing the named style from PS
//      4) loading the LayerStyle file descriptor
//      5) returning the LayerStyle descriptor found in the file descriptor
//
// With 'complete' set to true, it returns the entire style descriptor, including
// the file name and blend options
//
Styles.getLayerStyleDescriptor = function(doc, layer, complete) {
  var ad;
  var al;

  complete = !!complete;

  if (doc != app.activeDocument) {
    ad = app.activeDocument;
    app.activeDocument = doc;
  }

  if (layer != doc.activeLayer) {
    al = doc.activeLayer;
    doc.activeLayer = layer;
  }

  var file;
  var lsdesc = undefined;
  try {
    var idx = Styles.getPresets().length;
    file = new File(Styles.TEMP_FOLDER + '/' + Styles.TEMP_NAME + ".asl");

    Styles.newStyle(Styles.TEMP_NAME);
    Styles.saveStyle(file, idx);
    Styles.deleteStyle(idx);

    var desc = Styles.loadFileDescriptor(file);
    if (complete) {
      lsdesc = desc;
    } else if (desc.hasKey(PSKey.LayerEffects)) {
      lsdesc = desc.getObjectValue(PSKey.LayerEffects);
    }

  } catch (e) {
    alert(e.toSource().replace(/,/g, '\r'));

  } finally {
    if (file) {
      file.remove();
    }
    if (al) {
      doc.activeLayer = al;
    }
    if (ad) {
      app.activeDocument = ad;
    }
  }
  return lsdesc;
};

Styles.setLayerStyleDescriptor = function(doc, layer, lsdesc) {
  var ad;
  var al;

  if (doc != app.activeDocument) {
    ad = app.activeDocument;
    app.activeDocument = doc;
  }

  if (layer != doc.activeLayer) {
    al = doc.activeLayer;
    doc.activeLayer = layer;
  }

  try {
    var ref = new ActionReference();
    ref.putProperty(PSClass.Property, PSKey.LayerEffects);
    ref.putEnumerated(PSClass.Layer, PSType.Ordinal, PSEnum.Target);

    var desc = new ActionDescriptor();
    desc.putReference(PSString.Null, ref);

    desc.putObject(PSKey.To, PSClass.LayerEffects, lsdesc);

    executeAction(cTID('setd'), desc, DialogModes.NO);

  } catch (e) {
    alert(e.toSource().replace(/,/g, '\r'));

  } finally {
    if (al) {
      doc.activeLayer = al;
    }
    if (ad) {
      app.activeDocument = ad;
    }
  }
};

Styles.defineStyle = function(str, font, color) {
  function _styleDesc() {
    var descSet = new ActionDescriptor();
    var refProp = new ActionReference();
    refProp.putProperty( cTID('Prpr'), cTID('Lefx') );
    refProp.putEnumerated( cTID('Lyr '), cTID('Ordn'), cTID('Trgt') );
    descSet.putReference( cTID('null'), refProp );

    var descStyle = new ActionDescriptor();
    descStyle.putUnitDouble( cTID('Scl '), cTID('#Prc'), 100.000000 );
    descSet.putObject( cTID('T   '), cTID('Lefx'), descStyle );

    return descSet;
  };

  var doc = Stdlib.newDocument("New Style", "RGBM", 660, 480, 72, 16);
  doc.activeLayer.isBackgroundLayer = false;

  if (!str) {
    str = "Watermark Text";
  }

  var text = Stdlib.addTextLayer(doc, str, "Watermark", 128);

  if (!color) {
    color = Stdlib.createRGBColor(158, 158, 158);
  }
  text.textItem.color = color;
  if (font) {
    text.textItem.font = font;
  }

  var sdesc = undefined;
  var desc = _styleDesc();
  try {
    sdesc = executeAction(cTID('setd'), desc, DialogModes.ALL);

  } catch (e) {
    if (e.number != 8007) {
      throw e;
    }
  } finally {
    doc.close(SaveOptions.DONOTSAVECHANGES);
  }

  return sdesc;
};

//
//include "xlib/PSConstants.js"
//include "xlib/stdlib.js"
//include "xlib/Stream.js"
//include "xlib/Action.js"
//include "xlib/xml/atn2xml.jsx"

Styles.testXML = function(desc) {
  // if you want to dump the XML for a layer styles descriptor
  // fix up the include directives above and make a call like this:
  Stdlib.writeToFile("~/LayerStyle.xml", desc.toXML());
};


Styles.test = function() {
  var doc = app.activeDocument;

// this is some leftover test code. Leave it here for now.
//  $.level = 1; debugger;
//   Styles.saveCurrentStyle(new File("/c/work/Blank.asl"));
//   return;

//   var desc = Styles.loadFileDescriptor("/c/work/Blank.asl");
//   var sdesc = desc.getObjectValue(PSKey.LayerEffects);
//   Styles.writeToFile("/c/work/b2.bin", gdesc.toStream());

  var layer0 = doc.artLayers[0];
  var gdesc = Styles.getLayerStyleDescriptor(doc, layer0);

  if (!gdesc) {
    alert("There is no layer style associated with the layer");
    return;
  }

//   this will set the layer style to another layer.
//   var layer1 = doc.artLayers[1];
//   Styles.setLayerStyleDescriptor(doc, layer1, gdesc);

  if (!gdesc.hasKey(cTID('FrFX'))) {  // look for a stroke effect
    return;
  }
  var frfx = gdesc.getObjectValue(cTID('FrFX'));
  if (!frfx.hasKey(cTID('Clr '))) {   // look for the color
    return;
  }
  var clr = frfx.getObjectValue(cTID('Clr '));
  // we should really check that the objectType is RGBC
  var r = clr.getDouble(cTID('Rd  '));
  var g = clr.getDouble(cTID('Grn '));
  var b = clr.getDouble(cTID('Bl  '));
  if (r == 0 && g == 0xFF && b == 0xFF) {
    return;
  }
  clr.putDouble(cTID('Rd  '), 0);
  clr.putDouble(cTID('Grn '), 0xFF);
  clr.putDouble(cTID('Bl  '), 0xFF);

  frfx.putObject(cTID('Clr '), cTID('RGBC'), clr);
  gdesc.putObject(cTID('FrFX'), cTID('FrFX'), frfx);

  Styles.setLayerStyleDescriptor(doc, layer0, gdesc);
};

//Styles.test();

"Styles.jsx";
// EOF
