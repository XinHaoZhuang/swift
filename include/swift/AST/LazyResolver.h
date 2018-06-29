//===--- LazyResolver.h - Lazy Resolution for ASTs --------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines the LazyResolver abstract interface.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_AST_LAZYRESOLVER_H
#define SWIFT_AST_LAZYRESOLVER_H

#include "swift/AST/ProtocolConformanceRef.h"
#include "swift/AST/TypeLoc.h"
#include "llvm/ADT/PointerEmbeddedInt.h"

namespace swift {

class AssociatedTypeDecl;
class Decl;
class DeclContext;
class IterableDeclContext;
class ExtensionDecl;
class Identifier;
class NominalTypeDecl;
class NormalProtocolConformance;
class ProtocolConformance;
class ProtocolDecl;
class TypeDecl;
class ValueDecl;
class VarDecl;

/// Abstract interface used to lazily resolve aspects of the AST, such as the
/// types of declarations or protocol conformance structures.
class LazyResolver {
public:
  virtual ~LazyResolver();

  /// Resolve the type witnesses for the given associated type within the given
  /// protocol conformance.
  virtual void resolveTypeWitness(const NormalProtocolConformance *conformance,
                                  AssociatedTypeDecl *assocType) = 0;

  /// Resolve the witness for the given non-type requirement within
  /// the given protocol conformance.
  virtual void resolveWitness(const NormalProtocolConformance *conformance,
                              ValueDecl *requirement) = 0;

  /// Resolve the access of a value.
  ///
  /// It does no type-checking.
  virtual void resolveAccessControl(ValueDecl *VD) = 0;

  /// Resolve the type and declaration attributes of a value.
  ///
  /// This can be called when the type or signature of a value is needed.
  /// It does not perform full type-checking, only checks for basic
  /// consistency and provides the value a type.
  virtual void resolveDeclSignature(ValueDecl *VD) = 0;

  /// Resolve the "overridden" declaration of the given declaration.
  virtual void resolveOverriddenDecl(ValueDecl *VD) = 0;

  /// Resolve the "is Objective-C" bit for the given declaration.
  virtual void resolveIsObjC(ValueDecl *VD) = 0;

  /// Retrieve the superclass of the given class.
  virtual Type getSuperclass(const ClassDecl *classDecl) = 0;

  /// Retrieve the superclass of the given protocol.
  virtual Type getSuperclass(const ProtocolDecl *protocolDecl) = 0;

  /// Resolve the raw type of the given enum.
  virtual Type getRawType(EnumDecl *enumDecl) = 0;

  /// Get a specific inherited type from the given declaration.
  virtual Type getInheritedType(
                        llvm::PointerUnion<TypeDecl *, ExtensionDecl *> decl,
                        unsigned index) = 0;

  /// Resolve the trailing where clause of the given protocol in-place.
  virtual void resolveTrailingWhereClause(ProtocolDecl *proto) = 0;

  /// Bind an extension to its extended type.
  virtual void bindExtension(ExtensionDecl *ext) = 0;

  /// Resolve the type of an extension.
  ///
  /// This can be called to ensure that the members of an extension can be
  /// considered to be members of the extended type.
  virtual void resolveExtension(ExtensionDecl *ext) = 0;

  using ConformanceConstructionInfo = std::pair<SourceLoc, ProtocolDecl *>;
  /// Resolve enough of an extension to find which protocols it is declaring
  /// conformance to.
  ///
  /// This can be called to ensure that the "extension Foo: Bar, Baz" part of
  /// the extension is understood.
  virtual void resolveExtensionForConformanceConstruction(
      ExtensionDecl *ext,
      SmallVectorImpl<ConformanceConstructionInfo> &protocols) = 0;

  /// Resolve any implicitly-declared constructors within the given nominal.
  virtual void resolveImplicitConstructors(NominalTypeDecl *nominal) = 0;

  /// Resolve an implicitly-generated member with the given name.
  virtual void resolveImplicitMember(NominalTypeDecl *nominal, DeclName member) = 0;

  /// Mark the given conformance as "used" from the given declaration context.
  virtual void markConformanceUsed(ProtocolConformanceRef conformance,
                                   DeclContext *dc) = 0;
};

class LazyMemberLoader;

/// Context data for lazy deserialization.
class LazyContextData {
public:
  /// The lazy member loader for this context.
  LazyMemberLoader *loader;
};

/// Context data for generic contexts.
class LazyGenericContextData : public LazyContextData {
public:
  /// The context data used for loading the generic environment.
  uint64_t genericEnvData = 0;
};

/// Context data for iterable decl contexts.
class LazyIterableDeclContextData : public LazyGenericContextData {
public:
  /// The context data used for loading all of the members of the iterable
  /// context.
  uint64_t memberData = 0;

  /// The context data used for loading all of the conformances of the
  /// iterable context.
  uint64_t allConformancesData = 0;
};

/// A class that can lazily load members from a serialized format.
class alignas(void*) LazyMemberLoader {
  virtual void anchor();

public:
  virtual ~LazyMemberLoader() = default;

  /// Populates a given decl \p D with all of its members.
  ///
  /// The implementation should add the members to D.
  virtual void
  loadAllMembers(Decl *D, uint64_t contextData) = 0;

  /// Populates a vector with all members of \p IDC that have DeclName
  /// matching \p N.
  ///
  /// Returns None if an error occurred \em or named member-lookup
  /// was otherwise unsupported in this implementation or Decl.
  virtual Optional<TinyPtrVector<ValueDecl *>>
  loadNamedMembers(const IterableDeclContext *IDC, DeclBaseName N,
                   uint64_t contextData) = 0;

  /// Populates the given vector with all conformances for \p D.
  ///
  /// The implementation should \em not call setConformances on \p D.
  virtual void
  loadAllConformances(const Decl *D, uint64_t contextData,
                      SmallVectorImpl<ProtocolConformance *> &Conformances) = 0;

  /// Returns the default definition type for \p ATD.
  virtual TypeLoc loadAssociatedTypeDefault(const AssociatedTypeDecl *ATD,
                                            uint64_t contextData) = 0;

  /// Returns the generic environment.
  virtual GenericEnvironment *loadGenericEnvironment(const DeclContext *decl,
                                                     uint64_t contextData) = 0;
};

/// A class that can lazily load conformances from a serialized format.
class alignas(void*) LazyConformanceLoader {
  virtual void anchor();

public:
  virtual ~LazyConformanceLoader() = default;

  /// Populates the given protocol conformance.
  virtual void
  finishNormalConformance(NormalProtocolConformance *conformance,
                          uint64_t contextData) = 0;
};

}

#endif // LLVM_SWIFT_AST_LAZYRESOLVER_H
