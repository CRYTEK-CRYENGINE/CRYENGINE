//
// PathItem
//
// $Id: PathItem.js,v 1.8 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
PathItemUtil = function() {};
PathItemUtil.docW = 0;
PathItemUtil.docH = 0;

PathItemUtil.setReferenceDocument = function(doc) {
  PathItem.docW = doc.width;
  PathItem.docH = doc.height;
};

PathItemUtil.serialize = function(obj) {
  return PathItemUtil._serialize[obj.typename](obj);
};

PathItemUtil._serialize = [];

PathItemUtil._serialize["PathItems"] = function(obj) {
  var c = {};
  c.elements = [];
  for (var i = 0; i < obj.length; i++) {
    var element = obj[i];
    c.elements.push(PathItemUtil.serialize(element));
  }
  c.type = obj.typename;
  return c
};

PathItemUtil._serialize["PathItem"] = function(obj) {
  var c = {};
  c.kind = obj.kind.toString();
  c.name = obj.name;
  c.type = obj.typename;
  c.subPathItems = PathItemUtil.serialize(obj.subPathItems);
  return c;
};

PathItemUtil._serialize["SubPathItems"] = function(obj) {
  var c = {};
  c.elements = [];
  for (var i = 0; i < obj.length; i++) {
    var element = obj[i];
    c.elements.push(PathItemUtil.serialize(element));
  }
  c.type = obj.typename;
  return c;
};

PathItemUtil._serialize["SubPathItem"] = function(obj) {
  var c = {};
  c.closed = obj.closed;
  c.operation = obj.operation.toString();
  c.pathPoints = PathItemUtil.serialize(obj.pathPoints);
  c.type = obj.typename;
  return c;
};

PathItemUtil._serialize["PathPoints"] = function(obj) {
  var c = {};
  c.elements = [];
  for (var i = 0; i < obj.length; i++) {
    var element = obj[i];
    c.elements.push(PathItemUtil.serialize(element));
  }
  c.type = obj.typename;
  return c;
};

PathItemUtil._serialize["PathPoint"] = function(obj) {
  var c = {};
  c.anchor = obj.anchor;
  c.kind = obj.kind.toString();
  c.leftDirection = obj.leftDirection;
  c.rightDirection = obj.rightDirection;
  c.type = obj.typename;
  return c;
};


PathItemUtil.deserialize = function(obj) {
  return PathItemUtil._deserialize[obj.type](obj);
};
PathItemUtil._deserialize = [];

PathItemUtil._deserialize["PathItems"] = function(obj) {
  var c = [];
  var elements = obj.elements;
  for (var i = 0; i < elements.length; i++) {
    var element = elements[i];
    c.push(PathItemUtil.deserialize(element));
  }

  return c
};

PathItemUtil._deserialize["PathItem"] = function(obj) {
  var c = {};
  c.kind = eval(obj.kind);
  c.name = obj.name;
  c.subPathItems = PathItemUtil.deserialize(obj.subPathItems);
  return c;
};

PathItemUtil._deserialize["SubPathItems"] = function(obj) {
  var c = [];
  var elements = obj.elements;
  for (var i = 0; i < elements.length; i++) {
    var element = elements[i];
    c.push(PathItemUtil.deserialize(element));
  }
  return c;
};

PathItemUtil._deserialize["SubPathItem"] = function(obj) {
  var c = new SubPathInfo();
  c.closed = obj.closed;
  c.operation = eval(obj.operation);
  c.entireSubPath = PathItemUtil.deserialize(obj.pathPoints);
  return c;
};

PathItemUtil._deserialize["PathPoints"] = function(obj) {
  var c = [];
  var elements = obj.elements;
  for (var i = 0; i < elements.length; i++) {
    var element = elements[i];
    c.push(PathItemUtil.deserialize(element));
  }
  return c;
};

PathItemUtil._deserialize["PathPoint"] = function(obj) {
  var c = new PathPointInfo();
  c.anchor = obj.anchor;
  c.kind = eval(obj.kind);
  c.leftDirection = obj.leftDirection;
  c.rightDirection = obj.rightDirection;

  return c;
};

PathItemUtil._toStream = [];

PathItemUtil.toStream = function(obj) {
  return PathItemUtil._toStream[obj.typename](obj);
};
PathItemUtil._toStream["PathItems"] = function(obj) {
  var str = '';
  for (var i = 0; i < obj.length; i++) {
    var element = obj[i];
    str += PathItemUtil.toStream(element) + ',';
  }
  return '[' + str + ']';
};

PathItemUtil._toStream["PathItem"] = function(obj) {
  var str = '{';
  str += "kind: " + obj.kind.toString() + ',';
  str += "name: decodeURI(\"" + encodeURI(obj.name) + "\"),";
  str += "subPathItems: " + PathItemUtil.toStream(obj.subPathItems);
  str += '}';
  return str;
};

PathItemUtil._toStream["SubPathItems"] = function(obj) {
  var str = '';
  for (var i = 0; i < obj.length; i++) {
    var element = obj[i];
    str += PathItemUtil.toStream(element) + ',';
  }
  return '[' + str + ']';
};

PathItemUtil._toStream["SubPathItem"] = function(obj) {
  var str = "newSPI(";
  str += obj.closed + ',';
  str += obj.operation + ',';
  str += PathItemUtil.toStream(obj.pathPoints);
  str += ")";
  return str;
};

PathItemUtil._toStream["PathPoints"] = function(obj) {
  var str = '';
  for (var i = 0; i < obj.length; i++) {
    var element = obj[i];
    str += (PathItemUtil.toStream(element)) + ',';
  }
  return '[' + str + ']';
};

PathItemUtil._toStream["PathPoint"] = function(obj) {
  var str = "newPPI(";
  str += '[' + obj.anchor + "],";
  str += obj.kind + ',';
  str += '[' + obj.leftDirection + "],";
  str += '[' + obj.rightDirection + ']';
  str += ")";
  return str;
};

PathItemUtil.createSubPathInfo = function(closed, operation, subpath) {
  var obj = new SubPathInfo();
  obj.closed = closed;
  obj.operation = operation;
  obj.entireSubPath = subpath;
  return obj;
};

PathItemUtil.createPathPointInfo = function(anchor, kind, left, right) {
  var obj = new PathPointInfo();
  obj.anchor = anchor;
  obj.kind = kind;
  if (left) {
    obj.leftDirection = left;
  }
  if (right) {
    obj.rightDirection = right;
  }
  return obj;
};
var newPPI;
if (!newPPI) {
  newPPI = PathItemUtil.createPathPointInfo;
}
var newSPI;
if (!newSPI) {
  newSPI = PathItemUtil.createSubPathInfo;
}

PathItemUtil.pathToStream = function(doc, layer, path) {
  path.select();
  var vmdesc = Stdlib.getVectorMaskDescriptor(doc, layer);
  var pcdesc = vmdesc.getObjectValue(cTID('PthC'));
  var pclist = pcdesc.getList(sTID('pathComponents'));
  var desc = pclist.getObjectValue(0);
  var shpop = desc.getEnumerationValue(sTID('shapeOperation'));
  if (shpop != cTID('Add ')) {
    throw "Unexpected shape operation";
  }
  var shplist = desc.getList(cTID('SbpL'));
  var str = '[';
  for (var i = 0; i < shplist.count; i++) {
    if (i != 0) {
      str += ',';
    }
    var subpath = shplist.getObjectValue(i);
    str += PathItemUtil.subpathToStream(subpath);
  }
  str += ']';
  return str;
};
PathItemUtil.subpathToStream = function(subpath) {
  var str = "newSPI(";
  str += subpath.getBoolean(cTID('Clsp'));
  str += ", ShapeOperation.SHAPEXOR,["

  var points = subpath.getList(cTID('Pts '));
  for (var i = 0; i < points.count; i++) {
    if (i != 0) {
      str += ',';
    }
    var pathPoint = points.getObjectValue(i);
    str += PathItemUtil.pathPointToStream(pathPoint);
  }
  str += "])";
  return str;
};
PathItemUtil.pathPointToStream = function(pathPoint) {
  var str = "newPPI(";

  var fmt = "[%.3f,%.3f]";
  function pointToStr(desc) {
    return fmt.sprintf(desc.getUnitDoubleValue(cTID('Hrzn')),
                       desc.getUnitDoubleValue(cTID('Vrtc')));
  }

  var adesc = pathPoint.getObjectValue(cTID('Anch'));
  str += pointToStr(adesc) + ',';

  str += "PointKind." + (pathPoint.hasKey(cTID('Smoo')) &&
                         pathPoint.getBoolean(cTID('Smoo')) ?
                         "SMOOTHPOINT" : "CORNERPOINT");

  str += ',';
  if (pathPoint.hasKey(cTID('Fwd '))) {

    var desc = pathPoint.getObjectValue(cTID('Fwd '));
    str += pointToStr(desc);
  } else {
    str += pointToStr(adesc);
  }

  str += ',';
  if (pathPoint.hasKey(cTID('Bwd '))) {
    var desc = pathPoint.getObjectValue(cTID('Bwd '));
    str += pointToStr(desc);
  } else {
    str += pointToStr(adesc);
  }

  str += ")";
  return str;
};


// =======================================================
//--@include "json.js"
//--@include "stdlib.js"

PathItemUtil.percent = false;

PathItemUtil.test = function() {
  var doc = app.activeDocument;
  app.preferences.rulerUnits = Units.POINTS;

  var docW = doc.width.value;
  var docH = doc.height.value;

  $.writeln("docW=" + docW);
  $.writeln("docH=" + docH);

  var pitems = doc.pathItems;
  var str = PathItemUtil.toStream(pitems[0].subPathItems);

  $.level = 1; debugger;
  var spis;
  eval("spis = " + str);
  doc.pathItems.add("New Path2", spis);
  
  return;

  var obj = PathItemUtil.serialize(pitems);

  var str = JSON.stringify(obj);
  // /{/;  // for xemacs
  Stdlib.writeToFile("/c/work/pathtest.js", str.replace(/},/g, "},\n\n"));
  
  var obj2;
  eval("obj2 = " + str);
  var pitems2 = PathItemUtil.deserialize(obj2);

  str2 = JSON.stringify(PathItemUtil.serialize(pitems));
// /{/;  // for xemacs
  Stdlib.writeToFile("/c/work/pathtest2.js", str2.replace(/},/g, "},\n\n"));

  Stdlib.deselectActivePath(doc);
  
  var subPathItems = pitems2[0].subPathItems;
  doc.pathItems.add("New Path2", subPathItems);
};

// PathItemUtil.test();

function makeSelectionFromPath(name) {

var id575 = charIDToTypeID( "slct" );
    var desc111 = new ActionDescriptor();
    var id576 = charIDToTypeID( "null" );
        var ref113 = new ActionReference();
        var id577 = charIDToTypeID( "Path" );
        ref113.putName( id577, name );
    desc111.putReference( id576, ref113 );
executeAction( id575, desc111, DialogModes.NO );

var id564 = charIDToTypeID( "setd" );
    var desc110 = new ActionDescriptor();
    var id565 = charIDToTypeID( "null" );
        var ref111 = new ActionReference();
        var id566 = charIDToTypeID( "Chnl" );
        var id567 = charIDToTypeID( "fsel" );
        ref111.putProperty( id566, id567 );
    desc110.putReference( id565, ref111 );
    var id568 = charIDToTypeID( "T   " );
        var ref112 = new ActionReference();
        var id569 = charIDToTypeID( "Path" );
        var id570 = charIDToTypeID( "Ordn" );
        var id571 = charIDToTypeID( "Trgt" );
        ref112.putEnumerated( id569, id570, id571 );
    desc110.putReference( id568, ref112 );
    var id572 = charIDToTypeID( "AntA" );
    desc110.putBoolean( id572, true );
    var id573 = charIDToTypeID( "Fthr" );
    var id574 = charIDToTypeID( "#Pxl" );
    desc110.putUnitDouble( id573, id574, 0.000000 );
executeAction( id564, desc110, DialogModes.NO );
};
function makeSelectionFromCurrentPath() {
    var desc8 = new ActionDescriptor();
        var ref5 = new ActionReference();
        ref5.putProperty( cTID('Chnl'), cTID('fsel') );
    desc8.putReference( cTID('null'), ref5 );
        var ref6 = new ActionReference();
        ref6.putEnumerated( cTID('Path'), cTID('Path'), sTID('vectorMask') );
        ref6.putEnumerated( cTID('Lyr '), cTID('Ordn'), cTID('Trgt') );
    desc8.putReference( cTID('T   '), ref6 );
    desc8.putBoolean( cTID('AntA'), false );
    desc8.putUnitDouble( cTID('Fthr'), cTID('#Pxl'), 0.000000 );
    executeAction( cTID('setd'), desc8, DialogModes.NO );
};



"PathItem.js"

// EOF
