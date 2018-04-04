//
// PreviewWindow
//
// $Id: PreviewWindow.jsx,v 1.12 2014/10/19 00:40:21 anonymous Exp $
// Copyright: (c)2008, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//

// All values are in pixels

PreviewWindow = function() {
};
PreviewWindow.MAX_WIDTH = 1024;
PreviewWindow.MAX_HEIGHT = 1024;
PreviewWindow.DELTA_H = 200;

PreviewWindow.getPrimaryScreenSize = function() {
  var scrs = $.screens;

  for (var i = 0; i < scrs.length; i++) {
    var scr = scrs[i];
    if (scr.primary == true) {
      return [scr.right-scr.left, scr.bottom-scr.top];
    }
  }

  return undefined;
};

// Returns a valid File object
PreviewWindow._checkFile = function(file) {
  if (!file) {
    return undefined;
  }
  if (file.constructor == String) {
    file = new File(file);
  }
  if (!(file instanceof File)) {
    return undefined;
  }
  if (!file.exists) {
    return undefined;
  }

  return file;
};

PreviewWindow.open = function(file, w, h, title, ms, parent) {
  file = PreviewWindow._checkFile(file);

  if (!file) {
    return;
  }

  if (!w) {
    w = PreviewWindow.MAX_WIDTH;
  }
  if (!h) {
    var size = PreviewWindow.getPrimaryScreenSize();
    h = Math.min(PreviewWindow.MAX_HEIGHT, size[1]-PreviewWindow.DELTA_H);
  }

  var doc = app.open(file);

  var resized = false;
  if (doc.width.as("px") > w || doc.height.as("px") > h) {
    function cTID(s) { return app.charIDToTypeID(s); };
    function sTID(s) { return app.stringIDToTypeID(s); };

    var desc = new ActionDescriptor();
    desc.putUnitDouble( cTID('Wdth'), cTID('#Pxl'), w );
    desc.putUnitDouble( cTID('Hght'), cTID('#Pxl'), h );

    var fitId = sTID('3caa3434-cb67-11d1-bc43-0060b0a13dc4');
    executeAction(fitId , desc, DialogModes.NO );
    resized = true;
  }

  w = doc.width.as("px");
  h = doc.height.as("px");

  var remove = false;
  if (resized || !file.name.match(/.png$/i)) {
    remove = true;
    file = new File(Folder.temp + "/preview.png");
    file.remove();

    doc.saveAs(file, new PNGSaveOptions(), true);
  }

  doc.close(SaveOptions.DONOTSAVECHANGES);

  PreviewWindow.openFile(file, w, h, title, ms, parent);

  if (remove) {
    file.remove();
  }
};

PreviewWindow.openFile = function(file, w, h, title, ms, parent) {
  var type = (ms > 0) ? 'palette' : 'dialog';
  var win = new Window(type, title || "Preview: " + decodeURI(file.name));

  win.closeBtn = win.add('button', undefined, 'Close');
  win.preview = win.add('image', undefined);
  win.preview.icon = file;

  if (w && h) {
    win.preview.preferredSize = [w, h];
  }

  if (parent) {
    win.center(parent);
  }

  win.closeBtn.onClick = function() {
    this.parent.close(1);
  }

  win.show();
  if (ms > 0) {
    $.sleep(ms);
  }

  delete win;
  $.gc();
};


PreviewWindow.main = function() {
  PreviewWindow.open("~/Desktop/test.jpg");

  PreviewWindow.open("~/Desktop/test.jpg", undefined, undefined,
                     "Preview Test", 3000);
};

// PreviewWindow.main();

"PreviewWindow.jsx";
// EOF
