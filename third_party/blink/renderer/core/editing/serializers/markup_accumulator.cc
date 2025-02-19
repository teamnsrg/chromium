/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/serializers/markup_accumulator.h"

#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/cdata_section.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

MarkupAccumulator::MarkupAccumulator(EAbsoluteURLs resolve_urls_method,
                                     SerializationType serialization_type)
    : formatter_(resolve_urls_method, serialization_type) {}

MarkupAccumulator::~MarkupAccumulator() = default;

void MarkupAccumulator::AppendString(const String& string) {
  markup_.Append(string);
}

void MarkupAccumulator::AppendEndTag(const Element& element) {
  AppendEndMarkup(element);
}

void MarkupAccumulator::AppendStartMarkup(const Node& node) {
  switch (node.getNodeType()) {
    case Node::kTextNode:
      formatter_.AppendText(markup_, ToText(node));
      break;
    case Node::kElementNode:
      AppendElement(ToElement(node));
      break;
    case Node::kAttributeNode:
      // Only XMLSerializer can pass an Attr.  So, |documentIsHTML| flag is
      // false.
      formatter_.AppendAttributeValue(markup_, ToAttr(node).value(), false);
      break;
    default:
      formatter_.AppendStartMarkup(markup_, node);
      break;
  }
}

void MarkupAccumulator::AppendEndMarkup(const Element& element) {
  formatter_.AppendEndMarkup(markup_, element);
}

void MarkupAccumulator::AppendCustomAttributes(const Element&) {}

bool MarkupAccumulator::ShouldIgnoreAttribute(
    const Element& element,
    const Attribute& attribute) const {
  return false;
}

bool MarkupAccumulator::ShouldIgnoreElement(const Element& element) const {
  return false;
}

void MarkupAccumulator::AppendElement(const Element& element) {
  // https://html.spec.whatwg.org/multipage/parsing.html#html-fragment-serialisation-algorithm
  AppendStartTagOpen(element);

  AttributeCollection attributes = element.Attributes();
  if (SerializeAsHTMLDocument(element)) {
    // 3.2. Element: If current node's is value is not null, and the
    // element does not have an is attribute in its attribute list, ...
    const AtomicString& is_value = element.IsValue();
    if (!is_value.IsNull() && !attributes.Find(html_names::kIsAttr)) {
      AppendAttribute(element, Attribute(html_names::kIsAttr, is_value));
    }
  }
  for (const auto& attribute : attributes) {
    if (!ShouldIgnoreAttribute(element, attribute))
      AppendAttribute(element, attribute);
  }

  // Give an opportunity to subclasses to add their own attributes.
  AppendCustomAttributes(element);

  AppendStartTagClose(element);
}

void MarkupAccumulator::AppendStartTagOpen(const Element& element) {
  formatter_.AppendStartTagOpen(markup_, element);
  if (!SerializeAsHTMLDocument(element) && ShouldAddNamespaceElement(element)) {
    AppendNamespace(element.prefix(), element.namespaceURI());
  }
}

void MarkupAccumulator::AppendStartTagClose(const Element& element) {
  formatter_.AppendStartTagClose(markup_, element);
}

void MarkupAccumulator::AppendAttribute(const Element& element,
                                        const Attribute& attribute) {
  String value = formatter_.ResolveURLIfNeeded(element, attribute);
  if (SerializeAsHTMLDocument(element)) {
    MarkupFormatter::AppendAttributeAsHTML(markup_, attribute, value);
  } else {
    AppendAttributeAsXMLWithNamespace(element, attribute, value);
  }
}

void MarkupAccumulator::AppendAttributeAsXMLWithNamespace(
    const Element& element,
    const Attribute& attribute,
    const String& value) {
  // https://w3c.github.io/DOM-Parsing/#serializing-an-element-s-attributes

  // 3.3. Let attribute namespace be the value of attr's namespaceURI value.
  const AtomicString& attribute_namespace = attribute.NamespaceURI();

  // 3.4. Let candidate prefix be null.
  AtomicString candidate_prefix;

  // 3.5. If attribute namespace is not null, then run these sub-steps:

  // 3.5.1. Let candidate prefix be the result of retrieving a preferred
  // prefix string from map given namespace attribute namespace with preferred
  // prefix being attr's prefix value.
  // TODO(tkent): Implement it. crbug.com/906807
  candidate_prefix = attribute.Prefix();

  // 3.5.2. If the value of attribute namespace is the XMLNS namespace, then
  // run these steps:
  if (attribute_namespace == xmlns_names::kNamespaceURI) {
    if (!attribute.Prefix() && attribute.LocalName() != g_xmlns_atom)
      candidate_prefix = g_xmlns_atom;
    // Account for the namespace attribute we're about to append.
    AddPrefix(attribute.Prefix() ? attribute.LocalName() : g_empty_atom,
              attribute.Value());
  } else if (attribute_namespace == xml_names::kNamespaceURI) {
    // TODO(tkent): Remove this block when we implement 'retrieving a
    // preferred prefix string'.
    if (!candidate_prefix)
      candidate_prefix = g_xml_atom;
  } else {
    // TODO(tkent): Remove this block. The standard and Firefox don't
    // have this behavior.
    if (attribute_namespace == xlink_names::kNamespaceURI) {
      if (!candidate_prefix)
        candidate_prefix = g_xlink_atom;
    }

    // 3.5.3. Otherwise, the attribute namespace in not the XMLNS namespace.
    // Run these steps:
    if (ShouldAddNamespaceAttribute(attribute, element)) {
      if (!candidate_prefix) {
        // 3.5.3.1. Let candidate prefix be the result of generating a prefix
        // providing map, attribute namespace, and prefix index as input.
        candidate_prefix = GeneratePrefix(attribute_namespace);
        // 3.5.3.2. Append the following to result, in the order listed:
        MarkupFormatter::AppendAttribute(markup_, g_xmlns_atom,
                                         candidate_prefix, attribute_namespace,
                                         false);
      } else {
        DCHECK(candidate_prefix);
        AppendNamespace(candidate_prefix, attribute_namespace);
      }
    }
  }
  MarkupFormatter::AppendAttribute(markup_, candidate_prefix,
                                   attribute.LocalName(), value, false);
}

bool MarkupAccumulator::ShouldAddNamespaceAttribute(const Attribute& attribute,
                                                    const Element& element) {
  // xmlns and xmlns:prefix attributes should be handled by another branch in
  // AppendAttributeAsXMLWithNamespace().
  DCHECK_NE(attribute.NamespaceURI(), xmlns_names::kNamespaceURI);

  // Attributes are in the null namespace by default.
  if (!attribute.NamespaceURI())
    return false;

  // Attributes without a prefix will need one generated for them, and an xmlns
  // attribute for that prefix.
  if (!attribute.Prefix())
    return true;

  return !element.hasAttribute(WTF::g_xmlns_with_colon + attribute.Prefix());
}

void MarkupAccumulator::AppendNamespace(const AtomicString& prefix,
                                        const AtomicString& namespace_uri) {
  AtomicString found_uri = LookupNamespaceURI(prefix);
  if (!EqualIgnoringNullity(found_uri, namespace_uri)) {
    AddPrefix(prefix, namespace_uri);
    if (prefix.IsEmpty()) {
      MarkupFormatter::AppendAttribute(markup_, g_null_atom, g_xmlns_atom,
                                       namespace_uri, false);
    } else {
      MarkupFormatter::AppendAttribute(markup_, g_xmlns_atom, prefix,
                                       namespace_uri, false);
    }
  }
}

EntityMask MarkupAccumulator::EntityMaskForText(const Text& text) const {
  return formatter_.EntityMaskForText(text);
}

void MarkupAccumulator::PushNamespaces(const Node& node) {
  if (!node.IsElementNode() || SerializeAsHTMLDocument(node))
    return;
  DCHECK_GT(namespace_stack_.size(), 0u);
  // TODO(tkent): Avoid to copy the whole map.
  // We can't do |namespace_stack_.emplace_back(namespace_stack_.back())|
  // because back() returns a reference in the vector backing, and
  // emplace_back() can reallocate it.
  namespace_stack_.push_back(Namespaces(namespace_stack_.back()));
}

void MarkupAccumulator::PopNamespaces(const Node& node) {
  if (!node.IsElementNode() || SerializeAsHTMLDocument(node))
    return;
  namespace_stack_.pop_back();
}

// https://w3c.github.io/DOM-Parsing/#dfn-add
void MarkupAccumulator::AddPrefix(const AtomicString& prefix,
                                  const AtomicString& namespace_uri) {
  namespace_stack_.back().Set(prefix ? prefix : g_empty_atom, namespace_uri);
}

AtomicString MarkupAccumulator::LookupNamespaceURI(const AtomicString& prefix) {
  return namespace_stack_.back().at(prefix ? prefix : g_empty_atom);
}

// https://w3c.github.io/DOM-Parsing/#dfn-generating-a-prefix
AtomicString MarkupAccumulator::GeneratePrefix(
    const AtomicString& new_namespace) {
  AtomicString generated_prefix;
  do {
    // 1. Let generated prefix be the concatenation of the string "ns" and the
    // current numerical value of prefix index.
    generated_prefix = "ns" + String::Number(prefix_index_);
    // 2. Let the value of prefix index be incremented by one.
    ++prefix_index_;
  } while (LookupNamespaceURI(generated_prefix));
  // 3. Add to map the generated prefix given the new namespace namespace.
  AddPrefix(generated_prefix, new_namespace);
  // 4. Return the value of generated prefix.
  return generated_prefix;
}

bool MarkupAccumulator::SerializeAsHTMLDocument(const Node& node) const {
  return formatter_.SerializeAsHTMLDocument(node);
}

bool MarkupAccumulator::ShouldAddNamespaceElement(const Element& element) {
  // Don't add namespace attribute if it is already defined for this elem.
  const AtomicString& prefix = element.prefix();
  if (prefix.IsEmpty()) {
    if (element.hasAttribute(g_xmlns_atom)) {
      AddPrefix(g_empty_atom, element.namespaceURI());
      return false;
    }
    return true;
  }

  return !element.hasAttribute(WTF::g_xmlns_with_colon + prefix);
}

std::pair<Node*, Element*> MarkupAccumulator::GetAuxiliaryDOMTree(
    const Element& element) const {
  return std::pair<Node*, Element*>();
}

template <typename Strategy>
void MarkupAccumulator::SerializeNodesWithNamespaces(
    const Node& target_node,
    EChildrenOnly children_only) {
  if (target_node.IsElementNode() &&
      ShouldIgnoreElement(ToElement(target_node))) {
    return;
  }

  PushNamespaces(target_node);

  if (!children_only)
    AppendStartMarkup(target_node);

  if (!(SerializeAsHTMLDocument(target_node) &&
        ElementCannotHaveEndTag(target_node))) {
    Node* current = IsHTMLTemplateElement(target_node)
                        ? Strategy::FirstChild(
                              *ToHTMLTemplateElement(target_node).content())
                        : Strategy::FirstChild(target_node);
    for (; current; current = Strategy::NextSibling(*current))
      SerializeNodesWithNamespaces<Strategy>(*current, kIncludeNode);

    // Traverses other DOM tree, i.e., shadow tree.
    if (target_node.IsElementNode()) {
      std::pair<Node*, Element*> auxiliary_pair =
          GetAuxiliaryDOMTree(ToElement(target_node));
      Node* auxiliary_tree = auxiliary_pair.first;
      Element* enclosing_element = auxiliary_pair.second;
      if (auxiliary_tree) {
        if (auxiliary_pair.second)
          AppendStartMarkup(*enclosing_element);
        current = Strategy::FirstChild(*auxiliary_tree);
        for (; current; current = Strategy::NextSibling(*current)) {
          SerializeNodesWithNamespaces<Strategy>(*current, kIncludeNode);
        }
        if (enclosing_element)
          AppendEndTag(*enclosing_element);
      }
    }
  }

  if ((!children_only && target_node.IsElementNode()) &&
      !(SerializeAsHTMLDocument(target_node) &&
        ElementCannotHaveEndTag(target_node)))
    AppendEndTag(ToElement(target_node));
  PopNamespaces(target_node);
}

template <typename Strategy>
String MarkupAccumulator::SerializeNodes(const Node& target_node,
                                         EChildrenOnly children_only) {
  if (!SerializeAsHTMLDocument(target_node)) {
    // https://w3c.github.io/DOM-Parsing/#dfn-xml-serialization
    DCHECK_EQ(namespace_stack_.size(), 0u);
    // 2. Let prefix map be a new namespace prefix map.
    namespace_stack_.emplace_back();
    // 3. Add the XML namespace with prefix value "xml" to prefix map.
    AddPrefix(g_xml_atom, xml_names::kNamespaceURI);
    // 4. Let prefix index be a generated namespace prefix index with value 1.
    prefix_index_ = 1;
  }

  SerializeNodesWithNamespaces<Strategy>(target_node, children_only);
  return ToString();
}

template String MarkupAccumulator::SerializeNodes<EditingStrategy>(
    const Node&,
    EChildrenOnly);

}  // namespace blink
