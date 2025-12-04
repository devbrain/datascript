# Changelog

All notable changes to the DataScript project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **GitHub Repository Setup**
  - Comprehensive README.md with project overview, examples, and comparison to Kaitai Struct
  - MIT License for open source distribution
  - Complete documentation of features and use cases

- **C++ Library Mode with Introspection** (December 2-4, 2025)
  - Three-file architecture: runtime, public API, and implementation headers
  - Parse convenience functions with three overloads (pointer+length, std::vector, std::span)
  - StructView template for runtime introspection
  - Field and FieldIterator classes for dynamic field access
  - Metadata infrastructure (FieldMeta and StructMeta)
  - JSON serialization with pretty-printing support
  - Type queries (is_integral, is_enum, is_string, is_array, is_struct)
  - Value formatting with type-aware string conversion
  - Complete E2E test suite (37 tests, 150 assertions)

- **Documentation** (December 4, 2025)
  - Complete C++ code generation guide (`docs/CPP_CODE_GENERATION.md`, ~2000 lines)
  - Comprehensive API reference for both single-header and library modes
  - Three working examples with full source code
  - Integration guides for CMake, Makefile, and IDEs
  - Best practices and troubleshooting sections

- **ABNF Specification Synchronization** (December 4, 2025)
  - Updated `docs/datascript.abnf` to match parser implementation exactly
  - Added docstrings, endianness directives, struct shorthand syntax
  - Added type instantiation, functions, inline union/struct fields
  - Added complete operator precedence table (13 levels)
  - Added 15 detailed implementation notes
  - Documented all 4 array syntax variants
  - Complete token type list with all keywords and operators

### Changed
- **Multi-Language Backend Architecture** (December 2, 2025)
  - Refactored code generation to support multiple target languages
  - Language-agnostic intermediate representation (IR)
  - Pluggable backend architecture with RendererRegistry
  - C++20 backend production-ready, Go/Python/Java backends planned

- **Expression Rendering Integration** (December 2, 2025)
  - Moved expression rendering logic into CppRenderer
  - Removed separate ExprRenderer abstraction
  - Simplified architecture while maintaining flexibility

### Fixed
- Library mode header generation in CMake build system
- Field index calculations in E2E tests
- Network protocol schema validation

## [0.9.0] - 2025-12-02

### Added
- **Multi-Backend Code Generation Architecture**
  - Abstract BaseRenderer interface for language-independent code generation
  - Polymorphic renderer design enables multiple target languages
  - Language metadata system with case sensitivity, keywords, and capabilities
  - RendererRegistry singleton for centralized renderer management
  - Integrated keyword validation for identifier conflict detection
  - C++ keyword list: Complete set of 77 C++20 reserved keywords
  - Keyword sanitization with automatic conflict resolution
  - Thread-safe singleton pattern for renderer registry

### Changed
- Expression rendering fully integrated into CppRenderer
- Removed separate ExprRenderer abstract class
- ExprContext moved to base_renderer.hh for shared use

## [0.8.0] - 2025-11-27 to 2025-12-01

### Added
- **Docstring Support**
  - Javadoc-style documentation comments using `/** ... */` syntax
  - Attached to all major constructs (constants, constraints, types, fields, enums)
  - Smart processing: strips leading asterisks while preserving formatting
  - Stored in AST as `std::optional<std::string>`
  - 15 comprehensive tests covering single-line, multi-line, and edge cases
  - Zero-copy token handling

- **Complete Language Features**
  - Parameterized types with monomorphization (generic types)
  - Member functions with full expression support
  - Conditional fields (`if` expressions)
  - Constraints and subtypes with runtime validation
  - Labels and alignment directives
  - Global and per-field endianness control
  - Choice types (tagged unions)
  - Inline union and struct fields
  - Array types: fixed, variable, ranged, and unsized

- **Module System**
  - Package declarations
  - Import resolution with transitive dependencies
  - Wildcard imports (`import foo.*;`)
  - Circular import detection
  - Search path resolution (main dir, user paths, DATASCRIPT_PATH env var)
  - Automatic module deduplication

### Changed
- **Memory Safety with RAII**
  - Exception-safe list management using std::unique_ptr with custom deleters
  - RAII guards in all list builder functions
  - Parse error cleanup via Lemon %destructor directives
  - Automatic cleanup on all exit paths
  - Zero memory leaks verified with valgrind

- **Integer Overflow Protection**
  - C++20 std::from_chars replaces unsafe std::strtoull
  - Explicit overflow detection via std::errc::result_out_of_range
  - Locale-independent and faster parsing
  - Returns std::optional<uint64_t> for robust error handling

- **Type Safety Implementation**
  - Compile-time type checking using opaque pointer types
  - Zero-cost conversions via explicit helpers
  - Grammar-level enforcement with Lemon %type declarations
  - Type mismatches caught at compile time

- **Grammar Simplification**
  - One-line semantic actions in grammar
  - No manual token memory management
  - Zero-copy tokenization with buffer pointers
  - Deferred semantic processing

## [0.7.0] - 2025-11-26

### Added
- **7-Phase Semantic Analysis**
  - Phase 1: Symbol table construction
  - Phase 2: Name resolution
  - Phase 3: Type checking
  - Phase 4: Constant expression evaluation
  - Phase 5: Size calculation
  - Phase 6: Constraint validation
  - Phase 7: Reachability analysis

- **IR (Intermediate Representation)**
  - Language-agnostic IR layer between semantic analysis and code generation
  - Command-stream architecture
  - Support for all DataScript language features
  - JSON serialization for debugging

### Added
- **C++ Code Generation (Single-Header Mode)**
  - Generates standalone C++ headers
  - Type-safe struct definitions
  - Read functions for binary parsing
  - Endianness handling
  - Array support (fixed, variable, dynamic)
  - Exception-based error handling

## [0.6.0] - 2025-11-25

### Added
- **Complete Parser Implementation**
  - re2c-based lexer with zero-copy tokenization
  - Lemon (LALR) parser with type-safe semantic values
  - Full expression support with operator precedence
  - AST construction with source position tracking
  - Comprehensive error reporting

### Added
- **Testing Infrastructure**
  - 855 test cases using doctest framework
  - 4,012 assertions covering all features
  - Real-world format parsers (PE/NE, ELF, PNG, ZIP)
  - Zero memory leaks (valgrind verified)
  - Continuous testing in development

## [0.5.0] - Earlier Development

### Added
- Initial project structure
- CMake build system
- Basic parser infrastructure
- Core type system
- Expression evaluation

---

## Version History Summary

- **0.9.0+** - Production-ready compiler with library mode, multi-backend architecture, comprehensive documentation
- **0.8.0** - Complete language implementation with docstrings, parameterized types, module system
- **0.7.0** - 7-phase semantic analysis and IR generation
- **0.6.0** - Complete parser with C++ code generation
- **0.5.0** - Initial implementation

## Notes

### Current Status (December 4, 2025)

**Production Ready** - All features implemented and tested:
- ✅ Complete parser (855 tests, all passing)
- ✅ 7-phase semantic analysis
- ✅ C++ code generation (both modes)
- ✅ Parameterized types (monomorphization)
- ✅ Module loading with imports
- ✅ Comprehensive documentation
- ✅ Zero memory leaks

### Planned Features

- **Additional Language Backends**
  - Go code generation
  - Python code generation
  - Java code generation

- **Enhanced Features**
  - Write mode (binary data writing/serialization)
  - Validation mode (standalone constraint checking)
  - Schema evolution support
  - Default values for fields
  - Calculated fields

### Known Limitations

- No support for unbounded repetition (`while` loops)
- Read-only (write/serialization not yet implemented)
- Single language target (C++20 only, others planned)
- Array sizes must be known (from field or fixed in schema)
