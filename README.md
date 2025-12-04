# DataScript

A modern compiler for binary data format specifications. DataScript lets you describe binary structures in a declarative language and generates production-ready parsing code in multiple languages.

## Why DataScript?

### The Problem

Parsing binary data in C++ typically means writing tedious, error-prone code with manual pointer arithmetic, endianness handling, and boundary checks. Each file format or protocol requires hundreds of lines of boilerplate that's hard to maintain and easy to get wrong.

### The Solution

DataScript lets you specify binary formats declaratively. The compiler generates efficient, type-safe C++ code that handles all the low-level details correctly. Your specification becomes both documentation and implementation.

## DataScript vs Kaitai Struct

If you're familiar with Kaitai Struct, here's why you might choose DataScript:

### Superior C++ Code Generation

**DataScript:**
- Generates modern C++20 code without runtime library dependencies
- Single-header output that you can vendor directly into your codebase
- Produces idiomatic C++ with `std::vector`, `std::string`, proper RAII
- Optional library mode with runtime introspection for tooling
- Every feature optimized for the C++ ecosystem (move semantics, RAII, templates)

**Kaitai Struct:**
- Generated C++ code requires linking against Kaitai runtime library
- Generated code quality optimized for multi-language support, not C++ specifically

### Native C++ Focus (with more languages planned)

The current C++ backend is production-ready. The compiler architecture supports multiple backends:
- **C++20** - Production ready, fully featured
- **Go** - Planned
- **Python** - Planned
- **Java** - Planned

Each backend will generate idiomatic code for its language without compromising on quality or requiring runtime dependencies.

### Performance

Generated parsers are as fast as hand-written code:
- Zero-copy string views where possible
- Minimal allocations
- Direct memory access with bounds checking
- Compiler-friendly code that optimizes well

## Quick Example

### Define Your Format

```datascript
// packet.ds
struct Packet {
    uint32 magic;
    uint8 version;
    uint16 payload_length;
    uint8 payload[payload_length];
    uint32 checksum;
}
```

### Generate C++ Code

```bash
ds packet.ds -t cpp -o generated/
```

### Use It

```cpp
#include "packet.h"

int main() {
    std::vector<uint8_t> data = read_file("packet.bin");

    try {
        Packet packet = read_Packet(data.data(), data.size());

        std::cout << "Version: " << (int)packet.version << "\n";
        std::cout << "Payload: " << packet.payload.size() << " bytes\n";

        // packet.payload is std::vector<uint8_t>
        // packet.checksum is uint32_t
        // Everything is type-safe and idiomatic C++

    } catch (const parse_error& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
    }
}
```

## Language Features

### Type System

- **Integers**: `int8` through `int128`, `uint8` through `uint128`
- **Bit fields**: `bit:3` for fixed-width, `bit<expr>` for computed widths
- **Strings**: Null-terminated `string` type
- **Booleans**: Native `bool` support
- **Arrays**: Fixed-size, variable-size, and ranged arrays
- **Enums & Bitmasks**: Type-safe enumerations with underlying type control
- **Structs**: Composite types with field ordering
- **Unions**: Overlapping data representations
- **Choices**: Tagged unions with discriminator fields

### Advanced Features

**Parameterized Types** (Generics):
```datascript
struct Buffer(uint16 capacity) {
    uint8 data[capacity];
}

struct Message {
    Buffer(256) small;   // Buffer_256 generated
    Buffer(4096) large;  // Buffer_4096 generated
}
```

**Conditional Fields**:
```datascript
struct Header {
    uint8 flags;
    uint32 extended_data if flags & 0x01;  // Present only if flag set
}
```

**Constraints & Validation**:
```datascript
subtype UserID: uint32 : value > 0;        // Runtime validation
subtype Port: uint16 : value >= 1024;      // Reject invalid data
```

**Member Functions**:
```datascript
struct Point {
    int16 x;
    int16 y;

    function uint32 distance_squared() {
        return x * x + y * y;
    }
}
```

**Labels & Seeking**:
```datascript
struct File {
    uint32 data_offset;
    data_offset:              // Seek to position
    uint8 data[100];
}
```

**Alignment**:
```datascript
struct Aligned {
    uint8 flag;
    align(4):                 // Pad to 4-byte boundary
    uint32 value;
}
```

**Endianness Control**:
```datascript
little;  // Set global default

struct Header {
    little uint32 signature;  // Force little-endian
    big uint16 count;         // Force big-endian
}
```

### Module System

```datascript
package net.protocol;

import common.types;        // Import single module
import net.base.*;          // Import all modules in package

struct Packet {
    common.types.Timestamp ts;
    // ...
}
```

## Real-World Examples

DataScript handles complex binary formats found in production systems:

### Windows PE Executables

```datascript
struct DOSHeader {
    uint16 signature: signature == 0x5A4D;  // "MZ"
    uint16 bytes_in_last_block;
    uint16 blocks_in_file;
    // ... 30+ fields
    uint32 pe_offset;
}

struct NTHeaders {
    uint32 signature: signature == 0x4550;   // "PE\0\0"
    FileHeader file_header;

    choice OptionalHeader on file_header.magic {
        case 0x10B: PE32Header header32;
        case 0x20B: PE32PlusHeader header64;
    }
}
```

### Network Protocols

```datascript
struct TCPPacket {
    uint16 src_port;
    uint16 dst_port;
    uint32 sequence;
    uint32 acknowledgment;
    bit:4 data_offset;
    bit:3 reserved;
    bit:1 ns;
    bit:1 cwr;
    bit:1 ece;
    bit:1 urg;
    bit:1 ack;
    bit:1 psh;
    bit:1 rst;
    bit:1 syn;
    bit:1 fin;
    uint16 window_size;
    uint16 checksum;
    uint16 urgent_pointer;
    uint8 options[(data_offset * 4) - 20] if data_offset > 5;
}
```

### File Formats

```datascript
struct PNGChunk {
    big uint32 length;
    uint8 type[4];
    uint8 data[length];
    big uint32 crc;
}

struct BMPFileHeader {
    uint16 signature: signature == 0x4D42;  // "BM"
    uint32 file_size;
    uint16 reserved1;
    uint16 reserved2;
    uint32 pixel_data_offset;
}

struct BMPInfoHeader {
    uint32 header_size;
    int32 width;
    int32 height;
    uint16 planes;
    uint16 bits_per_pixel;
    uint32 compression;
    uint32 image_size;
    int32 x_pixels_per_meter;
    int32 y_pixels_per_meter;
    uint32 colors_used;
    uint32 important_colors;
}
```

## Code Generation Modes

### Single-Header Mode (Default)

Generates one `.h` file with everything you need:

```bash
ds format.ds -t cpp -o output/
```

Best for production deployments:
- Fast compilation
- Small binary footprint
- Standalone code you can vendor directly

### Library Mode (Optional)

Generates three headers with runtime introspection support:

```bash
ds format.ds -t cpp --cpp-mode=library -o output/
```

Adds dynamic inspection capabilities for debugging and tooling:
- Query struct metadata at runtime
- Iterate fields dynamically
- Export to JSON
- Pretty-print structures

Use this mode when building parsers, analyzers, or debugging tools.

## Building

Requirements:
- CMake 3.20+
- C++20 compiler (GCC 10+, Clang 12+, MSVC 2019+)

```bash
git clone https://github.com/yourusername/datascript.git
cd datascript
mkdir build && cd build
cmake ..
cmake --build .

# Compiler is now in ds/ds
./ds/ds --help
```

### Build Options

```bash
# Build with unit tests (default: OFF)
cmake -DDATASCRIPT_BUILD_TESTS=ON ..
cmake --build .
./unittest/datascript_unittest
```

### Dependencies

Build-time dependencies are fetched automatically by CMake:
- re2c (lexer generator)
- lemon (parser generator)
- doctest (test framework, only if DATASCRIPT_BUILD_TESTS=ON)

## Using DataScript in Your Project

You can integrate DataScript into your CMake project using `FetchContent` to automatically download, build, and generate C++ headers from your schemas.

### Complete Example

**Directory structure:**
```
my_project/
├── CMakeLists.txt
├── schemas/
│   └── packet.ds          # Your DataScript schema
└── src/
    └── main.cpp           # Your application
```

**schemas/packet.ds:**
```datascript
struct Packet {
    uint32 magic;
    uint8 version;
    uint16 length;
    uint8 data[length];
    uint32 checksum;
}
```

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.20)
project(MyProject)

set(CMAKE_CXX_STANDARD 20)

# Fetch DataScript compiler
include(FetchContent)
FetchContent_Declare(
    datascript
    GIT_REPOSITORY https://github.com/devbrain/datascript.git
    GIT_TAG        master  # or specific version tag
)
FetchContent_MakeAvailable(datascript)

# Generate C++ headers from DataScript schemas
set(SCHEMA_DIR ${CMAKE_SOURCE_DIR}/schemas)
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)

file(MAKE_DIRECTORY ${GENERATED_DIR})

# Generate header for each schema
add_custom_command(
    OUTPUT ${GENERATED_DIR}/packet.h
    COMMAND $<TARGET_FILE:ds> ${SCHEMA_DIR}/packet.ds -t cpp -o ${GENERATED_DIR}
    DEPENDS ds ${SCHEMA_DIR}/packet.ds
    COMMENT "Generating C++ header from packet.ds"
)

# Custom target to ensure generation happens
add_custom_target(generate_schemas ALL
    DEPENDS ${GENERATED_DIR}/packet.h
)

# Your application
add_executable(myapp src/main.cpp)

# Link against generated headers
target_include_directories(myapp PRIVATE ${GENERATED_DIR})
add_dependencies(myapp generate_schemas)
```

**src/main.cpp:**
```cpp
#include <iostream>
#include <fstream>
#include <vector>
#include "packet.h"

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

int main() {
    try {
        auto data = read_file("packet.bin");
        Packet packet = read_Packet(data.data(), data.size());

        std::cout << "Magic: 0x" << std::hex << packet.magic << "\n";
        std::cout << "Version: " << std::dec << (int)packet.version << "\n";
        std::cout << "Length: " << packet.length << "\n";
        std::cout << "Data: " << packet.data.size() << " bytes\n";
        std::cout << "Checksum: 0x" << std::hex << packet.checksum << "\n";

    } catch (const parse_error& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

**Build and run:**
```bash
mkdir build && cd build
cmake ..
cmake --build .
./myapp
```

### Using Library Mode

For library mode with introspection, modify the generation command:

```cmake
add_custom_command(
    OUTPUT ${GENERATED_DIR}/packet_runtime.h
           ${GENERATED_DIR}/packet.h
           ${GENERATED_DIR}/packet_impl.h
    COMMAND $<TARGET_FILE:ds> ${SCHEMA_DIR}/packet.ds
            -t cpp --cpp-mode=library -o ${GENERATED_DIR}
    DEPENDS ds ${SCHEMA_DIR}/packet.ds
    COMMENT "Generating C++ library mode headers from packet.ds"
)
```

Then include `packet_impl.h` instead of `packet.h` in your code to get introspection support.

### Multiple Schemas

For projects with multiple schemas:

```cmake
set(SCHEMAS packet message header)

foreach(SCHEMA ${SCHEMAS})
    add_custom_command(
        OUTPUT ${GENERATED_DIR}/${SCHEMA}.h
        COMMAND $<TARGET_FILE:ds> ${SCHEMA_DIR}/${SCHEMA}.ds
                -t cpp -o ${GENERATED_DIR}
        DEPENDS ds ${SCHEMA_DIR}/${SCHEMA}.ds
        COMMENT "Generating ${SCHEMA}.h"
    )
    list(APPEND GENERATED_HEADERS ${GENERATED_DIR}/${SCHEMA}.h)
endforeach()

add_custom_target(generate_schemas ALL
    DEPENDS ${GENERATED_HEADERS}
)
```

### Notes

- DataScript compiler is built automatically by FetchContent
- Generated headers are created during build, not at configure time
- Headers are regenerated when schemas change
- No need to commit generated files to version control
- Use `GIT_TAG` to pin to a specific version for reproducible builds

## Compiler Architecture

DataScript uses a classic compiler pipeline optimized for correctness and performance:

**Lexer** (re2c):
- DFA-based scanner
- Zero-copy tokenization
- Minimal backtracking

**Parser** (Lemon/LALR):
- Same parser generator as SQLite
- Type-safe semantic values
- Excellent error recovery

**Semantic Analysis** (7 phases):
1. Symbol table construction
2. Name resolution
3. Type checking
4. Constant expression evaluation
5. Size calculation
6. Constraint validation
7. Reachability analysis

**IR Generation**:
- Language-agnostic intermediate representation
- Command-stream architecture
- No target language details in IR

**Code Generation**:
- Pluggable backend architecture with language-agnostic IR
- C++20 backend: Production ready
- Planned backends: Go, Python, Java
- Each backend generates idiomatic code without runtime dependencies

## Testing

The compiler includes comprehensive tests (requires `DATASCRIPT_BUILD_TESTS=ON`):

```bash
# Configure with tests enabled
cmake -DDATASCRIPT_BUILD_TESTS=ON ..
cmake --build .

# Run tests
./unittest/datascript_unittest
```

**Current status:**
- 855 test cases, all passing
- 4,012 assertions verified
- Zero memory leaks (valgrind verified)
- Tests include real-world formats: PE/NE executables, ELF, PNG, ZIP, network protocols

## Documentation

- **Language Guide**: `docs/LANGUAGE_GUIDE.md` - Complete language reference
- **C++ Code Generation**: `docs/CPP_CODE_GENERATION.md` - Generated code API
- **ABNF Specification**: `docs/datascript.abnf` - Formal grammar
- **IR & Codegen**: `docs/IR_AND_CODEGEN.md` - Compiler internals

## Use Cases

DataScript excels at:

- **Network Protocol Parsers**: TCP/IP, custom protocols, RPC formats
- **File Format Tools**: Parse, validate, convert binary files
- **Firmware Analysis**: Reverse engineer embedded data structures
- **Game Asset Formats**: Parse proprietary game data files
- **Legacy System Integration**: Document and parse old binary formats
- **Binary Serialization**: Alternative to Protocol Buffers/FlatBuffers for C++ projects

## Project Status

**Production Ready**

All language features are implemented and extensively tested:
- Complete parser with full language support
- 7-phase semantic analysis with validation
- C++ code generation (both modes)
- Parameterized types with monomorphization
- Module system with import resolution
- Zero memory leaks, exception-safe

Used successfully for parsing real-world binary formats including Windows executables, ELF binaries, and network protocols.

## When NOT to Use DataScript

DataScript is designed for **parsing existing binary formats with known structure**. Consider alternatives if you need:

- **Unbounded repetition**: DataScript doesn't support parsing repeated structures until EOF (no `while` loops). Array sizes must be known from a length field or fixed in the schema.
- **Immediate multi-language support**: Currently only C++20 is production-ready. Go, Python, and Java backends are planned but not yet available.
- **Schema evolution**: Use formats designed for versioning if your format changes frequently
- **Zero-copy parsing**: Use Cap'n Proto or FlatBuffers for memory-mapped data structures
- **Writing/serialization**: DataScript currently focuses on reading; use established serialization libraries for write support

## License

MIT License - see LICENSE file for details.

## Contributing

Contributions welcome. Please ensure:
- All tests pass (`./unittest/datascript_unittest`)
- No memory leaks (`valgrind --leak-check=full`)
- Code follows existing style
- New features include tests and documentation

---

Built with C++20 | Parser: re2c + Lemon | Multi-language support planned | Production ready
