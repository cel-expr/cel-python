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
#include "checker/optional.h"
#include "compiler/compiler.h"
#include "compiler/optional.h"
#include "runtime/optional_types.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "cel_expr_python/cel_extension.h"
#include "cel_expr_python/py_error_status.h"

namespace cel_python {

class ExtOptional : public CelExtension {
 public:
  explicit ExtOptional(int version) : CelExtension("optional", "", version) {
    if (version < 0 || version > cel::kOptionalExtensionLatestVersion) {
      throw StatusToException(absl::InvalidArgumentError(absl::StrCat(
          "'optional' extension version: ", version, " not in range [0, ",
          cel::kOptionalExtensionLatestVersion, "]")));
    }
  }

  ExtOptional() : ExtOptional(cel::kOptionalExtensionLatestVersion) {}

  cel::CompilerLibrary GetCompilerLibrary() override {
    return cel::OptionalCompilerLibrary(version());
  }

  absl::Status ConfigureRuntime(cel::RuntimeBuilder& runtime_builder,
                                const cel::RuntimeOptions& opts) override {
    absl::Status status = cel::extensions::EnableOptionalTypes(runtime_builder);
    if (!status.ok() && status.code() != absl::StatusCode::kAlreadyExists) {
      return status;
    }
    return absl::OkStatus();
  }
};

CEL_VERSIONED_EXTENSION_MODULE(ext_optional, ExtOptional);

}  // namespace cel_python
