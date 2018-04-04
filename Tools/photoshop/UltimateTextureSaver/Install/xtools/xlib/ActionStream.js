//
// ActionDescriptor IO module. A replacement for to/fromStream for CS/PS7
//
// $Id: ActionStream.js,v 1.33 2014/11/25 04:11:22 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"

//-include "xlib/PSConstants.js"
//-include "xlib/stdlib.js"
//-include "xlib/Stream.js"
//-include "xlib/Action.js"
//-include "xlib/xml/atn2bin.jsx"
//-include "xlib/ieee754.js"

ActionStream = {};
ActionStream.logEnabled = false;
ActionStream.log = function(msg) {
  if (ActionStream.logEnabled) {
    Stdlib.log(msg);
  }
};

ActionStream.ERROR_CODE = 9200;

// Do we throw path-related errors or silently stuff in 'not found' stuff
ActionStream.PATH_ERRORS = false;

OSType = [];

// Descriptor/List
OSType['obj '] = "Reference";
OSType['Objc'] = "Descriptor";
OSType['VlLs'] = "List";
OSType['doub'] = "Double";
OSType['UntF'] = "UnitFloat";
OSType['TEXT'] = "String";
OSType['enum'] = "Enumerated";
OSType['long'] = "Integer";
OSType['bool'] = "Boolean";
OSType['GlbO'] = "GlobalObject";
OSType['type'] = "Class";
OSType['GlbC'] = "Class";
OSType['alis'] = "Alias";

// Undocumented
OSType['ObAr'] = "ObjectArray"; //??
OSType['tdta'] = "Tdata";       //??
OSType['Pth '] = "Path";

OSType['UnFl'] = "UnitFloat"; // just for type checking

OSType["DescValueType.REFERENCETYPE"] = 'obj ';
OSType["DescValueType.OBJECTTYPE"] = 'Objc';
OSType["DescValueType.LISTTYPE"] = 'VlLs';
OSType["DescValueType.DOUBLETYPE"] = 'doub';
OSType["DescValueType.UNITDOUBLE"] = 'UntF';
OSType["DescValueType.STRINGTYPE"] = 'TEXT';
OSType["DescValueType.ENUMERATEDTYPE"] = 'enum';
OSType["DescValueType.INTEGERTYPE"] = 'long';
OSType["DescValueType.LARGEINTEGERTYPE"] = 'comp';
OSType["DescValueType.BOOLEANTYPE"] = 'bool';
//OSType[DescValueType] = 'GlbO';
OSType["DescValueType.CLASSTYPE"] = 'type';
//OSType[DescValueType] = 'GlbC';

OSType["DescValueType.ALIASTYPE"] = 'Pth '; // 'alis';

// Undocumented
// OSType['ObAr'] = "ObjectArray"; //??
OSType["DescValueType.RAWTYPE"] = "tdta";

// OSType['Pth '] = "Path";

// Reference
OSReferenceType = [];

OSReferenceType['prop'] = "Property";
OSReferenceType['Clss'] = "ClassReference";
OSReferenceType['Enmr'] = "EnumeratedReference";
OSReferenceType['rele'] = "Offset";
OSReferenceType['Idnt'] = "Identifier";
OSReferenceType['indx'] = "Index";
OSReferenceType['name'] = "Name";

OSReferenceType["ReferenceFormType.PROPERTY"] = 'prop';
OSReferenceType["ReferenceFormType.CLASSTYPE"] = 'Clss';
OSReferenceType["ReferenceFormType.ENUMERATED"] = 'Enmr';
OSReferenceType["ReferenceFormType.OFFSET"] = 'rele';
OSReferenceType["ReferenceFormType.IDENTIFIER"] = 'Idnt';
OSReferenceType["ReferenceFormType.INDEX"] = 'indx';
OSReferenceType["ReferenceFormType.NAME"] = 'name';

//=========================================================================
//                              Reader
//=========================================================================
ActionItem.prototype.read = function(str) {
  var self = this;

  // $.level = 1; debugger;

  // This bit of code trys to get around an off-by-32bits problem
  // that crops up occasionally
  var b = str.readByte();
  if (b != 0 && b != 1) {
    str.ptr += 3;
  } else {
    str.ptr--;
  }

  self.expanded = str.readBoolean();
  self.enabled = str.readBoolean();
  self.withDialog = str.readBoolean();
  self.dialogOptions = str.readByte();

  self.identifier = str.readString(4);

  if (self.identifier == ActionItem.TEXT_ID) {
    self.event = str.readAscii();

  } else if (self.identifier == ActionItem.LONG_ID) {
    self.itemID = str.readWord();

  } else {
    $.level = 1; debugger;
    Error.runtimeError(ActionStream.ERROR_CODE,
                       "Bad ActionItem definition: ActionItem.id");
    var s = str.readString(4);
    if (s == ActionItem.TEXT_ID) {
      // this covers a case that exists in decompiling droplets
      self.identifier = s;
    } else {
      Error.runtimeError(ActionStream.ERROR_CODE,
                         "Bad ActionItem definition: ActionItem.id");
    }
  }

  self.name = str.readAscii();
  self.hasDescriptor = (str.readSignedWord() == -1);

  if (self.hasDescriptor) {
    var desc = new ActionDescriptor();
    desc.readFromStream(str);
    self.descriptor = desc;

  } else {
    self.descriptor = undefined;
  }
};

ActionDescriptor.prototype.readFromStream = function(s, raw) {
  var str;
  if (s instanceof Stream) {
    str = s;
  } else {
    str = new Stream(s);
    raw = true;
  }

  if (raw == true) {
    var ver = str.readWord();
  }

  return AD.read["ActionDescriptor"](this, str);
};

AD = function() {
};

// deprecated
AD.readFromStream = function(str, raw) {
  if (raw == true) {
    var ver = str.readWord();
  }
  return AD.read["ActionDescriptor"](undefined, str);
};


AD.read = {};

AD.read["ActionDescriptor"] = function(desc, str) {
  var classIDString = str.readUnicode();
  var classID       = str.readAsciiOrKey();
  var count         = str.readWord();

  if (!desc) {
    desc = new ActionDescriptor();
  }

  for (var i = 0; i < count; i++) {
    AD.readDescriptorItem(desc, str);
  }
  return desc;
};

AD.read["Descriptor"] = function(obj, str, key) {
  var classIDString = str.readUnicode();
  var classID       = str.readAsciiOrKey();
  var count         = str.readWord();

  var desc = new ActionDescriptor();

  for (var i = 0; i < count; i++) {
    AD.readDescriptorItem(desc, str);
  }

  if (obj) {
    if (key) {
      obj.putObject(key, xTID(classID), desc);
    } else {
      obj.putObject(xTID(classID), desc);
    }
  }
};

AD.readDescriptorItem = function(obj, str) {
  var key = str.readAsciiOrKey();
  var type = str.readKey();
  var ost = OSType[type.toString()];

  if (!ost) {
    Error.runtimeError(ActionStream.ERROR_CODE, "Unsupported type: " + type);
  }
  AD.read[ost.toString()](obj, str, xTID(key));
};


AD.read["Alias"] = function(obj, str, key) {
  var base = str.ptr;
  var len = str.readWord();
  var ofs = str.ptr;

  str._next = ofs + len;

  str.readInt16();  // padding
  var vlen = str.readWord();   // a copy of the length
  str.readWord();   // unknown

  function _onError(str, obj, key) {
    var f = File("~/Desktop/File or Folder not found");
    if (key) {
      obj.putPath(key, f);
    } else {
      obj.putPath(f);
    }

    str.ptr = str._next;

    return undefined;
  }

  if (len != vlen) {
    if (ActionStream.PATH_ERRORS) {
      Error.runtimeError(ActionStream.ERROR_CODE,
                         "Unable to read Macintosh path information.");
    } else {
      return _onError(str, obj, key);
    }
  }

  //debugger;

  str.ptr = ofs + 10;     // magic number!
  var slen = str.readByte();         // Volume ID
  var volumeID = str.readString(slen);

  str.ptr = ofs + 50;     // magic number!
  slen = str.readByte();
  var fileID = str.readString(slen);  // Filename


  str.ptr = ofs + 153;    // magic number!
  slen = str.readByte();
  var dir = str.readString(slen);     // Directory

  var max = str.str.length;
  //var sub = str.str.slice(str.ptr, Math.min(str.ptr + 256, max));
  var sub = str.str.substring(str.ptr);
  var idx = sub.search(RegExp('/:Volumes:' + volumeID + ':', 'i'));
  if (idx == -1) {
    idx = sub.search(RegExp(volumeID + ':', 'i'));
    if (idx == -1) {
      idx = sub.search(RegExp('/:Volumes:', 'i'));
    }
  }

  if (idx != -1) {
    str.ptr += idx - 1;
    slen = str.readByte();
    var path = str.readString(slen);     // Pathname

    if (isWindows()) {
      path = path.replace(/:/g, '/');
      path = path.replace("//", '/');
      path = path.replace(/^[^\/]+/, '');
    }

    // alert(path);
    // PS gets upset if the path does not exist when we add the file to
    // the descriptor

    var xp = path.replace(/:/g, '/');

    // force a leading '/'
    if (xp.charAt(0) != '/') {
      xp = '/' + xp;
    }

    var f = new File(xp);
    if (!f.parent.exists) {
      alert("The path " + f.parent.toUIString() + " does not exist.\r" +
            Folder.desktop + " will be used instead.");
      f = new File(Folder.desktop + "/" + f.name);
    }
    try {
      if (key) {
        obj.putPath(key, f);
      } else {
        obj.putPath(f);
      }

    } catch (e) {

      if (Stdlib.createFolder(xp)) {
        var f = new Folder(xp);
        f.remove();
        var f = new File(xp);
        if (key) {
          obj.putPath(key, f);
        } else {
          obj.putPath(f);
        }
      } else {
        // create a dummy filename
        obj.putPath(key, File(Folder.desktop + "/" + f.name));
      }
    }

  } else {
    if (ActionStream.PATH_ERRORS) {
      Error.runtimeError(ActionStream.ERROR_CODE,
                         "Unable to read Macintosh path information.");
    } else {
      return _onError(str, obj, key);
    }
  }

  str.ptr = ofs + len;
  //var data = str.readRaw(len);
};
AD.read["Path"] = function(obj, str, key) {
  var base = str.ptr;
  var len = str.readWord();

  try {
    var txtu = str.readString(4);

    if (txtu == 'txtu') {
      var lenByte = str.readByte();
      var plen = str.readWord();
      str.readInt16();
      var pathStr = str.readUnicodeString(plen);
      str.readByte();
      if (key) {
        obj.putPath(key, new File(pathStr));
      } else {
        obj.putPath(new File(pathStr));
      }

    } else {
      // skip over unknown stuff
      str.ptr += 9;

      var pathStr = str.readAscii();
      if (key) {
        obj.putPath(key, new File(pathStr));
      } else {
        obj.putPath(new File(pathStr));
      }
    }

  } catch (e) {

    if (ActionStream.PATH_ERRORS) {
      Error.runtimeError(ActionStream.ERROR_CODE,
                         "Bad path error.");
    } else {
      var file = new File(Folder.desktop + "/BAD_PATH_ERROR.txt");

      if (key) {
        obj.putPath(key, file);
        
      } else {
        obj.putPath(file);
      }
    }
  }

  str.ptr = base + len;
};

AD.read["Boolean"] = function(obj, str, key) {
  var v = str.readBoolean();

  if (key) {
    obj.putBoolean(key, v);
  } else {
    obj.putBoolean(v);
  }
};
AD.read["Class"] = function(obj, str, key) {
  var classIDString =  str.readUnicodeOrKey();
  var classID = str.readAsciiOrKey();

  if (key) {
    obj.putClass(key, xTID(classID));
  } else {
    obj.putClass(xTID(classID));
  }
};
AD.read["Double"] = function(obj, str, key) {
  var v = str.readDouble();

  if (key) {
    obj.putDouble(key, v);
  } else {
    obj.putDouble(v);
  }
};
AD.read["Enumerated"] = function(obj, str, key) {
  var enumType = str.readAsciiOrKey();
  var v = str.readAsciiOrKey();

  if (key) {
    obj.putEnumerated(key, xTID(enumType), xTID(v));
  } else {
    obj.putEnumerated(xTID(enumType), xTID(v));
  }
};
AD.read["Integer"] = function(obj, str, key) {
  var v = str.readWord();

  if (key) {
    obj.putInteger(key, v);
  } else {
    obj.putInteger(v);
  }
};
AD.read["List"] = function(obj, str, id) {
  AD.read["ActionList"](obj, str, id);
};

AD.read["Tdata"] = function(obj, str, key) {
  if (!obj.putData) {
    return;
  }
  var len = str.readWord();
  var data = str.readRaw(len);

  if (key) {
    obj.putData(key, data);
  } else {
    obj.putData(data);
  }
};
AD.read["Reference"] = function(obj, str, id) {
  AD.read["ActionReference"](obj, str, id);
};
AD.read["String"] = function(obj, str, key) {
  var v = str.readUnicode();

  if (key) {
    obj.putString(key, v);
  } else {
    obj.putString(v);
  }
};
AD.read["UnitFloat"] = function(obj, str, key) {
  var t = str.readWord();
  if (t == 0x000001CC) {
    t = "#Pxl";
  } else {
    t = app.typeIDToCharID(t);
  }
  var v = str.readDouble();

  if (key) {
    obj.putUnitDouble(key, cTID(t), v);
  } else {
    obj.putUnitDouble(cTID(t), v);
  }
};

AD.read["ActionList"] = function(obj, str, key) {
  var list = new ActionList();
  var len = str.readWord();

  for (var i = 0; i < len; i++) {
    var t = str.readKey();
    var ost = OSType[t.toString()];
    AD.read[ost.toString()](list, str);
  }

  if (obj) {
    if (key) {
      obj.putList(key, list);
    } else {
      obj.putList(list);
    }
  }
};

AD.read["ObjectArray"] = function(obj, str, key) {
  var list = new ActionList();
  var len = str.readWord();

  //debugger;

  // $.level = 1; debugger;

  var classIDString = str.readUnicode();
  var classID       = str.readAsciiOrKey();
  var count         = str.readWord();

  var descs = [];
  for (var i = 0; i < len; i++) {
    var desc = new ActionDescriptor();
    descs.push(desc);
  }

  for (var j = 0; j < count; j++) {
    var skey = str.readAsciiOrKey();
    var type = str.readKey();
    var ost = OSType[type.toString()];

    if (!ost) {
      Error.runtimeError(ActionStream.ERROR_CODE, "Unsupported type: " + type);
    }

    var t = str.readKey();
    var cnt = str.readWord();
    for (var i = 0; i < len; i++) {
      var desc = descs[i];
      if (type == "UnFl") {
        var v = str.readDouble();
        desc.putUnitDouble(xTID(skey), cTID(t), v);

      } else {
        Error.runtimeError(ActionStream.ERROR_CODE, "Unsupported type: " + type);
      }
    }
  }

  //debugger;

  for (var i = 0; i < len; i++) {
    var desc = descs[i];
    list.putObject(xTID(classID), desc);
  }

  if (obj) {
    if (key) {
      obj.putList(key, list);
    } else {
      obj.putList(list);
    }
  }
};


AD.read["ActionReference"] = function(obj, str, key) {
  var ref = new ActionReference();
  var len = str.readWord();

  //$.level = 1; debugger;
  for (var i = 0; i < len; i++) {
    var t = str.readKey();
    var ost = OSReferenceType[t];
    AD.readReference[ost.toString()](ref, str);
  }

  if (key) {
    obj.putReference(key, ref);
  } else {
    obj.putReference(ref);
  }
};

AD.readReference = {};

AD.readReference["ClassReference"] = function(obj, str) {
  var v = str.readUnicodeOrKey();
  var id = str.readAsciiOrKey();

  obj.putClass(xTID(id));
};
AD.readReference["EnumeratedReference"] = function(obj, str) {
  var c = str.readUnicodeOrKey();
  var id = str.readAsciiOrKey();
  var t = str.readAsciiOrKey();
  var v = str.readAsciiOrKey();

  obj.putEnumerated(xTID(id), xTID(t), xTID(v));
};
AD.readReference["Identifier"] = function(obj, str) {
  var c = str.readUnicodeOrKey();
  var id = str.readAsciiOrKey();
  var v = str.readWord();

  obj.putIdentifier(xTID(id), v);
};
AD.readReference["Index"] = function(obj, str, key) {
  var c = str.readUnicodeOrKey();
  var id = str.readAsciiOrKey();
  var v = str.readWord();

  obj.putIndex(xTID(id), v);
};
AD.readReference["Name"] = function(obj, str, key) {
  var c = str.readUnicodeOrKey();
  var id = str.readAsciiOrKey();
  var v = str.readUnicode();

  obj.putName(xTID(id), v);
};
AD.readReference["Offset"] = function(obj, str, key) {
  var c = str.readUnicodeOrKey();
  var id = str.readAsciiOrKey();
  var v = str.readWord();

  obj.putOffset(xTID(id), v);
};
AD.readReference["Property"] = function(obj, str, key) {
  var c = str.readUnicodeOrKey();
  var id = str.readAsciiOrKey();
  var v = str.readAsciiOrKey();

  obj.putProperty(xTID(id), xTID(v));
};

//=========================================================================
//                              Writer
//=========================================================================

ActionDescriptor.prototype.writeToStream = function(a1, a2) {
  var str;
  var raw;

  if (a2 != undefined) {
    str = a1;
    raw = a2;

  } else if (a1 != undefined) {
    if (typeof a1 == "boolean") {
      raw = a1;
    } else if (a1 instanceof Stream) {
      str = a1;
    } else {
      Error.runtimeError(ActionStream.ERROR_CODE,
                         "Bad argument to ActionDescriptor.writeToStream");
    }
  }

  if (!str) {
    str = new Stream();
    if (raw == undefined) {
      raw = true;
    }
  }
  if (raw) {
    str.writeWord(0x10);  // version number
  }
  var desc = new ActionDescriptor();
  desc.putObject(cTID('null'), cTID('null'), this);
  AD.write[this.typename](str, cTID('null'), desc);

  return str.str.join("");
};

AD.write = {};

AD.write["ActionDescriptor"] = function(str, key, obj) {
  var t = obj.getObjectType(key);
  var desc = obj.getObjectValue(key);

  //  $.level = 1; debugger;

  str.writeUnicode("");     // classIDString
  str.writeAsciiOrKey(t);   // object type
  str.writeWord(desc.count);

  for (var i = 0; i < desc.count; i++) {
    var k = desc.getKey(i);
    var t = desc.getType(k);
    var ost = OSType[t.toString()];

    str.writeAsciiOrKey(k);
    str.writeKey(ost);
    AD.write[t.toString()](str, k, desc);
  }

  return str;
};

// XXX There is no support for writing Mac paths
AD.write["DescValueType.ALIASTYPE"] = function(str, key, obj) {
  var f = obj.getPath(key);
  var fname = f.fsName;
  var len = (4 +                      // 'txtu'
             1 +                      // length byte
             4 +                      // filename length
             2 +                      // filename \u0000 prefix
             ((fname.length+1) * 2) + // filename as Unicode
             1);                      // byte pad

  str.writeWord(len);
  str.writeKey('txtu');
  str.writeByte(len & 0xFF);
  str.writeWord(fname.length + 1);
  str.writeInt16(0);
  str.writeUnicodeString(fname);
  str.writeByte(0);
};
AD.write["DescValueType.BOOLEANTYPE"] = function(str, key, obj) {
  var v = obj.getBoolean(key);
  str.writeBoolean(v);
};
AD.write["DescValueType.CLASSTYPE"] = function(str, key, obj) {
  var v = obj.getClass(key);
  str.writeUnicode("");
  str.writeAsciiOrKey(v);
};
AD.write["DescValueType.DOUBLETYPE"] = function(str, key, obj) {
  var v = obj.getDouble(key);
  str.writeDouble(v);
};
AD.write["DescValueType.ENUMERATEDTYPE"] = function(str, key, obj) {
  var t = obj.getEnumerationType(key);
  var v = obj.getEnumerationValue(key);

  str.writeAsciiOrKey(t);
  str.writeAsciiOrKey(v);
};
AD.write["DescValueType.INTEGERTYPE"] = function(str, key, obj) {
  var v = obj.getInteger(key);
  str.writeWord(v);
};
AD.write["DescValueType.LARGEINTEGERTYPE"] = function(str, key, obj) {
  var v = obj.getLargeInteger(key);
  str.writeLongWord(v);
};
AD.write["DescValueType.LISTTYPE"] = function(str, key, obj) {
  AD.write["ActionList"](str, key, obj);
};
AD.write["DescValueType.OBJECTTYPE"] = function(str, key, obj) {
  AD.write["ActionDescriptor"](str, key, obj);
};
AD.write["DescValueType.RAWTYPE"] = function(str, key, obj) {
  var v = obj.getData(key);
  str.writeWord(v.length);
  str.writeRaw(v);
};
AD.write["DescValueType.REFERENCETYPE"] = function(str, key, obj) {
  AD.write["ActionReference"](str, key, obj);
};
AD.write["DescValueType.STRINGTYPE"] = function(str, key, obj) {
  var v = obj.getString(key);
  str.writeUnicode(v);
};
AD.write["DescValueType.UNITDOUBLE"] = function(str, key, obj) {
  var t = obj.getUnitDoubleType(key);
  var v = obj.getUnitDoubleValue(key);

  str.writeKey(t);
  str.writeDouble(v);
};

AD.write["ActionList"] = function(str, key, obj) {
  var list = obj.getList(key);

  //if (key == sTID('meshPoints')) {
    // $.level = 1; debugger;
  //}

  str.writeWord(list.count);

  //  $.level = 1; debugger;

  for (var i = 0; i < list.count; i++) {
    var t = list.getType(i);
    var ost = OSType[t.toString()];
    str.writeKey(ost);
    AD.write[t.toString()](str, i, list);
  }

  return str;
};

AD.write["ActionReference"] = function(str, key, obj) {
  var ref = obj.getReference(key);

  var len = 0;
  var tref = ref;
  while (true) {
    try { tref = tref.getContainer(); } catch (e) { break; };
    len++;
  }

  str.writeWord(len);

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
    var ost = OSReferenceType[t.toString()];
    str.writeKey(ost);

    str.writeUnicode("");
    AD.writeReference[t.toString()](str, refItemId, ref);

    try { ref = ref.getContainer(); } catch (e) { ref = null; }
  } while (ref);
};

AD.writeReference = {};

AD.writeReference["ReferenceFormType.CLASSTYPE"] = function(str, key, obj) {
  var v = obj.getDesiredClass();
  str.writeAsciiOrKey(v);
};
AD.writeReference["ReferenceFormType.ENUMERATED"] = function(str, key, obj) {
  var c = obj.getDesiredClass();
  var t = obj.getEnumeratedType();
  var v = obj.getEnumeratedValue();

  str.writeAsciiOrKey(c);
  str.writeAsciiOrKey(t);
  str.writeAsciiOrKey(v);
};
AD.writeReference["ReferenceFormType.IDENTIFIER"] = function(str, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getIdentifier();

  str.writeAsciiOrKey(c);
  str.writeKey(v);
};
AD.writeReference["ReferenceFormType.INDEX"] = function(str, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getIndex();

  str.writeAsciiOrKey(c);
  str.writeWord(v);
};
AD.writeReference["ReferenceFormType.NAME"] = function(str, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getName();

  str.writeAsciiOrKey(c);
  str.writeUnicode(v);
};
AD.writeReference["ReferenceFormType.OFFSET"] = function(str, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getOffset();

  str.writeAsciiOrKey(c);
  str.writeWord(v);
};
AD.writeReference["ReferenceFormType.PROPERTY"] = function(str, key, obj) {
  var c = obj.getDesiredClass();
  var v = obj.getProperty();

  str.writeAsciiOrKey(c);
  str.writeAsciiOrKey(v);
};

//===========================================================================
//   Stream Extensions
//===========================================================================
Stream.prototype.readAsciiOrKey = function() {
  var self = this;
  var len = self.readWord();
  if (len > 20000) {
    len = self.readWord();

    if (len > 20000) {
      Error.runtimeError(ActionStream.ERROR_CODE,
                         "Read of string longer than 20K requested.");
    }
  }
  return self.readString(len ? len : 4);
};

Stream.prototype.readKey = function() {
  return this.readString(4);
};
Stream.prototype.readUnicodeString = function(len, readPad) {
  var self = this;
  var s = '';
  for (var i = 0; i < len; i++) {
    var uc = self.readInt16();
    if (uc == 0 && (i + 1) == len) {
      return s; // should have been one char shorter
    }
    s += String.fromCharCode(uc);
  }
  if (readPad == true) {
    self.readInt16();     // null pad
  }
  return s;
};
Stream.prototype.readUnicodeOrKey = function() {
  var self = this;
  //var len = self.int16();
  var len = self.readWord();
  if (len > 20000) {
    len = self.readWord();

    if (len > 20000) {
      Error.runtimeError(ActionStream.ERROR_CODE,
                         "Read of string longer than 20K requested.");
    }
  }

  var v;
  if (len == 0) {
    v = self.readString(4);
  } else {
    //self.ptr -= 4;
    v = self.readUnicodeString(len);
  }
  return v;
};

Stream.prototype.writeKey = function(k) {
  var key = xTID(k);
  this.writeWord(key);
};

Stream.prototype.writeAsciiOrKey = function(k) {
  if ((k & 0xFF000000) &&
      (k & 0xFF0000) &&
      (k & 0xFF00) &&
      (k & 0xFF)) {
    var key = xTID(k);
    this.writeWord(0);
    this.writeWord(key);
  } else {
    this.writeAscii(typeIDToStringID(k));
  }
};

//===========================================================================
//   Test code
//===========================================================================

AD.runTest = function(desc, ifname, ofname) {
  var testRead = true;
  var testWrite = true;

  if (isCS()) {
    var str = desc.writeToStream();
    var rdesc = desc.readFromStream();
    if (rdesc.isEqual(desc)) {
      return "ActionDescriptor.readFromStream failed: Descriptors not equal";
    }
    return true;
  }

  var str = desc.toStream();

  if (ifname) {
    Stream.writeToFile(ifname, str);  // for BinEdit only
    var fstr = Stream.readFromFile(ifname);
    if (fstr != str) throw "IO Error";
    str = fstr;
  }

//   var rdesc = AD.readFromStream(new Stream(str), true);

  var rdesc = new ActionDescriptor();

  if (testRead) {
    rdesc.readFromStream(new Stream(str), true);

    if (rdesc.isEqual(desc)) {
      return "ActionDescriptor.readFromStream failed: Descriptors not equal";
    }
  }

  var rstr;
  if (testWrite) {
    rstr = rdesc.writeToStream();

    if (ofname) {
      Stream.writeToFile(ofname, rstr); // for BinEdit
    }
  } else {
    rstr = rdesc.toStream();
  }

  if (rstr == str) {
    return true;
  }

  for (var i = 0; i < rstr.length; i++) {
    if (rstr.charCodeAt(i) != str.charCodeAt(i)) {
      return ("ActionDescriptor.writeToStream failed, differ at offset: " +
              Stdlib.longToHex(i));
    }
  }

  return "Unknown internal error";
};

AD.testDescriptor = function() {
  var desc = new ActionDescriptor();

  desc.putBoolean(cTID('MkVs'), false);
  desc.putClass(cTID('Nw  '), cTID('Chnl'));
  if (desc.putData) {
    desc.putData(cTID('fsel'), "Some random data");
  }
  desc.putDouble(sTID("pixelScaleFactor"), 1.5);
  desc.putEnumerated(cTID('Fl  '), cTID('Fl  '), cTID('Wht '));
  desc.putInteger(sTID("pause"), 2);

  var list  = new ActionList();
  list.putInteger(22);
  list.putDouble(22.2);
  list.putString("Some Random String");
  desc.putList(sTID("textShape"), list);

  var d = new ActionDescriptor();
  d.putInteger(sTID("pause"), 22);
  desc.putObject(cTID('T   '), cTID('PbkO'), d);

  desc.putPath(cTID('jsCt'), new File("/c/work/test.jsx"));

  var ref = new ActionReference();
  ref.putEnumerated(cTID('Chnl'), cTID('Chnl'), cTID('Msk '));
  desc.putReference(cTID('From'), ref);

  desc.putString(cTID('Nm  '), "Layer Name");
  desc.putUnitDouble(cTID('Wdth'), cTID('#Rlt'), 432.0);

  var ok = AD.runTest(desc, "/c/work/descTest.bin", "/c/work/descTestOut.bin");
  alert("Descriptor Test: " + ok);
  return ok;
};

AD.testList = function() {
  var list = new ActionList();

  list.putBoolean(false);
  list.putClass(cTID('Chnl'));
  if (list.putData) {
    list.putData("Some random data");
  }
  list.putDouble(1.5);
  list.putEnumerated(cTID('Fl  '), cTID('Wht '));
  list.putInteger(2);

  var l  = new ActionList();
  l.putInteger(22);
  l.putDouble(22.2);
  l.putString("Some Random String");
  list.putList(l);

  var d = new ActionDescriptor();
  d.putInteger(sTID("pause"), 22);
  list.putObject(cTID('PbkO'), d);

 list.putPath(new File("/c/work/test.jsx"));

  var ref = new ActionReference();
  ref.putEnumerated(cTID('Chnl'), cTID('Chnl'), cTID('Msk '));
  list.putReference(ref);

  list.putString("Layer Name");
  list.putUnitDouble(cTID('#Rlt'), 432.0);

  var desc = new ActionDescriptor();
  desc.putList(cTID('null'), list);

  var ok = AD.runTest(desc, "/c/work/listTest.bin", "/c/work/listTestOut.bin");
  alert("List Test: " + ok);
  return ok;
};

AD.testReference = function() {
  var ref = new ActionReference();

  ref.putClass(cTID('Cmnd'));
  ref.putEnumerated(cTID('Chnl'), cTID('Chnl'), cTID('Msk '));
  ref.putIdentifier(cTID('capp'), cTID('Rtte'));
  ref.putIndex(cTID('Cmnd'), 7);
  ref.putName(cTID('Aset'), "XActions");
  ref.putOffset(cTID('HstS'), -10);
  ref.putProperty(cTID('Chnl'), cTID('fsel'));

  var desc = new ActionDescriptor();
  desc.putReference(cTID('null'), ref);

  var ok = AD.runTest(desc, "/c/work/refTest.bin", "/c/work/refTestOut.bin");
  alert("Reference Test: " + ok);
  return ok;
};

AD.main = function() {
  AD.testReference();
  AD.testDescriptor();
  AD.testList();
  return;

  var infile = new File("/c/work/Set2.atn");
  var actFile = new ActionFile();
  actFile.read(infile);
};

//AD.main();

"ADIO.js";

// EOF
