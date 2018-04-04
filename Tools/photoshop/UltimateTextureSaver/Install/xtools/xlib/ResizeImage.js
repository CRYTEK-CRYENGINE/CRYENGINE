//
// ResizeImage.js
//
// Resize an image. Uses IM Photography plugin, amongst other techniques
//
// Classes:
//   ResizeImageOptions
//       width
//       height
//       resolution
//       fit
//       stepSize
//       stepCount
//       
// Functions:
//   function resizeImage()
//
// History:
//  2004-09-27 v0.1 Creation date
//
// $Id: ResizeImage.js,v 1.13 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//

var gResizeImageVersion  = "$Revision: 1.13 $";

ResizeOptions = function ResizeOptions(obj) {
  var self = this;
  self.newProperty("number",  "width",      NaN);
  self.newProperty("number",  "height",     NaN);
  self.newProperty("number",  "resolution", 72);
  
  // either stepSize or stepCount can be set
  // Size has priority if both are set
  self.newProperty("number",  "stepSize",   10);
  self.newProperty("number",  "stepCount",  NaN);
  self.newProperty("boolean", "fit",        false);

  self.put("serviceName", name);

  if (width) self.width = width;
  if (height) self.height = height;

  return opts;
};

//=========================== ResizeImage ====================================

ResizeImage = function ResizeImage() {};

ResizeImage.imssResize = function(doc, width, height, resolution,
                                  fit, ssSize, ssCount) {
  if (doc != app.activeDocument) {
    app.activeDocument = doc;
  }

  try {
    var id7 = sTID( "IM Software Group Stairstep Image Size" );
    var desc3 = new ActionDescriptor();

    if (!width && !height) {
      throw "width and/or height must be specified for resize";
    }

    if (width) {
      var id8 = cTID( "Wdth" );
      var id9 = cTID( "#Pxl" );
      desc3.putUnitDouble( id8, id9, width );
    }
    
    if (height) {
      var id10 = cTID( "Hght" );
      var id11 = cTID( "#Pxl" );
      desc3.putUnitDouble( id10, id11, height );
    }
    
    if (!resolution) {
      resolution = doc.resolution;
    }
    var id12 = cTID( "Rslt" );
    var id13 = cTID( "#Rsl" );
    desc3.putUnitDouble( id12, id13, resolution );   // required
    
    if (ssSize) {
      var id14 = cTID( "siS%" );
      desc3.putInteger( id14,  ssSize);
    } else if (ssCount) {
      var id14 = cTID( "siS#" );
      desc3.putInteger( id14,  ssCount);
    } else {
      var id14 = cTID( "siS%" );   // default to 10%
      desc3.putInteger( id14,  10);
    } 
    
    if (fit) {
      var id15 = cTID( "siFI" );
      desc3.putBoolean( id15, true );
    } else {
      var id15 = cTID( "siCP" );
      desc3.putBoolean( id15, true );
    }
    try {
      executeAction( id7, desc3, DialogModes.NO );
    } catch (e) {
      if (e.toString().match(/not currently available/)) {
        throw "The IM StairStep plugin is not installed. No resize";
      }
    }

  } finally {
    if (ad) try { app.activeDocument = ad; } catch (e) {}
  }
};

//
// ResizeImage.pscsFit([doc,] width [, height])
// Examples:
// ResizeImage.pscsFit(500);      Fits the active doc to a 500x500 box
// ResizeImage.pscsFit(500, 600); Fits the active doc to a 500x600 box
// ResizeImage.pscsFit(doc, 500); Fits the doc to a 500x500 box
// ResizeImage.pscsFit(doc, 500, 600); Fits the doc to a 500x600 box
//
ResizeImage.pscsFit = function(arg1, arg2, arg3) {
  var doc, width, height;
  var ad;
  if (typeof arg1 == "object" && arg1 instanceof Document) {
    doc = arg1;
    width = arg2;
    height = arg3;
    try { ad = app.activeDocument; } catch (e) {}
    app.activeDocument = doc;
  } else {
    doc = app.activeDocument; // this could blow chunks
    width = arg1;
    height = arg2;
  }
  if (width == undefined) {
    return false;
  }
  if (height == undefined) {
    height = width;
  }
  var ru = app.preferences.rulerUnits;
  try {
    app.preferences.rulerUnits = Units.PIXELS;
    var desc = new ActionDescriptor();
    desc.putUnitDouble(cTID("Wdth"),  cTID("#Pxl"), parseFloat(width));
    desc.putUnitDouble(cTID("Hght"),  cTID("#Pxl"), parseFloat(height));
    executeAction(sTID("3caa3434-cb67-11d1-bc43-0060b0a13dc4"),
                  desc, DialogModes.NO);
  } finally {
    app.preferences.rulerUnits = ru;
    if (ad) try { app.activeDocument = ad; } catch (e) {}
  }
};

//
// Thanks to Nate Berggren for the original idea
//
// doc is required
// maxWidth or maxHeight is required
// resolution and method are optional
//
ResizeImage.resize = function(doc, maxWidth, maxHeight, resolution, method) { 
  var ru = app.preferences.rulerUnits;
  try {
    app.preferences.rulerUnits = Units.PIXELS;

    var width  = parseInt(doc.width);
    var height = parseInt(doc.height);

    if (!maxWidth && !maxHeight) {
      return;
    }

    if (!maxWidth)  maxWidth = maxHeight;
    if (!maxHeight) maxHeight = maxWidth;
    
    if ((width/maxWidth) > (height/maxHeight)) {
      if (method == undefined) {
        method = (maxWidth > width) ?
          ResampleMethod.BICUBICSMOOTHER : ResampleMethod.BICUBICSHARPER;
      }
      doc.resizeImage(maxWidth, null, resolution, method);
    } else {
      if (method == undefined) {
        method = (maxHeight > height) ?
          ResampleMethod.BICUBICSMOOTHER : ResampleMethod.BICUBICSHARPER;
      }
      doc.resizeImage(null, maxHeight, resolution, method);
    }
  } finally {
    app.preferences.rulerUnits = ru;
  }
};

// doc, maxWidth, and maxHeight are required
// resolution and method are optional
ResizeImage.invariantResize = function(doc, maxWidth, maxHeight,
                                       resolution, method) {
  var ru = app.preferences.rulerUnits;
  try {
    app.preferences.rulerUnits = Units.PIXELS;
    var width = parseInt(doc.width);
    var height = parseInt(doc.height);
    var swap = false;

    if (width >= height && maxHeight > maxWidth) {
      swap = true;
    }
    if (width < height && maxHeight < maxWidth) {
      swap = true;
    }
    if (swap) {
      var tmp = maxHeight;
      maxHeight = maxWidth;
      maxWidth = tmp;
    }
    ResizeImage.resize(doc, maxWidth, maxHeight, resolution, method);
  } finally {
    app.preferences.rulerUnits = ru;
  }
};

ResizeImage.psResize = function(doc, width, height, resolution, method) { 
  var ru = app.preferences.rulerUnits;
  try {
    app.preferences.rulerUnits = Units.PIXELS;
    doc.resizeImage(width, height, resolution, method);

  } finally {
    app.preferences.rulerUnits = ru;
  }
};

ResizeImage.fileResize = function(doc, desiredSize) {
  var ru = app.preferences.rulerUnits;
  try {
    app.preferences.rulerUnits = Units.PIXELS;

    var currentSize = doc.fullName.length;
    var ratio =  Math.sqrt(desiredSize / currentSize);
    var newW = Math.floor(parseInt(doc.width) * ratio);
    var newH = Math.floor(parseInt(doc.height) * ratio);
    
    ResizeImage.resize(doc, newW, newH)
      //  var method = (ratio > 1) ? ResampleMethod.BICUBICSMOOTHER : ResampleMethod.BICUBICSHARPER;

      //  doc.resizeImage(newW, newH, undefined, method);
  } finally {
    app.preferences.rulerUnits = ru;
  }
};

testFileResize = function(doc) {
  ResizeImage.fileResize(doc, 2000000);
};


ResizeImage.exec = function(doc, opts) {
  if (!opts) opts = new ResizeOptions();
  ResizeImage.imssResize(doc, opts.width, opts.height, opts.resolution,
                         opts.fit, opts.stepSize, opts.stepCount);
};

"ResizeImage.js"
// EOF
