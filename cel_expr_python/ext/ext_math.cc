// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "compiler/compiler.h"
#include "extensions/math_ext.h"
#include "extensions/math_ext_decls.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "cel_expr_python/cel_extension.h"
#include "cel_expr_python/py_error_status.h"

namespace cel_python {

class ExtMath : public CelExtension {
 public:
  explicit ExtMath(int version)
      : CelExtension("cel.lib.ext.math", "math", version) {
    if (version < 0 || version > cel::extensions::kMathExtensionLatestVersion) {
      throw StatusToException(absl::InvalidArgumentError(absl::StrCat(
          "'math' extension version: ", version, " not in range [0, ",
          cel::extensions::kMathExtensionLatestVersion, "]")));
    }
  }

  ExtMath() : ExtMath(cel::extensions::kMathExtensionLatestVersion) {}

  cel::CompilerLibrary GetCompilerLibrary() override {
    return cel::extensions::MathCompilerLibrary(version());
  }

  absl::Status ConfigureRuntime(cel::RuntimeBuilder& runtime_builder,
                                const cel::RuntimeOptions& opts) override {
    return cel::extensions::RegisterMathExtensionFunctions(
        runtime_builder.function_registry(), opts, version());
  }
};

CEL_VERSIONED_EXTENSION_MODULE(ext_math, ExtMath);

}  // namespace cel_python
