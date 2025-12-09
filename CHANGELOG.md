# Changelog

All notable changes to the DataScript project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- **Inline Discriminator Default Case Byte Consumption** (December 9, 2025)
  - Fixed discriminator byte being incorrectly consumed in default case
  - **Bug**: For inline discriminator choices, the default case consumed the discriminator byte, causing data misalignment
  - **Example**: For `choice NEControlText : uint8 { case 0xFF: uint16 ordinal; default: string text; }`:
    - Input `"Hello\0"` (0x48='H', 0x65='e', ...) was parsed as `text = "ello"` instead of `text = "Hello"`
    - The 'H' (0x48) was consumed as discriminator but should be part of the string
  - **Root Cause**: Default case didn't restore position after peeking discriminator
  - **Fix**:
    - Save position before reading inline discriminator when there's a default case
    - Restore position for default case so field reads from discriminator position
    - Discriminator byte becomes part of the default case's data
  - **New behavior for inline discriminator choices**:
    - Explicit cases (e.g., `case 0xFF`): discriminator is consumed, field reads AFTER discriminator
    - Default case: discriminator is NOT consumed, field reads FROM discriminator position
    - Anonymous block cases: position restored, struct re-reads discriminator as first field
  - **Real-world impact**: Enables correct parsing of Windows NE/PE resource formats:
    - NE dialog control text: `0xFF` + ordinal vs. null-terminated string starting with first byte
    - PE resource name/ID: `0xFFFF` + ordinal vs. Unicode string starting with first uint16
  - Files: `command_builder.cc`
  - 10 new E2E tests, 4 existing tests updated

- **Inline Discriminator Position Restore for Anonymous Block Cases** (December 9, 2025)
  - Fixed byte misalignment when using inline struct syntax in choice cases
  - **Bug**: For `choice Foo : uint8 { case 0xFF: { uint8 marker; ... } data; }`, the discriminator byte was consumed but not available to the inline struct's first field
  - **Symptoms**: `marker` would read the wrong byte (next byte after discriminator instead of discriminator itself)
  - **Root Cause**: Inline discriminator choices consumed the discriminator but didn't restore position for anonymous block cases
  - **Fix**:
    - Added `is_anonymous_block` flag to IR `case_def`
    - CommandBuilder saves position before reading discriminator when any case is anonymous block
    - Position restored only for anonymous block cases (not regular field cases)
    - Preserved flag in Phase 0 desugaring (was incorrectly reset to false)
  - **Key distinction**:
    - Anonymous block cases: `case 0xFF: { uint8 marker; } data;` → position restored, struct re-reads discriminator
    - Regular field cases: `case 0xFF: uint16 value;` → position NOT restored, field reads after discriminator
    - Named struct cases: `case 0xFF: SomeStruct data;` → position NOT restored, struct reads after discriminator
  - Files: `ir.hh`, `ir_builder.cc`, `command_builder.cc`, `phase0_desugar.cc`
  - Comprehensive E2E tests for both library mode and code generation patterns

- **Multi-line Docstring Escaping in Library Mode** (December 9, 2025)
  - Fixed invalid C++ string literals when docstrings contain newlines
  - **Bug**: Multi-line docstrings were output directly without escaping, causing compilation errors
  - **Symptoms**: Generated code had unterminated string literals across multiple lines
  - **Fix**: Created `escape_cpp_string_literal()` utility in `cpp_string_utils.hh`
  - Escapes: `\n`, `\r`, `\t`, `\\`, `\"`
  - Applied to field documentation and struct documentation in metadata generation
  - Centralized utility available for all C++ codegen components

- **ODR Violation in Library Mode Conditional Tests** (December 9, 2025)
  - Fixed One Definition Rule violation causing test failures
  - **Bug**: Library mode tests defined `SimpleConditional` and `MultipleConditionals` in `namespace generated`, conflicting with E2E test definitions
  - **Symptoms**: Tests passed standalone but failed in test suite; wrong struct layout used at runtime
  - **Root Cause**: Both `test_e2e_conditionals.cc` and `test_library_mode_conditionals.cc` defined same-named structs with different fields in same namespace
  - **Fix**: Renamed library mode conditional structs with `LM` prefix (`LMSimpleConditional`, `LMMultipleConditionals`, etc.)
  - All 951 tests now pass (including new inline discriminator tests)

### Changed
- **Cleaned Up Test Output** (December 9, 2025)
  - Removed all debug output from unit tests for cleaner test runs
  - Removed 7 "Feature Coverage Summary" test cases that only printed documentation
  - Removed 3 `std::cout` debug statements from choice codegen tests
  - Test output now shows only failures, not misleading debug info

### Added
- **Library Mode E2E Tests for Inline Discriminator Choices** (December 9, 2025)
  - Comprehensive runtime tests for inline discriminator position restore behavior
  - Schema: `library_mode_inline_discrim.ds` with 3 choice types and 6 container structs
  - 18 test cases covering:
    - Anonymous block cases (position restored): ordinal, string, special cases
    - Regular field cases (position NOT restored): regular, default, simple cases
    - Named struct cases (position NOT restored): verifies discriminator consumed correctly
    - Mixed choices with both anonymous and named struct cases
    - Byte count verification for all case types
    - StructView introspection API
  - Tests validate the key semantic distinction between case types
  - All tests pass with zero memory leaks (valgrind verified)

- **C++ String Utilities for Code Generation** (December 9, 2025)
  - New header: `lib/include/datascript/codegen/cpp/cpp_string_utils.hh`
  - `escape_cpp_string_literal()` function for safe string literal output
  - Centralized utility for all C++ codegen components
  - Handles newlines, carriage returns, tabs, backslashes, and quotes

- **Inline Anonymous Structs for Choice Cases** (December 8, 2025)
  - Choice cases can now use inline struct syntax like unions
  - Syntax: `case 0xFFFF: { uint16 marker; uint16 ordinal; } data;`
  - Eliminates need for separate named structs
  - Desugared at Phase 0 into generated struct types
  - Consistent with union inline struct syntax
  - Full integration across parser, AST, semantic analysis, IR, and codegen
  - Zero runtime overhead (same as hand-written code)
  - Comprehensive tests (5 new parser tests, 45 assertions)

- **Package Path Validation** (December 8, 2025)
  - Java-like package system enforcement: file paths must match package declarations
  - Example: `package test.foo;` requires file at `.../test/foo.ds`
  - New `package_path_mismatch_error` exception with helpful error messages
  - Shows actual location, expected location, and fix suggestions
  - Automatic validation during module loading for all imports
  - Cross-platform path handling (both `/` and `\` separators)
  - Prevents module resolution errors and ensures predictable structure

- **E2E Tests for Choice Discriminators** (December 8, 2025)
  - Comprehensive test suite for discriminator behavior
  - 12 test cases covering inline/external discriminators, all integer types
  - Verifies correct discriminator reading and case selection
  - Tests nested choices, big-endian discriminators, optional choices
  - Validates generated C++ code structure and switch statements

### Fixed
- **Critical: Choice Default Case Code Generation** (December 8, 2025)
  - Fixed incorrect C++ code generation for choice types with default cases
  - **Bug**: Default case code was placed inside the last `if` block instead of an `else` block
  - **Impact**: Both the regular case and default case would execute sequentially, with default overwriting regular case
  - **Root Cause**: `render_start_choice_case()` in `cpp_renderer.cc` didn't generate `else` block for default cases (empty `case_values`)
  - **Fix**: Added proper handling to generate `else` block when `case_values` is empty and not first case
  - **Example of fix**:
    - Before (buggy): `if (x == 1) { case1; default; }  // default overwrites case1!`
    - After (fixed): `if (x == 1) { case1; } else { default; }`
  - Affects: All choice types with default cases (inline discriminators and external discriminators)
  - Test coverage: 3 comprehensive test cases with 22 assertions
  - File: `lib/src/codegen/cpp/cpp_renderer.cc:911-914`

- **Critical: Type Alias Resolution** (December 8, 2025)
  - Fixed type aliases to complex types (struct, union, enum, choice)
  - **Bug**: Aliases were resolved as `uint8_t` instead of target type
  - **Root Cause**: Phase 2 name resolution wasn't resolving qualified names in typedefs
  - **Fix**: Added `resolve_type()` call for all type alias target types
  - Affects: typedef to struct/union/enum/choice/subtype (all now work correctly)
  - Backward compatible: previously broken code now works
  - Test coverage: New IR generation test with 8 assertions

### Changed
- **BREAKING: Explicit Discriminator Type Required for Inline Choices** (December 8, 2025)
  - Inline discriminator choices now require explicit type declaration
  - **Old syntax (removed):** `choice Name { case 0xFFFF: ... }`
  - **New syntax (required):** `choice Name : uint16 { case 0xFFFF: ... }`
  - **Rationale:**
    - Implicit type inference was misleading and not obvious to users
    - Could silently change binary format if case values were modified
    - Explicit declaration makes wire format clear and stable
    - Consistent with enum underlying type syntax
  - **Migration:** Add `: uintN` after choice name (where N = 8/16/32/64)
  - Comprehensive validation: type must be unsigned integer, all case values must fit
  - Clear error messages guide users to correct syntax
  - Updated all documentation examples

### Added
- **Optional Field Syntax Sugar** (December 7, 2025)
  - New `optional` keyword as syntactic sugar for conditional fields
  - Syntax: `field optional condition;` (equivalent to `field if condition;`)
  - Both syntaxes generate identical C++ code using `std::optional<T>`
  - Added TOKEN_OPTIONAL to parser and "optional" keyword to lexer
  - Pure syntactic sugar - desugars during parsing, no AST/IR/codegen changes needed
  - Updated grammar (`docs/datascript.abnf`) to show both `if` and `optional` alternatives
  - Comprehensive documentation in language guide and C++ code generation reference

- **Inline Discriminator Choice Types** (December 7, 2025)
  - Choice types can read and test their own discriminator from the input stream
  - Syntax: `choice TypeName : discriminator_type { case value: fields... }` (no `on` clause)
  - Explicit discriminator type (uint8/16/32/64) required
  - Generated code uses peek-test-dispatch pattern (non-consuming reads)
  - Peek helper functions: `peek_uint8()`, `peek_uint16_le/be()`, `peek_uint32_le/be()`, `peek_uint64_le/be()`
  - Useful for formats with magic bytes, markers, or type-dependent headers (e.g., Windows resources: 0xFFFF)
  - Full backward compatibility with external discriminator choices (`choice on expr`)
  - Updated grammar (`docs/datascript.abnf`) with required discriminator type
  - Comprehensive documentation in language guide and C++ code generation reference

- **Unicode String Types** (December 7, 2025)
  - UTF-16 string type (`u16string`) with null-termination support
  - UTF-32 string type (`u32string`) with null-termination support
  - Endianness modifiers for Unicode strings (`little u16string`, `big u32string`)
  - Default little-endian encoding when endianness not specified
  - C++ code generation with std::u16string and std::u32string mappings
  - Reader functions: `read_u16string_le()`, `read_u16string_be()`, `read_u32string_le()`, `read_u32string_be()`
  - Full integration across lexer, parser, AST, IR, and code generation layers
  - Comprehensive documentation in language guide and C++ code generation reference

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
