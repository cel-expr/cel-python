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
#include "extensions/strings.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "cel_expr_python/cel_extension.h"
#include "cel_expr_python/py_error_status.h"

namespace cel_python {

class ExtStrings : public CelExtension {
 public:
  explicit ExtStrings(int version)
      : CelExtension("cel.lib.ext.strings", "strings", version) {
    if (version < 0 ||
        version > cel::extensions::kStringsExtensionLatestVersion) {
      throw StatusToException(absl::InvalidArgumentError(absl::StrCat(
          "'strings' extension version: ", version, " not in range [0, ",
          cel::extensions::kStringsExtensionLatestVersion, "]")));
    }
  }

  ExtStrings() : ExtStrings(cel::extensions::kStringsExtensionLatestVersion) {}

  cel::CompilerLibrary GetCompilerLibrary() override {
    return cel::extensions::StringsCompilerLibrary(version());
  }

  absl::Status ConfigureRuntime(cel::RuntimeBuilder& runtime_builder,
                                const cel::RuntimeOptions& opts) override {
    return cel::extensions::RegisterStringsFunctions(
        runtime_builder.function_registry(), opts,
        cel::extensions::StringsExtensionOptions{.version = version()});
  }
};

CEL_VERSIONED_EXTENSION_MODULE(ext_strings, ExtStrings);

}  // namespace cel_python
