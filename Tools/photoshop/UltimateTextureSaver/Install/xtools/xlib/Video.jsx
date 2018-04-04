//
// Video.jsx
//
function selectAllAnimationFrames(doc) {
  Stdlib.doEvent(doc, sTID("animationSelectAll"));
};

function deleteSelectedFrames(doc) {
  function _ftn() {
    var desc = new ActionDescriptor();
    var ref = new ActionReference();
    ref.putEnumerated(sTID("animationFrameClass"),
                      cTID("Ordn"),
                      cTID("Trgt"));
    desc.putReference(cTID("null"), ref );
    executeAction(cTID("Dlt "), desc, DialogModes.NO );
  }

  Stdlib.wrapLC(doc, _ftn);
};
function deleteAnimation(doc) {
  function _ftn() {
    var desc = new ActionDescriptor();
    var ref = new ActionReference();
    ref.putEnumerated(sTID("animationClass"),
                      cTID("Ordn"),
                      cTID("Trgt"));
    desc.putReference(cTID("null"), ref );
    executeAction(cTID("Dlt "), desc, DialogModes.NO );
  }

  Stdlib.wrapLC(doc, _ftn);
};
function makeFramesFromLayers(doc) {
  Stdlib.doEvent(doc, sTID("animationFramesFromLayers"));
};

function deinterlaceFilter(doc, layer, even, dupe) {

  function _ftn() {
    var desc = new ActionDescriptor();

    var eoFlag = (even ? cTID('ElmE') : cTID('ElmO'));
    var dupeFlag = (dupe ? cTID('CrtD') : cTID('CrtI'));

    desc.putEnumerated(cTID('IntE'), cTID('IntE'), eoFlag);
    desc.putEnumerated(cTID('IntC'), cTID('IntC'), dupeFlag);
    executeAction(cTID('Dntr'), desc, DialogModes.NO);
  }

  Stdlib.wrapLCLayer(doc, layer, _ftn);
};

function selectAnimationFrame(doc, idx) {
  var desc = new ActionDescriptor();
  var ref = new ActionReference();
  ref.putIndex(sTID('animationFrameClass'), idx);
  desc.putReference(cTID('null'), ref);
  executeAction(cTID('slct'), desc110, DialogModes.NO);
};

TimeCode = function() {
  var self = this;
  self.minutes = 0;
  self.seconds = 0;
  self.frame = 0;
  self.frameRate = 0;
};
function importVideo(doc, file, start, stop, makeAnimation, frameSkip) {
    var desc = new ActionDescriptor();

    desc.putPath(cTID('Usng'), file);

    if (start) {
      var startDesc = new ActionDescriptor();
      startDesc.putInteger(sTID('minutes'), start.minutes);
      startDesc.putInteger(sTID('seconds'), start.seconds);
      startDesc.putInteger(sTID('frame'), start.frame);
      startDesc.putDouble(sTID('frameRate'), start.frameRate);
      desc.putObject(sTID('startTime'), sTID('timecode'), startDesc);
    }
    if (stop) {
      var stopDesc = new ActionDescriptor();
      stopDesc.putInteger(sTID('minutes'), stop.minutes);
      stopDesc.putInteger(sTID('seconds'), stop.seconds);
      stopDesc.putInteger(sTID('frame'), stop.frame);
      stopDesc.putDouble(sTID('frameRate'), stop.frameRate);
      desc.putObject(sTID('endTime'), sTID('timecode'), stopDesc);
    }
    if (makeAnimation) {
      desc.putBoolean(sTID('makeAnimation'), true);
    }
    if (frameSkip) {
      desc.putInteger(sTID('frameSkip'), frameSkip);
    }
    executeAction(sTID('importVideoToLayers'), desc, DialogModes.NO);
};


