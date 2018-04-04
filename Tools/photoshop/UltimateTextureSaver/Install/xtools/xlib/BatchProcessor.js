//
// BatchProcessor
//
// $Id: BatchProcessor.js,v 1.2 2005/03/11 23:53:20 anonymous Exp $
//@show include
//
BatchProcessor = function() {} // define a class/namespace

BatchProcessor.prototype.subdirs = true;
BatchProcessor.prototype.enterFolder = function(folder) {
  writeln("entering " + decodeURI(folder.absoluteURI));
}
BatchProcessor.prototype.leaveFolder = function(folder) {
  writeln("leaving " + decodeURI(folder.absoluteURI));
}

BatchProcessor.getFolders = function(folder) {
  return BatchProcessor.getFiles(folder, function(file) { return file instanceof Folder; });
}

// dupe from stdlib.js
BatchProcessor.getFiles = function(folder, mask) {
  var files = [];

  if (mask instanceof RegExp) {
    var allFiles = folder.getFiles();
    for (var i = 0; i < allFiles.length; i = i + 1) {
      var f = allFiles[i];
      if (decodeURI(f.absoluteURI).match(mask)) {
        files.push(f);
      }
    }
  } else if (typeof mask == "function") {
    var allFiles = folder.getFiles();
    for (var i = 0; i < allFiles.length; i = i + 1) {
      var f = allFiles[i];
      if (mask(f)) {
        files.push(f);
      }
    }
  } else {
    files = folder.getFiles(mask);
  }

  return files;
}

//
// example:
// BatchProcessor.traverse(new Folder("/c/tmp"), writeln);
//
BatchProcessor.prototype.traverse = function(folder, ftn, mask) {
  var self = this;
  self.enterFolder(folder);

  try {
    if (!mask) mask = "*";

    var files = BatchProcessor.getFiles(folder, mask);
    for (var i = 0; i < files.length; i = i + 1) {
      ftn(files[i]);
    }

    var folders = BatchProcessor.getFolders(folder);
    for (var j = 0; j < folders.length; j = j + 1) {
      self.traverse(folders[j], ftn, mask);
    }
  } finally {
    self.leaveFolder(folder);
  }
}

//
// Sample code that generates thumbnails
//
GenThumbs = function() {} // define a class/namespace
GenThumbs.counter = 0;
GenThumbs.jpegQuality = 10;
GenThumbs.exec = function(file) {
  var doc;
  if (file.name.match(/-thumbnail\.jpg$/)) {
    //try { file.remove(); } catch (e) {}
    return;
  }
  try {
    writeln("processing " + decodeURI(file.absoluteURI));
    doc = app.open(file);
    
    if (true) {
      GenThumbs.processDocument(doc);
      var outf = file.name.replace(/\.jpg$/,
                                   "-" + (++GenThumbs.counter) + "-thumbnail.jpg");
      var opts = new JPEGSaveOptions();
      opts.quality = GenThumbs.jpegQuality;
      doc.saveAs(new File(file.path + '/' + outf),
                 opts, true, Extension.LOWERCASE);
      doc.close(SaveOptions.DONOTSAVECHANGES);
    } else {
      //Levels.autoAdjustRGBChannels(doc, 5);
      doc.close(SaveOptions.DONOTSAVECHANGES);
    }
  } catch (e) {
    writeln(e);
    if (doc) { try  { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (e) {} }
  }
}

// do whatever it is you need to do with the document
GenThumbs.thumbnailSize = 150;
GenThumbs.processDocument = function(doc) {
  var ad;
  try { ad = app.activeDocument; } catch (e) {}
  app.activeDocument = doc;

  // use File->Automate->Fit Image to resize down to a thumbnail
  try {
    var id7 = stringIDToTypeID( "3caa3434-cb67-11d1-bc43-0060b0a13dc4" );
    var desc3 = new ActionDescriptor();
    var id8 = charIDToTypeID( "Wdth" );
    var id9 = charIDToTypeID( "#Pxl" );
    desc3.putUnitDouble( id8, id9, GenThumbs.thumbnailSize);
    var id10 = charIDToTypeID( "Hght" );
    var id11 = charIDToTypeID( "#Pxl" );
    desc3.putUnitDouble( id10, id11, GenThumbs.thumbnailSize );
    executeAction( id7, desc3, DialogModes.NO );
  } finally {
    if (ad) try { app.activeDocument = ad; } catch (e) {}
  }
}

// make the call...
GenThumbs.test = function() {
  var proc = new BatchProcessor();
  var maskFtn = function (f) {
    var n = decodeURI(f.absoluteURI);
    //writeln("checking " + n);
    return n.match(/\.jpg$/) && !n.match(/-thumbnail\.jpg$/);
  }
  var folder = new Folder("/i/tmp/zzzz");

  //proc.traverse(folder, GenThumbs.exec);
  GenThumbs.counter = 0;
  proc.traverse(folder, GenThumbs.exec, "*.jpg");
  GenThumbs.counter = 0;
  proc.traverse(folder, GenThumbs.exec, /\.jpg$/);
  GenThumbs.counter = 0;
  proc.traverse(folder, GenThumbs.exec, maskFtn);
}

//GenThumbs.test();

//alert(GenThumbs.counter + " files processed");
