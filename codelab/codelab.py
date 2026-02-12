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

# This file contains code that demonstrates common CEL features.
# This code is intended for use with the cel.expr.python Codelab

"""This file contains code that demonstrates common CEL features.

This code is intended for use with the cel.expr.python Codelab.
"""

# pylint: disable=unused-import
import datetime
import json
import pprint
from cel_expr_python import cel
from cel_expr_python.ext import ext_math
from cel_expr_python.ext import ext_string
from google.rpc.context import attribute_context_pb2
# pylint: enable=unused-import


def exercise1():
  """exercise1 evaluates a simple literal expression: "Hello, World!".

  Compile, eval, profit!
  """

  print("=== Exercise 1: Hello World ===\n")

  print()


def exercise2():
  """exercise2 shows how to declare and use variables in expressions.

  Given a request of type google.rpc.context.AttributeContext.Request
  determine whether a specific auth claim is set.
  """
  print("=== Exercise 2: Use variables in a function ===\n")

  print()


def exercise3():
  """exercise3 demonstrates how CEL's commutative logical operators work.

  Construct an expression which checks if the `request.auth.claims.group`
  value is equal to admin or the `request.auth.principal` is
  `user:me@acme.co`. Issue two requests, one that specifies the proper
  user, and one that specifies an unexpected user.
  """
  print("=== Exercise 3: Logical AND/OR ===\n")

  print()


def exercise4():
  """exercise4 demonstrates the use of standard extensions."""

  print("=== Exercise 4: Standard extensions ===\n")

  print()


def exercise5():
  """exercise5 demonstrates how to extend CEL with custom functions.

  Declare a `contains` member function on map types that returns a boolean
  indicating whether the map contains the key-value pair.
  """

  print("=== Exercise 5: Customization ===\n")

  print()


def contains_key_value(cel_map, key, value):
  return key in cel_map and cel_map[key] == value


def exercise6():
  """exercise6 covers how to build complex objects as CEL literals.

  Given the input now, construct a JWT with an expiry of 5 minutes.
  """
  print("=== Exercise 6: Building JSON ===\n")

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
