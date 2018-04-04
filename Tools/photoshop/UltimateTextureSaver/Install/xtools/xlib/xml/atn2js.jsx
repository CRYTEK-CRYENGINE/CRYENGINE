//
// atn2js.jsx
//   This script converts action files and ActionDescriptor trees to
//   XML and back.
//
// $Id: atn2js.jsx,v 1.45 2014/11/25 02:57:01 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//-includepath "/c/Program Files/Adobe/xtools"
//
//-include "xlib/PSConstants.js"
//-include "xlib/stdlib.js"
//-include "xlib/Action.js"
//-include "xlib/xml/atn2bin.jsx"
//

JSWriter = function() {
  var self = this;
  self.descCount = 1;
  self.refCount  = 1;
  self.listCount = 1;
  self.write = JSWriter.prototype.write;
  self.symbols = false;
};
JSWriter.prototype.typename = "JSWriter";
JSWriter.prototype.toString = function() { return "[JSWriter]"; };

JSWriter.DESCRIPTOR_PREFIX = "desc";
JSWriter.LIST_PREFIX = "list";
JSWriter.REFERENCE_PREFIX = "ref";

JSWriter.isDesc = function(obj) {
  return (obj.typename == "ActionDescriptor" ||
          obj.startsWith(JSWriter.DESCRIPTOR_PREFIX));
};
JSWriter.isList = function(obj) {
  return (obj.typename == "ActionList" ||
          obj.startsWith(JSWriter.LIST_PREFIX));
};
JSWriter.isRef = function(obj) {
  return (obj.typename == "ActionReference" ||
          obj.startsWith(JSWriter.REFERENCE_PREFIX));
};
JSWriter.needsKey = function(obj, key) {
  return JSWriter.isDesc(obj) && key != undefined;
};

JSWriter.prototype.write = function(type, parent, key, obj, x) {
  var self = this;
  return JSWriter.write[type].call(self, parent, key, obj, x);
};

//
// If obj is an Action, arg (which is optional) can either be a boolean
// or a string. If it is true, the name "__main" is used for the name
// of the execution function generated at the bottom of the file and
// __main_loadSymbols is the name of the function used to define symbols
// required by this script. If arg is a string, it is used as the name of
// the script execution function and arg + "_loadSymbols" loads the needed
// symbols
//
JSWriter.prototype.writeToString = function(obj, arg) {
  var self = this;
  var str = self.header();

  str += 'cTID = function(s) { return app.charIDToTypeID(s); };\n';
  str += 'sTID = function(s) { return app.stringIDToTypeID(s); };\n\n';

  //$.level = 1; debugger;

  str += self.write(obj.typename, undefined, undefined, obj);

  if (arg == true) {
    arg = "_main";
  }
  str += self.trailer(arg);

  if (arg && obj.typename == "Action") {
    str += self.exec(obj, arg);
  }

  str += "// EOF\n";

  return str;
};
JSWriter.prototype.writeScriptHeader = function(obj, outfile) {
  var f = Stdlib.convertFptr(outfile);
  var str = ("#target photoshop\n"+
             "//\n" +
             "// " + decodeURI(f.name)  + "\n" +
             "//\n\n");
  return str;
};
JSWriter.prototype.writeScriptTrailer = function(obj, outfile) {
  var f = Stdlib.convertFptr(outfile);
  var str = ("\n\"" + decodeURI(f.name)  + "\"\n" +
             "// EOF\n");
  return str;
};
JSWriter.prototype.writeScript = function(obj, outfile, arg) {
  var f = Stdlib.convertFptr(outfile);
  var header = this.writeScriptHeader(obj, f);
  var str = this.writeToString(obj, arg);
  var trailer = this.writeScriptTrailer(obj, f);

  Stdlib.writeToFile(f, header + str + trailer, 'UTF-8');
};

JSWriter.prototype.header = function() {
  return ("//\n"  +
          "// Generated " + Date() + "\n" +
          "//\n\n");
};

// prefix is ignored for now
JSWriter.prototype.trailer = function(prefix) {
  var self = this;
  var str = '';

  if (!self.symbols) {
    return str;
  }

  var ftnName;

  if (prefix) {
    ftnName = prefix + ".loadSymbols";
  } else {
    ftnName = "_loadSymbols";
  }

  str += ("\n//\n" +
          "// " + ftnName + "\n" +
          "//   Loading up the symbol definitions like this makes it possible\n" +
          "//   to include several of these generated files in one master file\n" +
          "//   provided a prefix is specified other than the default. It also\n" +
          "//   skips the definitions if PSConstants has already been loaded.\n" +
          "//\n");

  str += (ftnName + " = " + "function() {\n" +
          "  var dbgLevel = $.level;\n" +
          "  $.level = 0;\n" +
          "  try {\n" +
          "    PSConstants;\n" +
          "    return; // only if PSConstants is defined\n" +
          "  } catch (e) {\n" +
          "  } finally {\n" +
          "    $.level = dbgLevel;\n" +
          "  }\n");


  str += ("  var needDefs = true;\n" +
          "  $.level = 0;\n" +
          "  try {\n" +
          "    PSClass;\n" +
          "    needDefs = false;\n" +
          "  } catch (e) {\n" +
          "  } finally {\n" +
          "    $.level = dbgLevel;\n" +
          "  }\n");

  str += "  if (needDefs) {\n";
  var tbl = PSConstants.symbolTypes;
  for (var name in tbl) {
    var kind = tbl[name];
    str += "    PS" + kind._name + " = function() {};\n";
  }

  str += "  }\n\n";
  str += ("  // We may still end up duplicating some of the following definitions\n" +
          "  // but at least we don't redefine PSClass, etc... and wipe out others\n");

  var names = [];
  for (var id in JSWriter.nameMap) {
    var n = JSWriter.nameMap[id];
    if (n.startsWith("cTID(") || n.startsWith('sTID')) {
      continue;
    }
    var idk = (n.startsWith("PSString") ? 'sTID' : 'cTID');
    names.push("  " + n + " = " + idk + "('" +
               PSConstants.reverseSymLookup(id) + "');\n");
  }
  names.sort();
  str += names.join("");
  str += "};\n\n" + ftnName + "(); // load up our symbols\n\n";
  return str;
};

JSWriter.prototype.exec = function(obj, ftn) {
  var name = JSWriter.massageName(obj.name);
  var ftnName = ftn + ".main";
  var str = '';

  str += "\n\n//=========================================\n";
  str += "//                    " + ftnName + "\n";
  str += "//=========================================\n";
  str += "//\n\n" + ftnName + " = function () {\n";
  str += "  " + name + "();\n";
  str += "};\n\n" + ftnName + "();\n\n";

  return str;
};

JSWriter.nameMap = {};

JSWriter.prototype.toName = function(karg, idarg) {
  var self = this;
  var id;
  var kind;
  if (idarg) {
    id = idarg;
    kind = karg;
  } else {
    id = karg;
  }

  function _asID(id) {
    var str = id2char(id);
    var ascii = Stdlib.numberToAscii(id);

    if (id != str) {
      // This first condition should always evaluate to true,
      // however, we may want to prefer cTID over sTID XXX
      if (sTID(str) == id && str != 'null') {
        v = "sTID(\"" + str + "\")";

      } else if (ascii.constructor == String && cTID(ascii) == id) {
        v = "cTID('" + ascii + "')";

      } else if (sTID(str) != id && PSString[str] == id) {
        // it's an aliased PSString
        v = "sTID(\"" + str + "\")";

      } else {
        Error.runtimeError(9200, "Internal error mapping id: " + id);
      }
    } else {
      // v = id;
      v = "cTID(" + id + ")";
    }

    return v;
  }

  if (!self.symbols) {
    return JSWriter.nameMap[id] || (JSWriter.nameMap[id] = _asID(id));
  }

  var v = JSWriter.nameMap[id];

  if (!v) {
    if (kind) {
      v = kind._reverseName[id];
      if (v) {
        v = "PS" + kind._name + "." + v;
      }
    }
    if (!v) {
      var tbl = PSConstants.symbolTypes;
      for (var name in tbl) {
        kind = tbl[name];
        v = kind._reverseName[id];
        if (v) {
          v = "PS" + kind._name + "." + v;
          break;
        }
      }
      if (!v) {
        v = _asID(id);
        // var str = id2char(id);
        // var ascii = Stdlib.numberToAscii(id);

        // if (id != str) {
        //   // This first condition should always evaluate to true,
        //   // however, we may want to prefer cTID over sTID XXX
        //   if (sTID(str) == id) {
        //     v = "sTID(\"" + str + "\")";
        //   } else if (ascii.constructor == String && cTID(ascii) == id) {
        //     v = "cTID('" + ascii + "')";
        //   } else {
        //     throw "Internal error mapping id: " + id;
        //   }
        // } else {
        //   v = "cTID(" + id + ")";
        // }
      }
    }
    if (v.endsWith(".null")) {
      v = "cTID('null')";
    }
    JSWriter.nameMap[id] = v;
  }

  return v;
};
JSWriter.prototype.toClass = function(id) { return this.toName(PSClass, id); };
JSWriter.prototype.toEnum  = function(id) { return this.toName(PSEnum, id); };
JSWriter.prototype.toEvent = function(id) { return this.toName(PSEvent, id); };
JSWriter.prototype.toForm  = function(id) { return this.toName(PSForm, id); };
JSWriter.prototype.toKey   = function(id) { return this.toName(PSKey, id); };
JSWriter.prototype.toType  = function(id) { return this.toName(PSType, id); };
JSWriter.prototype.toUnit  = function(id) { return this.toName(PSUnit, id); };

JSWriter.prototype.nextDescriptor = function() {
  return JSWriter.DESCRIPTOR_PREFIX + this.descCount++;
};
JSWriter.prototype.nextList = function() {
  return JSWriter.LIST_PREFIX + this.listCount++;
};
JSWriter.prototype.nextReference = function() {
  return JSWriter.REFERENCE_PREFIX + this.refCount++;
};
JSWriter.massageName = function(name) {
  var n = localize(name);
  n = n.replace(/\s+/g, '');  // remove any embedded spaces
  n = n.replace(/\W/g, '_');  // replace any non-word characters with '_'
  n = n.replace(/[_]+$/, ''); // remove any trailing '_'
  n = n.replace(/^(\d)/g, '_$1');  // add '_' prefix to names begining with a number
  return n;
};

JSWriter.write = {};
//
// JSWriter.write is indexed via:
//    typename properties
//    DescValueTypes
//    ReferenceFormTypes
//
JSWriter.write["ActionFile"] = function(parent, key, obj) {
  var self = this;
  var as = obj.getActionSet();
  return self.write(as.typename, obj, undefined, as);
};
JSWriter.write["ActionSet"] = function(parent, key, obj) {
  var self = this;
  var str = '';

  str +=
  '//\n' +
  '// ' + obj.getName() + '\n' +
  '//\n';

   var max = obj.getCount();
   for (var i = 0; i < max; i++) {
     var act = obj.byIndex(i);
     str += self.write(act.typename, obj, i+1, act);
   }

   return str;
};
JSWriter.write["Action"] = function(parent, key, obj) {
  var self = this;
  var str = '';

  var name = obj.getName();

  str += "//\n//==================== " + name + " ==============\n//\n";
  str += "function " + JSWriter.massageName(name) + "() {\n";

  var max = obj.getCount();
  for (var i = 0; i < max; i++) {
    var item = obj.byIndex(i);
    var n = "step" + (i+1);
    str += self.write(item.typename, undefined, item.getName(), item, n);
  }

  for (var i = 0; i < max; i++) {
    var item = obj.byIndex(i);
    if (!item.enabled || item.withDialog) {
      str += "  step" + (i+1) + "(" + item.enabled + ", " +
        item.withDialog + ");      // " + item.getName() + "\n";
    } else {
      str += "  step" + (i+1) + "();      // " + item.getName() + "\n";
    }
  }
  str += "};\n\n";
  return str;
};

JSWriter.write["ActionItem"] = function(parent, key, obj, name) {
  var self = this;
  var str = '';

  str += "  // " + obj.getName() + "\n";
  str += "  function " + name + "(enabled, withDialog) {\n";
  str += "    if (enabled != undefined && !enabled)\n";
  str += "      return;\n";
  str += "    var dialogMode = (withDialog ? DialogModes.ALL : DialogModes.NO);\n";
  var ev = obj.getName();
  var key = (obj.getEvent() ? obj.getEvent() : obj.getItemID());

  if (obj.containsDescriptor()) {
    self.descCount = 1;
    self.refCount  = 1;
    self.listCount = 1;

    var descName = self.nextDescriptor();
    var desc = obj.getDescriptor();
    if (ev == "Scripts") {
      str += "    var estr = ";
      var path = "";
      if (desc.hasKey(cTID('jsCt'))) {
        path = desc.getPath(cTID('jsCt'));

      } else if (desc.hasKey(cTID('jsMs'))) {
        path = Stdlib.SCRIPTS_FOLDER + "/" + desc.getString(cTID('jsMs'));

      } else {
        path = "UNABLE TO DETERMINE SCRIPT PATH";
      }

      str += "'//@include \"" + decodeURI(path) + "\";\\r\\n';\n";
      str += "    eval(estr);\n";
    } else {
      str += self.write(desc.typename, undefined, key, desc, descName);
    }
  }

  if (ev != "Scripts") {
    // XXX this should really be in toEvent
    // These gymnastics avoid ID collisions and may already
    // be taken care of in toName()
    var estr;
    var id = PSEvent[ev];

    if (id) {
      estr = JSWriter.nameMap[id];

      if (!estr) {
        estr = self.toEvent(id);

      } else {
        //
        // I don't think this is required any more...
        // 
        // estr = "sTID('" + key + "')";
        // JSWriter.nameMap[sTID(ev)] = estr;
      }

    } else {
      if (isNumber(key)) {
        estr = key.toString();
      } else {
        estr = "sTID('" + key + "')";
      }

      JSWriter.nameMap[sTID(ev)] = estr;
    }

    str += "    executeAction(" + estr + ", ";
    // XXX 'undefined' may really need to be 'new ActionDescriptor'
    str += ((obj.containsDescriptor() ? descName : "undefined") +
            ", dialogMode);\n");
  }
  str += "  };\n\n";

  return str;
};
JSWriter.write["ActionDescriptor"] = function(parent, key, obj, name) {
  var self = this;
  var str = '';
  if (!name) {
    name = self.nextDescriptor();
  }
  str += "    var " + name + " = new ActionDescriptor();\n";

  for (var i = 0; i < obj.count; i++) {
    var k = obj.getKey(i);
    var t = obj.getType(k);
    //confirm("k = " + k + ", t = " + t);
    str += self.write(t, name, k, obj, k);
  }

  if (parent && key) {
    alert("Internal Error: JSWriter.ActionDescriptor");
    str += "    " + parent + ".putObject(";
    str += self.toKey(key) + ", ";
    str += name + ");\n";
  }

  return str;
};

JSWriter.write[DescValueType.ALIASTYPE] = function(parent, key, obj) {
  var self = this;
  var str = '';
  var f = obj.getPath(key);

  str += "    " + parent + ".putPath(";
  if (JSWriter.needsKey(parent, key)) str += self.toKey(key) + ", ";
  str += "new File(\"" + decodeURI(f.absoluteURI) + "\"));\n";
  return str;
};
JSWriter.write[DescValueType.BOOLEANTYPE] = function(parent, key, obj) {
  var self = this;
  var str = '';
  str += "    " + parent + ".putBoolean(";
  if (JSWriter.needsKey(parent, key)) str += self.toKey(key) + ", ";
  str += obj.getBoolean(key) + ");\n";
  return str;
};
JSWriter.write[DescValueType.CLASSTYPE] = function(parent, key, obj) {
  var self = this;
  var str = '';
  str += "    " + parent + ".putClass(";
  if (JSWriter.needsKey(parent, key)) str += self.toKey(key) + ", ";
  str += self.toClass(obj.getClass(key)) + ");\n";
  return str;
};
JSWriter.write[DescValueType.DOUBLETYPE] = function(parent, key, obj) {
  var self = this;
  var str = '';

  str = "    " + parent + ".putDouble(";
  if (JSWriter.needsKey(parent, key)) str += self.toKey(key) + ", ";
  str += obj.getDouble(key) + ");\n";
  return str;
};
JSWriter.write[DescValueType.ENUMERATEDTYPE] = function(parent, key, obj) {
  var self = this;
  var str = '';
  str += "    " + parent + ".putEnumerated(";
  if (JSWriter.needsKey(parent, key)) str += self.toKey(key) + ", ";
  str += self.toType(obj.getEnumerationType(key)) + ", ";
  str += self.toName(obj.getEnumerationValue(key)) + ");\n";

  return str;
};
JSWriter.write[DescValueType.INTEGERTYPE] = function(parent, key, obj) {
  var self = this;
  var str = '';
  str += "    " + parent + ".putInteger(";
  if (JSWriter.needsKey(parent, key)) str += self.toKey(key) + ", ";
  str += obj.getInteger(key) + ");\n";
  return str;
};
try {
  JSWriter.write[DescValueType.LARGEINTEGERTYPE] = function(parent, key, obj) {
    var self = this;
    var str = '';
    str += "    " + parent + ".putLargeInteger(";
    if (JSWriter.needsKey(parent, key)) str += self.toKey(key) + ", ";
    str += obj.getLargeInteger(key) + ");\n";
    return str;
  };
} catch (e) {}
JSWriter.write[DescValueType.LISTTYPE] = function(parent, key, obj, id) {
  var self = this;
  var list = obj.getList(key);
  return self.write(list.typename, parent, key, list);
};
JSWriter.write[DescValueType.OBJECTTYPE] = function(parent, key, obj, id) {
  var self = this;
  var str = '';
  var name = self.nextDescriptor();

  var t = obj.getObjectType(key);
  var v = obj.getObjectValue(key);

  str += self.write(v.typename, undefined, undefined, v, name);

  if (parent && key != undefined) {
    str += "    " + parent + ".putObject(";
    if (id != undefined) {
      str += self.toKey(id) + ", ";
    }
//     if (!typeof(key) == "number") {
//       str += self.toKey(key) + ", ";
//     }
    str += self.toType(t) + ", ";
    str += name + ");\n";
  }

  return str;
};

try {
  JSWriter.write[DescValueType.RAWTYPE] = function(parent, key, obj) {
    var self = this;
    var str = '';
    str += ('var _hexToBin = ' + Stdlib.hexToBin + ";\n");
    str += ("    " + parent + ".putData(");
    if (JSWriter.needsKey(parent, key)) {
      str += (self.toKey(key) + ", ");
    }
    str += ("_hexToBin(" + Stdlib.hexToJS(Stdlib.binToHex(obj.getData(key))) +
            "));\n");
    return str;
  };
} catch (e) {}

JSWriter.write[DescValueType.REFERENCETYPE] = function(parent, key, obj, id) {
  var self = this;
  var ref = obj.getReference(key);

  return self.write(ref.typename, parent, key, ref);
};
JSWriter.cleanString = function(str) {
  str = str.replace(/"/g, '\\"'); /')/;//) xemacs auto-indent hack
  str = str.replace(/\n/g, "\\n");
  str = str.replace(/\r/g, "\\r");
  return str;
};
JSWriter.write[DescValueType.STRINGTYPE] = function(parent, key, obj) {
  var self = this;
  var str = '';
  str += "    " + parent + ".putString(";
  if (JSWriter.needsKey(parent, key)) str += self.toKey(key) + ", ";

  if (key != cTID('jsMs')) {
    str += "\"" + JSWriter.cleanString(obj.getString(key)) + "\");\n";
  } else {
    str += "'');\n";
  }
  return str;
};
JSWriter.write[DescValueType.UNITDOUBLE] = function(parent, key, obj) {
  var self = this;
  var str = '';

  var t = obj.getUnitDoubleType(key);
  var v = obj.getUnitDoubleValue(key);

  str += "    " + parent + ".putUnitDouble(";
  if (JSWriter.needsKey(parent, key)) str += self.toKey(key) + ", ";
  str += self.toUnit(t) + ", ";
  str += v + ");\n";
  return str;
};


JSWriter.write["ActionReference"] = function(parent, key, obj, name) {
  var self = this;
  var str = '';
  var ref = obj;

  if (!name) {
    name = self.nextReference();
  }
  var baseName = name;
  var needsDeclaration = true;

  do {
    var t = undefined;
    var refItemId = undefined;

    var l = $.level;
    level = 0;
    try {
      t = ref.getForm();
      refItemId = ref.getDesiredClass();
    } catch (e) {
    } finally {
      $.level = l;
    }
    if (!t || !refItemId) {
      break;
    }

    if (needsDeclaration) {
      str += "    var " + name + " = new ActionReference();\n";
      needsDeclaration = false;
    }
    str += self.write(t, name, refItemId, ref);

    try { ref = ref.getContainer(); } catch (e) { ref = null; }

  } while (ref);

  if (parent) {
    str += "    " + parent + ".putReference(";
    if (JSWriter.needsKey(parent, key)) {
      str += self.toKey(key) + ", ";
    }
    str += baseName + ");\n";
  }
  return str;
};

JSWriter.write[ReferenceFormType.CLASSTYPE] = function(parent, key, obj) {
  var self = this;
  var str = '';
  str += "    " + parent + ".putClass(";
  str += self.toClass(obj.getDesiredClass()) + ");\n";
  return str;
};
JSWriter.write[ReferenceFormType.ENUMERATED] = function(parent, key, obj) {
  var self = this;
  var str = '';
  var c = obj.getDesiredClass();
  var t = obj.getEnumeratedType();
  var v = obj.getEnumeratedValue();

  str += "    " + parent + ".putEnumerated(";
  str += self.toClass(c) + ", ";
  str += self.toType(t) + ", ";
  str += self.toName(v) + ");\n";

  return str;
};
JSWriter.write[ReferenceFormType.IDENTIFIER] = function(parent, key, obj) {
  var self = this;
  var str = '';
  var c = obj.getDesiredClass();
  var v = obj.getIdentifier();

  str += "    " + parent + ".putIdentifier(";
  str += self.toClass(c) + ", ";
  str += v + ");\n";
  return str;
};
JSWriter.write[ReferenceFormType.INDEX] = function(parent, key, obj) {
  var self = this;
  var str = '';
  var c = obj.getDesiredClass();
  var v = obj.getIndex();

  str += "    " + parent + ".putIndex(";
  str += self.toClass(c) + ", ";
  str += v + ");\n";
  return str;
};
JSWriter.write[ReferenceFormType.NAME] = function(parent, key, obj) {
  var self = this;
  var str = '';
  var c = obj.getDesiredClass();
  var v = obj.getName();

  str += "    " + parent + ".putName(";
  str += self.toClass(c) + ", \"";
  str += v + "\");\n";
  return str;
};
JSWriter.write[ReferenceFormType.OFFSET] = function(parent, key, obj) {
  var self = this;
  var str = '';
  var c = obj.getDesiredClass();
  var v = obj.getOffset();

  str += "    " + parent + ".putOffset(";
  str += self.toClass(c) + ", ";
  str += v + ");\n";
  return str;
};
JSWriter.write[ReferenceFormType.PROPERTY] = function(parent, key, obj) {
  var self = this;
  var str = '';
  var c = obj.getDesiredClass();
  var v = obj.getProperty();

  str += "    " + parent + ".putProperty(";
  str += self.toClass(c) + ", ";
  str += self.toName(v) + ");\n";
  return str;
};

JSWriter.write["ActionList"] = function(parent, key, obj, name) {
  var self = this;
  if (!name) {
    name = self.nextList();
  }
  var str = "    var " + name + " = new ActionList();\n";

  for (var i = 0; i < obj.count; i++) {
    var t = obj.getType(i);
    str += self.write(t, name, i, obj);
  }

  if (parent) {
    str += "    " + parent + ".putList(";
    if (JSWriter.needsKey(parent, key)) {
      str += self.toKey(key) + ", ";
    }
    str += name + ");\n";
  }

  return str;
};

"atn2js.jsx";
// EOF
