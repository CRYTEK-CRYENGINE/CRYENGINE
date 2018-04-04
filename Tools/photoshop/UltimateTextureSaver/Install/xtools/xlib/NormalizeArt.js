//
// NormalizeArt
// This is a version of Normalize that is tweaked for normalizing
// scanned in art.
//
// $Id: NormalizeArt.js,v 1.5 2006/11/16 17:30:10 anonymous Exp $
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/Levels.js"
//
var app; if (!app) app = this; // for PS7
isPS7 = function()  { return version.match(/^7\./); };
cTID = function(s) { return app.charIDToTypeID(s); };
sTID = function(s) { return app.stringIDToTypeID(s); };

NormalizeArtOptions = function(obj) {
  var self = this;
  self.colorProfile = ColorProfileNames.ADOBE_RGB;
  self.bitDepth = 16;
  self.maxSize = 0;
  self.adjustLevels = true;
  self.histogramCutoff = 4;
  self.convertProfile = true;
  //self.nrAction = "Normal";
  //self.nrActionSet = "ISO PS NR.atn";

  self.setBitDepth = function(depth) {
    switch (depth) {
      case BitsPerChannelType.ONE: this.bitDepth = 1; break;
      case BitsPerChannelType.EIGHT: this.bitDepth = 8; break;
      case BitsPerChannelType.SIXTEEN: this.bitDepth = 16; break;
      case BitsPerChannelType.THIRTYTWO: this.bitDepth = 32; break;
    }
  }
  self.getBitDepth = function() {
    var d = undefined;
    switch (this.bitDepth) {
      case 1:  d = BitsPerChannelType.ONE; break;
      case 8:  d = BitsPerChannelType.EIGHT; break;
      case 16: d = BitsPerChannelType.SIXTEEN; break;
      case 32: d = BitsPerChannelType.THIRTYTWO; break;
    }
    return d;
  }
  if (obj) {
    for (var idx in obj) {
      var v = obj[idx];
      if (typeof v != 'function') {
        self[idx] = v;
      }
    }
  }
};

ColorProfileNames = function ColorProfileNames() {};
ColorProfileNames.ADOBE_RGB    = "Adobe RGB (1998)";
ColorProfileNames.APPLE_RGB    = "Apple RGB";
ColorProfileNames.SRGB         = "sRGB IEC61966-2.1";


NormalizeArt = function NormalizeArt() {}

NormalizeArt.exec = function(doc, opts) {
  // we need to make this document the active document
  // because may be running actions later
  if (doc != app.activeDocument) {
    app.activeDocument = doc;
  }
  // Step 1: Save the file as a PSD and switch to it
  // step removed...

  // Step 2: Switch to RGB mode, if needed
  if (doc.mode != DocumentMode.RGB) {
    doc.changeMode(ChangeMode.RGB)
  }

  // Step 3: Assign/Convert the color profile to match the desired profile
  if (doc.colorProfileType == ColorProfile.NONE) {
    // Assign the doc to the Working Color Profile
    // but don't convert it
    doc.colorProfileType = ColorProfile.WORKING;
    if (doc.colorProfileName != opts.colorProfile) {
      throw "The working color profile must be adjusted to match: "
        + opts.colorProfile;
    }

  } else {

    var profName = doc.colorProfileName;
    if (profName != opts.colorProfile) {
      // this stunt gets us the name of the
      // profile name for the WORKING space
      doc.colorProfileType = ColorProfile.WORKING;
      var workingProfile = doc.colorProfileName;
      doc.colorProfileName = profName;
      
      if (workingProfile != opts.colorProfile) {
        throw "The working color profile must be adjusted to match: "
          + opts.colorProfile;
      }
        
      // Convert or Assign the doc to the Working Color Profile
      // if the profile is really different
      if (doc.colorProfileType == ColorProfile.CUSTOM) {
        if (opts.colorProfile != profName) {
          if (opts.convertProfile) {
            // Convert
            doc.convertProfile(opts.colorProfile,
                               Intent.RELATIVECOLORIMETRIC,
                               true, false, false);
          } else {
            // Assign
            doc.colorProfileType = ColorProfile.WORKING;
          }
        }
      }
    }
  }
    
  // Step 4: Set the bit depth to 16
  doc.bitsPerChannel = opts.getBitDepth();

  // Step 4.5: Noise Reduction
  if (opts.nrAction) {
    app.doAction(opts.nrAction, opts.nrActionSet);
  }

  // Step 5: Resize (optional)
  if (opts.maxSize > 100) {
    NormalizeArt.pscsFit(doc, opts.maxSize, opts.maxSize);
  }

  // Step 6: Adjust Levels
  if (opts.adjustLevels) {
    Levels.autoAdjustRGBChannels(doc, opts.histogramCutoff);
  }

  // Step 6.5 USM Haze
  // fix later...
  // app.doAction("USM Haze", "XActions.atn");

  // Step 7: Save changes and close
  if (!doc.name.match(/\.psd$/i)) {
    var file = new File(doc.fullName.absoluteURI.replace(/\.[^\.]+$/, ".psd"));
    doc.saveAs(file, NormalizeArt.defaultPSDOptions(), true);
    NormalizeArt.revertDocument(doc);
  } else {
    doc.saveAs(file, NormalizeArt.defaultPSDOptions());
  }
};
NormalizeArt.defaultPSDOptions = function() {
  var opts = new PhotoshopSaveOptions();
  opts.alphaChannels = true;
  opts.annotations = true;
  opts.embedColorProfile = true;
  opts.layers = true;
  opts.maximizeCompatibility = true;
  opts.spotColors = true;
  return opts;
};
NormalizeArt.pscsFit = function(doc, width, height) {
  if (doc != app.activeDocument) {
    app.activeDocument = doc;
  }
  var ru = app.preferences.rulerUnits;
  try {
    app.preferences.rulerUnits = Units.PIXELS;
    var desc = new ActionDescriptor();
    desc.putUnitDouble(cTID("Wdth"),  cTID("#Pxl"), width);
    desc.putUnitDouble(cTID("Hght"),  cTID("#Pxl"), height);
    executeAction(sTID("3caa3434-cb67-11d1-bc43-0060b0a13dc4"),
                  desc, DialogModes.NO);
  } finally {
    app.preferences.rulerUnits = ru;
  }
};
NormalizeArt.revertDocument = function(doc) {
  executeAction(cTID("Rvrt"), new ActionDescriptor(), DialogModes.NO);
};

function main() {
  if (isPS7()) {
    alert("This script will not run in PS7.");
    return;
  }
  if (!app.documents.length) {
    alert("Please open a document before running this script.");
    return;
  }


  var doc = app.activeDocument;

  // 'normalize' the image
  var opts = new NormalizeArtOptions();
  opts.nrAction = null;
  opts.adjustLevels = false;
  opts.maxSize = 1500;

  NormalizeArt.exec(doc, opts);
};

main();


"NormalizeArt.js";
// EOF
