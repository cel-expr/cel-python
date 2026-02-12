# cel-expr-python (CEL for Python) Codelab: Fast, safe, embedded expressions

<details>
<summary>

## Introduction

</summary>

### What is CEL?
CEL ([cel.dev](https://cel.dev)) is a non-Turing complete expression language
designed to be fast, portable, and safe to execute. CEL can be embedded into
a larger product.

CEL was designed as a language in which it is safe to execute user code.
While it's dangerous to blindly call `eval()` on a user's python code,
you can safely execute a user's CEL code. And because CEL prevents behavior
that would make it less performant, it evaluates safely on the order of
nanoseconds to microseconds; it's ideal for performance-critical
applications.

CEL evaluates expressions, which are similar to single line functions or
lambda expressions. While CEL is commonly used for boolean decisions,
it can also be used to construct more complex objects like JSON or
protobuf messages.

### cel-expr-python - CEL runtime for Python
Cel.expr.python is a collection of Python APIs for compilation, validation and
evaluation of CEL expressions. It also includes support for CEL extensions,
written either in Python or C++.

Cel.expr.python is a wrapper over CEL-C++, which is a flagship implementation of CEL
with a robust level of support.
</details>

<details>
<summary>

## Key concepts

</summary>

### Applications

CEL is general-purpose and has been used for diverse applications, from
routing RPCs to defining security policies. CEL is extensible, application
agnostic, and optimized for compile-once, evaluate-many workflows.

Many services and applications evaluate declarative configurations. For
example, Role-Based Access Control (RBAC) is a declarative configuration
that produces an access decision given a role and a set of users. If
declarative configurations cover 80% of use cases, CEL is a useful tool
for the remaining 20% when users need more expressive power.

### Compilation

An expression is compiled against an environment. The compilation step
produces a `cel.Expression` object containing the compilation result
(AST - Abstract Syntax Tree) in protobuf form. Compiled expressions are often
stored for future use to keep the evaluation as fast as possible. A single
compiled expression can be evaluated with many different inputs.

### Expressions

Users define expressions, while services and applications define the
environment in which these expressions are evaluated. CEL comes with a variety
of libraries with common functions for use within these environments.

In the following example, the expression takes a request object that
includes a claims token. The expression returns a boolean
indicating if the claims token is still valid.

```cel
// Check whether a JSON Web Token has expired by inspecting the 'exp' claim.
//
// Args:
//   claims - authentication claims.
//   now    - timestamp indicating the current system time.
// Returns: true if the token has expired.
//
timestamp(claims["exp"]) < now
```

### Environment

**Environments are defined by services**. Services and applications that
embed CEL declare the expression's environment. The environment is the
collection of variables and functions that can be used in expressions.

The environment includes declarations for inputs, such as function signatures,
which are defined externally to the CEL expression.

The declarations are used by the CEL type-checker to ensure
that all identifier and function references within an expression are
declared and used correctly.

### Phases of processing an expression

There are two phases in processing an expression: compile and
evaluate. The most common pattern for CEL is for a control plane to parse
and check expressions at config time and store the AST.

At runtime, the data plane retrieves and evaluates the AST repeatedly. CEL
is optimized for runtime efficiency, but compilation should not be
done in latency-critical code paths.

>
> **Warning**:  Parsing and checking should not be done in latency critical
> code paths.
>

During compilation, an expression is optionally checked against the
environment to ensure all variable and function identifiers in the
expression have been declared and are being used correctly. During this
phase, the compiler augments the compiled form of the
expression with type, variable, and function resolution metadata that can
drastically improve evaluation efficiency.

>
> **Best practice:** Perform type-checking to improve the speed and safety
> of parsed expressions, even with dynamic data like JSON where type
> inference is limited. Type-checking is enabled by default - just don't
> explicitly disable it without a compelling reason.
>

The CEL evaluator needs three things:

* Function bindings for any custom extensions
* Variable bindings
* A `cel.Expression` to evaluate

The function and variable bindings should match those used to compile
the expression. Any of these inputs can be reused across multiple evaluations,
such as an expression being evaluated across many sets of variable bindings,
the same variables being used against many expressions, or
function bindings being used across the lifetime of a process (a common case).
</details>

<details>
<summary>

## Setup

</summary>

Before we can start using cel-expr-python, let's install it in the Python environment:
```
pip install cel-expr-python
```

In this codelab, we will be using CEL along with some protocol buffers, so let's also
install the corresponding modules:
```
pip install protobuf googleapis-common-protos
```

We can also download the source code for this codelab:
```
git clone https://github.com/cel-expr/cel-python.git
cd cel-python/codelab
```

Run the `codelab.py` code to make sure the Python environment is properly
set up:
```
python codelab.py
```

You should see the following output:

```
=== Exercise 1: Hello World ===

=== Exercise 2: Variables ===

=== Exercise 3: Logical AND/OR ===

=== Exercise 4: Customization ===

=== Exercise 5: Building JSON ===

=== Exercise 6: Building Protos ===

=== Exercise 7: Macros ===

=== Exercise 8: Tuning ===
```
</details>

<details>
<summary>

## Hello, World!

</summary>

In the tradition of all programming languages, we'll start by creating and evaluating "Hello World!".

### Configure the environment
In your editor, open `codelab.py` and note that it starts off by importing the cel_expr_python module:

```python
from cel_expr_python import cel
```

In your editor, find the declaration of `exercise1`, and fill in the following to set up the environment:

```python
def exercise1():
  """exercise1 evaluates a simple literal expression: "Hello, World!".

  Compile, eval, profit!
  """

  print("=== Exercise 1: Hello World ===\n")
  env = cel.NewEnv()

  # Will add the compilation and evaluation steps here
```

CEL applications evaluate an expression against a `cel.Environment`. `env = cel.NewEnv()` configures the standard environment.

The standard CEL environment supports all of the types, operators, functions, and macros defined within the language specification.

### Compile the expression
Once the environment has been configured, expressions can be compiled. Add the following to your function:

```python
def exercise1():
  """exercise1 evaluates a simple literal expression: "Hello, World!".

  Compile, eval, profit!
  """

  print("=== Exercise 1: Hello World ===\n")
  env = cel.NewEnv()

  # Compile and type check the expression
  expr = env.compile("'Hello, World!'")

  # Check that the output type is a string.
  if expr.return_type() != cel.Type.STRING:
    raise ValueError(
        f"Got {expr.return_type()} expected return type {cel.Type.STRING}"
    )

  # Will add the eval step here
```

If there is a syntax or type-check error, the `compile` function
will raise a `RuntimeError` exception. When the expression
is well-formed, the result of the call is an executable `cel.Expression`.

### Evaluate the expression
Once the expression has been compiled into a `cel.Expression`, it can be evaluated against input by calling `eval()`. This call returns the result of evaluation or the error status.

Let's call eval:

```python
def exercise1():
  """exercise1 evaluates a simple literal expression: "Hello, World!".

  Compile, eval, profit!
  """

  print("=== Exercise 1: Hello World ===\n")
  env = cel.NewEnv()

  # Compile and type check the expression
  expr = env.compile("'Hello, World!'")

  # Check the output type is a string.
  if expr.return_type() != cel.Type.STRING:
    raise ValueError(
        f"Got {expr.return_type()} expected return type {cel.Type.STRING}"
    )

  # Evaluate the compiled expression without any arguments
  result = expr.eval()

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")

  print()
```

### Execute the code
On the command line, rerun the code:
```
  python codelab.py
```
You should see the following output, along with placeholders for the future exercises.

```
=== Exercise 1: Hello World ===

------ result ------
Value: Hello, World! (STRING)
```

</details>
<details>
<summary>

## Use variables in a function

</summary>

Most CEL applications will declare variables that can be referenced within expressions.
Variable declarations specify a name and a type.
A variable's type may be a CEL built-in type,
a Protocol Buffer well-known type, or any protobuf message type, as long as its
descriptor is also provided to CEL.

### Add the function

In your editor, find the declaration of `exercise2`, and add the following:

```
def exercise2():
  """exercise2 shows how to declare and use variables in expressions.

  Given a request of type google.rpc.context.AttributeContext.Request
  determine whether a specific auth claim is set.
  """
  print("=== Exercise 2: Use variables in a function ===\n")
  env = cel.NewEnv()
  expr = env.compile("request.auth.claims.group == 'admin'")

  # Create a request object that sets the proper group claim.
  request = attribute_context_pb2.AttributeContext.Request(
      auth={
          "principal": "user:me@acme.co",
          "claims": {"group": "admin"},
      }
  )

  expr.eval(data={"request": request})

  print()
```

### Rerun and understand the error

Rerun the program:

```
  python codelab.py
```

You should see the following output:

```
ERROR: <input>:1:20: undeclared reference to 'request.auth.claims.group' (in container '')
 | request.auth.claims.group == 'admin'
 | ...................^ [INVALID_ARGUMENT]
```

The type-checking phase of compilation produces an error for the request object, which conveniently includes the source snippet where the error occurred.
</details>
<details>
<summary>

## Declare the variables

</summary>

In the editor, let's fix the resulting error by providing a declaration for the request object as a message of type `google.rpc.context.AttributeContext.Request` like so:

```python
def exercise2():
  """exercise2 shows how to declare and use variables in expressions.

  Given a request of type google.rpc.context.AttributeContext.Request
  determine whether a specific auth claim is set.
  """
  print("=== Exercise 2: Use variables in a function ===\n")
  env = cel.NewEnv(
      variables={
          "request": cel.Type("google.rpc.context.AttributeContext.Request")
      }
  )

  try:
    expr = env.compile("request.auth.claims.group == 'admin'")
  except RuntimeError as e:
    print(f"Error compiling expression: {e}")
    return

  # Create a request object that sets the proper group claim.
  request = attribute_context_pb2.AttributeContext.Request(
      auth={
          "principal": "user:me@acme.co",
          "claims": {"group": "admin"},
      }
  )

  data = {"request": request}

  print("------ data ------")
  print(data)

  result = expr.eval(data=data)

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")

  print()
```

### Rerun successfully!

Run the program again:

```
  python codelab.py
```

You should see the following output:

```
=== Exercise 2: Use variables in a function ===

------ data ------
{'request': auth {
  principal: "user:me@acme.co"
  claims {
    fields {
      key: "group"
      value {
        string_value: "admin"
      }
    }
  }
}
}
------ result ------
Value: True (BOOL)
```

### Protos and type descriptors

To use variables that refer to protobuf messages or to handle the construction of protobuf messages, the compiler needs to know the type descriptor. A [type descriptor](https://googleapis.dev/python/protobuf/latest/google/protobuf/descriptor.html) is a protobuf message that describes the field declarations of a protobuf message. Descriptors can be thought of as a reflection type. Descriptors are used within the type-checker to determine field type references.

CEL finds
type descriptors in the descriptor pool linked to the environment. By default,
CEL uses the automatic descriptor pool that can be accessed as `google.protobuf.descriptor_pool.Default`.

The import statement `from google.rpc.context import attribute_context_pb2` at the top of `codelab.py` automatically loads all type descriptors defined within that module into the `Default` descriptor pool used by CEL. This is usually sufficient for making necessary type descriptors available.

**Advanced usage:** To gain more control, you can use an explicitly populated descriptor pool. Pass your custom descriptor pool to `cel.NewEnv` via the `descriptor_pool` argument: `cel.NewEnv(descriptor_pool=mypool)`. Since managing a descriptor pool can be quite involved, refer to the [DescriptorPool API documentation](https://googleapis.dev/python/protobuf/latest/google/protobuf/descriptor_pool.html) and the [Protobuf Tutorial](https://protobuf.dev/getting-started/pythontutorial/#protobuf-api) for further guidance.

>
> While `cel.NewEnv()` automatically attempts to import
> `google.protobuf.descriptor_pool` if not already present, skipping
> the `pip install protobuf` command means protobuf support will be unavailable.
> Consequently, any CEL operation requiring a type descriptor will fail.
> However, CEL expressions that don't use protobufs will still compile
> and evaluate normally.
>

To review, we just declared a variable for an error, assigned it a type descriptor, and then referenced the variable in the expression evaluation.

</details>
<details>
<summary>

## Logical AND/OR

</summary>

One of CEL's more unique features is its use of commutative logical operators. Either side of a conditional branch can short-circuit the evaluation, even in the face of errors or partial input.

In other words, CEL finds an evaluation order that gives a result whenever possible, ignoring errors or even missing data that might occur in other evaluation orders. Applications can rely on this property to minimize the cost of evaluation, deferring the gathering of expensive inputs when a result can be reached without them.

We'll add an AND/OR example, and then try it with different input to understand how CEL short-circuits evaluation.

### Create the function

In your editor, add the following content to exercise 3:

```
def exercise3():
  """exercise3 demonstrates how CEL's commutative logical operators work.

  Construct an expression which checks if the `request.auth.claims.group`
  value is equal to admin or the `request.auth.principal` is
  `user:me@acme.co`. Issue two requests, one that specifies the proper
  user, and one that specifies an unexpected user.
  """
  print("=== Exercise 3: Logical AND/OR ===\n")

  env = cel.NewEnv(
      variables={
          "request": cel.Type("google.rpc.context.AttributeContext.Request")
      }
  )
```

Next, include this OR statement that will return true if the user is either a member of the `admin` group or has a particular email identifier:

```
def exercise3():
  """exercise3 demonstrates how CEL's commutative logical operators work.

  Construct an expression which checks if the `request.auth.claims.group`
  value is equal to admin or the `request.auth.principal` is
  `user:me@acme.co`. Issue two requests, one that specifies the proper
  user, and one that specifies an unexpected user.
  """
  print("=== Exercise 3: Logical AND/OR ===\n")

  env = cel.NewEnv(
      variables={
          "request": cel.Type("google.rpc.context.AttributeContext.Request")
      }
  )

  expr = env.compile(
      "request.auth.claims.group == 'admin' "
      "|| request.auth.principal == 'user:me@acme.co'"
  )
```

And finally, add the `eval` case that evaluates the user with an empty claim set:

```
def exercise3():
  """exercise3 demonstrates how CEL's commutative logical operators work.

  Construct an expression which checks if the `request.auth.claims.group`
  value is equal to admin or the `request.auth.principal` is
  `user:me@acme.co`. Issue two requests, one that specifies the proper
  user, and one that specifies an unexpected user.
  """
  print("=== Exercise 3: Logical AND/OR ===\n")

  env = cel.NewEnv(
      variables={
          "request": cel.Type("google.rpc.context.AttributeContext.Request")
      }
  )

  expr = env.compile(
      "request.auth.claims.group == 'admin' "
      "|| request.auth.principal == 'user:me@acme.co'"
  )

  # Create a request object with an empty claim set.
  request = attribute_context_pb2.AttributeContext.Request(
      auth={"principal": "user:me@acme.co"}
  )

  data = {"request": request}

  print("------ data ------")
  print(data)

  result = expr.eval(data=data)

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")

  print()
```

### Run the code with the empty claim set

Rerunning the program, you should see the following new output:

```
=== Exercise 3: Logical AND/OR ===

------ data ------
{'request': auth {
  principal: "user:me@acme.co"
}
}
------ result ------
Value: True (BOOL)
```

### Update the evaluation case

Next, update the evaluation case to pass in a different principal with the empty claim set:

```
def exercise3():
  """exercise3 demonstrates how CEL's commutative logical operators work.

  Construct an expression which checks if the `request.auth.claims.group`
  value is equal to admin or the `request.auth.principal` is
  `user:me@acme.co`. Issue two requests, one that specifies the proper
  user, and one that specifies an unexpected user.
  """
  print("=== Exercise 3: Logical AND/OR ===\n")

  env = cel.NewEnv(
      variables={
          "request": cel.Type("google.rpc.context.AttributeContext.Request")
      }
  )

  expr = env.compile(
      "request.auth.claims.group == 'admin' "
      "|| request.auth.principal == 'user:me@acme.co'"
  )

  # Create a request object with an empty claim set.
  request = attribute_context_pb2.AttributeContext.Request(
      auth={"principal": "user:me@acme.co"}
  )

  data = {"request": request}

  print("------ data ------")
  print(data)

  result = expr.eval(data=data)

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")
  print()

  # Create a request object with an unexpected principal
  data["request"] = attribute_context_pb2.AttributeContext.Request(
      auth={"principal": "other:me@acme.co"}
  )

  print("------ data ------")
  print(data)

  result = expr.eval(data=data)

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")

  print()
```

### Run the code with a mismatching principal

Rerunning the program, you should see the following error:

```
=== Exercise 3: Logical AND/OR ===

------ data ------
{'request': auth {
  principal: "user:me@acme.co"
}
}
------ result ------
Value: True (BOOL)
------ data ------
{'request': auth {
  principal: "other:me@acme.co"
}
}
------ result ------
Value: NOT_FOUND: Key not found in map : "group" (ERROR)
```

In protobuf, we know what fields and types to expect. In map and JSON values, we don't know if a key will be present. Since there's no safe default value for a missing key, CEL defaults to an error.

</details>

<details>
<summary>

## Standard extensions

</summary>

CEL comes with a library of standard functions. You can find their descriptions in
this document: [CEL Extensions](https://github.com/google/cel-java/blob/main/extensions/src/main/java/dev/cel/extensions/README.md#extensions)

### Use functions in an expression

In your editor, add the following content to exercise 4:

```python
def exercise4():
  """exercise4 demonstrates the use of standard extensions.
  """

  print("=== Exercise 4: Standard extensions ===\n")
  env = cel.NewEnv()

  expr = env.compile("['\u221a2', string(math.sqrt(2))].join(' = ')")

  result = expr.eval()

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")

  print()
```

### Run the code

Rerunning the program, you should see the following error:

```
=== Exercise 4: Standard extensions ===

RuntimeError: INVALID_ARGUMENT: ERROR: <input>:1:1: undeclared reference to 'math' (in container '')
 | math.sqrt(2)
 | ^
```

The reason for this error is that standard extensions need to be explicitly enabled in the environment.

### Configure standard extensions

Update the code with this content:
```python
def exercise4():
  """exercise4 demonstrates the use of standard extensions.
  """

  print("=== Exercise 4: Standard extensions ===\n")
  env = cel.NewEnv(extensions=[ext_math.ExtMath(), ext_string.ExtString()])

  expr = env.compile("['\u221a2', string(math.sqrt(2))].join(' = ')")

  result = expr.eval()

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")

  print()
```

### Run the code with registered extensions

Now that the standard extensions are registered, the program should succeed:

```
=== Exercise 4: Standard extensions ===

------ result ------
Value: √2 = 1.41421 (STRING)
```

>
> For better control and safety, only register the extensions you actually need.
>

</details>

<details>
<summary>

## Custom extension

</summary>

While CEL includes many built-in functions, there are occasions when a custom function is useful. For example, custom functions can be used to improve the user experience for common conditions or to expose context-sensitive state.

In this exercise, we'll be exploring how to expose a function to package together commonly used checks.

### Call a custom function

First, create the code to set up an override called `contains` that determines if a key exists in a map and has a particular value. Leave placeholders for the function definition and function bindings:

```python
def exercise5():
  """exercise5 demonstrates how to extend CEL with custom functions.

  Declare a `contains` member function on map types that returns a boolean
  indicating whether the map contains the key-value pair.
  """

  print("=== Exercise 5: Customization ===\n")

  # Determine whether an optional claim is set to the proper value. The
  # custom map.contains(key, value) function is used as an alternative to:
  #   key in map && map[key] == value
  env = cel.NewEnv(
      # Declare the request.
      variables={
          "request": cel.Type("google.rpc.context.AttributeContext.Request")
      }
      # Add the custom function declaration here
  )
  expr = env.compile("request.auth.claims.contains('group', 'admin')")

  request = attribute_context_pb2.AttributeContext.Request(
      auth={"principal": "user:me@acme.co"}
  )

  data = {"request": request}

  print("------ data ------")
  print(data)

  result = expr.eval(data=data)

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")
  print()
```

### Run the code and understand the error

Rerunning the code, you should see the following error:

```
RuntimeError: INVALID_ARGUMENT: ERROR: <input>:1:29: undeclared reference to 'contains' (in container '')
 | request.auth.claims.contains('group', 'admin')
 | ............................^ [INVALID_ARGUMENT]
```

To fix the error, we need to add the `contains` function to the configuration
of the CEL environment.

### Add the custom function

Next, we'll add a new `contains` function that will use the target map and two arguments:

```python
def exercise5():
  """exercise4 demonstrates how to extend CEL with custom functions.

  Declare a `contains` member function on map types that returns a boolean
  indicating whether the map contains the key-value pair.
  """

  print("=== Exercise 4: Customization ===\n")

  # Determine whether an optional claim is set to the proper value. The
  # custom map.contains(key, value) function is used as an alternative to:
  #   key in map && map[key] == value
  env = cel.NewEnv(
      # Declare the request.
      variables={
          "request": cel.Type("google.rpc.context.AttributeContext.Request")
      },
      # Declare extensions
      extensions=[
          cel.CelExtension(
              "custom_map_functions",
              functions=[
                  cel.FunctionDecl(
                      "contains",
                      [
                          cel.Overload(
                              "contains_key_value",
                              return_type=cel.Type.BOOL,
                              parameters=[
                                  cel.Type.Map(cel.Type.STRING, cel.Type.DYN),
                                  cel.Type.STRING,
                                  cel.Type.DYN,
                              ],
                              is_member=True,
                              # Provide the implementation as a Python function
                          )
                      ],
                  )
              ],
          )
      ],
  )
  expr = env.compile("request.auth.claims.contains('group', 'admin')")

  request = attribute_context_pb2.AttributeContext.Request(
      auth={"principal": "user:me@acme.co"}
  )

  data = {"request": request}

  print("------ data ------")
  print(data)

  result = expr.eval(data=data)

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")

  print()
```

### Run the program to understand the error

Run the exercise. You should see the following error about the missing runtime function:

```
------ result ------
Value: UNKNOWN: No matching overloads found : contains(map, string, string) (ERROR)
```

Provide the function implementation to the `NewEnv` declaration using
a reference to a Python function.

For extra credit, try setting the admin claim on the input to verify the `contains` overload also returns true when the claim exists.

```python
def exercise5():
  """exercise5 demonstrates how to extend CEL with custom functions.

  Declare a `contains` member function on map types that returns a boolean
  indicating whether the map contains the key-value pair.
  """

  print("=== Exercise 5: Customization ===\n")

  # Determine whether an optional claim is set to the proper value. The
  # custom map.contains(key, value) function is used as an alternative to:
  #   key in map && map[key] == value
  env = cel.NewEnv(
      # Declare the request.
      variables={
          "request": cel.Type("google.rpc.context.AttributeContext.Request")
      },
      # Declare extensions
      extensions=[
          cel.CelExtension(
              "custom_map_functions",
              functions=[
                  cel.FunctionDecl(
                      "contains",
                      [
                          cel.Overload(
                              "contains_string_any",
                              return_type=cel.Type.BOOL,
                              parameters=[
                                  cel.Type.Map(cel.Type.STRING, cel.Type.DYN),
                                  cel.Type.STRING,
                                  cel.Type.DYN,
                              ],
                              is_member=True,
                              # Reference a Python function
                              impl=contains_key_value,
                          )
                      ],
                  )
              ],
          )
      ],
  )
  expr = env.compile("request.auth.claims.contains('group', 'admin')")

  # Empty claim set
  request = attribute_context_pb2.AttributeContext.Request(
      auth={
          "principal": "user:me@acme.co",
      }
  )

  data = {"request": request}

  print("------ data ------")
  print(data)

  result = expr.eval(data=data)

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")
  print()

  # Matching claim
  request = attribute_context_pb2.AttributeContext.Request(
      auth={"principal": "user:me@acme.co", "claims": {"group": "admin"}}
  )

  data = {"request": request}

  print("------ data ------")
  print(data)

  result = expr.eval(data=data)

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")
  print()


# The implementation of the extension function
def contains_key_value(map, key, value):
  return key in map and map[key] == value
```

The program should now run successfully:

```
=== Exercise 5: Customization ===

------ data ------
{'request': auth {
  principal: "user:me@acme.co"
}
}

------ result ------
Value: False (BOOL)

------ data ------
{'request': auth {
  principal: "user:me@acme.co"
  claims {
    fields {
      key: "group"
      value {
        string_value: "admin"
      }
    }
  }
}
}
------ result ------
Value: True (BOOL)

```
</details>

<details>
<summary>

## Building JSON

</summary>

CEL can also produce non-boolean outputs, such as JSON. Add the following to your function:

```
def exercise6():
  """exercise6 covers how to build complex objects as CEL literals.

  Given the input now, construct a JWT with an expiry of 5 minutes.
  """
  print("=== Exercise 6: Building JSON ===\n")
  env = cel.NewEnv(
      # Declare the 'now' variable as a Timestamp.
  )
  # Note the quoted keys in the CEL map literal. For proto messages the
  # field names are unquoted as they represent well-defined identifiers.
  expr = env.compile("""
      {'sub': 'serviceAccount:delegate@acme.co',
        'aud': 'my-project',
        'iss': 'auth.acme.com:12350',
        'iat': now,
        'nbf': now,
        'exp': now + duration('300s'),
        'extra_claims': {
            'group': 'admin'
        }}""")

  data = {"now": datetime.datetime.now(datetime.UTC)}

  print("------ data ------")
  print(data)

  result = expr.eval(data=data)

  print("------ result ------")
  pprint.pprint(result.value())
  print()
```

### Run the code

Rerunning the code, you should see the following errors:

```
ERROR: <input>:5:16: undeclared reference to 'now' (in container '')
 |         'iat': now,
 | ...............^
... and more ...
```

Add a declaration for the `now` variable of type `cel.Type.TIMESTAMP` to `cel.NewEnv()` and run again:

```python
def exercise6():
  """exercise6 covers how to build complex objects as CEL literals.

  Given the input now, construct a JWT with an expiry of 5 minutes.
  """
  print("=== Exercise 6: Building JSON ===\n")
  env = cel.NewEnv(
      variables={
          "now": cel.Type.TIMESTAMP,
      }
  )
  # Note the quoted keys in the CEL map literal. For proto messages the
  # field names are unquoted as they represent well-defined identifiers.
  expr = env.compile("""
      {'sub': 'serviceAccount:delegate@acme.co',
        'aud': 'my-project',
        'iss': 'auth.acme.com:12350',
        'iat': now,
        'nbf': now,
        'exp': now + duration('300s'),
        'extra_claims': {
            'group': 'admin'
        }}""")

  data = {"now": datetime.datetime.now(datetime.UTC)}

  print("------ data ------")
  print(data)

  result = expr.eval(data=data)

  print("------ result ------")
  pprint.pprint(result.value())
  print()
```

Rerun the code, and it should succeed:

```
=== Exercise 6: Building JSON ===

------ data ------
{'now': datetime.datetime(2026, 1, 30, 23, 47, 24, 677119, tzinfo=datetime.timezone.utc)}
------ result ------
{'aud': "my-project",
 'exp': 2026-01-30T23:52:24.677119Z,
 'extra_claims': {"group": "admin"},
 'iat': 2026-01-30T23:47:24.677119Z,
 'iss': "auth.acme.com:12350",
 'nbf': 2026-01-30T23:47:24.677119Z,
 'sub': "serviceAccount:delegate@acme.co"}
```

The program runs, but the output value needs to be explicitly converted to JSON. The internal CEL representation, in this case, is JSON-convertible as it only refers to types that JSON can support or for which there is a well-known JSON mapping.

```
def exercise6():
  """exercise6 covers how to build complex objects as CEL literals.

  Given the input now, construct a JWT with an expiry of 5 minutes.
  """
  ...

  json_obj = value_to_json(result)

  print("------ converted to JSON ------")
  print(json.dumps(json_obj, indent=2))

```

Once the type has been converted using the `value_to_json` helper function within the `codelab.py` file, you should see the following additional output:

```
------ converted to JSON ------
{
  "iat": "2026-01-31T00:13:30.973861+00:00",
  "exp": "2026-01-31T00:18:30.973861+00:00",
  "iss": "auth.acme.com:12350",
  "extra_claims": {
    "group": "admin"
  },
  "aud": "my-project",
  "sub": "serviceAccount:delegate@acme.co",
  "nbf": "2026-01-31T00:13:30.973861+00:00"
}
```

</details>

<details>
<summary>

## Building Protos

</summary>

CEL can build protobuf messages for any message type compiled into the application. Add the function to build a `google.rpc.context.AttributeContext.Request` from an input `jwt`:

```
def exercise7():
  """exercise7 describes how to build proto message types within CEL.

  Given an input jwt and time now construct a
  google.rpc.context.AttributeContext.Request with the time and auth
  fields populated according to the
  https://github.com/googleapis/googleapis/blob/master/google/rpc/context/attribute_context.proto
  specification.
  """

  print("=== Exercise 7: Building Protos ===\n")

  # Construct an environment and indicate that the container for all references
  # within the expression is `google.rpc.context.AttributeContext`.
  env = cel.NewEnv(
      # Add `container` option for 'google.rpc.context.AttributeContext'
      # Add variable definitions for 'jwt' as a map(string, Dyn) type
      # and for 'now' as a timestamp.
  )

  # Compile the Request message construction expression and validate that
  # the resulting expression type matches the fully qualified message name.
  #
  # Note: the field names within the proto message types are not quoted as they
  # are well-defined names composed of valid identifier characters. Also, note
  # that when building nested proto objects, the message name needs to prefix
  # the object construction.
  expr = env.compile("""
    Request{
        auth: Auth{
            principal: jwt.iss + '/' + jwt.sub,
            audiences: [jwt.aud],
            presenter: 'azp' in jwt ? jwt.azp : "",
            claims: jwt
        },
        time: now
    }""")

  # Construct the message. The result is a cel.Value that returns a dynamic
  # proto message.
  result = expr.eval(
      data={
          "jwt": {
              "sub": "serviceAccount:delegate@acme.co",
              "aud": "my-project",
              "iss": "auth.acme.com:12350",
              "extra_claims": {
                  "group": "admin",
              },
          },
          "now": datetime.datetime.now(datetime.UTC),
      },
  )

  out = result.value()

  # The value returned by CEL is a dynamic proto message.
  # Convert it to the concrete proto message type expected by using
  # `message.MergeFrom(dynamic_message)`

  print("------ result ------")
  print(out)
  print()
```

### Run the code

Rerunning the code, you should see the following error:

```
ERROR: <input>:2:10: undeclared reference to 'Request' (in container '')
 |   Request{
 | .........^
```

The container is basically the equivalent of a namespace or package, but can even be as granular as a protobuf message name. CEL containers use the same namespace resolution rules as  [Protobuf and C++](https://developers.google.com/protocol-buffers/docs/proto3#packages-and-name-resolution) for determining where a given variable, function, or type name is declared.

Given the container `google.rpc.context.AttributeContext`, the type-checker and evaluator will try the following identifier names for all variables, types, and functions:

* `google.rpc.context.AttributeContext.&lt;id&gt;`
* `google.rpc.context.&lt;id&gt;`
* `google.rpc.&lt;id&gt;`
* `google.&lt;id&gt;`
* `&lt;id&gt;`

For absolute names, prefix the variable, type, or function reference with a leading dot. In the example, the expression `.&lt;id&gt;` will only search for the top-level `&lt;id&gt;` identifier without first checking within the container.

Try specifying the `google.rpc.context.AttributeContext` option in the CEL environment and run again:

```python
def exercise7():
  """exercise7 describes how to build proto message types within CEL.

  Given an input jwt and time now construct a
  google.rpc.context.AttributeContext.Request with the time and auth
  fields populated according to the
  https://github.com/googleapis/googleapis/blob/master/google/rpc/context/attribute_context.proto
  specification.
  """

  print("=== Exercise 7: Building Protos ===\n")

  # Construct an environment and indicate that the container for all references
  # within the expression is `google.rpc.context.AttributeContext`.
  env = cel.NewEnv(
      # Add `container` option for 'google.rpc.context.AttributeContext'
      container="google.rpc.context.AttributeContext",
      # Add variable definitions for 'jwt' as a map(string, Dyn) type
      # and for 'now' as a timestamp.
  )

  ...
```

You should get the following output:

```
ERROR: <input>:4:27: undeclared reference to 'jwt.iss' (in container 'google.rpc.context.AttributeContext')
 |             principal: jwt.iss + '/' + jwt.sub,
 | ..........................^
```

... and many more errors...

Next, declare the `jwt` and `now` variables and implement conversion of the dynamic proto
to the expected proto message type:

```python
def exercise7():
  """exercise7 describes how to build proto message types within CEL.

  Given an input jwt and time now construct a
  google.rpc.context.AttributeContext.Request with the time and auth
  fields populated according to the
  https://github.com/googleapis/googleapis/blob/master/google/rpc/context/attribute_context.proto
  specification.
  """

  print("=== Exercise 7: Building Protos ===\n")

  # Construct an environment and indicate that the container for all references
  # within the expression is `google.rpc.context.AttributeContext`.
  env = cel.NewEnv(
      # Add `container` option for 'google.rpc.context.AttributeContext'
      container="google.rpc.context.AttributeContext",
      # Add variable definitions for 'jwt' as a map(string, Dyn) type
      # and for 'now' as a timestamp.
      variables={
          "jwt": cel.Type.Map(cel.Type.STRING, cel.Type.DYN),
          "now": cel.Type.TIMESTAMP,
      },
  )

  # Compile the Request message construction expression and validate that
  # the resulting expression type matches the fully qualified message name.
  #
  # Note: the field names within the proto message types are not quoted as they
  # are well-defined names composed of valid identifier characters. Also, note
  # that when building nested proto objects, the message name needs to prefix
  # the object construction.
  expr = env.compile("""
    Request{
        auth: Auth{
            principal: jwt.iss + '/' + jwt.sub,
            audiences: [jwt.aud],
            presenter: 'azp' in jwt ? jwt.azp : "",
            claims: jwt
        },
        time: now
    }""")

  data = {
      "jwt": {
          "sub": "serviceAccount:delegate@acme.co",
          "aud": "my-project",
          "iss": "auth.acme.com:12350",
          "extra_claims": {
              "group": "admin",
          },
      },
      "now": datetime.datetime.now(datetime.UTC),
  }

  print("------ data ------")
  print(data)

  # Construct the message. The result is a cel.Value that contains a dynamic
  # proto message.
  result = expr.eval(data=data)

  # The value returned by CEL is a dynamic proto message.
  # You can convert it to the expected concrete proto message type.
  request = attribute_context_pb2.AttributeContext.Request()
  request.MergeFrom(result.value())

  print("------ result ------")
  print(request)
  print()
```

Now the program should run as expected:

```
=== Exercise 7: Building Protos ===

------ data ------
{'jwt': {'sub': 'serviceAccount:delegate@acme.co', 'aud': 'my-project', 'iss': 'auth.acme.com:12350', 'extra_claims': {'group': 'admin'}}, 'now': datetime.datetime(2026, 1, 31, 1, 5, 21, 435972, tzinfo=datetime.timezone.utc)}
------ result ------
time {
  seconds: 1769821521
  nanos: 435972000
}
auth {
  principal: "auth.acme.com:12350/serviceAccount:delegate@acme.co"
  audiences: "my-project"
  claims {
    fields {
      key: "sub"
      value {
        string_value: "serviceAccount:delegate@acme.co"
      }
    }
    fields {
      key: "iss"
      value {
        string_value: "auth.acme.com:12350"
      }
    }
    fields {
      key: "extra_claims"
      value {
        struct_value {
          fields {
            key: "group"
            value {
              string_value: "admin"
            }
          }
        }
      }
    }
    fields {
      key: "aud"
      value {
        string_value: "my-project"
      }
    }
  }
}
```
</details>
