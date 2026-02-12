# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""This file contains code that demonstrates common CEL features.

This code is intended for use with the cel.expr.python Codelab.
"""

import datetime
import json
import pprint
from cel_expr_python import cel
from cel_expr_python.ext import ext_math
from cel_expr_python.ext import ext_string
from google.rpc.context import attribute_context_pb2


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


def exercise4():
  """exercise4 demonstrates the use of standard extensions."""

  print("=== Exercise 4: Standard extensions ===\n")
  env = cel.NewEnv(extensions=[ext_math.ExtMath(), ext_string.ExtString()])

  expr = env.compile("['\u221a2', string(math.sqrt(2))].join(' = ')")

  result = expr.eval()

  print("------ result ------")
  print(f"Value: {result.value()} ({result.type()})")

  print()


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
                      "containsKeyValue",
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
                              impl=contains_key_value,
                          )
                      ],
                  )
              ],
          )
      ],
  )
  expr = env.compile("request.auth.claims.containsKeyValue('group', 'admin')")

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


def contains_key_value(cel_map, key, value):
  return key in cel_map and cel_map[key] == value


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

  json_obj = value_to_json(result)

  print("------ converted to JSON ------")
  print(json.dumps(json_obj, indent=2))
  print()


def value_to_json(value: cel.Value):
  """value_to_json converts the CEL type to a JSON representation."""
  match value.type():
    case cel.Type.MAP:
      out = {}
      for key, val in value.value().items():
        out[key] = value_to_json(val)
      return out
    case cel.Type.TIMESTAMP:
      return value.value().isoformat()
    # This conversion is not exhaustive. To fully support JSON conversion,
    # we would need to handle other types such as cel.Type.DURATION and
    # cel.Type.LIST
    case _:
      return str(value.plain_value())


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


def main() -> None:
  exercise1()
  exercise2()
  exercise3()
  exercise4()
  exercise5()
  exercise6()
  exercise7()


if __name__ == "__main__":
  main()
