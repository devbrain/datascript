# DataScript Parser Testing

This directory contains unit tests for the DataScript parser using the [doctest](https://github.com/doctest/doctest) testing framework.

## Running Tests

### Build and Run All Tests

```bash
cd build
cmake --build . --target datascript_unittest
./unittest/datascript_unittest
```

### Run Specific Test Suites

```bash
# Run only tests from a specific suite
./unittest/datascript_unittest --test-suite="Integer Literal Parsing"
./unittest/datascript_unittest --test-suite="Parser - Basic Functionality"

# Run a specific test case
./unittest/datascript_unittest --test-case="*decimal literals*"
```

### Useful Command Line Options

```bash
# List all test cases without running them
./unittest/datascript_unittest --list-test-cases

# Show detailed output
./unittest/datascript_unittest -d

# Stop after first failure
./unittest/datascript_unittest --abort-after=1

# Show only failures
./unittest/datascript_unittest --success=false
```

## Test Structure

### File Organization

- `main.cc` - Entry point with doctest main() function
- `test_integer_literals.cc` - Tests for integer literal parsing
- `test_parser_basic.cc` - Basic parser functionality tests
- Additional test files can be added by creating `test_*.cc` files and adding them to `CMakeLists.txt`

### Test Auto-Registration

Doctest uses **automatic test registration**. Simply define test cases using the `TEST_CASE` macro and they will be automatically discovered and run:

```cpp
#include <doctest/doctest.h>
#include <datascript/parser.hh>

TEST_CASE("My test description") {
    auto mod = parse_datascript(std::string("const uint32 X = 1;"));
    CHECK(mod.constants.size() == 1);
}
```

**Important**: Always wrap string literals in `std::string()` when passing to `parse_datascript()` to avoid ambiguity with the `std::filesystem::path` overload:

```cpp
// ❌ WRONG - ambiguous
auto mod = parse_datascript("const uint32 X = 1;");

// ✅ CORRECT
auto mod = parse_datascript(std::string("const uint32 X = 1;"));
```

### Test Suites

Group related tests using `TEST_SUITE`:

```cpp
TEST_SUITE("Feature Name") {
    TEST_CASE("Test 1") {
        // ...
    }

    TEST_CASE("Test 2") {
        // ...
    }
}
```

## Writing Tests

### Basic Assertions

```cpp
CHECK(expr);                  // Non-fatal check
REQUIRE(expr);                // Fatal check (stops test on failure)
CHECK_FALSE(expr);            // Check that expr is false
REQUIRE_FALSE(expr);          // Require that expr is false
```

### Exception Testing

```cpp
CHECK_THROWS(parse_datascript(std::string("invalid")));
CHECK_THROWS_AS(some_func(), std::runtime_error);
CHECK_NOTHROW(valid_func());
```

### Comparison Assertions

```cpp
CHECK(a == b);
CHECK(a != b);
CHECK(a < b);
CHECK(a <= b);
CHECK(a > b);
CHECK(a >= b);
```

### Example Test

```cpp
TEST_SUITE("Parser - Primitives") {
    TEST_CASE("Parse uint32 constant") {
        auto mod = parse_datascript(std::string("const uint32 X = 42;"));

        // Check module structure
        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check type
        const auto* prim = std::get_if<ast::primitive_type>(&mod.constants[0].ctype);
        REQUIRE(prim != nullptr);
        CHECK(prim->is_signed == false);
        CHECK(prim->bits == 32);

        // Check value
        const auto* lit = std::get_if<ast::literal_int>(&mod.constants[0].value.node);
        REQUIRE(lit != nullptr);
        CHECK(lit->value == 42);
    }
}
```

## Adding New Tests

1. Create a new test file: `unittest/test_feature.cc`

2. Include necessary headers:
```cpp
#include <doctest/doctest.h>
#include <datascript/parser.hh>
// Include parser internals if needed:
#include "parser/grammar.hh"
#include "parser/actions/your_action.hh"
```

3. Write your tests using `TEST_CASE` and `TEST_SUITE` macros

4. Add the file to `unittest/CMakeLists.txt`:

```cmake
add_executable(datascript_unittest
        main.cc
        parser/ast/test_integer_literals.cc
        parser/ast/test_parser_basic.cc
        test_feature.cc  # <-- Add your new file here
)
```

5. Rebuild and run tests

## Current Test Status

**Test Results** (as of last run):
- ✅ 24 out of 27 test cases passed
- ✅ 107 out of 112 assertions passed

**Known Issues**:
1. `parse_universal_integer("0")` fails due to octal parsing bug (see CODE_REVIEW.md)
2. Some incomplete inputs don't throw exceptions as expected (parser may need stricter validation)

## Testing Internal Functions

Tests can access parser internals by including headers from `lib/src/parser/`:

```cpp
#include "parser/grammar.hh"
#include "parser/actions/integer_literal.hh"

TEST_CASE("Test internal parser function") {
    uint64_t value = datascript::actions::parse_universal_integer("0x2A");
    CHECK(value == 42);
}
```

This is configured in `CMakeLists.txt` via:
```cmake
target_include_directories(datascript_unittest PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/src
)
```

## Best Practices

1. **Use descriptive test names**: `"Parse hexadecimal literals"` not `"Test 1"`

2. **Group related tests**: Use `TEST_SUITE` to organize tests by feature

3. **Use REQUIRE for critical checks**: If a test can't continue after a failure, use `REQUIRE` instead of `CHECK`

4. **Test both success and failure cases**: Don't just test the happy path

5. **Keep tests independent**: Each test should be able to run in isolation

6. **Test one thing per TEST_CASE**: Makes failures easier to diagnose

7. **Use meaningful assertion messages** (optional):
```cpp
CHECK_MESSAGE(x == 42, "Expected x to be 42, got ", x);
```

## Resources

- [doctest Documentation](https://github.com/doctest/doctest/blob/master/doc/markdown/readme.md)
- [doctest Assertions](https://github.com/doctest/doctest/blob/master/doc/markdown/assertions.md)
- [doctest Command Line](https://github.com/doctest/doctest/blob/master/doc/markdown/commandline.md)
