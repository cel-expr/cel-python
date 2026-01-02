# CEL Python Wrapper (PyCEL)

This is a Python wrapper for the CEL C++ implementation.

## Usage

### Importing CEL module

```python
import py_cel
```

### Creating and configuring Cel

To create a CEL environment, you need to define
variable types that can be used in expressions.

```python
cel = py_cel.Cel(variables={"x": py_cel.Type.INT, "y": py_cel.Type.INT})
```

#### Optional configuration parameters

The `py_cel.Cel` constructor also accepts the following optional parameters:

*   `pool` (`descriptor_pool.DescriptorPool`): The descriptor pool used for
    resolving protobuf message types within CEL expressions. If not provided,
    a default pool (`descriptor_pool.Default()`) is used.
*   `container` (str): The container name used for name resolution. For example,
    if `container` is `"foo.bar"`, then `Baz` will resolve to
    `foo.bar.Baz`.
*   `extensions` (list): A list of extension objects to load. This can include
    standard extensions (like `math` or `string` libraries) or custom extensions
    defined in Python or C++.

### Compiling expressions

Use the `compile()` method to compile a CEL expression string into a reusable
expression object.

```python
expr = cel.compile("x + y > 10")
```

The `expr` object can be serialized into a binary format for persistence and
later deserialized.

```python
serialized_expr = expr.serialize()
# ... can be stored or sent over network ...
deserialized_expr = cel.deserialize(serialized_expr)
```

The `compile` method can take an optional `disable_check=True` argument, which
disables type checking until runtime. This could be useful when types of
variables are not known at compile time.

### Evaluation

To evaluate a compiled expression, you need to create an activation, which
provides bindings for variables, and then call `eval()`.

```python
# Provide variable values in a dictionary.
activation = cel.Activation({"x": 7, "y": 4})

# Evaluate the expression.
result = expr.eval(activation)

# The result is a `CelValue` object, which contains the result's CEL type and
# value.

# Get the result value.
print(f"Result type: {result.type()}")
print(f"Result value: {result.value()}")
```

This will output:

```
Result type: BOOL
Result value: True
```

### Using an Arena

An `Activation` can also take an `Arena` for memory management during
evaluation. This is a memory optimization technique that allows temporary
C++ objects created during the evaluation to be released as a group. The same
`Arena` can be shared across multiple activations; just keep in mind that none
of the associated objects are released until the last object using the arena is
garbage-collected in Python.

```python
arena = py_cel.Arena()

activation1 = cel.Activation({"x": 7, "y": 4}, arena)
# evaluate some expressions
activation2 = cel.Activation({"x": 8, "y": 9}, arena)
# evaluate some more expressions

# Process all results. Note: Don't put CelValues in long-lived data structures
# if you want the arena to be garbage-collected promptly.

# When `arena` and all `CelValue` objects produced with it go out of scope,
# all memory allocated for C++ objects during evaluation will be released.
```

### Working with Protobufs

You can pass protobuf messages as variables to an activation; CEL
expressions can return protobuf messages.

First, ensure your proto messages are available in the descriptor pool used by
`py_cel.Cel`, by importing your proto library in Python:

from cel.expr.conformance.proto2 import test_all_types_pb2 as test_pb

Then declare any variables of message type using `py_cel.Type` with their fully
qualified name.

```python
# Declare 'msg_var' as a message type.
cel = py_cel.Cel(
    pool,
    variables={
        "msg_var": py_cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
    },
)
```

Compile an expression that uses message fields:

```python
expr = cel.compile("msg_var.single_int32 == 42")
```

Pass a message in the activation. When passing a message to an activation, use
an instance of the Python proto message class.

```python
my_msg = test_pb.TestAllTypes(single_int32=42)

activation = cel.Activation({"msg_var": my_msg})
result = expr.eval(activation)
print(f"Result: {result.value()}")
```

An expression can also return a proto message:

```python
msg_expr = cel.compile(
    "cel.expr.conformance.proto2.TestAllTypes{single_int32: 123}"
)
msg_result = msg_expr.eval(activation)
proto_val = msg_result.value()
print(f"Resulting message type: {type(proto_val)}")
print(f"Resulting message value: {proto_val.single_int32}")
```

This will output:

```
Resulting message type: <class '...TestAllTypes'>
Resulting message value: 123
```

### Extensions

#### Standard extensions

Standard extensions are available under `py_cel.ext`.

```python
from py_cel.ext import ext_math

cel = py_cel.Cel(pool, extensions=[ext_math.ExtMath()])
expr = cel.compile("math.sqrt(4)")
```

#### Defining a custom extension in Python

You can define custom functions and pass them as an extension.

```python
def my_func_impl(x):
  return x + 1

my_ext = py_cel.CelExtension(
    "my_extension",
    [
        py_cel.FunctionDecl(
            "my_func",
            [
                py_cel.Overload(
                    "my_func_int",
                    py_cel.Type.INT,
                    [py_cel.Type.INT],
                    impl=my_func_impl,
                )
            ],
        )
    ],
)

cel = py_cel.Cel(pool, extensions=[my_ext])
expr = cel.compile("my_func(1)")
```

#### Defining a custom extension in C++

To define a custom extension in C++, define a class extending
`cel_python::PyCelExtension`. There are two methods you will need to implement:
`ConfigureCompiler` and `ConfigureRuntime`. The implementations of these methods
use the same API as extensions written for the C++ CEL runtime. In fact,
extensions written for the C++ runtime can be used unchanged with PyCEL - you
would just need to write a trivial wrapper class invoking the registration
functions defined by the C++ extension.

```cpp
  absl::Status ConfigureCompiler(
      cel::CompilerBuilder& compiler_builder,
      const proto2::DescriptorPool& descriptor_pool);
```
This method adds extension function definitions to the provided
`CompilerBuilder`, for example:

```cpp
absl::Status ConfigureCompiler(
      cel::CompilerBuilder& compiler_builder,
      const proto2::DescriptorPool& descriptor_pool) {
    PY_CEL_ASSIGN_OR_RETURN(
        auto func_translate,
        cel::MakeFunctionDecl("translate",
            cel::MakeMemberOverloadDecl("translate_inst",
                                /*return_type=*/cel::StringType(),
                                /*target=*/cel::StringType(),
                                /*from_lang=*/cel::StringType(),
                                /*to_lang=*/cel::StringType())));
    PY_CEL_RETURN_IF_ERROR(
        compiler_builder.GetCheckerBuilder().AddFunction(func_translate));
    return absl::OkStatus();
}
```

The other method registers the actual implementation
of the extension function with the runtime:

```cpp
absl::Status ConfigureRuntime(cel::RuntimeBuilder& runtime_builder,
                              const cel::RuntimeOptions& opts);
```

For example,

```cpp
static absl::StatusOr<cel::StringValue> Translate(
    const cel::StringValue& text, const cel::StringValue& from_lang,
    const cel::StringValue& to_lang, const proto2::DescriptorPool* absl_nonnull,
    proto2::MessageFactory* absl_nonnull, proto2::Arena* absl_nonnull arena) {
  return cel::StringValue::From("¡Hola Mundo!", arena);
}

absl::Status ConfigureRuntime(cel::RuntimeBuilder& runtime_builder,
                                const cel::RuntimeOptions& opts) override {
    using TranslateFunctionAdapter =
        ::cel::TernaryFunctionAdapter<absl::StatusOr<StringValue>,
                                      const StringValue&, const StringValue&,
                                      const StringValue&>;
    auto status = TranslateFunctionAdapter::RegisterMemberOverload(
        "translate", &Translate, runtime_builder.function_registry());
    PY_CEL_RETURN_IF_ERROR(status);
    return absl::OkStatus();
}
```

Once you have the custom subclass of `cel_python::PyCelExtension`, add this line
to turn this class into a Python module:

```cpp
CEL_EXTENSION_MODULE(translation_cel_ext, TranslationCelExtension);
```

To build the Python module, use the `pybind_extension` BUILD rule:

```bazel
pybind_extension(
    name = "translation_cel_ext",
    srcs = ["translation_cel_ext.cc"],
    data = [
        "@py_cel:py_cel",
    ]
    deps = [
        "@py_cel:py_cel",
        "@py_cel:py_cel_extension",
        "@py_cel:status_macros",
        ...
    ],
)
```

Now you can use the extension in PyCel:

```python
import translation_cel_ext

cel = py_cel.Cel(variables={},
  extensions=[translation_cel_ext.TranslationCelExtension()])

expr = cel.compile("'Hello, world!'.translate('en', 'es')")
```

#### Late-bound extension functions

Sometimes it is required to delay the binding of an extension function
implementation until the runtime. To do this in an extension written in Python,
simply leave the implementation parameter unspecified:

```python

my_ext = py_cel.CelExtension(
    "my_extension",
    [
        py_cel.FunctionDecl(
            "my_func",
            [
                py_cel.Overload(
                    "my_func_int",
                    py_cel.Type.INT,
                    [py_cel.Type.INT],
                    # Note: no impl provided here.
                )
            ],
        )
    ],
)
```

If the extension is written in C++, use the `RegisterLazyFunction` function:

```cpp

  absl::Status ConfigureRuntime(cel::RuntimeBuilder& runtime_builder,
                                const cel::RuntimeOptions& opts) override {
    using MyFunctionAdapter =
        ::cel::UnaryFunctionAdapter<absl::StatusOr<cel::IntValue>,
                                    const cel::IntValue&>;
    PY_CEL_RETURN_IF_ERROR(
        runtime_builder.function_registry().RegisterLazyFunction(
            MyFunctionAdapter::CreateDescriptor(
                "my_func",
                /*receiver_style=*/false)));
    return absl::OkStatus();
  }
```

Now you can bind the function at runtime:

```python
cel = py_cel.Cel(variables={}, extensions=[my_ext])
expr = cel.compile("my_func(42)")

multiplier = 2
act = cel.Activation({}, functions={"my_func": lambda x: x * multiplier})
res = expr.eval(act)
# res.value() == 84
```
