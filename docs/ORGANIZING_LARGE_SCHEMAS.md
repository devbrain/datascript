# Organizing Large Schemas in DataScript

This guide provides recommended patterns for organizing large, complex binary format specifications in DataScript.

## Module System Constraints

DataScript follows a strict package-to-file mapping similar to Java/Python:

- **One package per file**: Each `.ds` file declares exactly one package
- **One file per package**: Each package name maps to exactly one file location
- **Directory structure matches package hierarchy**: `package foo.bar.baz;` → file at `foo/bar/baz.ds`

**You CANNOT do this:**
```
❌ libexe/format/pe/part1.ds  → package libexe.format.pe;
❌ libexe/format/pe/part2.ds  → package libexe.format.pe;  // Conflict!
```

**But you CAN do this:**
```
✅ libexe/format/pe.ds             → package libexe.format.pe;
✅ libexe/format/pe/headers.ds     → package libexe.format.pe.headers;
✅ libexe/format/pe/sections.ds    → package libexe.format.pe.sections;
```

## Pattern 1: Hierarchical Package Organization

Break large formats into logical submodules using nested packages.

### Example: PE (Portable Executable) Format

```
libexe/
├── format/
│   └── pe.ds                    # Main entry point
│       └── pe/
│           ├── headers.ds       # File headers (DOS, PE, Optional)
│           ├── sections.ds      # Section table and data
│           ├── imports.ds       # Import directory and tables
│           ├── exports.ds       # Export directory and tables
│           ├── resources.ds     # Resource directory tree
│           ├── relocations.ds   # Base relocations
│           ├── debug.ds         # Debug information
│           └── certificates.ds  # Security certificates
```

**File: `libexe/format/pe.ds`** (Main entry point)
```datascript
package libexe.format.pe;

import libexe.format.pe.headers.*;
import libexe.format.pe.sections.*;
import libexe.format.pe.imports.*;

/**
 * Complete PE (Portable Executable) file format
 * Windows executable and DLL format
 */
struct PEFile {
    headers.DOSHeader dos_header;
    headers.PEHeader pe_header;
    headers.OptionalHeader optional_header;

    sections.SectionTable section_table(pe_header.number_of_sections);

    // Conditional data directories
    if (optional_header.number_of_rva_and_sizes > 1) {
        imports.ImportDirectory import_dir @ optional_header.data_directories[1].virtual_address;
    }
}
```

**File: `libexe/format/pe/headers.ds`**
```datascript
package libexe.format.pe.headers;

/** DOS header (legacy MZ header) */
struct DOSHeader {
    uint16 e_magic;       // 0x5A4D ("MZ")
    uint16 e_cblp;
    uint16 e_cp;
    // ... 30+ more fields
    uint32 e_lfanew;      // Offset to PE header
};

/** PE file header (COFF header) */
struct PEHeader {
    uint32 signature;     // 0x00004550 ("PE\0\0")
    uint16 machine;
    uint16 number_of_sections;
    // ... more fields
};

/** Optional header (required despite name) */
struct OptionalHeader {
    uint16 magic;
    uint8 major_linker_version;
    // ... 50+ more fields
};
```

**File: `libexe/format/pe/sections.ds`**
```datascript
package libexe.format.pe.sections;

/** Section table entry */
struct SectionHeader {
    uint8 name[8];
    uint32 virtual_size;
    uint32 virtual_address;
    uint32 size_of_raw_data;
    uint32 pointer_to_raw_data;
    // ... more fields
};

/** Section table (array of headers) */
struct SectionTable(uint16 count) {
    SectionHeader sections[count];
};
```

## Pattern 2: Feature-Based Organization

Organize by logical features rather than file format sections.

### Example: Network Protocol Stack

```
net/
├── proto/
│   └── tcp.ds               # Main TCP implementation
│       └── tcp/
│           ├── segment.ds   # TCP segment format
│           ├── options.ds   # TCP options parsing
│           ├── checksum.ds  # Checksum calculation helpers
│           └── state.ds     # Connection state structures
```

**File: `net/proto/tcp.ds`**
```datascript
package net.proto.tcp;

import net.proto.tcp.segment.*;
import net.proto.tcp.options.*;

/** TCP packet with all headers and options */
struct TCPPacket {
    segment.TCPSegment segment;

    if (segment.data_offset > 5) {
        options.TCPOptions options(segment.data_offset);
    }
};
```

## Pattern 3: Shared Type Libraries

Create reusable type libraries for common patterns.

### Example: Common Types Library

```
common/
├── types.ds          # Primitive type aliases
├── strings.ds        # String handling utilities
├── timestamps.ds     # Time/date structures
└── guids.ds          # GUID/UUID structures
```

**File: `common/types.ds`**
```datascript
package common.types;

/** 32-bit little-endian unsigned integer (Windows DWORD) */
type DWORD = little uint32;

/** 16-bit little-endian unsigned integer (Windows WORD) */
type WORD = little uint16;

/** 8-bit unsigned integer (Windows BYTE) */
type BYTE = uint8;

/** Relative Virtual Address (RVA) */
type RVA = little uint32;
```

**File: `common/guids.ds`**
```datascript
package common.guids;

/**
 * 128-bit GUID (Globally Unique Identifier)
 * Format: {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
 */
struct GUID {
    little uint32 data1;
    little uint16 data2;
    little uint16 data3;
    big uint64 data4;  // Big-endian for proper string representation
};
```

**Usage in other files:**
```datascript
package libexe.format.pe.debug;

import common.types.*;
import common.guids.*;

struct DebugDirectory {
    DWORD characteristics;
    DWORD time_date_stamp;
    GUID signature;
};
```

## Pattern 4: Import Strategy

Use targeted imports vs. wildcard imports based on your needs.

### Targeted Imports (Explicit)
```datascript
package myapp.parser;

// Import specific types - clear dependencies
import libexe.format.pe.headers.DOSHeader;
import libexe.format.pe.headers.PEHeader;
import libexe.format.pe.sections.SectionTable;

struct MyParser {
    DOSHeader dos;
    PEHeader pe;
    SectionTable sections(pe.number_of_sections);
};
```

### Wildcard Imports (Convenient)
```datascript
package myapp.parser;

// Import all types from package - convenient for related types
import libexe.format.pe.headers.*;
import libexe.format.pe.sections.*;

struct MyParser {
    DOSHeader dos;
    PEHeader pe;
    SectionTable sections(pe.number_of_sections);
};
```

### Directory Wildcard Imports (Load All)
```datascript
package myapp.parser;

// Import all .ds files in directory - loads everything
import libexe.format.pe.*;  // Loads all .ds files in libexe/format/pe/

struct MyCompleteParser {
    // All types from all submodules available
    headers.DOSHeader dos;
    sections.SectionTable sections(5);
};
```

## Pattern 5: Version-Specific Modules

Handle format versions with separate packages.

### Example: Multiple Format Versions

```
image/
└── png/
    ├── png.ds       # Main PNG, delegates to version
    ├── v1_0.ds      # PNG 1.0 format
    ├── v1_1.ds      # PNG 1.1 format
    └── v1_2.ds      # PNG 1.2 format
```

**File: `image/png/png.ds`**
```datascript
package image.png;

import image.png.v1_0.*;
import image.png.v1_1.*;
import image.png.v1_2.*;

/** PNG file with version detection */
struct PNGFile {
    uint8 signature[8];  // PNG signature

    // First chunk determines version
    uint32 chunk_length;
    uint32 chunk_type;

    // Dispatch to version-specific parser
    // (In real code, would use choice or conditional)
};
```

## Pattern 6: Platform-Specific Modules

Separate platform-specific implementations.

```
exe/
├── common.ds          # Common executable structures
└── format/
    ├── pe.ds          # Windows PE (Portable Executable)
    ├── elf.ds         # Linux/Unix ELF
    ├── macho.ds       # macOS Mach-O
    └── coff.ds        # Generic COFF
```

## Pattern 7: Real-World Example (Complete)

### Windows PE Format (Production-Scale)

```
libexe/
└── format/
    ├── pe.ds                      # Main entry point (200 lines)
    └── pe/
        ├── headers.ds             # DOS + PE + Optional headers (150 lines)
        ├── sections.ds            # Section table (100 lines)
        ├── data_directories.ds    # Data directory structures (80 lines)
        ├── imports.ds             # Import tables (120 lines)
        ├── exports.ds             # Export tables (100 lines)
        ├── resources.ds           # Resource directory tree (200 lines)
        ├── relocations.ds         # Base relocations (80 lines)
        ├── tls.ds                 # Thread-local storage (60 lines)
        ├── load_config.ds         # Load configuration (90 lines)
        ├── debug.ds               # Debug directory (70 lines)
        ├── certificates.ds        # Security certificates (80 lines)
        ├── exceptions.ds          # Exception handling (x64 only) (100 lines)
        └── clr.ds                 # .NET CLR metadata (150 lines)
```

**File: `libexe/format/pe.ds`** (Main orchestrator)
```datascript
package libexe.format.pe;

import libexe.format.pe.headers.*;
import libexe.format.pe.sections.*;
import libexe.format.pe.data_directories.*;
import libexe.format.pe.imports.*;
import libexe.format.pe.exports.*;

/**
 * Complete PE (Portable Executable) file format parser
 * Supports Windows EXE, DLL, SYS, and other PE-based files
 */
struct PEFile {
    // Core headers
    DOSHeader dos_header;
    PEHeader pe_header @ dos_header.e_lfanew;
    OptionalHeader optional_header;

    // Section table
    SectionTable section_table(pe_header.number_of_sections);

    // Data directories (conditional based on count)
    DataDirectories data_dirs(optional_header.number_of_rva_and_sizes);

    // Import directory (if present)
    if (data_dirs.has_import_directory()) {
        ImportDirectory imports @ data_dirs.import_rva;
    }

    // Export directory (if present)
    if (data_dirs.has_export_directory()) {
        ExportDirectory exports @ data_dirs.export_rva;
    }
};
```

## Best Practices Summary

1. **Keep files focused** - 100-300 lines per file ideal
2. **Use descriptive package names** - `libexe.format.pe.imports` not `libexe.p1`
3. **Create shared type libraries** - Reuse common patterns across formats
4. **Organize by logical grouping** - Group related structures together
5. **Use imports strategically** - Wildcard for closely related, explicit for clarity
6. **Document package purpose** - Add docstrings to main structures
7. **Maintain consistent hierarchy** - Use parallel structure for similar formats

## Anti-Patterns to Avoid

❌ **Don't create "god files"** - Avoid 2000+ line single files
```
bad/
└── everything.ds  // 5000 lines - impossible to maintain
```

❌ **Don't use flat structure for complex formats**
```
bad/
├── pe_headers.ds
├── pe_imports.ds
├── pe_exports.ds
├── elf_headers.ds
├── elf_sections.ds
└── ... 50 more files at root
```

❌ **Don't over-modularize simple formats**
```
overkill/
└── bmp/
    ├── header.ds          // 10 lines
    ├── info_header.ds     // 15 lines
    ├── color_table.ds     // 8 lines
    └── pixel_data.ds      // 5 lines

# Better as single file:
bmp.ds  // 50 lines total
```

## Migration from Monolithic Files

If you have a large existing file (e.g., 2000 lines), refactor incrementally:

1. **Identify logical boundaries** - Find natural groupings
2. **Extract one module at a time** - Don't refactor everything at once
3. **Test after each extraction** - Ensure nothing breaks
4. **Update imports** - Add imports as you extract modules

### Example Migration

**Before: `pe.ds` (2000 lines)**
```datascript
package libexe.format.pe;

struct DOSHeader { ... }      // Lines 1-50
struct PEHeader { ... }       // Lines 51-100
struct SectionHeader { ... }  // Lines 101-150
struct ImportDescriptor { ... } // Lines 151-200
// ... 1800 more lines
```

**After: Split into modules**
```
libexe/format/
├── pe.ds (main orchestrator, 100 lines)
└── pe/
    ├── headers.ds (200 lines - DOS + PE headers)
    ├── sections.ds (150 lines - sections)
    └── imports.ds (180 lines - imports)
```

## Conclusion

DataScript's module system encourages clear, maintainable organization through:
- Explicit package-to-file mapping
- Hierarchical namespace organization
- Targeted imports for clarity

For large schemas, use hierarchical packages to break down complexity while maintaining clear relationships between components.
