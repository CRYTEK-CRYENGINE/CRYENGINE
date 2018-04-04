//
// Getter.jsx
//   This script accesses just everything that can be reached from the
//   Javascript API. This includes:
//           - Application Info
//               (Preferences, PresetManager Info, Fonts, etc...)
//           for each open document
//             - Document Info
//             - Background Info
//             - Layer Info
//             - Channel Info
//             - Path Info
//             - History Info
//
//   If you can get information you want from the Javascript API, take a
//   look at the output of this script. This has everything you can get to,
//   although you do have to go through the ActionDescriptor API to get at
//   it.
//
// $Id: Getter.jsx,v 1.27 2014/11/25 02:32:13 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//
app;
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//-include "xlib/PSConstants.js"
//-include "xlib/stdlib.js"
//-include "xlib/Action.js"
//-include "xlib/xml/action2xml.jsx"

//$.level = 1; debugger;

Getter = function() {};

Getter.getDescriptorElement = function(desc, key) {
  if (!desc.hasKey(key)) {
    return undefined;
  }
  var typeID = desc.getType(key);
  var v;
  switch (typeID) {
    case DescValueType.ALIASTYPE:   v = desc.getPath(key); break;
    case DescValueType.BOOLEANTYPE: v = desc.getBoolean(key); break;
    case DescValueType.CLASSTYPE:   v = desc.getClass(key); break;
    case DescValueType.DOUBLETYPE:  v = desc.getDouble(key); break;
    case DescValueType.ENUMERATEDTYPE:
      v = new Object();
      v.type = desc.getEnumerationType(key);
      v.value = desc.getEnumerationValue(key);
      break;
    case DescValueType.INTEGERTYPE: v = desc.getInteger(key); break;
    case DescValueType.LARGEINTEGERTYPE: v = desc.getLargeInteger(key); break;
    case DescValueType.LISTTYPE:    v = desc.getList(key); break;
    case DescValueType.OBJECTTYPE:
      v = new Object();
      v.type = desc.getObjectType(key);
      v.value = desc.getObjectValue(key);
      break;
    case DescValueType.RAWTYPE:       v = desc.getData(key); break;
    case DescValueType.REFERENCETYPE: v = desc.getReference(key); break;
    case DescValueType.STRINGTYPE:    v = desc.getString(key); break;
    case DescValueType.UNITDOUBLETYPE:
      v = new Object();
      v.type = desc.getUnitDoubleType(key);
      v.value = desc.getUnitDoubleValue(key);
      break;
  }

  return v;
};

Getter.getInfo = function(classID, key) {
  var ref = new ActionReference();
  if (isNaN(classID)) {
    classID = cTID(classID);
  }
  if (key != undefined) {
    if (isNaN(key)) {
      key = cTID(key);
    }
    ref.putProperty(PSClass.Property, key);
  }

  ref.putEnumerated(classID, PSType.Ordinal, PSEnum.Target);

  var desc = app.executeActionGet(ref);

  return (key ? Getter.getDescriptorElement(desc, key) : desc);
};

Getter.getInfoByIndex = function(index, classID, key) {
  var ref = new ActionReference();
  if (isNaN(classID)) {
    classID = cTID(classID);
  }
  if (key != undefined) {
    if (isNaN(key)) {
      key = cTID(key);
    }
    ref.putProperty(PSClass.Property, key);
  }
  ref.putIndex(classID, index);

  var desc = app.executeActionGet(ref);

  // XXX potential problems with objects, units, and enums
  return (key ? Getter.getDescriptorElement(desc, key) : desc);
};

Getter.getInfoByIndexIndex = function(childIndex, parentIndex, childID,
                                      parentID, key) {
  var ref = new ActionReference();
  if (isNaN(childID)) {
    childID = cTID(childID);
  }
  if (isNaN(parentID)) {
    parentID = cTID(parentID);
  }
  if (key != undefined) {
    if (isNaN(key)) {
      key = cTID(key);
    }
    ref.putProperty(PSClass.Property, key);
  }
  ref.putIndex(childID, childIndex);
  ref.putIndex(parentID, parentIndex);

  var desc = app.executeActionGet(ref);

  // XXX potential problems with objects, units, and enums
  return (key ? Getter.getDescriptorElement(desc, key) : desc);
};

Getter.getApplicationInfo = function() {
  var desc = Getter.getInfo(PSClass.Application);
  return desc.serialize("ApplicationInfo");
};
Getter.getActionInfo = function() {
  var pal = new ActionsPalette();
  pal.readRuntime();
  return pal.serialize("ApplicationPalette");
};

Getter.getDocumentIndex = function(doc) {
  var docs = app.documents;
  for (var i = 0; i < docs.length; i++) {
    if (doc == docs[i]) {
      return i+1;
    }
  }
  return undefined;
};
Getter.getDocumentInfo = function(doc, docIndex) {
  if (docIndex == undefined) { docIndex = Getter.GetDocumentIndex(doc); }

  var desc = Getter.getInfoByIndex(docIndex, PSClass.Document);
  return desc.serialize("Document-" + docIndex);
};
Getter.getBackgroundInfo = function(doc, docIndex) {
  if (docIndex == undefined) { docIndex = Getter.getDocumentIndex(doc); }

  var ref = new ActionReference();
  ref.putProperty(PSClass.BackgroundLayer, PSKey.Background);
  ref.putIndex(PSClass.Document, docIndex);

  var str = '';
  try {
    var desc = app.executeActionGet(ref);
    str = desc.serialize("BackgroundInfo");
  } catch (e) {
  }
  return str;
};
Getter.getLayerInfo = function(doc, docIndex) {
  if (docIndex == undefined) { docIndex = Getter.getDocumentIndex(doc); }

  var str = '';
  var al = doc.activeLayer;
  try {
    for (var i = 1; true; i++) {
      // this will throw an exception when we're done...
      Stdlib.selectLayerByIndex(doc, i);

      // get the layer descriptor
      var ref = new ActionReference();
      ref.putIndex(charIDToTypeID( "Lyr " ), i);
      var ldesc = executeActionGet(ref);

      var name = ldesc.getString(cTID('Nm  '));

      try {
        var desc = Getter.getInfoByIndexIndex(i, docIndex, PSClass.Layer,
                                              PSClass.Document);
        str += desc.serialize("Layer-" + name);
      } catch (e) {
      }

      var ref = new ActionReference();
      ref.putEnumerated(PSClass.Path, PSType.Ordinal, PSString.vectorMask);
      var lpdesc;
      try {
        lpdesc = app.executeActionGet(ref);
        str += lpdesc.serialize("VectorMask-" + name);
      } catch (e) {
      }

      ref = new ActionReference();
      ref.putEnumerated(PSClass.Path, PSType.Ordinal, PSString.textShape);
      try {
        lpdesc = app.executeActionGet(ref);
        str += lpdesc.serialize("TextShape-" + name);
      } catch (e) {
      }
    }
  } catch (e) {
    // catch the layer selection exception
  } finally {
    doc.activeLayer = al;
  }
  return str;
};
Getter.getChannelInfo = function(doc, docIndex) {
  if (docIndex == undefined) { docIndex = Getter.getDocumentIndex(doc); }

  var str = '';
  var channels = doc.channels;
  for (var i = 1; i <= channels.length; i++) {
    var channel = channels[i-1];
    var desc = Getter.getInfoByIndexIndex(i, docIndex, PSClass.Channel,
                                          PSClass.Document);
    str += desc.serialize("Channel-" + channel.name);
  }
  return str;
};
Getter.getPathInfo = function(doc, docIndex) {
  if (docIndex == undefined) { docIndex = Getter.getDocumentIndex(doc); }

  var str = '';
  var ad = app.activeDocument;
  var al = doc.activeLayer;

  try {
    app.activeDocument = doc;

    var totalPaths = Getter.getInfo(PSClass.Document, PSKey.NumberOfPaths);
    var pathCount = 0;

    for (var i = 1; i <= totalPaths; i++) {
      // first, get normal paths
      var pdesc;
      try {
        pdesc = Getter.getInfoByIndex(i, PSClass.Path);
      } catch (e) {
        break;
      }
      str += pdesc.serialize("Path-" + i);
      pathCount++;
    }

    if (pathCount < totalPaths) {
      // try to get the workpath
      try {
        var ref = new ActionReference();
        ref.putProperty(PSClass.Path, PSKey.WorkPath);
        var wpdesc = app.executeActionGet(ref);
        str += wpdesc.serialize("WorkPath");
        pathCount++;
      } catch (e) {
      }
    }

    // other paths are layer specific
  } finally {
    if (al) doc.activeLayer = al;
    app.activeDocument = ad;
  }

  return str;
};
Getter.getHistoryInfo = function(doc, docIndex) {
  if (docIndex == undefined) { docIndex = Getter.getDocumentIndex(doc); }

  var str = '';
  try {
    var ad = app.activeDocument;
    app.activeDocument = doc;

    var max = Getter.getInfo(PSClass.HistoryState, PSKey.Count);

    for (var i = 1; i <= max; i++) {
      var hdesc = Getter.getInfoByIndex(i, PSClass.HistoryState);
      str += hdesc.serialize("History State-" + i);
    }
  } finally {
    app.activeDocument = ad;
  }

  return str;
};

"Getter.jsx";
