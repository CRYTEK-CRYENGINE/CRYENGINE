//
// Levels.js
//
// $Id: Levels.js,v 1.10 2010/03/29 02:23:23 anonymous Exp $
// Copyright: (c)2005, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
// Trim histogram tails
// Levels.autoAdjustRGBChannels(doc, cutoff);
// Levels.autoAdjust(doc, cutoff);
//   doc      The document to be modified
//   cutoff   The maximum value in the histogram we will ignore,
//            typically 1 or 0 (the default)
//
//   [return] true if the doc was modified, false if not
//

//============================= Levels =====================================
Levels = function Levels() {};

Levels.autoAdjustChannel = function(doc, name, cutoff) {
  var modified = false;
  if (doc == undefined) {
    throw "Document must be specified to adjust level(s)";
  }
  if (name == null) {
    throw "Channel name must be specified";
  }
  if (cutoff == undefined) {
    cutoff = 0;
  }
  var channel;
  var channels = doc.activeChannels;
  for (var i = 0; i < channels.length; i++) {
    if (name == channels[i].name) {
      channel = channels[i];
    }
  }

  var HIST_MAX = channel.histogram.length;
  var r = Histogram.range(channel.histogram, cutoff);

  if (r) {
    modified = true;
    var state = [];
    // make this channel the only active/selected/visible one
    for (var i = 0; i < channels.length; i++) {
      var ch = channels[i];
      state[i] = ch.visible;
      if (ch.name == name) {
        ch.visible = true;
        doc.activeChannels = new Array(ch);
      } else {
        ch.visible = false;
      }
    }
    // do the adjustment
    doc.activeLayer.adjustLevels(r.min, r.max, 1.00, 0, (HIST_MAX-1));
    doc.activeChannels = channels;
    // turn all of the channels back on
    for (var i = 0; i < channels.length; i++) {
      channels[i].visible = true;
    }
  }
  return modified;
};
Levels.autoAdjustRGBChannels = function(doc, cutoff) {
  var modified = Levels.autoAdjustChannel(doc, "Red", cutoff);
  modified = Levels.autoAdjustChannel(doc, "Green", cutoff) && modified;
  modified = Levels.autoAdjustChannel(doc, "Blue", cutoff) && modified;
  return modified;
};
Levels.autoAdjustAllChannels = function(doc, cutoff) {
  var modified = true;
  var channels = doc.activeChannels;
  for (var i = 0; i < channels.length; i++) {
    modified = Levels.autoAdjustChannel(doc, channels[i].name, cutoff)
      && modified;
  }
  return modified;
};

Levels.autoAdjust = function(doc, cutoff) {
  if (doc == undefined) {
    throw "Document must be specified to adjust levels";
  }
  if (cutoff == undefined) {
    cutoff = 0;
  }
  //$.level = 1; debugger;

  var modified = false;
  // noop for text layer
  if (doc.activeLayer.kind != LayerKind.TEXT) {

    // make sure we have enough bits per channel
    var bits = doc.bitsPerChannel;
    if (bits == BitsPerChannelType.EIGHT
        || bits == BitsPerChannelType.SIXTEEN) {

      // make sure we can deal with this mode
      var mode = doc.mode;
      if (mode == DocumentMode.CMYK
          || mode == DocumentMode.RGB
          || mode == DocumentMode.LAB) {
        
        var hist = doc.histogram;
        var HIST_MAX = hist.length;
        var r = Histogram.range(hist, cutoff);

        if (r) {
          doc.activeLayer.adjustLevels(r.min, r.max, 1.00, 0, (HIST_MAX-1));
          modified = true;
        }
      }
    }
  }

  return modified;
};


//--------------------------------- Histogram ----------------------------

Histogram = function Histogram() {};

Histogram.range = function(hist, lo_cutoff, hi_cutoff) {
  var HIST_MAX = hist.length;
  if (lo_cutoff == undefined) lo_cutoff = 0;
  if (hi_cutoff == undefined) hi_cutoff = lo_cutoff;

  // change this to true to see what
  // the histogram numbers really are
  if (Levels.dumpHistogram) {
    // debug stuff...
    var str = '';
    for (var i = 0; i < HIST_MAX; i++) {
      str += hist[i] + ",";
    }
    confirm(lo_cutoff + '\r' + hi_cutoff + '\r' + str);
  }

  // find the minimum level
  var min = 0;
  while (min < HIST_MAX && hist[min] <= lo_cutoff) {
    min++;
  }
  // find the maximum level
  var max = HIST_MAX-1;
  while (max >= 0 && hist[max] <= hi_cutoff) {
    max--;
  }
  var r = {};
  r.min = min;
  r.max = max;
  if (false) {
    alert(r.min + ':' + r.max);
  }

  // we need at least 2 distinct levels
  if (r.min != HIST_MAX && r.max != -1 && r.max != r.min) {
    // we also need to be sure that we're not already auto-leveled
    if (!(r.min == 0 && r.max == (HIST_MAX-1))) {
      return r;
    }
  }

  return null;
};

Histogram.mean = function(hist) {
  var acc  = 0;
  var cnt = 0;
  for (var i = 0; i < hist.length; i++) {
    acc += i*hist[i];
    cnt += hist[i];
  }
  return acc/cnt;
};
Histogram.median = function(hist) {
  var cnt = 0;
  for (var i = 0; i < hist.length; i++) {
    cnt += hist[i];
  }
  cnt = cnt/2;
  var acc = 0;
  for (var i = 0; i < hist.length; i++) {
    acc += hist[i];
    if (acc > cnt) return i-1;
  }
  return -1;
};

//
// These two aggregate functions produce interesting results,
// but I don't know how meaningful they actually are.
//
Histogram.aggregateMean = function(doc) {
  var chCnt = doc.channels.length;
  var acc = 0;
  for (var i = 0; i < chCnt; i++) {
    var ch = doc.channels[i];
    acc += Histogram.mean(ch.histogram);
  }
  return acc/chCnt;
};

Histogram.aggregateMedian = function(doc) {
  var chCnt = doc.channels.length;
  var acc = 0;
  for (var i = 0; i < chCnt; i++) {
    var ch = doc.channels[i];
    acc += Histogram.median(ch.histogram);
  }
  return acc/chCnt;
};

/*

// Sample usage
// Lets do some work, here..
var app;
if (!app) app = this;  // for PS7

var doc;
try { doc = app.activeDocument; } catch (e) {}
if (doc) {
   var adjusted = Levels.autoAdjustRGBChannels(doc, 3);
   // or 
   // Levels.autoAdjust(doc, 3);
   if (adjusted) {
     alert("Levels adjusted");
   } else {
     alert("Levels not adjusted");
   }
} else {
  alert("No document available");
}

*/

// $.level = 1; debugger;
// var doc = app.activeDocument;
// Histogram.aggregateMedian(doc);
// Histogram.aggregateMean(doc);

"Levels.js";
// EOF
