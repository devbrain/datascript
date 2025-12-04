# DataScript Language Guide

**Version:** 1.0
**Last Updated:** 2025-12-03

## Table of Contents

1. [Introduction](#introduction)
2. [Basic Concepts](#basic-concepts)
3. [Primitive Types](#primitive-types)
4. [Complex Types](#complex-types)
5. [Advanced Types](#advanced-types)
6. [Type Parameters (Generics)](#type-parameters-generics)
7. [Expressions and Operators](#expressions-and-operators)
8. [Constraints and Conditions](#constraints-and-conditions)
9. [Functions](#functions)
10. [Labels and Alignment](#labels-and-alignment)
11. [Endianness](#endianness)
12. [Constants and Enumerations](#constants-and-enumerations)
13. [Packages and Imports](#packages-and-imports)
14. [Documentation Comments](#documentation-comments)
15. [Complete Examples](#complete-examples)

---

## Introduction

DataScript is a domain-specific language for describing binary file formats and network protocols. It allows you to:

- **Declaratively specify** the structure of binary data
- **Automatically generate** parser code in multiple languages (C++, etc.)
- **Validate data** using constraints and type checking
- **Navigate complex formats** with labels and alignment
- **Handle different byte orders** with endianness modifiers

### Design Philosophy

- **Declarative**: Describe *what* the format looks like, not *how* to parse it
- **Type-safe**: Strong typing catches errors at compile time
- **Self-documenting**: Built-in documentation comments
- **Zero-cost abstractions**: Generated code is as efficient as hand-written parsers

---

## Basic Concepts

### Source Files

DataScript source files use the `.ds` extension:

```datascript
// myformat.ds
package com.example.formats;

import com.example.common.*;

struct MyFormat {
    uint32 magic;
    uint16 version;
};
```

### Comments

DataScript supports three types of comments:

```datascript
// Line comment - runs to end of line

/* Block comment
   spans multiple lines */

/**
 * Documentation comment (Javadoc-style)
 * Attached to the following declaration
 */
struct Documented {
    uint32 field;
};
```

### Identifiers

Identifiers must start with a letter or underscore, followed by letters, digits, or underscores:

```datascript
// Valid identifiers
struct MyStruct { }
uint32 _private_field;
uint8 field123;

// Invalid identifiers
// 123field  - starts with digit
// my-field  - contains hyphen
```

---

## Primitive Types

### Integer Types

DataScript supports signed and unsigned integers of various sizes:

| Type | Size | Range (Signed) | Range (Unsigned) |
|------|------|----------------|------------------|
| `int8` / `uint8` | 1 byte | -128 to 127 | 0 to 255 |
| `int16` / `uint16` | 2 bytes | -32,768 to 32,767 | 0 to 65,535 |
| `int32` / `uint32` | 4 bytes | -2³¹ to 2³¹-1 | 0 to 2³²-1 |
| `int64` / `uint64` | 8 bytes | -2⁶³ to 2⁶³-1 | 0 to 2⁶⁴-1 |

**Example:**

```datascript
struct AllIntegers {
    int8   signed_byte;
    uint8  unsigned_byte;
    int16  signed_word;
    uint16 unsigned_word;
    int32  signed_dword;
    uint32 unsigned_dword;
    int64  signed_qword;
    uint64 unsigned_qword;
};
```

### String Type

The `string` type represents null-terminated UTF-8 strings:

```datascript
struct Message {
    string sender;
    string recipient;
    string body;
};
```

**Binary Format:** Strings are read until a null byte (`0x00`) is encountered.

### Boolean Type

The `bool` type represents boolean values (stored as 1 byte):

```datascript
struct Flags {
    bool enabled;
    bool visible;
    bool readonly;
};
```

**Binary Format:** `0` = false, non-zero = true

### Bit Fields

Extract specific bits from the input stream:

```datascript
struct PackedFlags {
    bit:1 flag_a;      // 1 bit
    bit:1 flag_b;      // 1 bit
    bit:6 reserved;    // 6 bits (total: 8 bits = 1 byte)
};

struct Version {
    bit:3 major;       // 3 bits: 0-7
    bit:5 minor;       // 5 bits: 0-31
};
```

**Note:** Bit fields are read sequentially from the bit stream, MSB to LSB within each byte.

---

## Complex Types

### Structures

Structures define composite types with multiple fields:

```datascript
struct Point {
    uint16 x;
    uint16 y;
};

struct Rectangle {
    Point top_left;
    Point bottom_right;
};
```

### Arrays

#### Fixed-Size Arrays

Arrays with a compile-time constant size:

```datascript
struct FixedData {
    uint8 magic[4];        // Always 4 bytes
    uint16 values[10];     // Always 10 uint16s
};
```

#### Variable-Size Arrays

Arrays whose size is determined by a field value:

```datascript
struct VariableData {
    uint8 count;           // Number of elements
    uint32 items[count];   // Variable-size array
};
```

#### Expression-Based Array Sizes

Array sizes can use arbitrary expressions:

```datascript
struct Matrix {
    uint8 rows;
    uint8 cols;
    uint32 data[rows * cols];  // rows × cols elements
};
```

#### Arrays of Complex Types

```datascript
struct Point {
    uint16 x;
    uint16 y;
};

struct Path {
    uint8 num_points;
    Point points[num_points];  // Array of structures
};
```

### Enumerations

Enumerations define named constant values:

```datascript
enum uint8 Color {
    RED = 0,
    GREEN = 1,
    BLUE = 2,
    YELLOW = 3
};

enum uint16 FileType {
    UNKNOWN = 0,
    TEXT = 100,
    BINARY = 200,
    IMAGE = 300
};

struct Header {
    Color background;
    FileType type;
};
```

**Enum Scope:** Access enum values using `EnumName.VALUE`:

```datascript
function bool is_red() {
    return background == Color.RED;
}
```

---

## Advanced Types

### Unions

Unions represent alternative data formats with **runtime selection** (trial-and-error decoding):

```datascript
const uint16 TYPE_A = 1;
const uint16 TYPE_B = 2;

struct RecordA {
    uint16 value_a;
};

struct RecordB {
    uint32 value_b;
};

struct Container {
    uint16 record_type;

    union {
        RecordA a_record : record_type == TYPE_A;
        RecordB b_record : record_type == TYPE_B;
    } data;
};
```

**Union Semantics:**

- Decoder tries each branch sequentially
- If a constraint fails, tries the next branch
- If all branches have constraints and all fail, union remains **empty** (optional union with `std::monostate`)
- If any branch has no constraint, it acts as a **default case**

**Anonymous Block Syntax:**

```datascript
union {
    // Single field case
    RecordA a_record : record_type == TYPE_A;

    // Multi-field case (anonymous block)
    {
        uint32 signature;
        uint32 data_size;
        uint8 payload[data_size];
    } b_record : signature == 0x12345678;
} variant_data;
```

### Choices (Tagged Unions)

Choices represent **compile-time tagged unions** with a selector expression:

```datascript
const uint8 MSG_TEXT = 1;
const uint8 MSG_NUMBER = 2;
const uint8 MSG_BINARY = 3;

choice MessagePayload on msg_type {
    case MSG_TEXT:
        uint8 text_length;
        uint8 text_data[text_length];
    case MSG_NUMBER:
        uint32 number_value;
    case MSG_BINARY:
        uint16 binary_size;
        uint8 binary_data[binary_size];
    default:
        uint8 unknown_data;
};

struct Message {
    uint8 msg_type;
    MessagePayload payload;
};
```

**Choice vs Union:**

- **Choice**: Selector determines which branch at compile time (efficient switch statement)
- **Union**: Try each branch at runtime until one succeeds (flexible but slower)

### Subtypes

Subtypes are constrained base types that validate values:

```datascript
// User ID must be positive
subtype uint16 UserID : this > 0;

// Port number must be above 1024
subtype uint16 Port : this > 1024;

// Percentage must be 0-100
subtype uint8 Percentage : this <= 100;

struct Connection {
    UserID user;
    Port port;
    Percentage reliability;  // Validated at parse time
};
```

**Validation:** If the constraint fails, parsing throws an error.

---

## Type Parameters (Generics)

DataScript supports **compile-time type specialization** with type parameters:

```datascript
/** Generic buffer with capacity parameter */
struct Buffer(uint16 capacity) {
    uint8 data[capacity];

    function uint16 get_capacity() {
        return capacity;  // Parameter in scope
    }
};

/** Multi-parameter example */
struct Matrix(uint8 rows, uint8 cols) {
    uint32 data[rows * cols];

    function uint16 get_total_size() {
        return rows * cols;
    }
};

/** Using parameterized types */
struct DataContainer {
    Buffer(16) small_buffer;    // Generates Buffer_16
    Buffer(256) large_buffer;   // Generates Buffer_256
    Buffer(16) another_small;   // Reuses Buffer_16

    Matrix(3, 3) transform;     // Generates Matrix_3_3
    Matrix(4, 4) projection;    // Generates Matrix_4_4
};
```

**Monomorphization:**

- Each unique instantiation generates a concrete type
- Identical instantiations are cached and reused
- Zero runtime overhead - all specialization at compile time
- Base parameterized types are NOT included in generated code

---

## Expressions and Operators

### Arithmetic Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `a + b` |
| `-` | Subtraction | `a - b` |
| `*` | Multiplication | `a * b` |
| `/` | Division | `a / b` |
| `%` | Modulo | `a % b` |
| `-` (unary) | Negation | `-value` |

### Bitwise Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&` | Bitwise AND | `flags & 0x01` |
| `|` | Bitwise OR | `flags | 0x02` |
| `^` | Bitwise XOR | `a ^ b` |
| `~` | Bitwise NOT | `~flags` |
| `<<` | Left shift | `value << 8` |
| `>>` | Right shift | `value >> 4` |

### Comparison Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `==` | Equal to | `type == 1` |
| `!=` | Not equal to | `flags != 0` |
| `<` | Less than | `value < 100` |
| `<=` | Less than or equal | `count <= max` |
| `>` | Greater than | `size > 0` |
| `>=` | Greater than or equal | `version >= 2` |

### Logical Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&&` | Logical AND | `(a > 0) && (b < 10)` |
| `||` | Logical OR | `(flag1) || (flag2)` |
| `!` | Logical NOT | `!enabled` |

### Ternary Operator

```datascript
result = condition ? value_if_true : value_if_false;
```

### Operator Precedence

From highest to lowest:

1. Parentheses `( )`
2. Unary: `!`, `~`, `-`, `+`
3. Multiplicative: `*`, `/`, `%`
4. Additive: `+`, `-`
5. Shift: `<<`, `>>`
6. Relational: `<`, `<=`, `>`, `>=`
7. Equality: `==`, `!=`
8. Bitwise AND: `&`
9. Bitwise XOR: `^`
10. Bitwise OR: `|`
11. Logical AND: `&&`
12. Logical OR: `||`
13. Ternary: `? :`

### Field Access and Array Indexing

```datascript
struct Header {
    uint32 offset;
};

struct Data {
    Header header;
    uint8 count;
    uint32 values[count];

    function uint32 get_offset() {
        return header.offset;      // Field access
    }

    function uint32 get_first() {
        return values[0];          // Array indexing
    }
};
```

---

## Constraints and Conditions

### Inline Field Constraints

Validate field values with the `:` syntax:

```datascript
struct Header {
    uint32 magic : magic == 0x12345678;  // Must be this value
    uint16 version : version >= 1;        // Minimum version
    uint8 flags : (flags & 0x80) == 0;    // Top bit must be clear
};
```

**Behavior:** If constraint fails, parsing throws `ConstraintViolation`.

### Conditional Fields

Fields can be conditionally present using `if`:

```datascript
struct OptionalData {
    uint8 flags;
    uint16 optional_field if (flags & 0x01) != 0;  // Only if bit 0 set
    uint32 another_field if (flags & 0x02) != 0;   // Only if bit 1 set
};

struct VersionedData {
    uint8 version;
    uint16 legacy_field if version == 1;   // Only in version 1
    uint32 modern_field if version >= 2;   // Version 2 and later
};
```

**Behavior:** If condition is false, field is skipped during parsing.

### Constraint vs Condition

| Feature | Syntax | Purpose | Failure Behavior |
|---------|--------|---------|------------------|
| **Constraint** | `field : expr` | Validate value | Throw error |
| **Condition** | `field if expr` | Optional presence | Skip field |

---

## Functions

Functions provide computed values and utility methods:

```datascript
struct Rectangle {
    uint16 width;
    uint16 height;

    /** Calculate area */
    function uint32 area() {
        return width * height;
    }

    /** Calculate perimeter */
    function uint32 perimeter() {
        return 2 * (width + height);
    }

    /** Check if square */
    function bool is_square() {
        return width == height;
    }
};
```

**Function Features:**

- Return type must be specified
- Can access struct fields directly
- Can call other functions
- Parameters supported: `function uint32 get(uint8 index) { ... }`
- Return statement required

**Example with Parameters:**

```datascript
struct Array {
    uint8 count;
    uint32 values[count];

    /** Get element at index */
    function uint32 get(uint8 index) {
        return values[index];
    }

    /** Check if contains value */
    function bool contains(uint32 value) {
        return false;  // Simplified
    }
};
```

---

## Labels and Alignment

### Labels (Struct-Relative Positioning)

Labels allow seeking to specific offsets **relative to the start of the struct** before reading fields:

```datascript
struct FileWithIndex {
    uint32 data_offset;      // Offset from start of struct
    uint8 header_data[16];   // Some header info

    data_offset:             // Seek to byte <data_offset> from struct start
    uint32 payload;          // Read data at that position
};
```

**Example:** If `data_offset = 20`, the parser seeks to byte 20 from the **beginning of the struct** (where the `FileWithIndex` read started), not byte 20 of the file.

**Binary Layout:**
```
Offset  Field           Value
0-3     data_offset     20 (seek target)
4-19    header_data     16 bytes
20-23   payload         Read from here (offset 20 from struct start)
```

**Label with Field Access:**

```datascript
struct Header {
    uint32 section_offset;  // Offset from Header's start
};

struct File {
    Header header;

    header.section_offset:   // Seek to offset within File's scope
    uint8 section_data[100];
};
```

**Important:** The offset `header.section_offset` is interpreted as **bytes from the start of the `File` struct**, not from the start of the `Header` struct.

**Multiple Labels:**

```datascript
struct MultiSection {
    uint32 offset1;  // All offsets relative to MultiSection start
    uint32 offset2;
    uint32 offset3;

    offset1:         // Seek to byte <offset1> from struct start
    uint16 data1;

    offset2:         // Seek to byte <offset2> from struct start
    uint32 data2;

    offset3:         // Seek to byte <offset3> from struct start
    uint64 data3;
};
```

**Key Point:** Labels implement **struct-scoped absolute positioning** - offsets are always relative to where the current struct's `read()` method started, not the beginning of the file or parent struct.

### Alignment (Padding)

Align read position to specific byte boundaries:

```datascript
struct AlignedData {
    uint8 header;           // 1 byte

    align(4):               // Pad to 4-byte boundary
    uint32 aligned_dword;   // Guaranteed aligned

    align(8):               // Pad to 8-byte boundary
    uint64 aligned_qword;   // Guaranteed aligned
};
```

**Common Alignments:**

- `align(2)`: 16-bit (word) alignment
- `align(4)`: 32-bit (dword) alignment
- `align(8)`: 64-bit (qword) alignment
- `align(16)`: 128-bit alignment (SIMD)

### Combining Labels and Alignment

```datascript
struct Complex {
    uint32 data_offset;

    data_offset:            // Seek to offset
    uint8 unaligned_byte;

    align(8):               // Then align
    uint64 aligned_value;
};
```

---

## Endianness

### Default (Little-Endian)

By default, multi-byte integers are little-endian:

```datascript
struct LittleEndian {
    uint16 word;    // Little-endian (default)
    uint32 dword;   // Little-endian
    uint64 qword;   // Little-endian
};
```

### Big-Endian Modifier

Use `big` keyword for big-endian fields:

```datascript
struct BigEndian {
    big uint16 word;    // Big-endian
    big uint32 dword;   // Big-endian
    big uint64 qword;   // Big-endian
};
```

### Mixed Endianness

You can mix endianness within a single struct:

```datascript
struct NetworkPacket {
    big uint16 packet_length;  // Network byte order (big-endian)
    big uint16 packet_id;      // Network byte order
    uint32 payload_size;       // Host byte order (little-endian)
    big uint32 timestamp;      // Network byte order
};
```

### Endianness with Arrays

```datascript
struct MixedArray {
    big uint16 be_values[4];   // 4 big-endian uint16s
    uint32 le_values[4];       // 4 little-endian uint32s
};
```

**Note:** Endianness only applies to multi-byte integers (uint16, uint32, uint64, int16, int32, int64).

---

## Constants and Enumerations

### Constants

Define compile-time constant values:

```datascript
// Integer constants
const uint32 MAGIC_NUMBER = 0x12345678;
const uint16 VERSION = 2;
const uint8 MAX_ITEMS = 255;

// Use in constraints
struct Header {
    uint32 magic : magic == MAGIC_NUMBER;
    uint16 version : version <= VERSION;
};

// Use in array sizes
struct Buffer {
    uint8 data[MAX_ITEMS];
};
```

**Constant Syntax:**

```datascript
const <type> <name> = <value>;
```

### Enumerations (Revisited)

Enumerations are named sets of constants:

```datascript
/** File type enumeration */
enum uint8 FileType {
    UNKNOWN = 0,
    TEXT = 1,
    BINARY = 2,
    IMAGE = 3,
    VIDEO = 4
};

/** Permission flags */
enum uint8 Permission {
    READ = 0x01,
    WRITE = 0x02,
    EXECUTE = 0x04,
    ADMIN = 0x08
};

struct FileHeader {
    FileType type;
    Permission perms;

    /** Check if readable */
    function bool can_read() {
        return (perms & Permission.READ) != 0;
    }
};
```

---

## Packages and Imports

### Package Declaration

Declare package namespace at the top of file:

```datascript
package com.example.formats;

// Rest of file...
```

**Package Names:**

- Use reverse-domain notation: `com.example.project`
- Maps to directory structure: `com/example/project/filename.ds`

### Import Statements

Import types from other modules:

```datascript
// Import specific package
import com.example.common.types;

// Import all from package (wildcard)
import com.example.utils.*;
```

**Import Resolution:**

1. Main script directory
2. User-provided search paths
3. Current working directory
4. `DATASCRIPT_PATH` environment variable

**Module Loading:**

- Circular imports are detected and rejected
- Modules are loaded once and cached
- Transitive imports are resolved automatically

### Complete Example

**File: `com/example/common/types.ds`**

```datascript
package com.example.common.types;

struct Point {
    uint16 x;
    uint16 y;
};

const uint32 MAGIC = 0x12345678;
```

**File: `com/example/formats/shape.ds`**

```datascript
package com.example.formats.shape;

import com.example.common.types;

struct Rectangle {
    types.Point top_left;
    types.Point bottom_right;

    uint32 magic : magic == types.MAGIC;
};
```

---

## Documentation Comments

### Javadoc-Style Comments

Use `/** ... */` for documentation:

```datascript
/**
 * RGB color value stored as three bytes.
 * Each component ranges from 0 to 255.
 */
struct Color {
    uint8 red;
    uint8 green;
    uint8 blue;
};

/**
 * Network packet header.
 * All fields are in network byte order (big-endian).
 */
struct PacketHeader {
    /** Packet size in bytes */
    big uint16 size;

    /** Unique packet identifier */
    big uint32 id;

    /** Timestamp in milliseconds */
    big uint64 timestamp;
};
```

### Supported Locations

Documentation comments can be attached to:

- Constants
- Type definitions (struct, union, enum, choice)
- Fields
- Enum items
- Functions

### Formatting

```datascript
/**
 * Multi-line documentation.
 *
 * Leading asterisks are automatically stripped.
 *
 * @param value The input value
 * @return Calculated result
 */
function uint32 calculate(uint16 value) {
    return value * 2;
}
```

---

## Complete Examples

### Example 1: Network Protocol

```datascript
/**
 * Simple network protocol with request/response messages
 */
package com.example.protocol;

// Message type constants
const uint8 MSG_PING = 0x01;
const uint8 MSG_PONG = 0x02;
const uint8 MSG_DATA = 0x03;

/** Message header (common to all messages) */
struct MessageHeader {
    big uint32 magic : magic == 0x50525445;  // "PRTE"
    uint8 message_type;
    big uint16 payload_size;
};

/** Ping message (no payload) */
struct PingMessage {
    MessageHeader header : header.message_type == MSG_PING;
};

/** Pong message (timestamp response) */
struct PongMessage {
    MessageHeader header : header.message_type == MSG_PONG;
    big uint64 timestamp;
};

/** Data message (variable-size payload) */
struct DataMessage {
    MessageHeader header : header.message_type == MSG_DATA;
    uint8 payload[header.payload_size];
};

/** Generic message container */
struct Message {
    MessageHeader header;

    // Union tries each message type
    union {
        PingMessage ping : header.message_type == MSG_PING;
        PongMessage pong : header.message_type == MSG_PONG;
        DataMessage data : header.message_type == MSG_DATA;
    } body;
};
```

### Example 2: File Format with Index

```datascript
/**
 * Custom file format with header, index, and data sections
 */
package com.example.fileformat;

const uint32 FILE_MAGIC = 0x46494C45;  // "FILE"

/** File header at offset 0 */
struct FileHeader {
    uint32 magic : magic == FILE_MAGIC;
    uint16 version;
    uint16 num_entries;
    uint32 index_offset;
    uint32 data_offset;
};

/** Index entry describing a data section */
struct IndexEntry {
    uint32 section_offset;  // Absolute offset to section data
    uint32 section_size;
    uint8 section_type;
};

/** Complete file structure */
struct File {
    FileHeader header;

    // Seek to index and read entries
    header.index_offset:
    IndexEntry index[header.num_entries];

    // Data section starts here
    header.data_offset:
    uint8 data_start;  // Marker for data section start

    /** Get total file size */
    function uint32 get_size() {
        return header.data_offset + 1024;  // Simplified
    }
};
```

### Example 3: Parameterized Ring Buffer

```datascript
/**
 * Generic ring buffer with compile-time size
 */
package com.example.containers;

/** Ring buffer with parameterized capacity */
struct RingBuffer(uint16 capacity) {
    uint16 read_pos;
    uint16 write_pos;
    uint8 data[capacity];

    /** Get buffer capacity */
    function uint16 get_capacity() {
        return capacity;
    }

    /** Get number of bytes available to read */
    function uint16 available() {
        return (write_pos - read_pos) % capacity;
    }

    /** Check if buffer is empty */
    function bool is_empty() {
        return read_pos == write_pos;
    }

    /** Check if buffer is full */
    function bool is_full() {
        return available() == capacity - 1;
    }
};

/** Container with different buffer sizes */
struct BufferManager {
    RingBuffer(32) small;     // 32-byte buffer
    RingBuffer(256) medium;   // 256-byte buffer
    RingBuffer(4096) large;   // 4KB buffer
};
```

### Example 4: Windows Executable (Simplified)

```datascript
/**
 * Simplified Windows PE executable format
 */
package com.example.exe;

const uint16 DOS_MAGIC = 0x5A4D;  // "MZ"
const uint32 PE_SIGNATURE = 0x00004550;  // "PE\0\0"

/** DOS header (compatibility stub) */
struct ImageDosHeader {
    uint16 e_magic : e_magic == DOS_MAGIC;
    uint16 e_cblp;
    uint16 e_cp;
    // ... other DOS header fields ...
    uint32 e_lfanew;  // Offset to PE header
};

/** PE file header */
struct ImageFileHeader {
    uint16 Machine;
    uint16 NumberOfSections;
    uint32 TimeDateStamp;
    uint32 PointerToSymbolTable;
    uint32 NumberOfSymbols;
    uint16 SizeOfOptionalHeader;
    uint16 Characteristics;
};

/** Section header */
struct ImageSectionHeader {
    uint8 Name[8];
    uint32 VirtualSize;
    uint32 VirtualAddress;
    uint32 SizeOfRawData;
    uint32 PointerToRawData;
    uint32 PointerToRelocations;
    uint32 PointerToLinenumbers;
    uint16 NumberOfRelocations;
    uint16 NumberOfLinenumbers;
    uint32 Characteristics;
};

/** Complete executable */
struct Executable {
    ImageDosHeader dos_header;

    // Seek to PE header (offset relative to Executable start)
    // e_lfanew contains byte offset from start of Executable struct
    dos_header.e_lfanew:
    uint32 signature : signature == PE_SIGNATURE;
    ImageFileHeader file_header;

    // Read section headers
    ImageSectionHeader sections[file_header.NumberOfSections];
};
```

**Note:** The label `dos_header.e_lfanew:` seeks to a position **relative to the start of the `Executable` struct**. If `e_lfanew = 64`, the parser reads from byte 64 of the executable file (assuming the `Executable` struct is at file offset 0).

### Example 5: Tagged Union with Choice

```datascript
/**
 * Network packet with different payload types
 */
package com.example.network;

// Packet type constants
const uint8 PKT_HEARTBEAT = 0;
const uint8 PKT_REQUEST = 1;
const uint8 PKT_RESPONSE = 2;
const uint8 PKT_ERROR = 3;

/** Packet payload based on type */
choice PacketPayload on packet_type {
    case PKT_HEARTBEAT:
        big uint64 timestamp;

    case PKT_REQUEST:
        big uint32 request_id;
        uint8 method_length;
        uint8 method[method_length];

    case PKT_RESPONSE:
        big uint32 request_id;
        big uint16 status_code;
        big uint32 data_length;
        uint8 data[data_length];

    case PKT_ERROR:
        big uint16 error_code;
        uint8 message_length;
        uint8 message[message_length];

    default:
        uint8 unknown_payload;
};

/** Network packet */
struct Packet {
    big uint32 magic : magic == 0x504B5400;  // "PKT\0"
    uint8 packet_type;
    PacketPayload payload;

    /** Check if error packet */
    function bool is_error() {
        return packet_type == PKT_ERROR;
    }
};
```

---

## Best Practices

### 1. Use Descriptive Names

```datascript
// Good
struct FileHeader { ... }
uint32 section_count;

// Bad
struct FH { ... }
uint32 sc;
```

### 2. Document Public APIs

```datascript
/**
 * Represents a color in RGB format.
 * Each component is a byte (0-255).
 */
struct Color {
    uint8 red;
    uint8 green;
    uint8 blue;
};
```

### 3. Use Constants for Magic Numbers

```datascript
// Good
const uint32 MAGIC = 0x12345678;
uint32 header : header == MAGIC;

// Bad
uint32 header : header == 0x12345678;
```

### 4. Validate Critical Fields

```datascript
struct Header {
    uint32 magic : magic == EXPECTED_MAGIC;
    uint16 version : version >= MIN_VERSION;
    uint8 flags : (flags & RESERVED_MASK) == 0;
};
```

### 5. Use Subtypes for Domain Constraints

```datascript
// Good
subtype uint16 Port : this > 1024;
struct Connection {
    Port server_port;  // Validated automatically
};

// Less ideal
struct Connection {
    uint16 server_port : server_port > 1024;  // Constraint here
};
```

### 6. Prefer Choices for Known Tag Values

```datascript
// When tag is known at parse time, use choice
choice Payload on type {
    case TYPE_A: FieldsA;
    case TYPE_B: FieldsB;
};

// When detection requires trial-and-error, use union
union {
    TypeA a : a.signature == SIGNATURE_A;
    TypeB b : b.signature == SIGNATURE_B;
} variant_data;
```

---

## Language Reference Summary

### Keywords

```
align       big         bit         bool        case
choice      const       default     enum        function
if          import      int8        int16       int32
int64       little      on          package     return
string      struct      subtype     this        union
uint8       uint16      uint32      uint64
```

### Operators (by precedence)

```
()                      // Grouping
! ~ - +                 // Unary
* / %                   // Multiplicative
+ -                     // Additive
<< >>                   // Shift
< <= > >=               // Relational
== !=                   // Equality
&                       // Bitwise AND
^                       // Bitwise XOR
|                       // Bitwise OR
&&                      // Logical AND
||                      // Logical OR
? :                     // Ternary
```

### Built-in Types

```
int8, int16, int32, int64       // Signed integers
uint8, uint16, uint32, uint64   // Unsigned integers
bool                             // Boolean
string                           // Null-terminated UTF-8
bit:<N>                          // N-bit field (1-64)
```

---

## Appendix: Grammar Quick Reference

### Type Definitions

```datascript
struct <name> [(<params>)] { <fields> };
union <name> { <cases> };
choice <name> on <expr> { <cases> };
enum <type> <name> { <items> };
subtype <base> <name> : <constraint>;
```

### Field Definition

```datascript
[<endian>] <type> <name> [<array>] [: <constraint>] [if <condition>] ;
<label>:
align(<N>):
```

### Function Definition

```datascript
function <return-type> <name>([<params>]) {
    <statements>
}
```

### Expressions

```datascript
<field>                          // Field access
<struct>.<field>                 // Nested field
<array>[<index>]                 // Array indexing
<enum>.<value>                   // Enum value
<expr> <op> <expr>               // Binary operation
<op> <expr>                      // Unary operation
<expr> ? <expr> : <expr>         // Ternary
(<expr>)                         // Grouping
```

---

## Additional Resources

- **ABNF Grammar**: See `datascript.abnf` for complete syntax specification
- **Language Overview PDF**: `DataScriptLanguageOverview.pdf`
- **Full Specification PDF**: `DataScript-_A_Specification_and_Scripting_Language.pdf`
- **Example Schemas**: `unittest/codegen/schemas/*.ds`

---

**End of Language Guide**

