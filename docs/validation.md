# Nanopb Validation

Nanopb validation provides declarative constraints for Protocol Buffer messages that are enforced through generated C validation code. This feature enables embedded systems to validate messages efficiently without heap allocation or runtime reflection.

## Overview

The validation feature allows you to:
- Define constraints directly in `.proto` files using custom options
- Generate C validation functions automatically
- Validate messages before encoding or after decoding
- Get detailed violation reports without dynamic memory allocation

## Enabling Validation

Validation code is produced by a separate protoc plugin,
`protoc-gen-nanopb-validate`. Running that plugin *is* the request for
validation - there is no `--validate` flag any more.

```bash
protoc   --nanopb_out=--protoc-insertion-points:.   --nanopb-validate_out=.   message.proto
```

This writes the usual `message.pb.h` / `message.pb.c`, plus
`message_validate.h` / `message_validate.c`.

Two things are required and easy to get wrong:

* `--nanopb_out` must come **before** `--nanopb-validate_out`. protoc runs
  generators in command line order, and the validate plugin injects into the
  files nanopb produced.
* nanopb must be given `--protoc-insertion-points`, otherwise there are no
  markers to inject into.

Options that affect C naming (`-C`, `--custom-style`, `-s`, `-f`, `-I`, `-x`)
must be passed to **both** plugins, since the validate plugin rebuilds nanopb's
view of the file in order to see the same mangled type names.

> **Note:** `validate.proto` declares a file-level `option (validate.validate)`
> and a message-level `validate.message` extension. Neither is implemented by
> the generator; only field-level `(validate.rules)` are read.

## Field-Level Constraints

### Numeric Constraints

```protobuf
message Product {
    // Basic numeric constraints
    int32 quantity = 1 [
        (validate.rules).int32.gte = 0,      // Greater than or equal
        (validate.rules).int32.lte = 1000    // Less than or equal
    ];
    
    // Exact value constraint
    int32 version = 2 [(validate.rules).int32.const = 1];
    
    // Value must be in list
    int32 status = 3 [(validate.rules).int32.in = [1, 2, 3]];
    
    // Value must not be in list
    int32 type = 4 [(validate.rules).int32.not_in = [99, 100]];
}
```

Supported for: `int32`, `int64`, `uint32`, `uint64`, `sint32`, `sint64`, `fixed32`, `fixed64`, `sfixed32`, `sfixed64`, `float`, `double`

### String Constraints

```protobuf
message User {
    // Length constraints
    string username = 1 [
        (validate.rules).string.min_len = 3,
        (validate.rules).string.max_len = 20
    ];
    
    // Pattern matching
    string email = 2 [
        (validate.rules).string.contains = "@",
        (validate.rules).string.suffix = ".com"
    ];
    
    // ASCII only
    string id = 3 [(validate.rules).string.ascii = true];
    
    // Exact match or in list
    string role = 4 [(validate.rules).string.in = ["admin", "user", "guest"]];
}
```

**Note**: String length constraints only work when strings are generated as static arrays (using `max_size` option). Callback-based strings skip length validation.

### Bytes Constraints

```protobuf
message Data {
    // Length constraints
    bytes payload = 1 [
        (validate.rules).bytes.min_len = 10,
        (validate.rules).bytes.max_len = 1024
    ];
    
    // Prefix/suffix matching
    bytes header = 2 [(validate.rules).bytes.prefix = "\x00\x01\x02"];
}
```

### Enum Constraints

```protobuf
enum Status {
    UNKNOWN = 0;
    ACTIVE = 1;
    INACTIVE = 2;
}

message Record {
    // Ensure enum value is defined (default: true)
    Status status = 1 [(validate.rules).enum.defined_only = true];
    
    // Restrict to subset of values
    Status filtered = 2 [(validate.rules).enum.in = [1, 2]];
}
```

### Repeated Field Constraints

```protobuf
message Collection {
    // Item count constraints
    repeated string items = 1 [
        (nanopb).max_count = 100,  // Required for static allocation
        (validate.rules).repeated.min_items = 1,
        (validate.rules).repeated.max_items = 50
    ];
    
    // Unique items only
    repeated int32 ids = 2 [(validate.rules).repeated.unique = true];
}
```

### Required Fields

```protobuf
message Config {
    // Make optional field required for validation
    optional string name = 1 [(validate.rules).required = true];
    
    // In a oneof, require this specific arm
    oneof setting {
        string text = 2 [(validate.rules).oneof_required = true];
        int32 number = 3;
    }
}
```

## Message-Level Constraints

### Field Dependencies

```protobuf
message Address {
    string city = 1;
    string state = 2;
    string country = 3;
    
    // If city is set, state must also be set
    option (validate.message).requires = "state";
}
```

### Mutual Exclusion

```protobuf
message Settings {
    bool use_default = 1;
    string custom_value = 2;
    
    // Only one of these fields can be set
    option (validate.message).mutex = {
        fields: ["use_default", "custom_value"]
    };
}
```

### At Least N Fields

```protobuf
message Contact {
    string email = 1;
    string phone = 2;
    string address = 3;
    
    // At least 2 contact methods required
    option (validate.message).at_least = {
        n: 2
        fields: ["email", "phone", "address"]
    };
}
```

## Using Generated Validation Code

### Basic Validation

```c
#include "message.pb.h"
#include "message_validate.h"

void validate_example() {
    MyMessage msg = MyMessage_init_zero;
    pb_violations_t violations;
    
    // Initialize violations collector
    pb_violations_init(&violations);
    
    // Validate the message
    if (!pb_validate_MyMessage(&msg, &violations)) {
        // Handle validation errors
        for (size_t i = 0; i < violations.count; i++) {
            printf("Validation error: %s - %s: %s\n",
                   violations.violations[i].field_path,
                   violations.violations[i].constraint_id,
                   violations.violations[i].message);
        }
    }
}
```

### Validation Hooks

Enable automatic validation during encode/decode:

```c
// In your build configuration or source file:
#define PB_VALIDATE_BEFORE_ENCODE
#define PB_VALIDATE_AFTER_DECODE

// Then use the validation-aware macros:
pb_ostream_t stream = ...;
if (!pb_validate_encode(&stream, MyMessage, &msg)) {
    // Validation or encoding failed
}

pb_istream_t stream = ...;
if (!pb_validate_decode(&stream, MyMessage, &msg)) {
    // Decoding or validation failed
}
```

## Configuration Options

### Compile-Time Settings

```c
// Maximum number of violations to collect (default: 16)
#define PB_VALIDATE_MAX_VIOLATIONS 32

// Stop validation on first error (default: 1)
#define PB_VALIDATE_EARLY_EXIT 0

// Maximum length for field paths in violations (default: 128)
#define PB_VALIDATE_MAX_PATH_LENGTH 256
```

### Generator Options

Passed via `--nanopb-validate_opt=` (or before the `:` in
`--nanopb-validate_out=`):

- `--root-message=NAME`: Decode and validate every packet as this message type.
- `--envelope-mode=oneof|any`: How to detect the envelope message. `oneof`
  (default) looks for an opcode enum plus a oneof payload, or a bare oneof;
  `any` looks for a `google.protobuf.Any` payload.
- `--envelope-name=NAME`: Use this message as the envelope instead of
  auto-detecting one.

## Limitations

1. **No Regex Support**: Pattern matching is limited to simple string operations (contains, prefix, suffix)
2. **Callback Fields**: `pb_callback_t` fields are not validated at all.
   Validation applies to statically allocated fields (`POINTER` allocation
   included); give strings, bytes and repeated fields a `max_size`/`max_count`
   so they are not converted to callbacks
3. **No Heap Usage**: All validation data structures are statically allocated
4. **Proto3 Only**: Currently only supports proto3 syntax

## Integration with Build Systems

### Make
`PROTOC_POST_OPTS` is appended after `--nanopb_out`, which is what keeps the
plugin ordering correct:

```makefile
PROTOC_OPTS      += --nanopb_opt=--protoc-insertion-points
PROTOC_POST_OPTS += --nanopb-validate_out=.
```

### CMake
Define `NANOPB_VALIDATE_OPTIONS` to switch the plugin on. An empty string means
"validate, with no special envelope handling"; `NANOPB_OPTIONS` are forwarded to
the plugin automatically.

```cmake
set(NANOPB_VALIDATE_OPTIONS "")
nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS message.proto)
```

### Bazel
Not currently supported. `cc_nanopb_proto_library` builds its outputs with a
single `proto_common.compile()` action and one plugin, so running a second
plugin that injects into nanopb's output would need a separate toolchain plus
declared `_validate.h`/`_validate.c` outputs.

## Performance Considerations

- Validation functions are generated as static inline where possible
- Rule data is stored in const arrays in program memory
- Early exit can be disabled for complete error collection
- No dynamic memory allocation during validation

## Error Messages

Validation errors include:
- `field_path`: Dotted path to the field (e.g., "user.email")
- `constraint_id`: Type of constraint violated (e.g., "string.min_len")
- `message`: Human-readable error description

Example violations:
```
user.email: string.contains - Field must contain '@'
user.age: int32.gte - Value must be >= 0
items[2]: string.max_len - String exceeds maximum length of 50
```
