//
// Gradient.jsx
//
// $Id: Gradient.jsx,v 1.4 2014/11/25 02:32:13 anonymous Exp $
//
//@show include
//
try {
  cTID;
 } catch (e) {
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
    Error.runtimeError(9001, "Bad typeid: " + s);
  };
}

GradientType = function() {

};
GradientType.ANGLE     = 'Angl';   // PSEnum.Angle
GradientType.DIAMOND   = 'Dmnd';   // PSEnum.Diamond
GradientType.LINEAR    = 'Lnr ';   // PSEnum.Linear
GradientType.RADIAL    = 'Rdl ';   // PSEnum.Radial
GradientType.REFLECTED = 'Rflc';   // PSEnum.Reflected

GradientType.ANGLE.toString     = function() { return 'Angle'; };
GradientType.DIAMOND.toString   = function() { return 'Diamond'; };
GradientType.LINEAR.toString    = function() { return 'Linear'; };
GradientType.RADIAL.toString    = function() { return 'Radial'; };
GradientType.REFLECTED.toString = function() { return 'Reflected'; };
GradientType.list = ['Linear',
                     'Radial',
                     'Angle',
                     'Diamond',
                     'Reflected'];

GradientType.map = function(str) {
  str = str.toUpperCase();

  for (var idx in GradientType) {
    var v = GradientType[idx];
    if (idx.toString().toUpperCase() == str) {
      return v;
    }
  }
  return undefined;
};

Gradient = function() {
  var self = this;

  self.from    = [0,50];   // x,y in %
  self.to      = [100,50];   // x,y in %
  self.grType  = GradientType.LINEAR;
  self.dither  = true;
  self.useMask = true;
  self.reverse = false; // fixed
  self.blendMode = 'Nrml';  // default to normal
  self.opacityStops = [0, 100];   // default stops in %
  self.form  = "Foreground to Transparent";    // fixed

  return self;
}

Gradient.createRGBColor = function(r, g, b) {
  var c = new RGBColor();
  if (r instanceof Array) {
    b = r[2]; g = r[1]; r = r[0];
  }
  c.red = parseInt(r); c.green = parseInt(g); c.blue = parseInt(b);
  var sc = new SolidColor();
  sc.rgb = c;
  return sc;
};

Gradient.BLACK   = Gradient.createRGBColor(0, 0, 0);       // fixed
Gradient.WHITE   = Gradient.createRGBColor(255, 255, 255); // fixed

// =======================================================
Gradient.prototype.toObjectString = function() {
  var self = this;
  var str = '';
  str += "{\n";
  str += "from: [" + self.from[0] + ',' + self.from[1] + "],\n";
  str += "to: [" + self.to[0] + ',' + self.to[1] + "],\n";
  str += "grType: \"" + self.grType + "\",\n";
  str += "dither: \"" + self.dither + "\",\n";
  str += "useMask: \"" + self.useMask + "\",\n";
  str += "reverse: \"" + self.reverse + "\",\n";
  str += "blendMode: \"" + self.blendMode + "\",\n";
  str += ("opacityStops: [" + self.opacityStops[0] + ',' +
          self.opacityStops[1] + "],\n");
  str += "form: \"" + self.form + "\"\n}\n";
  return str;
};

// =======================================================
Gradient.fromObjectString = function(str, gr) {
  if (!gr) {
    gr = new Gradient();
  }

  var obj;
  eval("obj = " + str);
  for (var idx in obj) {
    gr[idx] = obj[idx];
  }

  return gr;
};
// =======================================================
Gradient.prototype.apply = function() {
  var self = this;
    var desc = new ActionDescriptor();
        var fromDesc = new ActionDescriptor();
        fromDesc.putUnitDouble( PSEnum.Horizontal,
                                PSUnit.Percent,
                                self.from[0] );
        fromDesc.putUnitDouble( PSEnum.Vertical,
                                PSUnit.Percent,
                                self.from[1] );
    desc.putObject( PSKey.From, PSClass.Point, fromDesc );

        var toDesc = new ActionDescriptor();
        toDesc.putUnitDouble( PSEnum.Horizontal,
                              PSUnit.Percent,
                              self.to[0] );
        toDesc.putUnitDouble( PSEnum.Vertical,
                              PSUnit.Percent,
                              self.to[1] );
    desc.putObject( PSKey.To, PSClass.Point, toDesc );

    desc.putEnumerated( PSKey.Type,
                        PSType.GradientType,
                        cTID(self.grType) );
    desc.putBoolean( PSKey.Dither, self.dither );
    desc.putBoolean( PSKey.UseMask, self.useMask );
    desc.putBoolean( PSKey.Reverse, self.reverse )
    desc.putEnumerated(PSKey.Mode, PSType.BlendMode, cTID(self.blendMode) );

        var grDesc = new ActionDescriptor();
        grDesc.putString( PSKey.Name, self.form);
        grDesc.putEnumerated( PSType.GradientForm,
                              PSType.GradientForm,
                              PSEnum.CustomStops );
        grDesc.putDouble( PSKey.Interpolation, 4096.000000 );

            var clrList = new ActionList();
                var cstop1 = new ActionDescriptor();
                cstop1.putEnumerated( PSKey.Type,
                                      PSType.ColorStopType,
                                      PSEnum.ForegroundColor );
                cstop1.putInteger( PSKey.Location, 0 );
                cstop1.putInteger( PSKey.Midpoint, 50 );
            clrList.putObject( PSClass.ColorStop, cstop1 );
                var cstop2Desc = new ActionDescriptor();
                cstop2Desc.putEnumerated( PSKey.Type,
                                          PSType.ColorStopType,
                                          PSEnum.ForegroundColor );
                cstop2Desc.putInteger( PSKey.Location, 4096 );
                cstop2Desc.putInteger( PSKey.Midpoint, 50 );
            clrList.putObject( PSClass.ColorStop, cstop2Desc );
        grDesc.putList( PSKey.Colors, clrList );

            var trsnpList = new ActionList();
                var tstop1Desc = new ActionDescriptor();
                tstop1Desc.putUnitDouble( PSKey.Opacity,
                                          PSUnit.Percent,
                                          100.000000 );
                tstop1Desc.putInteger( PSKey.Location,
                                       self.opacityStops[0] * 40.96);
                tstop1Desc.putInteger( PSKey.Midpoint, 50 );
            trsnpList.putObject( PSClass.TransparencyStop, tstop1Desc );
                var tstop2Desc = new ActionDescriptor();
                tstop2Desc.putUnitDouble( PSKey.Opacity,
                                          PSUnit.Percent,
                                          0.000000 );
                tstop2Desc.putInteger( PSKey.Location,
                                       self.opacityStops[1] * 40.96);
                tstop2Desc.putInteger( PSKey.Midpoint, 50 );
            trsnpList.putObject( PSClass.TransparencyStop, tstop2Desc );
        grDesc.putList( PSEnum.Transparent, trsnpList );

    desc.putObject( PSKey.Gradient, PSClass.Gradient, grDesc );
    executeAction( PSClass.Gradient, desc, DialogModes.NO );
};

Gradient.loadTypeIDs = function() {
  var needsDefs = true;
  try {
    PSClass;
    needsDefs = false;
  } catch (e) {
  }

  if (needsDefs) {
    PSClass = function() {};
    PSEnum = function() {};
    PSEvent = function() {};
    PSForm = function() {};
    PSKey = function() {};
    PSType = function() {};
    PSUnit = function() {};
    PSString = function() {};
  }

  PSClass.ColorStop = cTID('Clrt');
  PSClass.Document = cTID('Dcmn');
  PSClass.Gradient = cTID('Grdn');
  PSClass.Layer = cTID('Lyr ');
  PSClass.Point = cTID('Pnt ');
  PSClass.TransparencyStop = cTID('TrnS');
  PSEnum.CustomStops = cTID('CstS');
  PSEnum.ForegroundColor = cTID('FrgC');
  PSEnum.Horizontal = cTID('Hrzn');
  PSEnum.Linear = cTID('Lnr ');
  PSEnum.Transparent = cTID('Trns');
  PSEnum.Vertical = cTID('Vrtc');
  PSKey.Colors = cTID('Clrs');
  PSKey.Dither = cTID('Dthr');
  PSKey.From = cTID('From');
  PSKey.Gradient = cTID('Grad');
  PSKey.Interpolation = cTID('Intr');
  PSKey.Location = cTID('Lctn');
  PSKey.Mode = cTID('Md  ');
  PSKey.Midpoint = cTID('Mdpn');
  PSKey.Name = cTID('Nm  ');
  PSKey.Opacity = cTID('Opct');
  PSKey.Reverse = cTID('Rvrs')
  PSKey.To = cTID('T   ');
  PSKey.Type = cTID('Type');
  PSKey.UseMask = cTID('UsMs');
  PSType.BlendMode = cTID("BlnM");
  PSType.ColorStopType = cTID('Clry');
  PSType.GradientForm = cTID('GrdF');
  PSType.GradientType = cTID('GrdT');
  PSUnit.Percent = cTID('#Prc');
};

Gradient.loadTypeIDs();

GradientUI = function() {};
GradientUI.createWindow = function() {
  var wrect = {
    x: 200, 
    y: 200,
    w: 275,
    h: 210
  };

  var xOfs = 20;
  var yOfs = 20;
  var yy = yOfs;
  var xx = xOfs;
  var col2 = 90;
  var def = new Gradient();

  var win = new Window('dialog', 'Gradient Mask Creator',
                       [wrect.x, wrect.y, wrect.x+wrect.w, wrect.y+wrect.h]);

  win.add('statictext', [xx, yy, xx+col2, yy+20], 'From (x,y): ');
  xx += col2;
  win.fromXY = win.add('edittext', [xx, yy, xx+60, yy+20],
                       def.from[0] + ',' + def.from[1]);
  xx += 70;
  win.add('statictext', [xx, yy, xx+55, yy+20], '(percent)');
  yy += 30;
  xx = xOfs;

  win.add('statictext', [xx, yy, xx+col2, yy+20], 'To (x,y): ');
  xx += col2;
  win.toXY = win.add('edittext', [xx, yy, xx+60, yy+20],
                     def.to[0] + ',' + def.to[1]);
  xx += 70;
  win.add('statictext', [xx, yy, xx+55, yy+20], '(percent)');
  yy += 30;
  xx = xOfs;

  win.add('statictext', [xx, yy, xx+col2, yy+20], 'Opacity Stops: ');
  xx += col2;
  win.opacityStops = win.add('edittext', [xx, yy, xx+60, yy+20],
                             def.opacityStops[0] + ',' + def.opacityStops[1]);
  xx += 70;
  win.add('statictext', [xx, yy, xx+55, yy+20], '(percent)');
  yy += 30;
  xx = xOfs;

  win.add('statictext', [xx, yy, xx+col2, yy+20], 'Gradient Type: ');
  xx += col2;
  win.grType = win.add('dropdownlist', [xx, yy, xx+90, yy+20],
                       GradientType.list);
  win.grType.selection = win.grType.items[0];
  yy += 30;
  xx = xOfs;

  win.dither = win.add('checkbox', [xx, yy, xx+70, yy+20], 'Dither');
  win.dither.value = def.dither;
  xx += 70;

  win.useMask = win.add('checkbox', [xx, yy, xx+90, yy+20], 'Use Mask');
  win.useMask.value = def.useMask;
  xx += 90;

  win.reverse = win.add('checkbox', [xx, yy, xx+70, yy+20], 'Reverse');
  win.reverse.value = def.reverse;
  yy += 30;
  xx = xOfs;

  var btnY = wrect.h - 30;
  win.process   = win.add('button', [30,btnY,130,btnY+20], 'Process');
  win.cancel    = win.add('button', [150,btnY,250,btnY+20], 'Cancel');

  win.defaultElement = win.process;

  win.process.onClick = function() {
    try {
      var rc = this.parent.validate();
      if (!rc) {
        this.parent.close(2);
      }
    } catch (e) {
      alert(e.toSource());
    }
  }

  win.validate = GradientUI.validateCB;

  return win;
};

GradientUI.run = function() {
  var win = GradientUI.createWindow();

  win.show();
  //win.validate();
  //win.gr = new Gradient();

  if (win.gr && win.gr instanceof Gradient) {
    return win.gr;
  } else {
    return undefined;
  }
};
GradientUI.validateCB = function() {
  var win = this;
  function errorPrompt(str) {
    return confirm(str + "\r\rContinue?", false);
  }
  function strToRange(str, min, max) {
    try {
      str = str.replace(/\s/g, '')
      var p = str.split(',');
      var ar = [ Number(p[0]), Number(p[1])];

      if (ar[0] < min || ar[0] > max ||
          ar[1] < min || ar[1] > max) {
        return undefined;
      }

      return ar;

    } catch (e) {
      alert(e.toSource());
      return undefined;
    }
  }
  try {
    var gr = new Gradient();

    gr.from = strToRange(win.fromXY.text, 0, 100);
    if (!gr.from) {
      return errorPrompt("Bad 'from' range");
    }
    
    gr.to = strToRange(win.toXY.text, 0, 100);
    if (!gr.to) {
      return errorPrompt("Bad 'to' range");
    }
  
    gr.opacityStops = strToRange(win.opacityStops.text, 0, 100);
    if (!gr.opacityStops) {
      return errorPrompt("Bad opacity stops");
    }
    if (gr.opacityStops[0] != 0 || gr.opacityStops[1] != 100) {
      gr.form = "Custom"
    }

    gr.dither = win.dither.value;
    gr.useMask = win.useMask.value;
    gr.reverse = win.reverse.value;

    gr.grType = GradientType.map(win.grType.selection.text);

    win.gr = gr;
    win.close(1);
    return true;

  } catch (e) {

    alert(e.toSource());
    return false;
  }
};

Getter = function() {};

Getter.getDocumentIndex = function(doc) {
  var docs = app.documents;
  for (var i = 0; i < docs.length; i++) {
    if (doc == docs[i]) {
      return i+1;
    }
  }
  return undefined;
};

Getter.getDescriptorElement = function(desc, key) {
  if (!desc.hasKey(key)) {
    return undefined
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
    default:
      try {
        if (typ == DescValueType.LARGEINTEGERTYPE) {
          v = desc.getLargeInteger(key);
        }
      }  catch (e) {
      }
      break;
  }

  return v;
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

  return (key ? Getter.getDescriptorElement(desc, key) : desc);
};

Getter.getLayerDescriptor = function(doc, layer) {
  var docIndex = Getter.getDocumentIndex(doc);
  var layerIndex = -1;
  var layers = doc.layers;

  var hasBackground = false;
  try {
    doc.backgroundLayer;
    hasBackground = true;
  } catch (e) {
  }

  for (var i = 0; i < layers.length; i++) {
    if (layer == layers[i]) {
      layerIndex = layers.length - i;
      if (hasBackground) layerIndex--;
      break;
    }
  }
  if (layerIndex == -1) {
    throw "Unable to find indicated layer in the document.";
  }

  return Getter.getInfoByIndexIndex(layerIndex, docIndex, PSClass.Layer,
                                    PSClass.Document);
};
Gradient.createLayerMask = function(doc, layer) {
  function _ftn() {
    var desc = new ActionDescriptor();
    desc.putClass(cTID("Nw  "), cTID("Chnl"));
    var ref = new ActionReference();
    ref.putEnumerated(cTID("Chnl"), cTID("Chnl"), cTID("Msk "));
    desc.putReference(cTID("At  "), ref);
    desc.putEnumerated(cTID("Usng"), cTID("UsrM"), cTID("RvlA"));
    executeAction(cTID("Mk  "), desc, DialogModes.NO);
  }

  if (doc != app.activeDocument) {
    app.activeDocument = doc;
  }
  if (layer && layer != doc.activeLayer) {
    doc.activeLayer = layer;
  }
  _ftn();
};
Gradient.selectLayerMask = function(doc, layer) {
  function _ftn() {
    var desc = new ActionDescriptor();
    var ref = new ActionReference();

    ref.putEnumerated(cTID("Chnl"), cTID("Chnl"), cTID("Msk "));
    desc.putReference(cTID("null"), ref);
    desc.putBoolean(cTID("MkVs"), false );
    executeAction(cTID("slct"), desc, DialogModes.NO );
  }

  _ftn();
};
Gradient.hasMask = function(doc, layer) {
  var ldesc = Getter.getLayerDescriptor(doc, layer);
  return (ldesc.hasKey(cTID('UsrM')) && ldesc.getBoolean(cTID('UsrM')));
};
Gradient.resetFgBg = function() {
  var ref = new ActionReference();
  ref.putProperty( cTID('Clr '), cTID('Clrs'));
  var desc = new ActionDescriptor();
  desc.putReference( cTID('null'), ref );
  executeAction(cTID('Rset'), desc, DialogModes.NO );
};
Gradient.test = function() {
  var gr = GradientUI.run();
  if (!gr) {
    return;
  }
  var doc = app.activeDocument;
  var layer = doc.activeLayer;

  // this code will create a layer mask and apply the
  // gradient to the mask
  if (false) {
    if (layer.isBackgroundLayer) {
      layer.isBackgroundLayer = false;
      return;
    }
    if (!Gradient.hasMask(doc, layer)) {
      Gradient.createLayerMask(doc, layer);
    } else {
      // clear the existing mask
      Gradient.selectLayerMask(doc, layer);
    }

    doc.selection.selectAll();
    doc.selection.fill(Gradient.WHITE);
    doc.selection.deselect();
  }

  gr.apply();

  // Test writing/reading the gradient settings
  var str = gr.toObjectString();
  writeToFile(new File(Folder.temp + "/gradient.ini"), str);
  alert(str);
  var g = Gradient.fromObjectString(str);
  alert(gr.toObjectString());
};
function writeToFile(file, str) {
  file.open("w");
  file.writeln(str);
  file.close();
}

//Gradient.test();

"Gradient.jsx";

// EOF
