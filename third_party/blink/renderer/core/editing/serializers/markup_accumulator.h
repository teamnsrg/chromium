/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SERIALIZERS_MARKUP_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SERIALIZERS_MARKUP_ACCUMULATOR_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/editing/editing_strategy.h"
#include "third_party/blink/renderer/core/editing/serializers/markup_formatter.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class Attribute;
class Element;
class Node;

using Namespaces = HashMap<AtomicString, AtomicString>;

class MarkupAccumulator {
  STACK_ALLOCATED();

 public:
  MarkupAccumulator(EAbsoluteURLs,
                    SerializationType = SerializationType::kAsOwnerDocument);
  virtual ~MarkupAccumulator();

  template <typename Strategy>
  String SerializeNodes(const Node&, EChildrenOnly);

 protected:
  // Serialize a Node, without its children and its end tag.
  virtual void AppendStartMarkup(const Node&);
  virtual void AppendElement(const Element&);
  virtual void AppendAttribute(const Element&, const Attribute&);

  MarkupFormatter formatter_;
  StringBuilder markup_;

 private:
  bool SerializeAsHTMLDocument(const Node&) const;
  String ToString() { return markup_.ToString(); }

  void AppendString(const String&);
  void AppendStartTagOpen(const Element&);
  void AppendStartTagClose(const Element&);
  bool ShouldAddNamespaceElement(const Element&);
  void AppendNamespace(const AtomicString& prefix,
                       const AtomicString& namespace_uri);
  void AppendAttributeAsXMLWithNamespace(const Element& element,
                                         const Attribute& attribute,
                                         const String& value);
  static bool ShouldAddNamespaceAttribute(const Attribute& attribute,
                                          const Element& element);

  void AppendEndTag(const Element&);
  void AppendEndMarkup(const Element&);

  EntityMask EntityMaskForText(const Text&) const;

  void PushNamespaces(const Node& node);
  void PopNamespaces(const Node& node);
  void AddPrefix(const AtomicString& prefix, const AtomicString& namespace_uri);
  AtomicString LookupNamespaceURI(const AtomicString& prefix);
  AtomicString GeneratePrefix(const AtomicString& new_namespace);

  virtual void AppendCustomAttributes(const Element&);
  virtual bool ShouldIgnoreAttribute(const Element&, const Attribute&) const;
  virtual bool ShouldIgnoreElement(const Element&) const;

  // Returns an auxiliary DOM tree, i.e. shadow tree, that needs also to be
  // serialized. The root of auxiliary DOM tree is returned as an 1st element
  // in the pair. It can be null if no auxiliary DOM tree exists. An additional
  // element used to enclose the serialized content of auxiliary DOM tree
  // can be returned as 2nd element in the pair. It can be null if this is not
  // needed. For shadow tree, a <template> element is needed to wrap the shadow
  // tree content.
  virtual std::pair<Node*, Element*> GetAuxiliaryDOMTree(const Element&) const;

  template <typename Strategy>
  void SerializeNodesWithNamespaces(const Node& target_node,
                                    EChildrenOnly children_only);

  Vector<Namespaces> namespace_stack_;
  // https://w3c.github.io/DOM-Parsing/#dfn-generated-namespace-prefix-index
  uint32_t prefix_index_;

  DISALLOW_COPY_AND_ASSIGN(MarkupAccumulator);
};

extern template String MarkupAccumulator::SerializeNodes<EditingStrategy>(
    const Node&,
    EChildrenOnly);

}  // namespace blink

#endif
