//
// AdjustmentLayers
//
// $Id: AdjustmentLayers.jsx,v 1.12 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2006, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//include "xlib/stdlib.js"
//

AdjustmentLayers = function() {
};

AdjustmentLayers.createDescriptor = function(info, aldesc) {
  var desc = new ActionDescriptor();
  var ref = new ActionReference();

  ref.putClass(cTID('AdjL'));
  desc.putReference(cTID('null'), ref);

  var adesc = new ActionDescriptor();
  var id = (info.id.length == 4 ? cTID(info.id) : sTID(info.id));

  if (aldesc && aldesc.count != 0) {
    adesc.putObject(cTID('Type'), id, aldesc);
  } else {
    adesc.putClass(cTID('Type'), id);
  }

  desc.putObject(cTID('Usng'), cTID('AdjL'), adesc);

  return desc;
};

AdjustmentLayers.create = function(info, doc, mode, aldesc) {
  function _ftn() {
    var desc = AdjustmentLayers.createDescriptor(info, aldesc);
    try {
      return executeAction(cTID('Mk  '), desc, mode);

    } catch (e) {
      if (e.number == 8007) {  // User cancelled
        return undefined;
      } else {
        throw e;
      }
    }
  }

  return _ftn();
};

AdjustmentLayers.modifyDescriptor = function(info, aldesc) {
  var desc = new ActionDescriptor();
  var ref = new ActionReference();

  var id = (info.id.length == 4 ? cTID(info.id) : sTID(info.id));

  ref.putEnumerated(cTID('AdjL'), cTID('Ordn'), cTID('Trgt'));
  desc.putReference(cTID('null'), ref);

  if (!aldesc) {
    aldesc = new ActionDescriptor();
  }
  desc.putObject(cTID('T   '), id, aldesc);

  return desc;
};

AdjustmentLayers.modify = function(info, doc, layer, mode, aldesc) {
  function _ftn() {
    var desc = AdjustmentLayers.modifyDescriptor(info, aldesc);
    try {
      return executeAction(cTID('setd'), desc, mode);

    } catch (e) {
      if (e.number == 8007) {  // User cancelled
        return undefined;
      } else {
        throw e;
      }
    }
  }

  return Stdlib.wrapLCLayer(doc, layer, _ftn);
};

AdjustmentLayers.adjInfo = {
  levels:             { name: "levels", id: 'Lvls' },
  curves:             { name: "curves", id: 'Crvs' },
  colorBalance:       { name: "colorBalance", id: 'ClrB' },
  brightnessContrast: { name: "brightnessContrast", id: 'BrgC' },
  hueSaturation:      { name: "hueSaturation", id: 'HStr' },
  selectiveColor:     { name: "selectiveColor", id: 'SlcC' },
  channelMixer:       { name: "channelMixer", id: 'ChnM' },
  gradientMap:        { name: "gradientMap", id: 'GdMp' },
  photoFilter:        { name: "photoFilter", id: 'photoFilter' },
  invert:             { name: "invert", id: 'Invr' },
  threshold:          { name: "threshold", id: 'Thrs' },
  posterize:          { name: "posterize", id: 'Pstr' },
  blackAndWhite:      { name: "blackAndWhite", id: 'BanW' },
  gradientMap:        { name: "gradientMap", id: 'GdMp' },
  vibrance:           { name: "vibrance", id: 'vibrance' },
  selectiveColor:     { name: "selectiveColor", id: 'SlcC' },
  solidColorLayer:    { name: "solidColorLayer", id: "solidColorLayer"},
  gradientLayer:      { name: "gradientLayer", id: "gradientLayer"},
  patternLayer:       { name: "patternLayer", id: "patternLayer"},
};

AdjustmentLayers._init = function() {
  if (isCS3() || isCS4() || isCS5()) {
    AdjustmentLayers.adjInfo['exposure'] =
    { name: "exposure", id: 'Exps' };
  }
};
AdjustmentLayers._init();

AdjustmentLayers.adjInfo.getByID = function(id) {
  var self = this;
  if (id.constructor == String) {
    id = xTID(id);
  }
  for (var idx in self) {
    var el = self[idx];
    if (typeof(el) == "function") {
      continue;
    }
    if (xTID(el.id) == id) {
      return el;
    }
  }
  return undefined;
};

// alert(AdjustmentLayers.brightnessContrastDescriptor);
// alert(AdjustmentLayers.adjInfo.brightnessContrast.desc);

AdjustmentLayers._init = function() {
  var tbl = AdjustmentLayers.adjInfo;
  for (var idx in tbl) {
    var info = tbl[idx];
    AdjustmentLayers[idx] = {
      info: info,
      create: function(doc, mode, adjDesc) {
        return AdjustmentLayers.create(this.info, doc, mode, adjDesc);
      },
      createLayer: function(doc, mode, adjDesc) {
        AdjustmentLayers.create(this.info, doc, mode, adjDesc);
        return doc.activeLayer;
      },
      modify: function(doc, layer, mode, adjDesc) {
        return AdjustmentLayers.modify(this.info, doc, layer, mode, adjDesc);
      }
    };
  }
};

AdjustmentLayers._init();

//include "xlib/PSConstants.js"
//include "xlib/LogWindow.js"
//include "xlib/Action.js"
//include "xlib/xml/atn2xml.jsx"

function displayDescriptor(desc) {
  var xml = desc.toXML();
  LogWindow.open(xml, 'Descriptor XML');
};


AdjustmentLayers.test = function() {
  if (app.documents.length == 0) {
    alert("Please open a document before running this script.");
    return;
  }

  var doc = app.activeDocument;

  var mode = app.displayDialogs;
  if (mode != DialogModes.NO || app.playbackParameters.count == 0) {
    mode = DialogModes.ALL;
  }
  var desc = app.playbackParameters;

  // adjDesc = AdjustmentLayers.brightnessContrast.create(doc, DialogModes.NO,
  //desc);

  $.level = 1; debugger;
  adjDesc = AdjustmentLayers.brightnessContrast.modify(doc, doc.activeLayer,
                                                       mode, desc);

  displayDescriptor(adjDesc);

  if (adjDesc) {
    app.playbackParameters = adjDesc;
  }
};

//AdjustmentLayers.test();

"AdjustmentLayers.jsx";
// EOF
