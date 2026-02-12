# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""A sample CEL extension implemented entirely in Python."""

from typing import Any

from cel_expr_python import cel


class SampleCelExtension(cel.CelExtension):
  """A sample class for a CEL extension."""

  def __init__(self):
    super().__init__(
        "sample_cel_extension",
        functions=[
            cel.FunctionDecl(
                "translate",
                [
                    cel.Overload(
                        "translate_inst",
                        return_type=cel.Type.STRING,
                        parameters=[
                            cel.Type.STRING,
                            cel.Type.STRING,
                            cel.Type.STRING,
                        ],
                        is_member=True,
                        impl=self.translate,
                    )
                ],
            ),
            cel.FunctionDecl(
                "translate_late",
                [
                    cel.Overload(
                        "late_bound_translation",
                        return_type=cel.Type.STRING,
                        parameters=[cel.Type.STRING],
                        is_member=False,
                        # Note: no impl provided here.
                    )
                ],
            ),
            # Tests for type adaptation.
            cel.FunctionDecl(
                "lerp",
                [
                    cel.Overload(
                        "lerp_int_int_double",
                        return_type=cel.Type.DOUBLE,
                        parameters=[
                            cel.Type.INT,
                            cel.Type.INT,
                            cel.Type.DOUBLE,
                        ],
                        is_member=False,
                        impl=self.lerp,
                    ),
                    cel.Overload(
                        "lerp_uint_uint_double",
                        return_type=cel.Type.DOUBLE,
                        parameters=[
                            cel.Type.UINT,
                            cel.Type.UINT,
                            cel.Type.DOUBLE,
                        ],
                        is_member=False,
                        impl=self.lerp,
                    ),
                ],
            ),
            cel.FunctionDecl(
                "getOrDefault",
                [
                    cel.Overload(
                        "map_get_or_default_string_dyn",
                        return_type=cel.Type.DYN,
                        parameters=[
                            cel.Type.Map(cel.Type.STRING, cel.Type.DYN),
                            cel.Type.STRING,
                            cel.Type.DYN,
                        ],
                        is_member=True,
                        impl=self.map_get_or_default,
                    ),
                    cel.Overload(
                        "get_or_default_map_string_dyn",
                        return_type=cel.Type.DYN,
                        parameters=[
                            cel.Type.Map(cel.Type.STRING, cel.Type.DYN),
                            cel.Type.STRING,
                            cel.Type.DYN,
                        ],
                        is_member=False,
                        impl=self.map_get_or_default,
                    ),
                ],
            ),
        ],
    )

  def translate(self, text: str, from_lang: str, to_lang: str) -> str:
    if from_lang != "en" or to_lang != "es":
      raise ValueError("We can only translate from 'en' to 'es'")
    if text != "Hello, world!":
      raise ValueError("Come on, this is just 'Hello, world!'")
    return "¡Hola Mundo!"

  def lerp(self, a: int, b: int, t: float) -> float:
    """Linearly interpolate between a and b using t."""
    if t < 0.0 or t > 1.0:
      raise ValueError("t must be between 0.0 and 1.0")
    return a + (b - a) * t

  def map_get_or_default(
      self, m: dict[str, Any], key: str, default: Any
  ) -> Any:
    """Get the value for the key from the map, or return the default value."""
    return m.get(key, default)
