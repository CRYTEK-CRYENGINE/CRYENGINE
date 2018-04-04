//
// TextTest.jsx
//
//============================================================================
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/PSConstants.js"
//@include "xlib/stdlib.js"
//@include "xlib/Text.jsx"
//@include "xlib/Action.js"
//@include "xlib/xml/atn2xml.jsx"
//

//============================================================================
// Demo stuff
//============================================================================


function newTextLayerDemo(doc, title, doAlert) {
  var layerName = "Test Layer";

  if (doAlert != false) {
    alert("This first demo illustrates how to add a new TextLayer to a\r" +
          "document using the TextOptions object. Nothing fancy.\r" +
          "If a document doesn't exist, one will be created.");
  }

  if (!doc) {
    doc = app.documents.add(640, 480, 72, title);
  }

  // specify the contents and use the TextOption defaults

  var opts = new TextOptions("test content");
  opts.layerName = layerName;
  opts.width = 400;
  opts.height = 400;
  Text.addNewTextLayer(doc, opts);
};

function tsrDemo(doc) {
  var layerName = "Test Layer";

  alert("This demo is sigificantly more complicated. It replaces the\r" +
        "existing text from the previous demo, creates a 3 text styles\r" +
        "with different fonts, sizes, and colors and then applies them\r" +
        "over 4 different ranges of text\r");

  
  doc = doc.duplicate();
  var layer = doc.layers[layerName];
  doc.activeLayer = layer;
  var opts = new TextOptions(layer.textItem);
  opts.contents = "123567875688\radsfasfasdfasdfasdf\n(&#^&^#$";

  // construct the styles
  var s1 = new TextStyle("ArialMT");
  var s2 = new TextStyle("Courier-Bold", 30, Text.toRGBColor(0, 255, 0));

  // this style will use the default font...
  var s3 = new TextStyle(undefined, 42, Text.toRGBColor(255, 0, 0));

  // superscript test
  var s0 = new TextStyle(s1);
  s0.baseline = PSString.superScript;

  // Now create the set of ranges to apply the styles over
  var ranges = new TextStyleRanges();
  ranges.add(new TextStyleRange(0, 1, s0));
  ranges.add(new TextStyleRange(1, opts.contents.length, s1));
  ranges.add(new TextStyleRange(5, 10, s2));
  ranges.add(new TextStyleRange(7, 15, s3));
  ranges.add(new TextStyleRange(25, undefined, s2));
 
  opts.ranges = ranges;
  Text.modifyTextLayer(doc, opts, doc.layers[layerName]);
};


Text.test= function() {
  var doc;
  if (app.documents.length) {
    doc = app.activeDocument
  }
  newTextLayerDemo(doc, "newTextLayer demo");
  
  tsrDemo(app.activeDocument);
};

Text.test();

"TextTest.jsx";

// EOF
