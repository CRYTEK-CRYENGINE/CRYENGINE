// =========================================================================
//
// xmlxpath.js - a partially W3C compliant XPath parser for XML for <SCRIPT>
//
// version 3.1
//
// =========================================================================
//
// Copyright (C) 2003 Jon van Noort <jon@webarcana.com.au> and David Joham <djoham@yahoo.com>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// visit the XML for <SCRIPT> home page at xmljs.sourceforge.net
//
// Contributor - Jon van Noort <jon@webarcana.com.au>
//
// Contains code derived from works by Mark Bosley & Fred Baptiste
//  (see: http://www.topxml.com/people/bosley/strlib.asp)
//
// Contains text (used within comments to methods) from the
//  XML Path Language (XPath) Version 1.0 W3C Recommendation
//  Copyright © 16 November 1999 World Wide Web Consortium,
//  (Massachusetts Institute of Technology,
//  European Research Consortium for Informatics and Mathematics, Keio University).
//  All Rights Reserved.
//  (see: http://www.w3.org/TR/xpath)
//


// =========================================================================
// CONSTANT DECLARATIONS:
// =========================================================================
DOMNode.NODE_TYPE_TEST = 1;
DOMNode.NODE_NAME_TEST = 2;

DOMNode.ANCESTOR_AXIS            = 1;
DOMNode.ANCESTOR_OR_SELF_AXIS    = 2;
DOMNode.ATTRIBUTE_AXIS           = 3;
DOMNode.CHILD_AXIS               = 4;
DOMNode.DESCENDANT_AXIS          = 5;
DOMNode.DESCENDANT_OR_SELF_AXIS  = 6;
DOMNode.FOLLOWING_AXIS           = 7;
DOMNode.FOLLOWING_SIBLING_AXIS   = 8;
DOMNode.NAMESPACE_AXIS           = 9;
DOMNode.PARENT_AXIS              = 10;
DOMNode.PRECEDING_AXIS           = 11;
DOMNode.PRECEDING_SIBLING_AXIS   = 12;
DOMNode.SELF_AXIS                = 13;

DOMNode.ROOT_AXIS                = 14;  // the self axis of the root node

// =========================================================================
//  XPATHNode Helpers 
// =========================================================================

// =========================================================================
// _@method
// DOMNode.getStringValue - Return the concatenation of all of the DescandantOrSelf Text nodes
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : string 
// 
// ========================================================================= 
DOMNode.prototype.getStringValue = function() {
  var thisBranch = this._getDescendantOrSelfAxis();
  var textNodes = thisBranch.getTypedItems(DOMNode.TEXT_NODE);
  var stringValue = "";

  if (textNodes.length > 0) {
    stringValue += textNodes.item(0).nodeValue;

    for (var i=1; i < textNodes.length ; i++) {
      stringValue += " "+ textNodes.item(i).nodeValue;  // separate TextNodes with spaces
    }
  }

  return stringValue;
};

DOMNode.prototype._filterByAttributeExistance = function(expr, containerNodeSet) {

    try {
        //handle [@id] condition
        if (expr.indexOf("*") < 0) {
            var attributeName = expr.substr(1, expr.length);
            var attribute = this.getAttributes().getNamedItem(attributeName);

            if (attribute != null) {
                return true;
            }
            else {
                return false;
            }
        }

        //handle [@*] situation
        else {
            if (this.getAttributes().getLength() > 0 ) {
                return true;
            }
            else {
                return false;
            }
        }
    }
    catch (e) {
        return false;
    }
}

DOMNode.prototype._filterByAttributeValue = function(expr, containerNodeSet) {

    //handle situation such as [@id=1], [@id='1'] and/or [@id="1"]
    try {
        //get some locations we'll need later
        var nameStart = expr.indexOf("@") + 1;
        var equalsStart = expr.indexOf("=");

        //get the attribute name we're looking for
        var attributeName = expr.substr(nameStart, expr.indexOf("=", nameStart) - 1);

        //get the value we're looking for
        var valueTesting = expr.substr(equalsStart + 1, expr.length);

        //handle situations like [@id="1"] and [@id='1']
        if (valueTesting.charAt(0) == "\"" || valueTesting.charAt(0) == "'") {
            valueTesting = valueTesting.substr(1, valueTesting.length);
        }

        if (valueTesting.charAt(valueTesting.length - 1) == "\"" || valueTesting.charAt(valueTesting.length - 1) == "'") {
            valueTesting = valueTesting.substr(0, valueTesting.length -1);
        }

        //now I have the attribute name. Try to get it
        var attribute = this.getAttributes().getNamedItem(attributeName);

        //test for equality
        if (attribute.getValue() == valueTesting) {
            return true;
        }
        else {
            return false;
        }
    }
    catch (e) {
        return false
    }

}

DOMNode.prototype._filterByAttribute = function(expr, containerNodeSet) {

    //handle searches for the existance of any attributes
    if (expr == "@*") {
        return this._filterByAttributeExistance(expr, containerNodeSet);
    }

    //test to see if they just want elements with a specific attribute [@id]
    if (expr.indexOf("=") < 0) {
        return this._filterByAttributeExistance(expr, containerNodeSet);
    }

    //if we got here, we're looking for an attribute value
    return this._filterByAttributeValue(expr, containerNodeSet);

}

DOMNode.prototype._filterByNot = function(expr, containerNodeSet) {

    //handle something like [not(<expression>)]
    //first, remove the "not part
    expr = expr.substr(4, expr.length);

    //find the end of the not
    var endNotLocation = this._findExpressionEnd(expr, "(", ")");

    //get the full expression
    expr = expr.substr(0, endNotLocation);

    //now return the not of the filter value
    return !this._filter(expr, containerNodeSet);
}

DOMNode.prototype._findExpressionEnd = function(expression, startCharacter, endCharacter) {

    //make sure to handle the case where we have nested expressions
    var startCharacterNum = 0;
    var endExpressionLocation = 0;
    var intCount = expression.length;


    for (intLoop = 0; intLoop < intCount; intLoop++) {
        var character = expression.charAt(intLoop);
        switch(character) {
            case startCharacter:
                startCharacterNum++;
                break;

            case endCharacter:
                if (startCharacterNum == 0) {
                    endExpressionLocation = intLoop;
                }
                else {
                    startCharacterNum--;
                }
                break;
        }

        if (endExpressionLocation != 0) {
            break;
        }
    }

    return endExpressionLocation;

}

DOMNode.prototype._filterByLocation = function(expr, containerNodeSet) {

    //handle situation like [1]
    var item = 0 + expr - 1; //xpath is 1 based, w3cdom is 0 based
    //compare ourselves to the list
    if (this == containerNodeSet.item(item)) {
        return true;
    }
    else {
        return false;
    }

}

DOMNode.prototype._filterByLast = function(expr, containerNodeSet) {

    //handle situation likd [last()]
    if (this == containerNodeSet.item(containerNodeSet.length -1)) {
        return true;
    }
    else {
        return false;
    }
}

DOMNode.prototype._filterByCount = function(expr, containerNodeSet) {

    //get the count we're looking for
    var countStart = expr.indexOf("=") + 1;
    var countStr = expr.substr(countStart, expr.length);

    var countInt = parseInt(countStr);

    //separate out the "count(" part
    expr = expr.substr(6, expr.length);

    //get the expression
    expr = expr.substr(0, this._findExpressionEnd(expr, "(", ")"));
    //if expr is "*", the logic here doesn't work. Just return false
    //the reason it doesn't work is that while we may find a node under us
    //that has the proper number of children, we can only return true
    //or false for *this* node. So, if we find something that matches,
    //we'll be returning false for the wrong node.
    //TODO: fix this somehow
    if (expr == "*") {
        return false;
    }

    //otherwise, give it a go...
    var tmpNodeSet = this.selectNodeSet(expr);
    var tmpNodeSetLength = tmpNodeSet.length;

    if (tmpNodeSetLength == countInt) {
        return true;
    }
    else {
        return false;
    }

}

DOMNode.prototype._filterByName = function(expr, containerNodeSet) {

    //get the name we're looking for
    //start at the first equal sign
    var equalLocation = expr.indexOf("=");

    //get the next character. This should be either a single or double quote
    var quoteChar = expr.charAt(equalLocation + 1);

    //our name will be the text after the quoteChar until the next quoteChar is found
    var name = "";
    for (intLoop = equalLocation + 2; intLoop < expr.length; intLoop++) {
        if (expr.charAt(intLoop) == quoteChar) {
            break;
        }
        else {
            name += expr.charAt(intLoop);
        }
    }

    if (this.getNodeName() == name) {
        return true;
    }
    else {
        return false;
    }

}

DOMNode.prototype._filterByPosition = function(expr, containerNodeSet) {

    //find the location I'm looking for
    var equalsLocation = expr.indexOf("=");
    var tmpPos = expr.substr(equalsLocation + 1, expr.length);

    //I need to check for position()=last()
    if (tmpPos.indexOf("last()") == 0) {
        //see if we're the last dude in the list
        if (this == containerNodeSet.item(containerNodeSet.length -1)) {
            return true;
        }
        else {
            return false;
        }
    }

    //OK, it wasn't last so that means there's a numeric number here. Find it
    var intCount = tmpPos.length;
    var positionStr = "";
    for (intLoop = 0; intLoop < intCount; intLoop++) {
        if (isNaN(positionStr + tmpPos.charAt(intLoop)) == false) {
            positionStr+=tmpPos.charAt(intLoop);
        }
        else {
            break;
        }
    }

    //OK, we have the position, get the int value and do the comparison
    var positionInt = parseInt(positionStr) - 1; //minus 1 because xpath is 1 based but w3cdom is 0 based
    if (this == containerNodeSet.item(positionInt) ) {
        return true;
    }
    else {
        return false;
    }

}

// =========================================================================
// _@method
// DOMNode._filter - Returns true if the expression evaluates to true
//                      and otherwise returns false
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param
// expr : string - The XPath Predicate Expression
// containerNodeSet : XPATHNodeSet - the NodeSet containing this node
//
// _@return
// : boolean
//
// =========================================================================
DOMNode.prototype._filter = function(expr, containerNodeSet) {

  expr = trim(expr, true, true);

  //handle not()
  if (expr.indexOf("not(") == 0) {
    return this._filterByNot(expr, containerNodeSet);
  }

  //handle count()
  if (expr.indexOf("count(") == 0) {
    return this._filterByCount(expr, containerNodeSet);
  }

  //handle name()
  if (expr.indexOf("name(") == 0) {
    return this._filterByName(expr, containerNodeSet);
  }

  //handle [position() = 1]
  if (expr.indexOf("position(") == 0) {
    return this._filterByPosition(expr, containerNodeSet);
  }


  //handle attribute searches
  if (expr.indexOf("@") > -1 ) {
    return this._filterByAttribute(expr, containerNodeSet);
  }


  //handle the case of something like [1]
  if (isNaN(expr) == false) {
    return this._filterByLocation(expr, containerNodeSet);
  }

  //handle [last()]
  if (expr == "last()") {
    return this._filterByLast(expr, containerNodeSet);
  }


}


// =========================================================================
// _@method
// DOMNode._getAxis - Return a NodeSet containing all Nodes in the specified Axis
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param
// axisConst : int - the id of the Axis Type
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getAxis = function(axisConst) {
  if (axisConst == DOMNode.ANCESTOR_AXIS)                return this._getAncestorAxis();
  else if (axisConst == DOMNode.ANCESTOR_OR_SELF_AXIS)   return this._getAncestorOrSelfAxis();
  else if (axisConst == DOMNode.ATTRIBUTE_AXIS)          return this._getAttributeAxis();
  else if (axisConst == DOMNode.CHILD_AXIS)              return this._getChildAxis();
  else if (axisConst == DOMNode.DESCENDANT_AXIS)         return this._getDescendantAxis();
  else if (axisConst == DOMNode.DESCENDANT_OR_SELF_AXIS) return this._getDescendantOrSelfAxis();
  else if (axisConst == DOMNode.FOLLOWING_AXIS)          return this._getFollowingAxis();
  else if (axisConst == DOMNode.FOLLOWING_SIBLING_AXIS)  return this._getFollowingSiblingAxis();
  else if (axisConst == DOMNode.NAMESPACE_AXIS)          return this._getNamespaceAxis();
  else if (axisConst == DOMNode.PARENT_AXIS)             return this._getParentAxis();
  else if (axisConst == DOMNode.PRECEDING_AXIS)          return this._getPrecedingAxis();
  else if (axisConst == DOMNode.PRECEDING_SIBLING_AXIS)  return this._getPrecedingSiblingAxis();
  else if (axisConst == DOMNode.SELF_AXIS)               return this._getSelfAxis();
  else if (axisConst == DOMNode.ROOT_AXIS)               return this._getRootAxis();


  else {
    alert('Error in DOMNode._getAxis: Attempted to get unknown axis type '+ axisConst);
    return null;
  }
};


// =========================================================================
// The ancestor, descendant, following, preceding and self axes partition a document
// (ignoring attribute and namespace nodes): they do not overlap and together they contain
// all the nodes in the document.
// =========================================================================

// =========================================================================
// _@method
// DOMNode._getAncestorAxis - Return a NodeSet containing all Nodes in ancestor axis.
//   The ancestor axis contains the ancestors of the context node; the ancestors of the
//   context node consist of the parent of context node and the parent's parent and so on;
//   thus, the ancestor axis will always include the root node, unless the context node is
//   the root node.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getAncestorAxis = function() {
  var parentNode = this.parentNode;

  if (parentNode.nodeType != DOMNode.DOCUMENT_NODE ) {
    return this.parentNode._getAncestorOrSelfAxis();
  }
  else {
    return new XPATHNodeSet(this.ownerDocument, this.parentNode, null);  // return empty NodeSet
  }
};


// =========================================================================
// _@method
// DOMNode._getAncestorOrSelfAxis - Return a NodeSet containing all Nodes in ancestor or self axes.
//   The ancestor-or-self axis contains the context node and the ancestors of the
//   context node; thus, the ancestor axis will always include the root node.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getAncestorOrSelfAxis = function() {
  return this._getSelfAxis().union(this._getAncestorAxis());
};


// =========================================================================
// _@method
// DOMNode._getAttributeAxis - Return a NodeSet containing all Nodes in the attribute axis.
//   The attribute axis contains the attributes of the context node; the axis will
//   be empty unless the context node is an element.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getAttributeAxis = function() {
  return new XPATHNodeSet(this.ownerDocument, this.parentNode, this.attributes);
};


// =========================================================================
// _@method
// DOMNode._getChildAxis - Return a NodeSet containing all Nodes in the child axis.
//   The child axis contains the children of the context node.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getChildAxis = function() {
  return new XPATHNodeSet(this.ownerDocument, this.parentNode, this.childNodes);
};


// =========================================================================
// _@method
// DOMNode._getDescendantAxis - Return a NodeSet containing all Nodes in the descendant axis.
//   The descendant axis contains the descendants of the context node; a descendant is
//   a child or a child of a child and so on; thus the descendant axis never contains
//   attribute or namespace nodes.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getDescendantAxis = function() {
  var descendantNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode, null);

  for (var i=0; i < this.childNodes.length; i++) {
    descendantNodeSet.union(this.childNodes.item(i)._getDescendantOrSelfAxis());
  }

  return descendantNodeSet;
};

// =========================================================================
// _@method
// DOMNode._getReversedDescendantAxis - Return a NodeSet containing all Nodes in the descendant axis,
//   in Reverse Document Order .
//   The descendant axis contains the descendants of the context node; a descendant is
//   a child or a child of a child and so on; thus the descendant axis never contains
//   attribute or namespace nodes.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getReversedDescendantAxis = function() {
  var descendantNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode, null);

  for (var i=this.childNodes.length -1; i >= 0; i--) {
    descendantNodeSet.union(this.childNodes.item(i)._getReversedDescendantOrSelfAxis());
  }

  return descendantNodeSet;
};


// =========================================================================
// _@method
// DOMNode._getDescendantOrSelfAxis - Return a NodeSet containing all Nodes in the descendant or self axes.
//   The descendant-or-self axis contains the context node and the descendants of the context node.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getDescendantOrSelfAxis = function() {
  return this._getSelfAxis().union(this._getDescendantAxis());
};


// =========================================================================
// _@method
// DOMNode._getReversedDescendantOrSelfAxis - Return a NodeSet containing all Nodes in the descendant or self axes,
//   in Reverse Document Order.
//   The descendant-or-self axis contains the context node and the descendants of the context node.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getReversedDescendantOrSelfAxis = function() {
  return this._getSelfAxis().union(this._getReversedDescendantAxis());
};


// =========================================================================
// _@method
// DOMNode._getFollowingAxis - Return a NodeSet containing all Nodes in the following axes.
//   The following axis contains all nodes in the same document as the context node that
//   are after the context node in document order, excluding any descendants and excluding
//   attribute nodes and namespace nodes.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getFollowingAxis = function() {
  var followingNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode, null);

  if (this.nextSibling) {
    followingNodeSet._appendChild(this.nextSibling);
    followingNodeSet.union(this.nextSibling._getDescendantAxis());
    followingNodeSet.union(this.nextSibling._getFollowingAxis());
  }

  return followingNodeSet;
};


// =========================================================================
// _@method
// DOMNode._getFollowingSiblingAxis - Return a NodeSet containing all sibling Nodes in the following axes.
//   The following-sibling axis contains all the following siblings of the context node;
//   if the context node is an attribute node or namespace node, the following-sibling axis is empty.
//   ie, only the nodes that are sibling to the context node, not their descandants
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getFollowingSiblingAxis = function() {
  var followingSiblingNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode, null);

  if (this.nextSibling) {
    followingSiblingNodeSet._appendChild(this.nextSibling);
    followingSiblingNodeSet.union(this.nextSibling._getFollowingSiblingAxis());
  }

  return followingSiblingNodeSet;
};


// =========================================================================
// _@method
// DOMNode._getNamespaceAxis - Return a NodeSet containing nodes in the Namespace axis.
//   the namespace axis contains the namespace nodes of the context node; the axis will
//   be empty unless the context node is an element.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
//DOMNode.prototype._getNamespaceAxis = function() {}

// XXX This looks like it might work XXX
DOMNode.prototype._getNamespaceAxis = function() {
   var namespaceNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode, null);

   for (var i = 0; i < this._namespaces.length; i++) {
     namespaceNodeSet._appendChild(this._namespaces._nodes[i]);
   }

   return namespaceNodeSet;
};


// =========================================================================
// _@method
// DOMNode._getParentAxis - Return a NodeSet containing the parent node of the context node.
//   The parent axis contains the parent of the context node, if there is one.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getParentAxis = function() {
  var parentNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode, null);
  var parentNode = this.parentNode;

  if (parentNode) {
    parentNodeSet._appendChild(this);
  }

  return parentNodeSet;
};


// =========================================================================
// _@method
// DOMNode._getPrecedingAxis - Return a NodeSet containing all Nodes in the preceding axis.
//   The preceding axis contains all nodes in the same document as the context node that
//   are before the context node in document order, excluding any ancestors and excluding
//   attribute nodes and namespace nodes.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getPrecedingAxis = function() {
  var precedingNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode, null);

  if (this.previousSibling) {
    precedingNodeSet.union(this.previousSibling._getReversedDescendantAxis());
    precedingNodeSet._appendChild(this.previousSibling);
    precedingNodeSet.union(this.previousSibling._getPrecedingAxis());
  }

  return precedingNodeSet;
};


// =========================================================================
// _@method
// DOMNode._getPrecedingSiblingAxis - Return a NodeSet containing all sibling Nodes in the preceding axis.
//   The preceding axis contains all nodes in the same document as the context node that
//   are before the context node in document order, excluding any ancestors and excluding
//   attribute nodes and namespace nodes.
//   ie, only the nodes that are sibling to the context node, not their descandants
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getPrecedingSiblingAxis = function() {
  var precedingSiblingNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode, null);

  if (this.previousSibling) {
    precedingSiblingNodeSet._appendChild(this.previousSibling);
    precedingSiblingNodeSet.union(this.previousSibling._getPrecedingSiblingAxis());
  }

  return precedingSiblingNodeSet;
};


// =========================================================================
// _@method
// DOMNode._getSelfAxis - Return a NodeSet containing the context node.
//   The self axis contains just the context node itself.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getSelfAxis = function() {
  var selfNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode, null);
  selfNodeSet._appendChild(this);

  return selfNodeSet;
};


// =========================================================================
// _@method
// DOMNode._getRootAxis - Return a NodeSet containing the root node.
//   The root axis contains just the root node itself.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype._getRootAxis = function() {
  var rootNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode, null);
  rootNodeSet._appendChild(this.documentElement);

  return rootNodeSet;
};


// =========================================================================
// _@method
// DOMNode.selectNodeSet - Returns NodeSet containing nodes matching XPath expression
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param
// locationPath : string - the XPath expression
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
DOMNode.prototype.selectNodeSet = function (locationPath) {
  // transform '//' root-recursive reference before split
  locationPath = locationPath.replace(/^\//g, 'root::');
  var result;
  try {
    result = this.selectNodeSet_recursive(locationPath);
    return result;
  }
  catch (e) {
    return null;
  }

}

// =========================================================================
// _@method
// DOMNode.selectNodeSet_recursive - Returns NodeSet containing nodes matching XPath expression
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param
// locationPath : string - the XPath expression
// 
// _@return 
// : XPATHNodeSet
// 
// ========================================================================= 
DOMNode.prototype.selectNodeSet_recursive = function (locationPath) {

  // split LocationPath into Steps on '/'
  var locationSteps = locationPath.split('/');
  var candidateNodeSet;
  var resultNodeSet = new XPATHNodeSet(this.ownerDocument, this.parentNode);

  // get first step

  if (locationSteps.length > 0) {

    /* original code - didn't work in IE 5.x for the mac
    var stepStr = locationSteps.shift();
    */

    /* new code. Slower, but more compatible. */
    var stepStr = locationSteps[0];
    locationSteps = __removeFirstArrayElement(locationSteps);
    /*end new code*/

    var stepObj = this._parseStep(stepStr);

    // parse Step (Axis, NodeTest, PredicateList)
    var axisStr          = stepObj.axis;
    var nodeTestStr      = stepObj.nodeTest;
    var predicateListStr = stepObj.predicateList;

    // get AxisType
    var axisType = this._parseAxis(axisStr);

    // parse NodeTest (type, value)
    var nodeTestObj = this._parseNodeTest(nodeTestStr);
    var nodeTestType  = nodeTestObj.type;
    var nodeTestValue = nodeTestObj.value;

    // Build NodeSet with Nodes from AxisSpecifier
    candidateNodeSet = this._getAxis(axisType);

    // Filter NodeSet by NodeTest
    if (nodeTestType == DOMNode.NODE_TYPE_TEST) {
      candidateNodeSet = candidateNodeSet.getTypedItems(nodeTestValue);
    }  
    else if (nodeTestType == DOMNode.NODE_NAME_TEST) {
      candidateNodeSet = candidateNodeSet.getNamedItems(nodeTestValue);
    }

    // parse Predicates
    var predicateList = this._parsePredicates(predicateListStr);
        
    // apply each predicate in turn
    for (predicate in predicateList) {
      // filter NodeSet by applying Predicate
      candidateNodeSet = candidateNodeSet.filter(predicateList[predicate]);
    }
    
    // recursively apply remaining steps in Location Path
    if (locationSteps.length > 0) {
      var remainingLocationPath = locationSteps.join('/');
      candidateNodeSet = candidateNodeSet.selectNodeSet_recursive(remainingLocationPath);
    }
  }  
  
  return candidateNodeSet;
};


// =========================================================================
// _@method
// DOMNode._nodeTypeIs - Returns true if the node is of specified type 
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// node : DOMNode - the Node to be tested
// type : int - the id of the required type
// 
// _@return 
// : boolean 
// 
// ========================================================================= 
DOMNode.prototype._nodeTypeIs = function(node, type) {
  return (node.nodeType == type);
};
  

// =========================================================================
// _@method
// DOMNode._nodeNameIs - Returns true if the node has the specified name 
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// node : DOMNode - the Node to be tested
// name : string - the name of the required node(s)
// 
// _@return 
// : boolean 
// 
// ========================================================================= 
DOMNode.prototype._nodeNameIs = function(node, name) {
  return (node.nodeName == name);
};

// =========================================================================
// _@method
// DOMNode._parseStep - Parse the LocationStep
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// step : string - the LocationStep expression, ie, the string between containing '/'s
// 
// _@return 
// (
//   axis : string - the axis
//   nodeTest : string - the name or type of node
//   predicateList : string - zero or more Predicate Expressions, ie, the expressions contained within '[..]'
// )
// 
// ========================================================================= 
DOMNode.prototype._parseStep = function(step) {
  var resultStep = new Object();
  resultStep.axis = "";
  var nodeTestStartInd = 0;      // start of NodeTest string defaults to starting at begining of string
  
  // test existance of AxisSpecifier
  var axisEndInd = step.indexOf('::');
  if (axisEndInd > -1) {
    resultStep.axis = step.substring(0, axisEndInd);                              // extract axis
    nodeTestStartInd = axisEndInd +2;
  }
  
  // test existance of Predicates
  resultStep.predicateList = "";
  var predicateStartInd = step.indexOf('[');
  if (predicateStartInd > -1) {
    resultStep.predicateList = step.substring(predicateStartInd);             // extract predicateList
    resultStep.nodeTest = step.substring(nodeTestStartInd, predicateStartInd);    // extract nodeTest
  }
  else {
    resultStep.nodeTest = step.substring(nodeTestStartInd);                       // extract nodeTest
  }

  if (resultStep.nodeTest.indexOf('@') == 0) {                                    // '@' is shorthand for
    resultStep.axis = 'attribute';                                                // axis is atttribute
    resultStep.nodeTest = resultStep.nodeTest.substring(1);                       // remove '@' from string
  }

  if (resultStep.nodeTest.length == 0) {                                          // '//' is shorthand for
    resultStep.axis = 'descendant-or-self';                                       // axis is descendant-or-self
  }

  if (resultStep.nodeTest == '..') {                                              // '..' is shorthand for
    resultStep.axis = 'parent';                                                   // axis is parent
    resultStep.nodeTest = 'node()';
  }

  if (resultStep.nodeTest == '.') {                                               // '.' is shorthand for
    resultStep.axis = 'self';                                                     // axis is self
    resultStep.nodeTest = 'node()';
  }

  return resultStep;
};

// =========================================================================
// _@method
// DOMNode._parseAxis - Parse the Axis, return axis type id
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// axisStr : string - the axis string
// 
// _@return 
// : int
// ========================================================================= 
DOMNode.prototype._parseAxis = function(axisStr) {

  var returnAxisType = DOMNode.CHILD_AXIS;  // axis defaults to "child"
  // convert axis & alias strings to axisType consts
  if (axisStr == 'ancestor')                returnAxisType = DOMNode.ANCESTOR_AXIS;
  else if (axisStr == 'ancestor-or-self')   returnAxisType = DOMNode.ANCESTOR_OR_SELF_AXIS;
  else if (axisStr == 'attribute')          returnAxisType = DOMNode.ATTRIBUTE_AXIS;
  else if (axisStr == 'child')              returnAxisType = DOMNode.CHILD_AXIS;
  else if (axisStr == 'descendant')         returnAxisType = DOMNode.DESCENDANT_AXIS;
  else if (axisStr == 'descendant-or-self') returnAxisType = DOMNode.DESCENDANT_OR_SELF_AXIS;
  else if (axisStr == 'following')          returnAxisType = DOMNode.FOLLOWING_AXIS;
  else if (axisStr == 'following-sibling')  returnAxisType = DOMNode.FOLLOWING_SIBLING_AXIS;
  else if (axisStr == 'namespace')          returnAxisType = DOMNode.NAMESPACE_AXIS;
  else if (axisStr == 'parent')             returnAxisType = DOMNode.PARENT_AXIS;
  else if (axisStr == 'preceding')          returnAxisType = DOMNode.PRECEDING_AXIS;
  else if (axisStr == 'preceding-sibling')  returnAxisType = DOMNode.PRECEDING_SIBLING_AXIS;
  else if (axisStr == 'self')               returnAxisType = DOMNode.SELF_AXIS
  else if (axisStr == 'root')               returnAxisType = DOMNode.ROOT_AXIS


  return returnAxisType;
}


// =========================================================================
// _@method
// DOMNode._parseNodeTest - Parse the NodeTest, return axis type id
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// nodeTestStr : string - the node test string
// 
// _@return
// (
//   type : int - the nodeTest type id
//   value : string - the string of the nodeName or nodeType (depending on type)
// )
// ========================================================================= 
DOMNode.prototype._parseNodeTest = function(nodeTestStr) {
  var returnNodeTestObj = new Object();
  
  if (nodeTestStr.length == 0) {
    returnNodeTestObj.type = DOMNode.NODE_TYPE_TEST
    returnNodeTestObj.value = 'node';
  }
  else {
    var funInd = nodeTestStr.indexOf('(');
    if (funInd > -1) {
      returnNodeTestObj.type = DOMNode.NODE_TYPE_TEST
      returnNodeTestObj.value = nodeTestStr.substring(0, funInd);
    }
    else {
      returnNodeTestObj.type = DOMNode.NODE_NAME_TEST
      returnNodeTestObj.value = nodeTestStr;
    }
  }
    
  return returnNodeTestObj;
};


// =========================================================================
// _@method
// DOMNode._parsePredicates - Parse the PredicateList, return array of Predicate Expression strings
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// predicateListStr : string - the string containing list of (stacked) predicates
// 
// _@return 
// [
//   : string - the string of a Predicate Expression (sans enclosing '[]'s)
// ]
// ========================================================================= 
DOMNode.prototype._parsePredicates = function(predicateListStr) {
  var returnPredicateArray = new Array();
  
  if (predicateListStr.length > 0) {
    // remove top & tail square brackets to simplify the following split
    var firstOpenBracket = predicateListStr.indexOf('[');
    var lastCloseBracket = predicateListStr.lastIndexOf(']');

    predicateListStr = predicateListStr.substring(firstOpenBracket+1, lastCloseBracket);
    
    // split predicate list on ']['
    returnPredicateArray = predicateListStr.split('][');
  }
  
  return returnPredicateArray;
}


// =========================================================================
// _@class
// XPATHNodeSet - Container of Nodes selected by XPath (sub)expression
//
// _@extends
// DOMNodeList
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// doc : string - the dom document that actually contains the Node data
// arr : [int]  - initial set of DOMNode _id(s)
// ========================================================================= 
XPATHNodeSet = function(ownerDocument, parentNode, nodeList) {
  this.DOMNodeList = DOMNodeList;
  this.DOMNodeList(ownerDocument, parentNode);
   if (nodeList) {
     for (var i=0; i < nodeList.length; i++) {
	    this._appendChild(nodeList.item(i));
	 }
   }

};
XPATHNodeSet.prototype = new DOMNodeList();


// =========================================================================
// _@method
// XPATHNodeSet.selectNodeSet_recursive - Returns NodeSet containing nodes matching XPath expression
//   for all Nodes within NodeSet.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// xpath : string - the XPath expression
// 
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype.selectNodeSet_recursive = function(xpath) {
  var selectedNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    var candidateNode = this.item(i);
    selectedNodeSet.union(candidateNode.selectNodeSet_recursive(xpath));
  }
  
  return selectedNodeSet;
};

// =========================================================================
// _@method
// XPATHNodeSet.getNamedItems - Returns NodeSet containing all nodes with specified name
//   selected from the Nodes within the NodeSet.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// xpath : string - the XPath expression
// 
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype.getNamedItems = function(nodeName) {
  var namedItemsNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    var candidateNode = this.item(i);
    if ((nodeName == '*') || (candidateNode.nodeName == nodeName)) {
      namedItemsNodeSet._appendChild(candidateNode);
    }
  }
  
  return namedItemsNodeSet;
};

// =========================================================================
// _@method
// XPATHNodeSet.getTypedItems - Returns NodeSet containing all nodes with specified type
//   selected from the Nodes within the NodeSet.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// nodeType : int - the type id 
// 
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype.getTypedItems = function(nodeType) {
  var typedItemsNodeSet = new XPATHNodeSet(this.ownerDocument);
  var nodeTypeId;
  
       if (nodeType.toLowerCase() == "node")    { nodeTypeId = 0; }
  else if (nodeType.toLowerCase() == "text")    { nodeTypeId = DOMNode.TEXT_NODE; }
  else if (nodeType.toLowerCase() == "comment") { nodeTypeId = DOMNode.COMMENT_NODE; }
  else if (nodeType.toLowerCase() == "processing-instruction") { nodeTypeId = DOMNode.PROCESSING_INSTRUCTION_NODE; }
  
  for (var i=0; i < this.length; i++) {
    var candidateNode = this.item(i);
    if ((nodeTypeId == 0) || (candidateNode.nodeType == nodeTypeId)) {
      typedItemsNodeSet._appendChild(candidateNode);
    }
  }
  
  return typedItemsNodeSet;
};




// =========================================================================
// _@method
// XPATHNodeSet._getAxis - Returns a NodeSet containing the concatenation of all of the nodes 
//   in the specified Axis for each node in the current NodeSet.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param 
// axisConst : int - the Axis type id
// 
// _@return 
// : XPATHNodeSet
// 
// ========================================================================= 
XPATHNodeSet.prototype._getAxis = function(axisConst) { 
  if (axisConst == DOMNode.ANCESTOR_AXIS)                return this._getAncestorAxis();
  else if (axisConst == DOMNode.ANCESTOR_OR_SELF_AXIS)   return this._getAncestorOrSelfAxis();
  else if (axisConst == DOMNode.ATTRIBUTE_AXIS)          return this._getAttributeAxis();
  else if (axisConst == DOMNode.CHILD_AXIS)              return this._getChildAxis();
  else if (axisConst == DOMNode.DESCENDANT_AXIS)         return this._getDescendantAxis();
  else if (axisConst == DOMNode.DESCENDANT_OR_SELF_AXIS) return this._getDescendantOrSelfAxis();
  else if (axisConst == DOMNode.FOLLOWING_AXIS)          return this._getFollowingAxis();
  else if (axisConst == DOMNode.FOLLOWING_SIBLING_AXIS)  return this._getFollowingSiblingAxis();
  else if (axisConst == DOMNode.NAMESPACE_AXIS)          return this._getNamespaceAxis();
  else if (axisConst == DOMNode.PARENT_AXIS)             return this._getParentAxis();
  else if (axisConst == DOMNode.PRECEDING_AXIS)          return this._getPrecedingAxis();
  else if (axisConst == DOMNode.PRECEDING_SIBLING_AXIS)  return this._getPrecedingSiblingAxis();
  else if (axisConst == DOMNode.SELF_AXIS)               return this._getSelfAxis();
  else if (axisConst == DOMNode.ROOT_AXIS)               return this._getRootAxis();

  else {
    alert('Error in XPATHNodeSet._getAxis: Attempted to get unknown axis type '+ axisConst);
    return null;
  }
};

   
// =========================================================================
// _@method
// XPATHNodeSet._getAncestorAxis - Return a NodeSet containing the concatenation of all of the nodes
//   in the ancestor Axis for each node in the current NodeSet.
//   The ancestor axis contains the ancestors of the context node; the ancestors of the
//   context node consist of the parent of context node and the parent's parent and so on; 
//   thus, the ancestor axis will always include the root node, unless the context node is
//   the root node.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getAncestorAxis = function() {
  var ancestorAxisNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    ancestorAxisNodeSet.union(this.item(i)._getDescendantAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getAncestorOrSelfAxis - Return a NodeSet  containing the concatenation of all of the nodes
//   in the ancestor or self Axis for each node in the current NodeSet.
//   The ancestor-or-self axis contains the context node and the ancestors of the
//   context node; thus, the ancestor axis will always include the root node.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getAncestorOrSelfAxis = function() {
  var ancestorOrSelfAxisNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    ancestorOrSelfAxisNodeSet.union(this.item(i)._getAncestorOrSelfAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getAttributeAxis - Return a NodeSet  containing the concatenation of all of the nodes 
//   in the attribute Axis for each node in the current NodeSet.
//   The attribute axis contains the attributes of the context node; the axis will 
//   be empty unless the context node is an element.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getAttributeAxis = function() { 
  var attributeAxisNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    attributeAxisNodeSet.union(this.item(i)._getAttributeAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getChildAxis - Return a NodeSet  containing the concatenation of all of the nodes 
//   in the child Axis for each node in the current NodeSet.
//   The child axis contains the children of the context node.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getChildAxis = function() { 
  var childNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    childNodeSet.union(this.item(i)._getChildAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getDescendantAxis - Return a NodeSet  containing the concatenation of all of the nodes 
//   in the descendant Axis for each node in the current NodeSet.
//   The descendant axis contains the descendants of the context node; a descendant is 
//   a child or a child of a child and so on; thus the descendant axis never contains 
//   attribute or namespace nodes.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getDescendantAxis = function() { 
  var descendantNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    descendantNodeSet.union(this.item(i)._getDescendantAxis());
  }
};



// =========================================================================
// _@method
// DOMNode._getReversedDescendantAxis - Return a NodeSet  containing the concatenation of all of the nodes 
//   in the descendant Axis for each node in the current NodeSet in Reverse Document Order.
//   The descendant axis contains the descendants of the context node; a descendant is 
//   a child or a child of a child and so on; thus the descendant axis never contains 
//   attribute or namespace nodes.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getReversedDescendantAxis = function() { 
  var descendantNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=this.length-1; i >= 0 ; i--) {
    descendantNodeSet.union(this.item(i)._getReversedDescendantAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getDescendantOrSelfAxis - Return a NodeSet  containing the concatenation of all of the nodes 
//   in the descendant or self  Axis for each node in the current NodeSet.
//   The descendant-or-self axis contains the context node and the descendants of the context node.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getDescendantOrSelfAxis = function() { 
  var descendantOrSelfNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    descendantOrSelfNodeSet.union(this.item(i)._getDescendantOrSelfAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getFollowingAxis - Return a NodeSet  containing the concatenation of all of the nodes 
//   in the following Axis for each node in the current NodeSet.
//   The following axis contains all nodes in the same document as the context node that 
//   are after the context node in document order, excluding any descendants and excluding 
//   attribute nodes and namespace nodes.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getFollowingAxis = function() { 
  var followingNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    followingNodeSet.union(this.item(i)._getFollowingAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getFollowingSiblingAxis - Return a NodeSet  containing the concatenation of all of the nodes 
//   in the following-sibling Axis for each node in the current NodeSet.
//   The following-sibling axis contains all the following siblings of the context node; 
//   if the context node is an attribute node or namespace node, the following-sibling axis is empty.
//   ie, only the nodes that are sibling to the context node, not their descandants
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getFollowingSiblingAxis = function() { 
  var followingSibilingNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    followingSibilingNodeSet.union(this.item(i)._getFollowingSiblingAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getNamespaceAxis - Return a NodeSet  containing the concatenation of all of the nodes 
//   in the namespace Axis for each node in the current NodeSet.
//   the namespace axis contains the namespace nodes of the context node; the axis will 
//   be empty unless the context node is an element.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
// XPATHNodeSet.prototype._getNamespaceAxis = function() { };
//  NOT IMPLEMENTED



// =========================================================================
// _@method
// DOMNode._getParentAxis - Return a NodeSet containing the concatenation of all of the nodes 
//   in the parent Axis for each node in the current NodeSet.
//   The parent axis contains the parent of the context node, if there is one.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getParentAxis = function() {
  var parentNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    parentNodeSet.union(this.item(i)._getParentAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getPrecedingAxis - Return a NodeSet containing the concatenation of all of the nodes 
//   in the preceding Axis for each node in the current NodeSet.
//   The preceding axis contains all nodes in the same document as the context node that 
//   are before the context node in document order, excluding any ancestors and excluding
//   attribute nodes and namespace nodes.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getPrecedingAxis = function() { 
  var precedingNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    precedingNodeSet.union(this.item(i)._getPrecedingAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getPrecedingSiblingAxis - Return a NodeSet  containing the concatenation of all of the nodes 
//   in the preceding Axis for each node in the current NodeSet.
//   The preceding axis contains all nodes in the same document as the context node that 
//   are before the context node in document order, excluding any ancestors and excluding 
//   attribute nodes and namespace nodes.
//   ie, only the nodes that are sibling to the context node, not their descandants
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getPrecedingSiblingAxis = function() { 
  var precedingSiblingNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    precedingSiblingNodeSet.union(this.item(i)._getPrecedingSiblingAxis());
  }
};


// =========================================================================
// _@method
// DOMNode._getSelfAxis - Return a NodeSet  containing the concatenation of all of the nodes 
//   in the self Axis for each node in the current NodeSet.
//   The self axis contains just the context node itself.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getSelfAxis = function() { 
  var selfNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    selfNodeSet.union(this.item(i)._getSelfAxis());
  }
};


// =========================================================================
// _@method
// DOMNode.union - Return a NodeSet containing the concatenation of the current NodeSet
//   and the supplied NodeSet
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param
// nodeSet : XPATHNodeSet - the NodeSet to be appended to this NodeSet
//
// _@return
// : XPATHNodeSet
//
// ========================================================================= 
XPATHNodeSet.prototype.union = function(nodeSet) {
  for (var i=0; i < nodeSet.length; i++) {
    this._appendChild(nodeSet.item(i));
  }

  return this;
};


// =========================================================================
// _@method
// DOMNode._getContainingNodeSet - Return this NodeSet.
//   Note: this method is called from within the evaluation of a Predicate Expression,
//   so that the Context Node can be advised of its containing NodeSet.
//   This is required for the purposes of functions like poisition().
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@return 
// : XPATHNodeSet 
// 
// ========================================================================= 
XPATHNodeSet.prototype._getContainingNodeSet = function() { 
  return this;
};

XPATHNodeSet.prototype.getLength = function() {
    return this.length;
}

// =========================================================================
// _@method
// XPATHNodeSet.filter - Returns a NodeSet containing the Nodes within the current NodeSet
//   that satisfy the Predicate Expression.
//
// _@author
// Jon van Noort (jon@webarcana.com.au)
//
// _@param
// expressionStr : string - the XPathe Predicate Expression
//
// _@return
// : XPATHNodeSet
//
// =========================================================================
XPATHNodeSet.prototype.filter = function(expressionStr) {
  var matchingNodeSet = new XPATHNodeSet(this.ownerDocument);

  for (var i=0; i < this.length; i++) {
    if (this.item(i)._filter(expressionStr, this)) {
      matchingNodeSet._appendChild(this.item(i));
    }
  }

  return matchingNodeSet;
};


function __removeFirstArrayElement(oldArray) {
    var newArray = new Array();

    try {
        for (intLoop = 1; intLoop < oldArray.length; intLoop++) {
            newArray[newArray.length] = oldArray[intLoop];
        }
    }
    catch (e) {
        //no op
    }
    return newArray;

}

