//
// atn2xml.jsx
//   This script converts action files and ActionDescriptor trees to
//   XML and back.
//
// $Id: atn2xml.jsx,v 1.34 2014/11/25 02:57:01 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//-target photoshop
//
//@show include
//
var atn2xml_jsx = true;

function __decodeString(s) {
  return s;
  // $.writeln("__decodeString(" + s + ")");
  return decodeURIComponent(s);
};
function __encodeString(s) {
  return s;
  // $.writeln("__encodeString(" + s + ")");
  return encodeURIComponent(s);
};


//@include "xlib/xml/xmlsax.js"
//@include "xlib/xml/xmlw3cdom.js"

var atn2xmlTestMode = false;

//
// W3C DOM API extensions
//
DOMNode.prototype.addAttribute = function(name, val) {
  var self = this;
  var attrNode = self.getAttributeNode(name);
  if (!attrNode) {
    attrNode = self.getOwnerDocument().createAttribute(name);
  }
  attrNode.setValue(val);
  self.setAttributeNode(attrNode);
};
DOMDocument.prototype.addAttribute = DOMNode.prototype.addAttribute;
DOMElement.prototype.addAttribute  = DOMNode.prototype.addAttribute;

DOMNode.prototype.getChildNode = function(name) {
  var self = this;
  var children = self.getChildNodes();
  var childCnt = children.getLength();

  for (var i = 0; i < childCnt; i++) {
    var child = children.item(i);
    if (child.getNodeType() == DOMNode.ELEMENT_NODE &&
        child.getTagName() == name) {
      return child;
    }
  }
  return undefined;
};
DOMDocument.prototype.getChildNode = DOMNode.prototype.getChildNode;
DOMElement.prototype.getChildNode  = DOMNode.prototype.getChildNode;

DOMNode.prototype.getElements = function(name) {
  var self = this;
  var children = self.getChildNodes();
  var childCnt = children.getLength();
  var elements = [];

  for (var i = 0; i < childCnt; i++) {
    var child = children.item(i);
    if (child.getNodeType() == DOMNode.ELEMENT_NODE) {
      elements.push(child);
    }
  }
  return elements;
};
DOMDocument.prototype.getElements = DOMNode.prototype.getElements;
DOMElement.prototype.getElements  = DOMNode.prototype.getElements;

//
// The original DOMElement.getAttribute call return a String object, which
// causes all kinds of havoc. We wrap the method here to make sure it returns
// a string primitive
//
DOMElement.prototype._getAttribute = function(name) {
  var v = this._orig_getAttribute(name);
  return String(v);
};

if (DOMElement.prototype.getAttribute != DOMElement.prototype._getAttribute) {
  DOMElement.prototype._orig_getAttribute = DOMElement.prototype.getAttribute;
  DOMElement.prototype.getAttribute = DOMElement.prototype._getAttribute;;
}

// this is missing from the w3c api
DOMNode.prototype.getAttribute = function(name) {
  var ret = "";
  var attr = this.attributes.getNamedItem(name);
  if (attr) {
    ret = String(attr.value);
  }
  return ret; // if Attribute exists, return its value, otherwise, return ""
};

// this is missing from the w3c api
DOMNode.prototype.getTagName = function() {
  return this.tagName;
};


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

  var domdoc = new DOMDocument(new DOMImplementation());
  var node;
  if (wrapObject) {
    node = domdoc.createElement("ActionDocument");
    node.addAttribute("date", Stdlib.toISODateString());
    node.addAttribute("RcsId",
              "$Id: atn2xml.jsx,v 1.34 2014/11/25 02:57:01 anonymous Exp $");
  } else {
    node = domdoc.createElement(obj.typename);
  }
  domdoc.appendChild(node);

  if (wrapObject) {
    node = XMLWriter.addChildNode(node, key, obj.typename, id);
  } else {
    node.addAttribute("key", key);
  }

  XMLWriter.write[obj.typename](node, key, obj);
  return domdoc;
};

XMLWriter.serialize = function(obj, id, wrapObject) {
  return XMLWriter.toXML(obj, id, wrapObject).toString();
};

XMLWriter.__serialize = function(id) {
  return XMLWriter.serialize(this, id);
};

XMLWriter.__toXML = function(id) {
  return XMLWriter.serialize(this, id);
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
  tag = tag.toString();
  var node = parent.getOwnerDocument().createElement(tag);
  parent.appendChild(node);

  if (key == undefined) {
    key = id;
  }
  if (key != undefined) {
    if (key && isNaN(key)) {
      node.@key = key;
    } else {
      node.@key = new Number(key);
    }
  }

  if (id != undefined && !isNaN(id)) {
    node.@id = id;

    var symname = PSConstants.reverseNameLookup(id);
    if (symname) {
      node.@symname = symname;
    }

    var sym = PSConstants.reverseSymLookup(id);
    if (!sym) {
      sym = Stdlib.numberToAscii(id);
    }
    if (sym) {
      node.@sym = sym;
    }
  }

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
  node.addAttribute("name", __encodeString(obj.getName()));
  node.addAttribute("count", count);

  for (var i = 0; i < count; i++) {
    var as = obj.byIndex(i);
    var child = XMLWriter.addChildNode(node, i+1, as.typename);

    XMLWriter.write[as.typename](child, undefined, as);
  }

  return node;
};
XMLWriter.write["ActionsPaletteFile"] = function(node, key, obj) {
  XMLWriter.log("XMLWriter.writeActionsPaletteFile(" + obj.getFile() + ")");
  node.addAttribute("file", obj.getFile());
  node.addAttribute("version", new Number(obj.getVersion()));

  var ap = obj.getActionsPalette();
  var child = XMLWriter.addChildNode(node, undefined, ap.typename);

  XMLWriter.write[ap.typename](child, undefined, ap);
  return node;
};

XMLWriter.write["ActionFile"] = function(node, key, obj) {
  XMLWriter.log("XMLWriter.writeActionFile(" + obj.getFile() + ")");
  node.addAttribute("file", obj.getFile());
  var as = obj.getActionSet();
  var child = XMLWriter.addChildNode(node, undefined, as.typename);

  XMLWriter.write[as.typename](child, undefined, as);
  return node;
};
XMLWriter.write["ActionSet"] = function(node, key, obj) {
  XMLWriter.log("XMLWriter.writeActionSet(" + obj.getName() + ")");
  node.addAttribute("version", obj.getVersion());
  node.addAttribute("name", __encodeString(obj.getName()));
  node.addAttribute("expanded", new Boolean(obj.getExpanded()));
  node.addAttribute("count", new Number(obj.getCount()));

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
  node.addAttribute("name", __encodeString(obj.getName()));

  if (obj.getIndex()) {
    node.addAttribute("index", new Number(obj.getIndex()));
  }
  if (obj.getShiftKey()) {
    node.addAttribute("shiftKey", new Boolean(obj.getShiftKey()));
  }
  if (obj.getCommandKey()) {
    node.addAttribute("commandKey", new Boolean(obj.getCommandKey()));
  }
  if (obj.getColorIndex()) {
    node.addAttribute("colorIndex", new Number(obj.getColorIndex()));
  }
  node.addAttribute("expanded", new Boolean(obj.getExpanded()));
  node.addAttribute("count", new Number(obj.getCount()));

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
  node.addAttribute("expanded", new Boolean(obj.getExpanded()));
  node.addAttribute("enabled", new Boolean(obj.getEnabled()));
  node.addAttribute("withDialog", new Boolean(obj.getWithDialog()));
  node.addAttribute("dialogOptions", new Number(obj.getDialogOptions()));
  node.addAttribute("identifier", obj.getIdentifier());
  var ev = obj.getEvent();
  if (ev) node.addAttribute("event", ev);
  var iid = obj.getItemID();
  if (iid) node.addAttribute("itemID", iid);
  node.addAttribute("name", obj.getName());
  node.addAttribute("hasDescriptor", new Boolean(obj.containsDescriptor()));

  var desc = obj.getDescriptor();
  if (desc) {
    var key = (obj.getEvent() ? obj.getEvent() : obj.getItemID());
    var child = XMLWriter.addChildNode(node, key, desc.typename);
    XMLWriter.write[desc.typename](child, key, desc);
  }

  return node;
};
XMLWriter.write["ActionDescriptor"] = function(node, key, obj) {
  node.addAttribute("count", new Number(obj.count));

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
  node.addAttribute("path", f.absoluteURI);
  return node;
};
XMLWriter.write[DescValueType.BOOLEANTYPE] = function(node, key, obj) {
  var v = obj.getBoolean(key);
  node.addAttribute("boolean", new Boolean(v));
  return node;
};
XMLWriter.write[DescValueType.CLASSTYPE] = function(node, key, obj) {
  var v = obj.getClass(key);
  node.addAttribute("classString", id2char(v, "Class"));
  node.addAttribute("class", id2charId(v, "Class"));
  return node;
};
XMLWriter.write[DescValueType.DOUBLETYPE] = function(node, key, obj) {
  var v = obj.getDouble(key);
  node.addAttribute("double", new Number(v));
  return node;
};
XMLWriter.write[DescValueType.ENUMERATEDTYPE] = function(node, key, obj) {
  var t = obj.getEnumerationType(key);
  var v = obj.getEnumerationValue(key);

  node.addAttribute("enumeratedTypeString", id2char(t, "Type"));
  node.addAttribute("enumeratedType", id2charId(t, "Type"));
  node.addAttribute("enumeratedValueString", id2char(v, "Enum"));
  node.addAttribute("enumeratedValue", id2charId(v, "Enum"));

  return node;
};
XMLWriter.write[DescValueType.INTEGERTYPE] = function(node, key, obj) {
  var v = obj.getInteger(key);
  node.addAttribute("integer", Number(v));
  return node;
};
try {
  XMLWriter.write[DescValueType.LARGEINTEGERTYPE] = function(node, key, obj) {
    var v = obj.getLargeInteger(key);
    node.addAttribute("largeInteger", Number(v));
    return node;
  };
} catch (e) {}
XMLWriter.write[DescValueType.LISTTYPE] = function(node, key, obj, id) {
  var list = obj.getList(key);
  var child = XMLWriter.addChildNode(node, key, list.typename, id);
  XMLWriter.write["ActionList"](child, key, list);

  return node;
};
XMLWriter.write[DescValueType.OBJECTTYPE] = function(node, key, obj, id) {
  var t = obj.getObjectType(key);
  var v = obj.getObjectValue(key);

  node.addAttribute("objectTypeString", id2char(t, "Class"));
  node.addAttribute("objectType", id2charId(t, "Class"));
  node.addAttribute("count", new Number(v.count));

  var child = XMLWriter.addChildNode(node, key, v.typename, id);
  XMLWriter.write["ActionDescriptor"](child, key, v);

  return node;
};
try {
  XMLWriter.write[DescValueType.RAWTYPE] = function(node, key, obj) {
    var v = obj.getData(key);
    var rawHex = Stdlib.binToHex(v, true);
    var txtNode = node.getOwnerDocument().createTextNode('\n' + rawHex + '\n');
    node.appendChild(txtNode);
//  node.addAttribute("data", rawHex);
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
  node.addAttribute("string", __encodeString(v));
  return node;
};
XMLWriter.write[DescValueType.UNITDOUBLE] = function(node, key, obj) {
  var t = obj.getUnitDoubleType(key);
  var v = obj.getUnitDoubleValue(key);

  node.addAttribute("unitDoubleTypeString", id2char(t, "Unit"));
  node.addAttribute("unitDoubleType", id2charId(t, "Unit"));
  node.addAttribute("unitDoubleValue", new Number(v));

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

  node.addAttribute("count", refCnt);

  return node;

//   var t = obj.getForm();
//   node.addAttribute("form", t);
//   XMLWriter.write[t](node, id, obj);
};

XMLWriter.write[ReferenceFormType.CLASSTYPE] = function(node, key, obj) {
  var v = obj.getDesiredClass();
  node.addAttribute("classString", id2char(v, "Class"));
  node.addAttribute("class", id2charId(v, "Class"));

  return node;
};
XMLWriter.write[ReferenceFormType.ENUMERATED] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var t = obj.getEnumeratedType();
  var v = obj.getEnumeratedValue();

  node.addAttribute("desiredClassString", id2char(c, "Class"));
  node.addAttribute("desiredClass", id2charId(c, "Class"));
  node.addAttribute("enumeratedTypeString", id2char(t, "Type"));
  node.addAttribute("enumeratedType", id2charId(t, "Type"));
  node.addAttribute("enumeratedValueString", id2char(v, "Enum"));
  node.addAttribute("enumeratedValue", id2charId(v, "Enum"));

  return node;
};
XMLWriter.write[ReferenceFormType.IDENTIFIER] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getIdentifier();

  node.addAttribute("desiredClassString", id2char(c, "Class"));
  node.addAttribute("desiredClass", id2charId(c, "Class"));
  node.addAttribute("identifier", v);

  return node;
};
XMLWriter.write[ReferenceFormType.INDEX] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getIndex();

  node.addAttribute("desiredClassString", id2char(c, "Class"));
  node.addAttribute("desiredClass", id2charId(c, "Class"));
  node.addAttribute("index", new Number(v));

  return node;
};
XMLWriter.write[ReferenceFormType.NAME] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getName();

  node.addAttribute("desiredClassString", id2char(c, "Class"));
  node.addAttribute("desiredClass", id2charId(c, "Class"));
  node.addAttribute("name", __encodeString(v));

  return node;
};
XMLWriter.write[ReferenceFormType.OFFSET] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getOffset();

  node.addAttribute("desiredClassString", id2char(c, "Class"));
  node.addAttribute("desiredClass", id2charId(c, "Class"));
  node.addAttribute("offset", v);

  return node;
};
XMLWriter.write[ReferenceFormType.PROPERTY] = function(node, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getProperty();

  node.addAttribute("desiredClassString", id2char(c, "Class"));
  node.addAttribute("desiredClass", id2charId(c, "Class"));
  node.addAttribute("property", v);
  var psym = PSConstants.reverseNameLookup(v, "Key");
  if (psym) {
    node.addAttribute("propertyName", psym);
  }
  node.addAttribute("propertySym", id2charId(v, "Key"));

  return node;
};

XMLWriter.write["ActionList"] = function(node, key, obj) {
  node.addAttribute("count", new Number(obj.count));

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
  return new DOMImplementation().loadXML(str);
};

//
//
//
XMLReader.deserialize = function(domdoc) {
  XMLReader.log("XMLReader.deserialize()");
  if (domdoc instanceof File) {
    XMLReader.log("Reading " + domdoc.length + " bytes from " +
                  domdoc.toUIString());
    domdoc = Stdlib.readFromFile(domdoc, "UTF-8");
  }
  if (domdoc.constructor == String) {
    var xmlstr = XMLReader.readXML(domdoc);
    delete domdoc;
    $.gc();
    domdoc = xmlstr;
  }
  var node = domdoc.getChildNode("ActionDocument");
  var objNode;

  if (node) {
    var elements = node.getElements();
    objNode = elements[0];
  } else {
    objNode = domdoc.getElements()[0];
  }
  var id = objNode.getAttribute("id");
  var tag = objNode.getTagName();
  var obj = XMLReader.read[tag](objNode, id);
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

  var v = node.getAttribute("file");
  if (v) apf.file = new File(v);
  XMLReader.log("XMLReader.readActionsPalettFile(" + v + ")");

  apf.version = XMLReader.convertNumber(node.getAttribute("version"));

  var child = node.getElements()[0];
  apf.actionsPalette = XMLReader.read[child.getTagName()](child, apf);

  return apf;
};
XMLReader.read["ActionFile"] = function(node) {
  var af = new ActionFile();

  af.file = node.getAttribute("file");
  if (af.file) af.file = new File(af.file);

  XMLReader.log("XMLReader.readActionFile(" + af.file + ")");

  var child = node.getElements()[0];
  af.actionSet = XMLReader.read[child.getTagName()](child, af);

  return af;
};
XMLReader.read["ActionsPalette"] = function(node, parent) {
  var ap = new ActionsPalette();

  ap.parent = parent;
  ap.name = __decodeString(node.getAttribute("name"));
  ap.count = XMLReader.convertNumber(node.getAttribute("count"));
  ap.version = XMLReader.convertNumber(node.getAttribute("version"));

  XMLReader.log("XMLReader.readActionsPalette(" + ap.name + ")");

 var children = node.getElements();
  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    var as = XMLReader.read[child.getTagName()](child, ap);
    ap.add(as);
  }

  return ap;
};
XMLReader.read["ActionSet"] = function(node, parent) {
  var as = new ActionSet();
  var v;

  as.parent = parent;
  as.name = __decodeString(node.getAttribute("name"));
  as.count = XMLReader.convertNumber(node.getAttribute("count"));
  as.version = XMLReader.convertNumber(node.getAttribute("version"));
  as.expanded = XMLReader.convertBoolean(node.getAttribute("expanded"));

  XMLReader.log("XMLReader.readActionSet(" + as.name + ")");

  //$.level = 1; debugger;

  var children = node.getElements();
  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    var act = XMLReader.read[child.getTagName()](child, as);
    as.add(act);
  }

  return as;
};
XMLReader.read["Action"] = function(node, parent) {
  var act = new Action();
  var v;

  act.parent = parent;
  act.name = __decodeString(node.getAttribute("name"));
  act.index = XMLReader.convertNumber(node.getAttribute("index"));
  act.shiftKey = XMLReader.convertBoolean(node.getAttribute("shiftKey"));
  act.commandKey = XMLReader.convertBoolean(node.getAttribute("commandKey"));
  act.colorIndex = XMLReader.convertNumber(node.getAttribute("colorIndex"));
  act.expanded = XMLReader.convertBoolean(node.getAttribute("expanded"));
  act.count = XMLReader.convertNumber(node.getAttribute("count"));

  XMLReader.log("XMLReader.readAction(" + act.name + ")");

  var children = node.getElements();

  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    var ai = XMLReader.read[child.getTagName()](child, act);
    act.add(ai);
  }

  return act;
};
XMLReader.read["ActionItem"] = function(node, parent) {
  var ai = new ActionItem();
  var v;

  ai.parent = parent;

  ai.expanded = XMLReader.convertBoolean(node.getAttribute("expanded"));
  ai.enabled = XMLReader.convertBoolean(node.getAttribute("enabled"));
  ai.withDialog = XMLReader.convertBoolean(node.getAttribute("withDialog"));
  ai.dialogOptions =
     XMLReader.convertNumber(node.getAttribute("dialogOptions"));

  ai.identifier = node.getAttribute("identifier");
  ai.event = node.getAttribute("event");
  ai.itemID = node.getAttribute("itemID");
  ai.name = node.getAttribute("name");
  ai.hasDescriptor =
     XMLReader.convertBoolean(node.getAttribute("hasDescriptor"));

  if (ai.hasDescriptor) {
    var child = node.getElements()[0];
    ai.descriptor = XMLReader.read[child.getTagName()](child, ai);
  }

  return ai;
};

//
//=========================== ActionDescriptor ================================
//
XMLReader.read["ActionDescriptor"] = function(node, key, obj, type) {
  var ad = new ActionDescriptor();

  var children = node.getElements();
  // assert(children.length == count);

  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    var ckey = child.getAttribute("key");
    XMLReader.read[child.getTagName()](child, ckey, ad);
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
  var v = node.getAttribute("boolean");
  obj.putBoolean(id, v == "true");
};

XMLReader.read[DescValueType.CLASSTYPE] = function(node, id, obj) {
  var v = node.getAttribute("class");
  obj.putClass(id, xTID(v));
};
XMLReader.read[DescValueType.DOUBLETYPE] = function(node, id, obj) {
  var v = node.getAttribute("double");
  obj.putDouble(id, v);
};
XMLReader.read[DescValueType.ENUMERATEDTYPE] = function(node, id, obj) {
  var t = node.getAttribute("enumeratedType");
  var v = node.getAttribute("enumeratedValue");
  obj.putEnumerated(id, xTID(t), xTID(v));
};
XMLReader.read[DescValueType.INTEGERTYPE] = function(node, id, obj) {
  var v = node.getAttribute("integer");
  obj.putInteger(id, v);
};
try {
  XMLReader.read[DescValueType.LARGEINTEGERTYPE] = function(node, id, obj) {
    var v = node.getAttribute("largeInteger");
    obj.putLargeInteger(id, v);
  };
} catch (e) {}
XMLReader.read[DescValueType.LISTTYPE] = function(node, id, obj) {
  var child = node.getChildNode("ActionList");
  XMLReader.read["ActionList"](child, id, obj);
};
XMLReader.read[DescValueType.OBJECTTYPE] = function(node, id, obj) {
  var type = node.getAttribute("objectType");
  var count = node.getAttribute("count");
  var child = node.getChildNode("ActionDescriptor");
  XMLReader.read["ActionDescriptor"](child, id, obj, xTID(type));
};
try {
  XMLReader.read[DescValueType.RAWTYPE] = function(node, id, obj) {
//     var v = node.getAttribute("data");
    // var v = node.getContent();
    var v = node.getFirstChild().toString();
    var raw = Stdlib.hexToBin(v);
    obj.putData(id, raw);
  };
} catch (e) {}
XMLReader.read[DescValueType.ALIASTYPE] = function(node, id, obj) {
  var v = node.getAttribute("path");
  if (!v) {
    var msg = ("path attribute missing for Alias type:\n" +
               node.parent.toString());
    XMLReader.log(msg);
    alert(msg);
  }

  try {
    obj.putPath(id, new File(v));

  } catch (e) {
    try {
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
      obj.putPath(id, File(fld + fname));

    } catch (e) {
      var msg = Stdlib.exceptionMessage(e);
      XMLReader.log(msg);
    }
  }
};
XMLReader.read[DescValueType.REFERENCETYPE] = function(node, id, obj) {
  var child = node.getChildNode("ActionReference");
  XMLReader.read["ActionReference"](child, id, obj);
};
XMLReader.read[DescValueType.STRINGTYPE] = function(node, id, obj) {
  var v = node.getAttribute("string");
  obj.putString(id, __decodeString(v));
};
XMLReader.read[DescValueType.UNITDOUBLE] = function(node, id, obj) {
  var t = node.getAttribute("unitDoubleType");
  var v = node.getAttribute("unitDoubleValue");
  obj.putUnitDouble(id, xTID(t), v);
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
  var count = node.getAttribute("count");
  var children = node.getElements();

  var lst = new ActionList();
  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    XMLReader.lread[child.getTagName()](child, lst);
  }
  if (id) {
    obj.putList(id, lst);
  } else {
    obj.putList(lst);
  }
  return lst;
};
XMLReader.lread[DescValueType.BOOLEANTYPE] = function(node, obj) {
  var v = node.getAttribute("boolean");
  obj.putBoolean(v == "true");
};
XMLReader.lread[DescValueType.CLASSTYPE] = function(node, obj) {
  var v = node.getAttribute("class");
  obj.putClass(xTID(v));
};
XMLReader.lread[DescValueType.DOUBLETYPE] = function(node, obj) {
  var v = node.getAttribute("double");
  obj.putDouble(v);
};
XMLReader.lread[DescValueType.ENUMERATEDTYPE] = function(node, obj) {
  var t = node.getAttribute("enumeratedType");
  var v = node.getAttribute("enumeratedValue");
  obj.putEnumerated(xTID(t), xTID(v));
};
XMLReader.lread[DescValueType.INTEGERTYPE] = function(node, obj) {
  var v = node.getAttribute("integer");
  obj.putInteger(v);
};
try {
  XMLReader.lread[DescValueType.LARGEINTEGERTYPE] = function(node, obj) {
    var v = node.getAttribute("largeInteger");
    obj.putLargeInteger(v);
  };
} catch (e) {}
XMLReader.lread[DescValueType.LISTTYPE] = function(node, obj) {
  var child = node.getChildNode("ActionList");
  XMLReader.read["ActionList"](child, undefined, obj);
};
XMLReader.lread[DescValueType.OBJECTTYPE] = function(node, obj) {
  var type = node.getAttribute("objectType");
  var count = node.getAttribute("count");
  var child = node.getChildNode("ActionDescriptor");
  XMLReader.read["ActionDescriptor"](child, undefined, obj, xTID(type));
};
try {
  XMLReader.lread[DescValueType.RAWTYPE] = function(node, obj) {
//     var v = node.getAttribute("data");
    // var v = node.getContent();
    var v = node.getFirstChild().toString();
    var raw = Stdlib.hexToBin(v);
    obj.putData(raw);
  };
} catch (e) {}
XMLReader.lread[DescValueType.ALIASTYPE] = function(node, obj) {
  var v = node.getAttribute("path");
  obj.putPath(new File(v));
};
XMLReader.lread[DescValueType.REFERENCETYPE] = function(node, obj) {
  var child = node.getChildNode("ActionReference");
  XMLReader.read["ActionReference"](child, undefined, obj);
};
XMLReader.lread[DescValueType.STRINGTYPE] = function(node, obj) {
  var v = node.getAttribute("string");
  obj.putString(__decodeString(v));
};
XMLReader.lread[DescValueType.UNITDOUBLE] = function(node, obj) {
  var t = node.getAttribute("unitDoubleType");
  var v = node.getAttribute("unitDoubleValue");
  obj.putUnitDouble(xTID(t), v);
};


//
//============================ ActionReference ================================
//
XMLReader.read["ActionReference"] = function(node, id, obj) {
  var count = node.getAttribute("count");
  var children = node.getElements();

  var ref = new ActionReference();

  for (var i = 0; i < children.length; i++) {
    var child = children[i];
    XMLReader.read[child.getTagName()](child, ref);
  }

  if (id) {
    obj.putReference(id, ref);
  } else {
    obj.putReference(ref);
  }
  return ref;
};

XMLReader.read[ReferenceFormType.CLASSTYPE] = function(node, obj) {
  var v = node.getAttribute("class");
  obj.putClass(xTID(v));
};
XMLReader.read[ReferenceFormType.ENUMERATED] = function(node, obj) {
  var c = node.getAttribute("desiredClass");
  var t = node.getAttribute("enumeratedType");
  var v = node.getAttribute("enumeratedValue");
  obj.putEnumerated(xTID(c), xTID(t), xTID(v));
};
XMLReader.read[ReferenceFormType.IDENTIFIER] = function(node, obj) {
  var c = node.getAttribute("desiredClass");
  var v = node.getAttribute("identifier");
  obj.putIdentifier(xTID(c), v);
};
XMLReader.read[ReferenceFormType.INDEX] = function(node, obj) {
  var c = node.getAttribute("desiredClass");
  var v = node.getAttribute("index");
  obj.putIndex(xTID(c), v);
};
XMLReader.read[ReferenceFormType.NAME] = function(node, obj) {
  var c = node.getAttribute("desiredClass");
  var v = __decodeString(node.getAttribute("name"));
  obj.putName(xTID(c), v);
};
XMLReader.read[ReferenceFormType.OFFSET] = function(node, obj) {
  var c = node.getAttribute("desiredClass");
  var v = node.getAttribute("offset");
  obj.putOffset(xTID(c), v);
};
XMLReader.read[ReferenceFormType.PROPERTY] = function(node, obj) {
  var c = node.getAttribute("desiredClass");
  var v = node.getAttribute("property");
  obj.putProperty(xTID(c), v);
};


var atn2xml_test;
if (atn2xmlTestMode && !atn2xml_test) {
  eval('//@include "xlib/xml/atn2xml-test.jsx";\r');
}

"atn2xml.jsx";
// EOF
