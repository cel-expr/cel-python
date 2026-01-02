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
import py_cel as cel


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
        ],
    )

  def translate(self, text: str, from_lang: str, to_lang: str) -> str:
    if from_lang != "en" or to_lang != "es":
      raise ValueError("We can only translate from 'en' to 'es'")
    if text != "Hello, world!":
      raise ValueError("Come on, this is just 'Hello, world!'")
    return "¡Hola Mundo!"
