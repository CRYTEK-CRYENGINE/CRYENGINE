//
// SaveAs
// Store default "Save As..." stuff here
//
// This is far from complete, but what's here does work
//
// History:
//  2004-09-26 v0.5 Added docs. Formatting improved
//  2004-09-25 v0.1 Creation date
//
// $Id: SaveAs.js,v 1.12 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//

SaveAs = function SaveAs() {}

SaveAs.defaultPSDOptions = function() {
  var opts = new PhotoshopSaveOptions();
  opts.alphaChannels = true;
  opts.annotations = true;
  opts.embedColorProfile = true;
  opts.layers = true;
  opts.maximizeCompatibility = true;
  opts.spotColors = true;
  return opts;
};
  
SaveAs.defaultJPEGOptions = function(quality) {
  var opts = new JPEGSaveOptions();
  opts.embedProfile = true;
  opts.formatOptions = FormatOptions.STANDARDBASELINE;
  opts.quality = quality || 12;
  return opts;
};

SaveAs.defaultTiffOptions = function() {
  var opts = new TiffSaveOptions();
  opts.alphaChannel = false;
  opts.annotations = false;
  opts.embedColorProfile = true;
  opts.imageCompression = TIFFEncoding.TIFFLZW;
  opts.layers = false;
  return opts;
};

SaveAs.save = function(doc, ext, opts, copy) {
  if (copy == undefined) { copy = true; }

  // append the new extension
  if (ext.charAt(0) != '.') { ext = '.' + ext; }

  var file = new File(doc.fullName.absoluteURI.replace(/\.[^\.]+$/, ext));

  // save the file
  doc.saveAs(file, opts, copy, Extension.LOWERCASE);
  return file;
};

SaveAs.savePSD = function(doc, opts, copy) {
  if (opts == undefined) {
    opts = SaveAs.defaultPSDOptions();
  }
  return SaveAs.save(doc, "psd", opts, copy);
};

SaveAs.saveJPEG = function(doc, arg, copy) {
  var opts;

  if (arg == undefined) {
    opts = SaveAs.defaultJPEGOptions();
  } else if (typeof arg == "object" && arg instanceof JPEGSaveOptions) {
    opts = arg;
  } else if (!isNaN(Number(arg))) {
    opts = SaveAs.defaultJPEGOptions(Number(arg));
  } else {
    throw "Error: bad argument \"" + arg + "\"";
  }
  return SaveAs.save(doc, "jpg", opts, copy);
};

SaveAs.saveTiff = function(doc, opts, copy) {
  if (opts == undefined) {
    opts = SaveAs.defaultTiffOptions();
  }
  return SaveAs.save(doc, "tif", opts, copy);
};

SaveAs.saveForWeb = function(doc, quality, dest) {

  var ad;
  if (doc) {
    try { ad = app.activeDocument; } catch (e) {}
    app.activeDocument = doc;
  }
  try {

  var id82 = charIDToTypeID( "Expr" );
  var desc7 = new ActionDescriptor();
  var id83 = charIDToTypeID( "Usng" );
  var desc8 = new ActionDescriptor();
  var id84 = charIDToTypeID( "Op  " );
  var id85 = charIDToTypeID( "SWOp" );
  var id86 = charIDToTypeID( "OpSa" );
  desc8.putEnumerated( id84, id85, id86 );
  var id87 = charIDToTypeID( "Fmt " );
  var id88 = charIDToTypeID( "IRFm" );
  var id89 = charIDToTypeID( "JPEG" );
  desc8.putEnumerated( id87, id88, id89 );
//         var id90 = charIDToTypeID( "Intr" );
//         desc8.putBoolean( id90, false );
  var id91 = charIDToTypeID( "Qlty" );
  desc8.putInteger( id91, quality );
//         var id92 = charIDToTypeID( "QChS" );
//         desc8.putInteger( id92, 0 );
//         var id93 = charIDToTypeID( "QCUI" );
//         desc8.putInteger( id93, 0 );
//         var id94 = charIDToTypeID( "QChT" );
//         desc8.putBoolean( id94, false );
//         var id95 = charIDToTypeID( "QChV" );
//         desc8.putBoolean( id95, false );
  var id96 = charIDToTypeID( "Optm" );
  desc8.putBoolean( id96, true );
//         var id97 = charIDToTypeID( "Pass" );
//         desc8.putInteger( id97, 1 );
//         var id98 = charIDToTypeID( "blur" );
//         desc8.putDouble( id98, 0.000000 );
//         var id99 = charIDToTypeID( "EICC" );
//         desc8.putBoolean( id99, false );
//         var id100 = charIDToTypeID( "Mtt " );
//         desc8.putBoolean( id100, true );
//         var id101 = charIDToTypeID( "MttR" );
//         desc8.putInteger( id101, 255 );
//         var id102 = charIDToTypeID( "MttG" );
//         desc8.putInteger( id102, 255 );
//         var id103 = charIDToTypeID( "MttB" );
//         desc8.putInteger( id103, 255 );
//         var id104 = charIDToTypeID( "SHTM" );
//         desc8.putBoolean( id104, false );
//         var id105 = charIDToTypeID( "SImg" );
//         desc8.putBoolean( id105, true );
//         var id106 = charIDToTypeID( "SSSO" );
//         desc8.putBoolean( id106, false );
//         var id107 = charIDToTypeID( "SSLt" );
//             var list3 = new ActionList();
//         desc8.putList( id107, list3 );

  var id108 = charIDToTypeID( "DIDr" );
  desc8.putBoolean( id108, dest instanceof Folder );
  var id109 = charIDToTypeID( "In  " );
  desc8.putPath( id109, dest );
  var id110 = stringIDToTypeID( "SaveForWeb" );
  desc7.putObject( id83, id110, desc8 );
  
  // remove the old copy, if it exists
  if (dest.exists && dest instanceof File) {
    dest.remove();
  }
  executeAction( id82, desc7, DialogModes.NO );

  } finally {
    if (ad) try { app.activeDocument = ad; } catch (e) {}
  }
};

var app;
if (!app) { app = this; } // for PS7

SaveAs.test = function() {
  var doc = app.activeDocument;

  // save a copy, using default options
  SaveAs.savePSD(doc, undefined, true);
  // save a copy, using JPEG quality 10
  SaveAs.saveJPEG(doc, 10, true);
  // save a copy, using default options
  SaveAs.saveTiff(doc, undefined, true);
  // save a copy, using JPEG quality 10
  SaveAs.saveForWeb(doc, 10, new Folder("/c/temp"));
 
};

"SaveAs.js";
// EOF
