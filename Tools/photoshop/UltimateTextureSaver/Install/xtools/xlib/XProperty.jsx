//
// XProperty.jsx
//
//
// $Id: XProperty.jsx,v 1.16 2012/03/23 13:37:24 anonymous Exp $
// Copyright: (c)2006, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//

//================================ XProperty =================================

XProperty = function(prop, defValue, typeInfo, helpTip) {
  var self = this;

  self.setInfo(prop, defValue, typeInfo, helpTip);
};
XProperty.prototype.typename = "XProperty";

XProperty.ALERT_ON_ERRORS = false;

XProperty.prototype.psConstants = false;

XProperty._init = function() {
  var lvl = $.level;
  $.level = 0;
  try {
    PSConstants;
    XProperty.prototype.psConstants = true;
  } catch (e) {
  } finally {
    $.level = lvl;
  }
};
XProperty._init();

XProperty.prototype.setInfo = function(propInfo, defValue, typeInfo, helpTip) {
  var self = this;

  // $.level = 1; debugger;

  self.name = propInfo.name;
  self.sym = propInfo.sym;
  self.id = xTID(propInfo.sym);

  self.type = typeInfo;

  self.helpTip = helpTip;

  if (self.psConstants) {
    PSString._add(self.name, self.sym);
  }
};
XProperty.prototype.toString = function() {
  return "[XProperty " + this.name + "]";
};

//============================== XPropertyTable ===============================

XPropertyTable = function(props) {
  this.table = {};

  if (props) {
    this.loadTableResource(props);
  }
};
XPropertyTable.prototype.typename = "XPropertyTable";

//
// Add a property definition to this table
//
XPropertyTable.prototype.add = function(prop) {
  var self = this;

  self.table[prop.name] = prop;
  self.table[prop.id] = prop;
  prop.parent = self;
};

//
// Insert the key/val pair into the descriptor as
// an untyped (String) property

XPropertyTable.insertProperty = function(desc, id, val) {
  var pdesc = new ActionDescriptor();
  pdesc.putString(cTID('Nm  '), id);
  pdesc.putString(cTID('Vl  '), (val ? val.toString() : ''));
  desc.putObject(xTID(id), cTID('Prpr'), pdesc);
};

XPropertyTable.prototype.toDescriptor = function(obj, desc) {
  var self = this;
  var table = self.table;

  //  $.level = 1; debugger;

  if (!desc) {
    desc = new ActionDescriptor();
  }

  for (var idx in obj) {
    if (idx.charAt(0) == '_') {
      continue;
    }

    var val = obj[idx];
    if (typeof(val) == "function") {
      continue;
    }

    var typ = table[idx];

    if (typ) {
      if (val == undefined) {
        var msg = ("XPropertyTable.toDescriptor()->" +
                   "undefined value for attribute: " + id2char(typ.id, "Key"));

        Stdlib.log(msg);
        if (XProperty.ALERT_ON_ERRORS) {
          alert(msg);
        }
        continue;
      }

      typ.type.write(desc, typ.id, val);

    } else {

      if (val == undefined) {
        var msg = ("XPropertyTable.toDescriptor()->" +
                   "undefined value for property: " + idx);
        Stdlib.log(msg);
        if (XProperty.ALERT_ON_ERRORS) {
          alert(msg);
        }
        continue;
      }

      XPropertyTable.insertProperty(desc, idx, val);
    }
  }
  return desc;
};


XPropertyTable.prototype.fromDescriptor = function(desc, obj) {
  var self = this;
  var table = self.table;

  //$.level = 1; debugger;

  if (!obj) {
    obj = {};
  }
  var cnt = desc.count;

  for (var i = 0; i < cnt; i++) {
    var key = desc.getKey(i);
    var typ = table[key];
    if (typ) {
      var v = typ.type.read(desc, typ.id);
      obj[typ.name] = v;
    } else {
      if (desc.getType(key) == DescValueType.OBJECTTYPE &&
          desc.getObjectType(key) == cTID('Prpr')) {
        var odesc = desc.getObjectValue(key);
        var okey = odesc.getString(cTID('Nm  '));
        var oval = odesc.getString(cTID('Vl  '));
        obj[okey] = oval;

      } else {
        alert("Unknown property: " + id2char(key, "Key"));
      }
    }
  }
  return obj;
};

XPropertyTable.prototype.asString = function(obj) {
  var self = this;
  var table = self.table;

  //  $.level = 1; debugger;

  var str = '';
  for (var idx in obj) {
    if (idx.charAt(0) == '_') {
      continue;
    }
    if (idx == "typename") {
      continue;
    }
    var val = obj[idx];
    if (typeof(val) == "function") {
      continue;
    }
    str += (idx + ": ");

    var typ = table[idx];
    if (typ) {
      str += typ.type.asString(val);
    } else {
      str += val;
    }
    str += '\n';
  }
  return str;
};
XPropertyTable.prototype.fromString = function(str, obj) {
  var self = this;

  // $.level = 1; debugger;

  // split it up like a normal INI string
  var xobj = XPropertyTable.fromIniString(str);

  return self.rationalize(xobj, obj);
};

XPropertyTable.prototype.rationalize = function(xobj, obj) {
  var self = this;
  var table = self.table;

  if (obj == undefined) {
    obj = {};
  }

  for (var idx in xobj) {
    if (idx.charAt(0) == '_') {
      continue;
    }
    if (idx == "typename") {
      continue;
    }
    var val = xobj[idx];
    if (typeof(val) == "function") {
      continue;
    }

    var typ = table[idx];
    if (typ) {
      try {
        val = typ.type.fromString(val);

      } catch (e) {
        alert(e + '@' + e.line + "\n" + idx + ": " + val);
      }
    }

    obj[idx] = val;
  }

  return obj;
};

XPropertyTable.prototype.loadTableResource = function(props) {
  var self = this;

  for (var i = 0; i < props.length; i++) {
    var propDef = props[i];
    var prop = new XProperty(propDef.prop,
                             propDef.defValue,
                             propDef.typeInfo,
                             propDef.helpTip);
    self.add(prop);
  }
};

//========================== XPropertyTableResource ==========================


//=================================== XOpt ===================================

XOpt = function() {};

//================================ XOpt.Type =================================

XOpt.Type = function(descType, typename) {
  var self = this;
  self.type = descType;
  self.typename = typename;
};
XOpt.Type.prototype.typename = "XOpt.Type";
XOpt.Type.prototype.read = function(desc, sym) {
  var self = this;
  var id;

  if (sym.constructor == Number) {
    id = sym;
  } else {
    id = xTID(sym);
  }

  if (desc.hasKey(id)) {
    return self._read(desc, id);
  } else {
    return undefined;
  }
};
XOpt.Type.prototype.write = function(desc, sym, val) {
  var self = this;
  var id;

  if (sym.constructor == Number) {
    id = sym;
  } else {
    id = xTID(sym);
  }

  self._write(desc, id, val);
};
XOpt.Type.prototype.asString = function(val)   { return val.toString(); };
XOpt.Type.prototype.fromString = function(str) { return str; };


//================================ XOpt.Null =================================

XOpt.Null = new XOpt.Type(undefined, "XOpt.Null");
XOpt.Null.read = function(desc, sym) {};
XOpt.Null.write = function(desc, sym, val) {};
XOpt.Null.asString = function(val)   { };
XOpt.Null.fromString = function(str) { };


//================================= XOpts.Bool ===============================

XOpt.Bool = new XOpt.Type(DescValueType.BOOLEANTYPE, "XOpt.Bool");
XOpt.Bool._read = function(desc, id) {
  return desc.getBoolean(id);
};
XOpt.Bool._write = function(desc, id, val) {
  val = toBoolean(val);
  desc.putBoolean(id, val);
};
XOpt.Bool.asString = function(val) {
  return toBoolean(val).toString();
};
XOpt.Bool.fromString = function(str) {
  return toBoolean(str);
};


//================================= XOpts.Integer ============================

XOpt.Integer = new XOpt.Type(DescValueType.INTEGERTYPE, "XOpt.Integer");
XOpt.Integer._read = function(desc, id) {
  return desc.getInteger(id);
};
XOpt.Integer._write = function(desc, id, val) {
  val = Number(val);
  desc.putInteger(id, val);
};
XOpt.Integer.asString = function(val) {
  var n = toNumber(val);
  return (isNaN(n) ? n : Math.round(n).toString());
};
XOpt.Integer.fromString = function(str) {
  return Math.round(Number(str));
};


//================================= XOpts.String ============================

XOpt.String  = new XOpt.Type(DescValueType.STRINGTYPE, "XOpt.String");
XOpt.String._read = function(desc, id) {
  return desc.getString(id);
};
XOpt.String._write = function(desc, id, val) {
  desc.putString(id, val.toString());
};


//================================= XOpts.Double ============================

XOpt.Double  = new XOpt.Type(DescValueType.DOUBLETYPE, "XOpt.Double");
XOpt.Double._read = function(desc, id) {
  return desc.getDouble(id);
};
XOpt.Double._write = function(desc, id, val) {
  val = Number(val);
  desc.putDouble(id, val);
};
XOpt.Double.asString = function(val) {
  return toNumber(val);
};
XOpt.Double.fromString = function(str) {
  return Number(str);
};


//================================ XOpt.Path =================================

XOpt.Path    = new XOpt.Type(DescValueType.ALIASTYPE, "XOpt.Path");
XOpt.Path._read = function(desc, id) {
  return desc.getPath(id);
};
XOpt.Path._write = function(desc, id, val) {
  if (!val) {
    val = '';
  }
  val = File(val);
  desc.putPath(id, val);
};
XOpt.Path.asString = function(val) {
  return val.toUIString();
};
XOpt.Path.fromString = function(str) {
  return File(str);
};


//================================ XOpt.Enum =================================

XOpt.Enum = function(type, values, strs) {
  var self = this;

  if (typeof(self) == "function") {
    return new XOpt.Enum(type, values, strs);
  }

  self.type   = type;
  self.typeID = xTID(type);
  self.values = values;
  self.strings = strs;
};
XOpt.Enum.prototype = new XOpt.Type(DescValueType.ENUMERATEDTYPE, "XOpt.Enum");

XOpt.Enum.prototype._read = function(desc, id) {
  var self = this;
  var vid = desc.getEnumerationValue(id);
  var vals = self.values;
  var res = id2char(vid, "Enum");
  var strs = self.strings;

  // if there is not a strs array, use the values as the str
  if (!strs) {
    strs = self.values;
  }

  for (var i = 0; i < vals.length; i++) {
    if (xTID(vals[i]) == vid) {
      res = strs[i];
      break;
    }
  }

  return res;
};
XOpt.Enum.prototype._write = function(desc, id, val) {
  var self = this;
  var strs = self.strings;
  var valID = xTID(val);

  //$.level = 1; debugger;

  // if there is not a strs array, use the values as the str
  if (!strs) {
    strs = self.values;
  }

  for (var i = 0; i < strs.length; i++) {
    if (strs[i] == val) {
      valID = xTID(self.values[i]);
      break;
    }
  }

  if (i == strs.length) {
    var msg = ("XOpt.Enum._write->" +
               "Bad Enum Value: " + val + " for Enum:" + self.type);
    Stdlib.log(msg);
    if (XProperty.ALERT_ON_ERRORS) {
      alert(msg);
    }
  }

  desc.putEnumerated(id, self.typeID, valID);
};


//=========================== XOpt.UnitDouble =================================

XOpt.UnitDouble = function(type) {
  var self = this;

  if (typeof(self) == "function") {
    return new XOpt.UnitDouble(type);
  }

  self.type   = type;
  self.typeID = xTID(type);
};
XOpt.UnitDouble.prototype = new XOpt.Type(DescValueType.UNITDOUBLE,
                                          "XOpt.UnitDouble");

XOpt.UnitDouble.prototype._read = function(desc, id) {
  var self = this;
  return desc.getUnitDoubleValue(id);
};
XOpt.UnitDouble.prototype._write = function(desc, id, val) {
  var self = this;
  desc.putUnitDouble(id, self.typeID, toNumber(val));
};
XOpt.UnitDouble.fromString = function(str) {
  return Number(str);
};

//================================ XOpt.Obj ================================

XOpt.Obj = function(type) {
  var self = this;

  if (typeof(self) == "function") {
    return new XOpt.Obj(type);
  }

  var self = this;
  self.type   = type;
  self.typeID = xTID(type);
};
XOpt.Obj.prototype = new XOpt.Type(DescValueType.OBJECTTYPE, "XOpt.Obj");


//================================= XOpt.Fptr ============================

XOpt.Fptr = new XOpt.Obj("Fptr");
XOpt.Fptr.typename = "XOpt.Fptr";

XOpt.Fptr._read = function(desc, id) {
  var self = this;
  var fdesc = desc.getObjectValue(id);
  var fptr = fdesc.getString(cTID('Path'));
  return fptr;
};
XOpt.Fptr._write = function(desc, id, val) {
  var self = this;
  var fdesc = new ActionDescriptor();
  fdesc.putString(cTID('Path'), val.toString());
  desc.putObject(id, cTID('Fptr'), fdesc);
};
XOpt.Fptr.asString = function(val) {
  return val.toString();
};
XOpt.Fptr.fromString = function(str) {
  return str;
};


//================================= XOpt.Color ============================

XOpt.Color = new XOpt.Obj("Clr ");
XOpt.Color.typename = "XOpt.Color";

XOpt.Color._read = function(desc, id) {
  var res = '';
  try {
    var cdesc = desc.getObjectValue(id);

    var color = cdesc.getString(cTID('Clr '));

    if (color == "RGB") {
      var rgb = new RGBColor();

      rgb.red   = cdesc.getDouble(cTID('Rd  '));
      rgb.green = cdesc.getDouble(cTID('Grn '));
      rgb.blue  = cdesc.getDouble(cTID('Bl  '));

      var sc = new SolidColor();
      sc.rgb = rgb;
      res = sc;

    } else {
      var strs = XOpt.Color.strings;
      var val = color;
      for (var i = 0; i < strs.length; i++) {
        if (val == strs[i]) {
          break;
        }
      }
      if (i == strs.length) {
        alert("Bad Color Value: " + val + " for " + self.type);
      }
      res = color;
    }
  } catch (e) {
    alert(e);
  }

  return res;
};
XOpt.Color.strings = ["White",
                      "Black",
                      "Foreground",
                      "Background"];

XOpt.Color._write = function(desc, id, val) {
  var self = this;
  var cdesc = new ActionDescriptor();

  if (val.constructor == String) {
    var strs = XOpt.Color.strings;
    var valStr = val;

    // check for symbolic name
    for (var i = 0; i < strs.length; i++) {
      if (val == strs[i]) {
        break;
      }
    }

    // if it's not a symbolic name, parse it up as an RGB
    if (i == strs.length) {
      var color = XOpt.Color.rgbFromString(val);
      if (color) {
        val = "RGB";
        var rgb = color.rgb;
        cdesc.putDouble(cTID('Rd  '), rgb.red);
        cdesc.putDouble(cTID('Grn '), rgb.green);
        cdesc.putDouble(cTID('Bl  '), rgb.blue);

      } else {
        alert("Bad Color Value: " + val + " for " + self.type);
        val = "";
      }
    } else {
      // the symbolic names do not have RGB values in cdesc
    }


    if (val) {
      cdesc.putString(cTID('Clr '), val);
      desc.putObject(id, self.typeID, cdesc);
    }

  } else if (val instanceof SolidColor) {
    var rgbColor = val.rgb;

    cdesc.putString(cTID('Clr '), "RGB");
    cdesc.putDouble(cTID('Rd  '), rgbColor.red);
    cdesc.putDouble(cTID('Grn '), rgbColor.green);
    cdesc.putDouble(cTID('Bl  '), rgbColor.blue);
    desc.putObject(id, self.typeID, cdesc);

  } else {
    alert("Bad Color Value for " + self.type);
  }
};
XOpt.Color.asString = function(val) {
  var str;
  if (val.constructor == String) {
    for (var i = 0; i < strs.length; i++) {
      if (val == strs[i]) {
        str = strs[i];
        break;
      }
    }
  } else {
    str = XOpt.Color.rgbToString(val).replace(/[\[\]]/g, '');
  }
  return str;
};
XOpt.Color.fromString = function(str) {
  if (str instanceof SolidColor) {
    return str;
  }

  var clr = XOpt.Color.rgbFromString(str);

  if (!clr) {
    var strs = XOpt.Color.strings;
    for (var i = 0; i < strs.length; i++) {
      if (str == strs[i]) {
        clr = strs[i];
        break;
      }
    }
  }

  return clr;
};

XOpt.Color.rgbToString = function(c) {
  return "[" + c.rgb.red + "," + c.rgb.green + "," + c.rgb.blue + "]";
};
XOpt.Color.rgbFromString = function(str) {
  var rex = /([\d\.]+),([\d\.]+),([\d\.]+)/;
  var m = str.match(rex);
  if (m) {
    return XOpt.Color.createRGBColor(Number(m[1]),
                                     Number(m[2]),
                                     Number(m[3]));
  }
  return undefined;
};
XOpt.Color.createRGBColor = function(r, g, b) {
  var c = new RGBColor();
  if (r instanceof Array) {
    b = r[2]; g = r[1]; r = r[0];
  }
  c.red = parseInt(r); c.green = parseInt(g); c.blue = parseInt(b);
  var sc = new SolidColor();
  sc.rgb = c;
  return sc;
};

XOpt.Color.getSymbolicColor = function(obj) {
  var color = undefined;

  if (obj.typename == "SolidColor") {
    color = obj;
  } else if (obj == 'White') {
    color = Stdlib.createRGBColor(255, 255, 255);
  } else if (obj == 'Black') {
    color = Stdlib.createRGBColor(0, 0, 0);
  } else if (obj == 'Foreground') {
    color = app.foregroundColor;
  } else if (obj == 'Background') {
    color = app.backgroundColor;
  }

  return color;
};

//================================= XOpt.HexColor ============================

XOpt.HexColor = new XOpt.Type(DescValueType.STRINGTYPE, "XOpt.HexColor");
XOpt.HexColor.typename = "XOpt.HexColor";

XOpt.HexColor.REX = /^#?[A-Fa-f0-9]{6}$/;
XOpt.HexColor._read = function(desc, id) {
  var res = '';
  try {
    var cstr = desc.getString(id);
    var sc = new SolidColor();
    sc.rgb.hexValue = cstr.slice(1);
    res = sc;

  } catch (e) {
    alert(e);
  }

  return res;
};
XOpt.HexColor.strings = ["White",
                         "Black",
                         "Foreground",
                         "Background"];

XOpt.HexColor._write = function(desc, id, val) {
  var self = this;

  if (val.constructor == String) {
    if (val.match(XOpt.HexColor.REX)) {
      // we're good but could check the format better...
      if (!val[0] == '#') {
        val = '#' + val;
      }
    } else {
      alert("Bad Color Value: " + val + " for " + self.type);
      val = "#FFFFFF";
    }

    desc.putString(id, val);

  } else if (val instanceof SolidColor) {
    var cstr = '#' + val.rgb.hexValue;
    desc.putString(id, cstr);

  } else if (val instanceof RGBColor) {
    var cstr = '#' + val.hexValue;
    desc.putString(id, cstr);

  } else {
    alert("Bad Color Value for " + self.type);
  }
};
XOpt.HexColor.asString = function(val) {
  var str;

  if (val.constructor == String) {
    str = val;

  } else if (val instanceof SolidColor) {
    str = '#' + val.rgb.hexValue;

  } else if (val instanceof RGBColor) {
    str = '#' + val.hexValue;

  } else {
    alert("Unexpected color value: " + val);
  }

  return str;
};
XOpt.HexColor.fromString = function(str) {
  var clr = undefined;

  if (str.constructor == String) {
    if (str.match(XOpt.HexColor.REX)) {
      clr = new SolidColor();
      if (str[0] == '#') {
        str = str.slice(1);
      }

      clr.rgb.hexValue = str;
    }
  } else if (str instanceof SolidColor) {
    clr = str;
  }

  return clr;
};


//================================ init code ==================================

XProperty._need_cTID = false;
XProperty.__init = function() {
  var __lvl = $.level;
  $.level = 0;

  try {
    cTID;

  } catch (e) {
    XProperty._need_cTID = true;

  } finally {
    $.level = __lvl;
    delete __lvl;
  }
};

if (XProperty._need_cTID) {
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
    Error.runtimeError(19, "s");  // Bad Argument
  };
  id2char = function(s) {
    if (isNaN(Number(s))){
      return '';
    }
    var v;

    // Use every mechanism available to map the typeID
    try {
      if (!v) {
        try { v = PSConstants.reverseNameLookup(s); } catch (e) {}
      }
      if (!v) {
        try { v = PSConstants.reverseSymLookup(s); } catch (e) {}
      }
      if (!v) {
        try { v = app.typeIDToCharID(s); } catch (e) {}
      }
      if (!v) {
        try { v = app.typeIDToStringID(s); } catch (e) {}
      }
    } catch (e) {
    }
    if (!v) {
      v = _numberToAscii(s);
    }
    return v ? v : s;
  };

  _numberToAscii = function(n) {
    if (isNaN(n)) {
      return n;
    }
    var str = '';
    str += String.fromCharCode(n >> 24);
    str += String.fromCharCode((n >> 16) & 0xFF);
    str += String.fromCharCode((n >> 8) & 0xFF);
    str += String.fromCharCode(n & 0xFF);

    return str.match(/^\w{4}$/) ? str : n;
  };

}

XPropertyTable.fromIniString = function(str, obj) {
  if (!obj) {
    obj = {};
  }
  var lines = str.split(/[\r\n]+/);

  var rexp = new RegExp(/([^:]+):(.*)$/);

  for (var i = 0; i < lines.length; i++) {
    var line = lines[i].trim();
    if (!line || line.charAt(0) == '#') {
      continue;
    }
    var ar = rexp.exec(line);
    if (!ar) {
      alert("Bad line in config file: \"" + line + "\"");
      return;
    }
    obj[ar[1].trim()] = ar[2].trim();
  }
  return obj;
};

//=====================================================================
//
// Everything below this point is test data
//
//=====================================================================


"XProperty.jsx";
// EOF
