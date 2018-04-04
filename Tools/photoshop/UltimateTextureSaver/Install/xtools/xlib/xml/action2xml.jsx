//
// action2xml.jsx
//   This script converts Action files and ActionDescriptor trees to
//   XML and back. This uses the new builtin XML support present in CS3+
//
// $Id: action2xml.jsx,v 1.13 2014/11/25 02:57:01 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//-target photoshop
//
//@show include
//
function __decodeString(s) { return s.toString(); };
function __encodeString(s) { return s; };

//
//================================== XMLWriter ================================
//
XMLWriter = function XMLWriter() {};

XMLWriter.logEnabled = true;

XMLWriter.log = function(msg) {
  if (XMLWriter.logEnabled) {
    Stdlib.log(msg);
  }
};


XMLWriter.toXML = function(obj, id, wrapObject) {
  var key = id;
  if (!key) key = obj.typename + "Object";

  var node;
  var root;

  if (wrapObject) {
    root = new XML("<ActionDocument/>");
    root.@date = Stdlib.toISODateString();
    root.@RcsId = "$Id: action2xml.jsx,v 1.13 2014/11/25 02:57:01 anonymous Exp $";

    node = XMLWriter.addChildNode(root, key, obj.typename, id);

  } else {
    node = new XML("<" + obj.typename + "/>");
    node.@key = key;
    root = node;
  }

  XMLWriter.write[obj.typename](node, key, obj);

  return root;
};

XMLWriter.serialize = function(obj, id, wrapObject) {
  var xml = XMLWriter.toXML(obj, id, wrapObject);
  return xml.toXMLString();
};

XMLWriter.__serialize = function(id) {
  return XMLWriter.serialize(this, id);
};

XMLWriter.__toXML = function(id) {
  return XMLWriter.toXML(this, id);
};

ActionFile.prototype.serialize         = XMLWriter.__serialize;
ActionsPaletteFile.prototype.serialize = XMLWriter.__serialize;
ActionsPalette.prototype.serialize     = XMLWriter.__serialize;
Action.prototype.serialize             = XMLWriter.__serialize;
ActionItem.prototype.serialize         = XMLWriter.__serialize;
ActionDescriptor.prototype.serialize   = XMLWriter.__serialize;

ActionFile.prototype.toXML         = XMLWriter.__toXML;
ActionsPaletteFile.prototype.toXML = XMLWriter.__toXML;
ActionsPalette.prototype.toXML     = XMLWriter.__toXML;
Action.prototype.toXML             = XMLWriter.__toXML;
ActionItem.prototype.toXML         = XMLWriter.__toXML;
ActionDescriptor.prototype.toXML   = XMLWriter.__toXML;

XMLWriter.addChildNode = function(parent, key, tag, id) {
  var node = new XML("<" + tag.toString() + "/>");

  if (key == undefined) {
    key = id;
  }

  if (key != undefined) {
    node.@key = key;
  }

  if (id != undefined && !isNaN(id)) {
    node.@id = id;

    var str = id2char(id);

    var symname = PSConstants.reverseNameLookup(id);
    if (symname) {
      node.@symname = symname;
    } else {
      if (str.length != 4) {
        node.@symname = str;
      }
    }

    var sym = PSConstants.reverseSymLookup(id);
    if (!sym) {
      sym = Stdlib.numberToAscii(id);
    }
    if (sym) {
      node.@sym = sym;
    }

  }

  parent.appendChild(node);

  return node;
};

//
// XMLWriter.write is indexed via:
//    typename properties
//    DescValueTypes
//    ReferenceFormTypes
//
XMLWriter.write = {};

XMLWriter.write["ActionsPalette"] = function(node, key, obj) {
  XMLWriter.log("XMLWriter.writeActionsPalette(" + obj.getName() + ")");

  var count = obj.getCount();
  node.@name = __encodeString(obj.getName());
  node.@count = count;

  for (var i = 0; i < count; i++) {
    var as = obj.byIndex(i);
    var child = XMLWriter.addChildNode(node, i+1, as.typename);

    XMLWriter.write[as.typename](child, undefined, as);
  }

  return node;
};
XMLWriter.write["ActionsPaletteFile"] = function(node, key, obj) {
  XMLWriter.log("XMLWriter.writeActionsPaletteFile(" + obj.getFile() + ")");
  node.@file = obj.getFile();
  node.@version = toNumber(obj.getVersion());

  var ap = obj.getActionsPalette();
  var child = XMLWriter.addChildNode(node, undefined, ap.typename);

  XMLWriter.write[ap.typename](child, undefined, ap);
  return node;
};

XMLWriter.write["ActionFile"] = function(node, key, obj) {
  XMLWriter.log("XMLWriter.writeActionFile(" + obj.getFile() + ")");
  node.@file = obj.getFile();
  var as = obj.getActionSet();
  var child = XMLWriter.addChildNode(node, undefined, as.typename);

  XMLWriter.write[as.typename](child, undefined, as);
  return node;
};
XMLWriter.write["ActionSet"] = function(node, key, obj) {
  XMLWriter.log("XMLWriter.writeActionSet(" + obj.getName() + ")");
  node.@version = obj.getVersion();
  node.@name = __encodeString(obj.getName());
  node.@expanded = new Boolean(obj.getExpanded());
  node.@count = new Number(obj.getCount());

  //$.level = 1; debugger;
  var max = obj.getCount();
  for (var i = 0; i < max; i++) {
    var act = obj.byIndex(i);
    var child = XMLWriter.addChildNode(node, i+1, act.typename);
    XMLWriter.write[act.typename](child, i+1, act);
  }

  return node;
};
XMLWriter.write["Action"] = function(node, key, obj) {
  XMLWriter.log("XMLWriter.writeAction(" + obj.getName() + ")");
  node.@name = __encodeString(obj.getName());

  if (obj.getIndex()) {
    node.@index = new Number(obj.getIndex());
  }
  if (obj.getShiftKey()) {
    node.@shiftKey = new Boolean(obj.getShiftKey());
  }
  if (obj.getCommandKey()) {
    node.@commandKey = new Boolean(obj.getCommandKey());
  }
  if (obj.getColorIndex()) {
    node.@colorIndex = new Number(obj.getColorIndex());
  }
  node.@expanded = new Boolean(obj.getExpanded());
  node.@count = new Number(obj.getCount());

  //$.level = 1; debugger;
  var max = obj.getCount();
  for (var i = 0; i < max; i++) {
    var item = obj.byIndex(i);
    var child = XMLWriter.addChildNode(node, item.getIdentifier(), item.typename);
    XMLWriter.write[item.typename](child, item.getIdentifier(), item);
  }

  return node;
};
XMLWriter.write["ActionItem"] = function(node, key, obj) {
  node.@expanded = new Boolean(obj.getExpanded());
  node.@enabled = new Boolean(obj.getEnabled());
  node.@withDialog = new Boolean(obj.getWithDialog());
  node.@dialogOptions = new Number(obj.getDialogOptions());
  node.@identifier = obj.getIdentifier();
  var ev = obj.getEvent();
  if (ev) node.@event = ev;
  var iid = obj.getItemID();
  if (iid) node.@itemID = iid;
  node.@name = obj.getName();
  node.@hasDescriptor = new Boolean(obj.containsDescriptor());

  var desc = obj.getDescriptor();
  if (desc) {
    var key = (obj.getEvent() ? obj.getEvent() : obj.getItemID());
    var child = XMLWriter.addChildNode(node, key, desc.typename);
    XMLWriter.write[desc.typename](child, key, desc);
  }

  return node;
};
XMLWriter.write["ActionDescriptor"] = function(node, key, obj) {
  node.@count = new Number(obj.count);

  for (var i = 0; i < obj.count; i++) {
    var k = obj.getKey(i);
    var t = obj.getType(k);
    //confirm("k = " + k + ", t = " + t);
    var child = XMLWriter.addChildNode(node, k, t, k);
    XMLWriter.write[t](child, k, obj, k);
  }

  return node;
};

XMLWriter.write[DescValueType.ALIASTYPE] = function(node, key, obj) {
  var f = obj.getPath(key);
  node.@path = f.absoluteURI;
  return node;
};
XMLWriter.write[DescValueType.BOOLEANTYPE] = function(node, key, obj) {
  var v = obj.getBoolean(key);
  eval('node.@boolean = new Boolean(v);');
  return node;
};
XMLWriter.write[DescValueType.CLASSTYPE] = function(node, key, obj) {
  var v = obj.getClass(key);
  node.@classString = id2char(v, "Class");
  eval('node.@class = id2charId(v, "Class");');
  return node;
};
XMLWriter.write[DescValueType.DOUBLETYPE] = function(node, key, obj) {
  var v = obj.getDouble(key);
  eval('node.@double = new Number(v);');
  return node;
};
XMLWriter.write[DescValueType.ENUMERATEDTYPE] = function(node, key, obj) {
  var t = obj.getEnumerationType(key);
  var v = obj.getEnumerationValue(key);

  node.@enumeratedTypeString = id2char(t, "Type");
  node.@enumeratedType = id2charId(t, "Type");
  node.@enumeratedValueString = id2char(v, "Enum");
  node.@enumeratedValue = id2charId(v, "Enum");

  return node;
};
XMLWriter.write[DescValueType.INTEGERTYPE] = function(node, key, obj) {
  var v = obj.getInteger(key);
  node.@integer = v;
  return node;
};
try {
  XMLWriter.write[DescValueType.LARGEINTEGERTYPE] = function(node, key, obj) {
    var v = obj.getLargeInteger(key);
    node.@largeInteger = v;
    return node;
  };
} catch(e) {}
XMLWriter.write[DescValueType.LISTTYPE] = function(node, key, obj, id) {
  var list = obj.getList(key);
  var child = XMLWriter.addChildNode(node, key, list.typename, id);
  XMLWriter.write["ActionList"](child, key, list);

  return node;
};
XMLWriter.write[DescValueType.OBJECTTYPE] = function(node, key, obj, id) {
  var t = obj.getObjectType(key);
  var v = obj.getObjectValue(key);

  node.@objectTypeString = id2char(t, "Class");
  node.@objectType = id2charId(t, "Class");
  node.@count = new Number(v.count);

  var child = XMLWriter.addChildNode(node, key, v.typename, id);
  XMLWriter.write["ActionDescriptor"](child, key, v);

  return node;
};
try {
  XMLWriter.write[DescValueType.RAWTYPE] = function(node, key, obj) {
    var v = obj.getData(key);
    var rawHex = Stdlib.binToHex(v, true);
    node.appendChild(rawHex);
//     node.@data = rawHex;
    return node;
  };
} catch (e) {}
XMLWriter.write[DescValueType.REFERENCETYPE] = function(node, key, obj, id) {
  var ref = obj.getReference(key);
  var child = XMLWriter.addChildNode(node, key, ref.typename, id);
  XMLWriter.write["ActionReference"](child, key, ref);
  return node;
};
XMLWriter.write[DescValueType.STRINGTYPE] = function(node, key, obj) {
  var v = obj.getString(key);
  node.@string = __encodeString(v);
  return node;
};
XMLWriter.write[DescValueType.UNITDOUBLE] = function(node, key, obj) {
  var t = obj.getUnitDoubleType(key);
  var v = obj.getUnitDoubleValue(key);

  node.@unitDoubleTypeString = id2char(t, "Unit");
  node.@unitDoubleType = id2charId(t, "Unit");
  node.@unitDoubleValue = new Number(v);

  return node;
};


XMLWriter.write["ActionReference"] = function(node, key, obj) {
  var ref = obj;
  var refCnt = 0;

  do {
    var t = undefined;
    var refItemId = undefined;
    try {
      t = ref.getForm();
      refItemId = ref.getDesiredClass();
    } catch (e) {
    }
    if (!t || !refItemId) {
      break;
    }
    refCnt++;
    var child = XMLWriter.addChildNode(node, refItemId, t, refItemId);
    XMLWriter.write[t](child, refItemId, ref);

    try { ref = ref.getContainer(); } catch (e) { ref = null; }
  } while (ref);

  node.@count = refCnt;

  return node;

//   var t = obj.getForm();
//   node.@form = t;
//   XMLWriter.write[t](node, id, obj);
};

XMLWriter.write[ReferenceFormType.CLASSTYPE] = function(node, key, obj) {
  var v = obj.getDesiredClass();
  node.@classString = id2char(v, "Class");
  eval('node.@class = id2charId(v, "Class");');

  return node;
};
XMLWriter.write[ReferenceFormType.ENUMERATED] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var t = obj.getEnumeratedType();
  var v = obj.getEnumeratedValue();

  node.@desiredClassString = id2char(c, "Class");
  node.@desiredClass = id2charId(c, "Class");
  node.@enumeratedTypeString = id2char(t, "Type");
  node.@enumeratedType = id2charId(t, "Type");
  node.@enumeratedValueString = id2char(v, "Enum");
  node.@enumeratedValue = id2charId(v, "Enum");

  return node;
};
XMLWriter.write[ReferenceFormType.IDENTIFIER] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getIdentifier();

  node.@desiredClassString = id2char(c, "Class");
  node.@desiredClass = id2charId(c, "Class");
  node.@identifier = v;

  return node;
};
XMLWriter.write[ReferenceFormType.INDEX] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getIndex();

  node.@desiredClassString = id2char(c, "Class");
  node.@desiredClass = id2charId(c, "Class");
  node.@index = new Number(v);

  return node;
};
XMLWriter.write[ReferenceFormType.NAME] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getName();

  node.@desiredClassString = id2char(c, "Class");
  node.@desiredClass = id2charId(c, "Class");
  node.@name = __encodeString(v);

  return node;
};
XMLWriter.write[ReferenceFormType.OFFSET] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getOffset();

  node.@desiredClassString = id2char(c, "Class");
  node.@desiredClass = id2charId(c, "Class");
  node.@offset = v;

  return node;
};
XMLWriter.write[ReferenceFormType.PROPERTY] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getProperty();

  node.@desiredClassString = id2char(c, "Class");
  node.@desiredClass = id2charId(c, "Class");
  node.@property = v;
  var psym = PSConstants.reverseNameLookup(v, "Key");
  if (psym) {
    node.@propertyName = psym;
  }
  node.@propertySym = id2charId(v, "Key");

  return node;
};

XMLWriter.write["ActionList"] = function(node, key, obj) {
  node.@count = new Number(obj.count);

  for (var i = 0; i < obj.count; i++) {
    var t = obj.getType(i);
    var child = XMLWriter.addChildNode(node, i, t);
    XMLWriter.write[t](child, i, obj);
  }

  return node;
};

//
//=============================== XMLReader ===================================
//
XMLReader = function XMLReader() {};

XMLReader.logEnabled = true;
XMLReader.log = function(msg) {
  if (XMLReader.logEnabled) {
    Stdlib.log(msg);
  }
};

//
//
//
XMLReader.readXML = function(str) {
  XMLReader.log("XMLReader.readXML(" + str.substring(0, 20) + ")");
  return undefined;
};

//
//
//
XMLReader.deserialize = function(domdoc) {
  var xml;

  XMLReader.log("XMLReader.deserialize()");

  if (domdoc instanceof File) {
    XMLReader.log("Reading " + domdoc.length + " bytes from " +
                  domdoc.toUIString());
    xml = Stdlib.readXMLFile(domdoc);

  } else if (domdoc.constructor == String) {
    XMLReader.log("Creating XML object from string");
    xml = new XML(domdoc);
    delete domdoc;
    $.gc();

  } else if (domdoc instanceof XML) {
    xml = domdoc;

  } else {
    return undefined;
  }

  var name = xml.name();
  var node = xml;
  if (name == "ActionDocument") {
    node = xml.elements()[0];
  }

  var id = node.id;
  var tag = node.name();

  var obj = XMLReader.read[tag](node, id);
  return obj;
};
XMLReader.__deserialize = function(xmlstr) {
  return XMLReader.deserialize(xmlstr);
};

ActionsPaletteFile.deserialize = XMLReader.__deserialize;
ActionFile.deserialize         = XMLReader.__deserialize;
ActionsPalette.deserialize     = XMLReader.__deserialize;
Action.deserialize             = XMLReader.__deserialize;
ActionDescriptor.deserialize   = XMLReader.__deserialize;

//
// XMLReader.read is indexed via:
//    typename properties
//    DescValueTypes
//    ReferenceFormTypes
//
XMLReader.read = {};
XMLReader.convertBoolean = function(b) {
  return b.toString() == "true";
};
XMLReader.convertNumber = function(n) {
  return (n == undefined) ? 0 : Number(n);
};
XMLReader.read["ActionsPaletteFile"] = function(node) {
  var apf = new ActionsPaletteFile();

  var v = node.@file;
  if (v) apf.file = new File(v);
  XMLReader.log("XMLReader.readActionsPalettFile(" + v + ")");

  apf.version = XMLReader.convertNumber(node.@version);

  var child = node.elements()[0];
  apf.actionsPalette = XMLReader.read[child.name()](child, apf);

  return apf;
};
XMLReader.read["ActionFile"] = function(node) {
  var af = new ActionFile();

  af.file = node.@file;
  if (af.file) af.file = new File(af.file);

  XMLReader.log("XMLReader.readActionFile(" + af.file + ")");

  var child = node.elements()[0];
  af.actionSet = XMLReader.read[child.name()](child, af);

  return af;
};
XMLReader.read["ActionsPalette"] = function(node, parent) {
  var ap = new ActionsPalette();

  ap.parent = parent;
  ap.name = __decodeString(node.@name);
  ap.count = XMLReader.convertNumber(node.@count);
  ap.version = XMLReader.convertNumber(node.@version);

  XMLReader.log("XMLReader.readActionsPalette(" + ap.name + ")");

 var children = node.elements();
  for (var i = 0; i < children.length(); i++) {
    var child = children[i];
    var as = XMLReader.read[child.name()](child, ap);
    ap.add(as);
  }

  return ap;
};
XMLReader.read["ActionSet"] = function(node, parent) {
  var as = new ActionSet();
  var v;

  as.parent = parent;
  as.name = __decodeString(node.@name);
  as.count = XMLReader.convertNumber(node.@count);
  as.version = XMLReader.convertNumber(node.@version);
  as.expanded = XMLReader.convertBoolean(node.@expanded);

  XMLReader.log("XMLReader.readActionSet(" + as.name + ")");

  //$.level = 1; debugger;

  var children = node.elements();
  for (var i = 0; i < children.length(); i++) {
    var child = children[i];
    var act = XMLReader.read[child.name()](child, as);
    as.add(act);
  }

  return as;
};
XMLReader.read["Action"] = function(node, parent) {
  var act = new Action();
  var v;

  act.parent = parent;
  act.name = __decodeString(node.@name);
  act.index = XMLReader.convertNumber(node.@index);
  act.shiftKey = XMLReader.convertBoolean(node.@shiftKey);
  act.commandKey = XMLReader.convertBoolean(node.@commandKey);
  act.colorIndex = XMLReader.convertNumber(node.@colorIndex);
  act.expanded = XMLReader.convertBoolean(node.@expanded);
  act.count = XMLReader.convertNumber(node.@count);

  XMLReader.log("XMLReader.readAction(" + act.name + ")");

  var children = node.elements();

  for (var i = 0; i < children.length(); i++) {
    var child = children[i];
    var ai = XMLReader.read[child.name()](child, act);
    act.add(ai);
  }

  return act;
};
XMLReader.read["ActionItem"] = function(node, parent) {
  var ai = new ActionItem();
  var v;

  ai.parent = parent;

  ai.expanded = XMLReader.convertBoolean(node.@expanded);
  ai.enabled = XMLReader.convertBoolean(node.@enabled);
  ai.withDialog = XMLReader.convertBoolean(node.@withDialog);
  ai.dialogOptions =
     XMLReader.convertNumber(node.@dialogOptions);

  ai.identifier = node.@identifier.toString();
  ai.event = __decodeString(node.@event);
  ai.itemID = node.@itemID;
  ai.name = __decodeString(node.@name);
  ai.hasDescriptor =
     XMLReader.convertBoolean(node.@hasDescriptor);

  if (ai.hasDescriptor) {
    var child = node.elements()[0];
    ai.descriptor = XMLReader.read[child.name()](child, ai);
  }

  return ai;
};

XMLReader._getKey = function(node) {
  var str = node.@key.toString();
  if (!str) {
    Error.runtimeError(9003, "XML node missing key property");
  }
  var key = toNumber(str);
  if (isNaN(key)) {
    Error.runtimeError(9003, "Node key is not a number");
  }
  if (!(key & 0xFF000000)) {
    // We have a StringID
    var symname = node.@symname.toString();
    if (!symname) {
      Error.runtimeError(9003, "Node missing StringID");
    }
    key = sTID(symname);
  }

  return key;
};

//
//=========================== ActionDescriptor ================================
//
XMLReader.read["ActionDescriptor"] = function(node, key, obj, type) {
  var ad = new ActionDescriptor();

  var children = node.elements();
  // assert(children.length == count);

  for (var i = 0; i < children.length(); i++) {
    var child = children[i];
    var ckey = XMLReader._getKey(child);
    XMLReader.read[child.name()](child, ckey, ad);
  }

  // obj is currently undefined if this is a top-level ActionDescriptor
  if (obj) {
    if (key != undefined) {
      obj.putObject(key, type, ad);
    } else {
      obj.putObject(type, ad);
    }
  }
  return ad;
};

XMLReader.read[DescValueType.BOOLEANTYPE] = function(node, id, obj) {
  eval('var v = node.@boolean;');
  obj.putBoolean(id, v == "true");
};

XMLReader.read[DescValueType.CLASSTYPE] = function(node, id, obj) {
  eval('var v = node.@class;');
  obj.putClass(id, xTID(v));
};
XMLReader.read[DescValueType.DOUBLETYPE] = function(node, id, obj) {
  eval('var v = node.@double;');
  obj.putDouble(id, toNumber(v));
};
XMLReader.read[DescValueType.ENUMERATEDTYPE] = function(node, id, obj) {
  var t = node.@enumeratedType;
  var v = node.@enumeratedValue;
  obj.putEnumerated(id, xTID(t), xTID(v));
};
XMLReader.read[DescValueType.INTEGERTYPE] = function(node, id, obj) {
  var v = node.@integer;
  obj.putInteger(id, toNumber(v));
};
try {
  XMLReader.read[DescValueType.LARGEINTEGERTYPE] = function(node, id, obj) {
    var v = node.@largeInteger;
    obj.putLargeInteger(id, toNumber(v));
  };
} catch (e) {}
XMLReader.read[DescValueType.LISTTYPE] = function(node, id, obj) {
  var child = node.ActionList;
  XMLReader.read["ActionList"](child, id, obj);
};
XMLReader.read[DescValueType.OBJECTTYPE] = function(node, id, obj) {
  var type = node.@objectType;
  var count = node.@count;
  var child = node.ActionDescriptor;
  XMLReader.read["ActionDescriptor"](child, id, obj, xTID(type));
};
try {
  XMLReader.read[DescValueType.RAWTYPE] = function(node, id, obj) {
//     var v = node.@data;
    // var v = node.getContent();
    var v = node.child(0).toString().trim();
    var raw = Stdlib.hexToBin(v);
    obj.putData(id, raw);
  };
} catch (e) {}
XMLReader.read[DescValueType.ALIASTYPE] = function(node, id, obj) {
  var v = node.@path;
  if (!v) {
    Error.runtimeError(9003, "'path' attribute missing for Alias type");
  }

  try {
    obj.putPath(id, new File(v));

  } catch (e) {
    var fname = v;
    var fld = "~/";
    if (v.contains('/')) {
      var m = v.match(/(.*)\/([^\/]+)$/);
      fld = m[1];
      if (!Stdlib.createFolder(fld)) {
        fld = "~/";
      } else {
        fld += '/';
      }
      fname = m[2];
    }
    try {
      obj.putPath(id, File(fld + fname));
    } catch (e) {
      Error.runtimeError(9003, "Failed to create Alias from XML for: " + v);
    }
  }
};
XMLReader.read[DescValueType.REFERENCETYPE] = function(node, id, obj) {
  var child = node.ActionReference;
  XMLReader.read["ActionReference"](child, id, obj);
};
XMLReader.read[DescValueType.STRINGTYPE] = function(node, id, obj) {
  var v = node.@string;
  obj.putString(id, __decodeString(v));
};
XMLReader.read[DescValueType.UNITDOUBLE] = function(node, id, obj) {
  var t = node.@unitDoubleType;
  var v = node.@unitDoubleValue;
  obj.putUnitDouble(id, xTID(t), toNumber(v));
};


//
//============================== ActionList ===================================
//
XMLReader.lread = {};
XMLReader.lread["ActionDescriptor"] = function(node, obj) {
  XMLReader.read["ActionDescriptor"](node, undefined, obj);
}
XMLReader.lread["ActionList"] = function(node, obj) {
  XMLReader.read["ActionList"](node, undefined, obj);
}
XMLReader.lread["ActionReference"] = function(node, obj) {
  XMLReader.read["ActionReference"](node, undefined, obj);
}

XMLReader.read["ActionList"] = function(node, id, obj) {
  var count = node.@count;
  var children = node.elements();

  var lst = new ActionList();
  for (var i = 0; i < children.length(); i++) {
    var child = children[i];
    XMLReader.lread[child.name()](child, lst);
  }
  if (id) {
    obj.putList(id, lst);
  } else {
    obj.putList(lst);
  }
  return lst;
};
XMLReader.lread[DescValueType.BOOLEANTYPE] = function(node, obj) {
  eval('var v = node.@boolean;');
  obj.putBoolean(v == "true");
};
XMLReader.lread[DescValueType.CLASSTYPE] = function(node, obj) {
  eval('var v = node.@class;');
  obj.putClass(xTID(v));
};
XMLReader.lread[DescValueType.DOUBLETYPE] = function(node, obj) {
  eval('var v = node.@double;');
  obj.putDouble(toNumber(v));
};
XMLReader.lread[DescValueType.ENUMERATEDTYPE] = function(node, obj) {
  var t = node.@enumeratedType;
  var v = node.@enumeratedValue;
  obj.putEnumerated(xTID(t), xTID(v));
};
XMLReader.lread[DescValueType.INTEGERTYPE] = function(node, obj) {
  var v = node.@integer;
  obj.putInteger(toNumber(v));
};
try {
  XMLReader.lread[DescValueType.LARGEINTEGERTYPE] = function(node, obj) {
    var v = node.@largeInteger;
    obj.putLargeInteger(toNumber(v));
  };
} catch (e) {}
XMLReader.lread[DescValueType.LISTTYPE] = function(node, obj) {
  var child = node.ActionList;
  XMLReader.read["ActionList"](child, undefined, obj);
};
XMLReader.lread[DescValueType.OBJECTTYPE] = function(node, obj) {
  var type = node.@objectType;
  var count = node.@count;
  var child = node.ActionDescriptor;
  XMLReader.read["ActionDescriptor"](child, undefined, obj, xTID(type));
};
try {
  XMLReader.lread[DescValueType.RAWTYPE] = function(node, obj) {
    // var v = node.@data;
    // var v = node.getContent();
    var v = node.getFirstChild().toString();
    var raw = Stdlib.hexToBin(v);
    obj.putData(raw);
  };
} catch (e) {}
XMLReader.lread[DescValueType.ALIASTYPE] = function(node, obj) {
  var v = node.@path;
  obj.putPath(new File(v));
};
XMLReader.lread[DescValueType.REFERENCETYPE] = function(node, obj) {
  var child = node.ActionReference;
  XMLReader.read["ActionReference"](child, undefined, obj);
};
XMLReader.lread[DescValueType.STRINGTYPE] = function(node, obj) {
  var v = node.@string;
  obj.putString(__decodeString(v));
};
XMLReader.lread[DescValueType.UNITDOUBLE] = function(node, obj) {
  var t = node.@unitDoubleType;
  var v = node.@unitDoubleValue;
  obj.putUnitDouble(xTID(t), toNumber(v));
};


//
//============================ ActionReference ================================
//
XMLReader.read["ActionReference"] = function(node, id, obj) {
  var count = node.@count;
  var children = node.elements();

  var ref = new ActionReference();

  for (var i = 0; i < children.length(); i++) {
    var child = children[i];
    XMLReader.read[child.name()](child, ref);
  }

  if (id) {
    obj.putReference(id, ref);
  } else {
    obj.putReference(ref);
  }
  return ref;
};

XMLReader.read[ReferenceFormType.CLASSTYPE] = function(node, obj) {
  eval('var v = node.@class;');
  obj.putClass(xTID(v));
};
XMLReader.read[ReferenceFormType.ENUMERATED] = function(node, obj) {
  var c = node.@desiredClass;
  var t = node.@enumeratedType;
  var v = node.@enumeratedValue;
  obj.putEnumerated(xTID(c), xTID(t), xTID(v));
};
XMLReader.read[ReferenceFormType.IDENTIFIER] = function(node, obj) {
  var c = node.@desiredClass;
  var v = node.@identifier;
  obj.putIdentifier(xTID(c), toNumber(v));
};
XMLReader.read[ReferenceFormType.INDEX] = function(node, obj) {
  var c = node.@desiredClass;
  var v = node.@index;
  obj.putIndex(xTID(c), toNumber(v));
};
XMLReader.read[ReferenceFormType.NAME] = function(node, obj) {
  var c = node.@desiredClass;
  var v = __decodeString(node.@name);
  obj.putName(xTID(c), v);
};
XMLReader.read[ReferenceFormType.OFFSET] = function(node, obj) {
  var c = node.@desiredClass;
  var v = node.@offset;
  obj.putOffset(xTID(c), toNumber(v));
};
XMLReader.read[ReferenceFormType.PROPERTY] = function(node, obj) {
  var c = node.@desiredClass;
  var v = node.@property;
  obj.putProperty(xTID(c), toNumber(v));
};

"action2xml.jsx";
// EOF
