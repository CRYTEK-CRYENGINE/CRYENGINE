//
// Conf.js
// This file is intended to provide features similar to autoconf in the
// Unix world. It provides a standard way of handling version incompatibilities
// and version specific bugs.
//

if (!app) { app = this; } // for V7

function Conf() {}

Conf.V7  = "7";
Conf.CS  = "8";
Conf.CS2 = "9";

Conf.isV7  = function() { return app.version.charAt(0) == Conf.V7; }
Conf.isCS  = function() { return app.version.charAt(0) == Conf.CS; }
Conf.isCS2 = function() { return app.version.charAt(0) == Conf.CS2; }

// Check for File.getFiles filter bug
if (Conf.isV7() || Conf.isCS()) {
  Conf.File_getFiles_filterBug = true;
}

// Check for Document.resizeImage resampling bug
if (Conf.isCS2()) {
  Conf.Document_resizeImage_resampleBug = true;
}

// Check for Document.resizeImage units bug
if (Conf.isCS2()) {
  Conf.Document_resizeImage_unitsBug = true;
}

/*
function getByName(container, name) {
    for (var i = 0; i < container.length; i++) {
       if (container[i].name == name) {
          return container[i];
       }
    }
    return null;
}

This way you can do
    var layer = getByName(doc.layers, "status");
    var layerComp = getByName(doc.layerComps, "status");
    var layerSet = getByName(doc.layerSets, "status");
    var artLayer = getByName(doc.artLayers, "status");

Or, you can swap out the original busted getByName methods like this:

   function getByNameFIX(name) {
       for (var i = 0; i < this.length; i++) {
          if (this[i].name == name) {
             return this[i];
          }
       }
       return null;
    }

    Layers.prototype.getByName = getByNameFIX;
    LayerComps.prototype.getByName = getByNameFIX;
    LayerSets.prototype.getByName = getByNameFIX;
    ArtLayer.prototype.getByName = getByNameFIX;

This way, you can do the following
    var layer = doc.layers.getByName("status");
    var layerComp = doc.layerComps.getByName("status");
    var layerSet = doc.layerSets.getByName("status");
    var artLayer = doc.artLayers.getByName("status");

*/
//
