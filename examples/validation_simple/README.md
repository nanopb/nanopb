# Nanopb Validation Simple Example

This example demonstrates the basic usage of nanopb's validation feature, which allows you to define declarative constraints in your `.proto` files that are automatically enforced through generated C validation code.

## Features Demonstrated

- Field-level validation constraints
  - String length validation (min/max)
  - Numeric range validation
  - String content validation (contains)
  - Enum value validation
  - Required field validation
- Validation error reporting
- Integration with encode/decode operations

## Building

To build this example:

```bash
make
```

This will:
1. Generate the protobuf files with validation support
2. Compile the example program
3. Link with nanopb and validation runtime

## Running

```bash
make run
# or
./validation_simple
```

## Expected Output

The example will demonstrate:
1. Validation of an empty message (showing required field violations)
2. Validation with invalid values (too short username, invalid age, etc.)
3. Successful validation with correct values
4. Encoding and decoding with validation

## Key Files

- `validation_simple.proto` - Protocol buffer definition with validation constraints
- `validation_simple.c` - Example usage of validation functions
- `validation_simple.options` - Nanopb options file setting field sizes and enabling validation
- `validation_simple_validate.h/c` - Generated validation code

## Validation Rules Used

```protobuf
// Username must be 3-20 characters and is required
string username = 1 [
    (validate.rules).string.min_len = 3,
    (validate.rules).string.max_len = 20,
    (validate.rules).required = true
];

// Age must be between 13 and 120
int32 age = 2 [
    (validate.rules).int32.gte = 13,
    (validate.rules).int32.lte = 120
];

// Email must contain @ and is required
string email = 3 [
    (validate.rules).string.contains = "@",
    (validate.rules).required = true
];

// Status must be a defined enum value
Status status = 4 [(validate.rules).enum.defined_only = true];
```

## Integration with Your Project

To use validation in your own project:

1. Import `validate.proto` in your `.proto` file
2. Add validation constraints using `(validate.rules)` options
3. Enable validation generation by running the `protoc-gen-nanopb-validate`
   plugin after `--nanopb_out` (see the Makefile and CMakeLists.txt here)
4. Include the generated `*_validate.h` files
5. Call `pb_validate_YourMessage()` to validate messages
6. Check `pb_violations_t` for detailed error information

See the [validation documentation](../../docs/validation.md) for complete details.
