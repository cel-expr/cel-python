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
#include "checker/type_checker_builder.h"
#include "compiler/compiler.h"
#include "extensions/encoders.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "cel_expr_python/cel_extension.h"
#include "google/protobuf/descriptor.h"

namespace cel_python {

class ExtEncoders : public CelExtension {
 public:
  explicit ExtEncoders() : CelExtension("cel.lib.ext.encoders") {}

  absl::Status ConfigureCompiler(
      cel::CompilerBuilder& compiler_builder,
      const google::protobuf::DescriptorPool& descriptor_pool) override {
    return compiler_builder.GetCheckerBuilder().AddLibrary(
        cel::extensions::EncodersCheckerLibrary());
  }

  absl::Status ConfigureRuntime(cel::RuntimeBuilder& runtime_builder,
                                const cel::RuntimeOptions& opts) override {
    return cel::extensions::RegisterEncodersFunctions(
        runtime_builder.function_registry(), opts);
  }
};

CEL_EXTENSION_MODULE(ext_encoders, ExtEncoders);

}  // namespace cel_python
