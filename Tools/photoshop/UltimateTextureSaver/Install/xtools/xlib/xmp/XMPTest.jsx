app;
//
// XMPTest.jsx
//
// $Id: XMPTest.jsx,v 1.4 2010/03/29 02:23:24 anonymous Exp $
// Copyright: (c)2007, xbytor
// License: http://www.opensource.org/licenses/bsd-license.php
// Contact: xbytor@gmail.com
//
//@show include
//
//@includepath "/c/Program Files/Adobe/xtools;/Developer/xtools"
//
//@include "xlib/stdlib.js"
//
//xdom = new XML(Stdlib.readFromFile("/c/work/scratch/test.xmp"));
//xdom.xpath("/rdf:RDF/rdf:Description/photoshop:Source")

//-include "xlib/XMPScript.jsx"

if (BridgeTalk.appName == "bridge") {
  if (ExternalObject.AdobeXMPScript == undefined) {
    ExternalObject.AdobeXMPScript = new ExternalObject('lib:AdobeXMPScript');
  }
} else {
  if (ExternalObject.AdobeXMPScript == undefined) {
    ExternalObject.AdobeXMPScript = new ExternalObject('lib:AdobeXMPScript');
  }
//   $.evalFile("/c/work/xtools/xlib/XMPScript.jsx");
}


function main() {
//   alert(XMPMeta.dumpNamespaces());
//   alert(XMPMeta.dumpAliases());
//   alert(XMPMeta.resolveAlias(XMPMeta.getNamespaceURI("photoshop"), "Caption"));

  var f = new File("/c/work/scratch/20060804_BC_0250.TIF");
  var xmpf = new XMPFile(f.fsName, XMPConst.FILE_TIFF, XMPConst.OPEN_FOR_READ);
  var xmp = xmpf.getXMP();
  //   var str = xmp.serialize();
  //   alert(str);

  var prop = xmp.getProperty(XMPConst.NS_PHOTOSHOP,
                             "ColorMode");
//   alert(prop);

//   prop = xmp.getArrayItem(XMPConst.NS_TIFF,
//                           "PrimaryChromaticities/rdf:Seq/rdf:li", 3);
//   alert(prop);

  prop = xmp.getStructField(XMPConst.NS_EXIF, "Flash",
                            XMPConst.NS_EXIF, "RedEyeMode");

  xmp.doesPropertyExist(XMPConst.NS_PDF, "Author");

  //  alert(prop);

  xmp.serialize(XMPConst.SERIALIZE_OMIT_PACKET_WRAPPER);
  debugger;
};

main();

"XMPTest.jsx";
// EOF
