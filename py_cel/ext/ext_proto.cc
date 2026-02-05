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
#include "compiler/compiler.h"
#include "extensions/proto_ext.h"
#include "py_cel/py_cel_extension.h"
#include "google/protobuf/descriptor.h"

namespace cel_python {

class ExtProto : public PyCelExtension {
 public:
  explicit ExtProto() : PyCelExtension("cel.lib.ext.proto") {}

  absl::Status ConfigureCompiler(
      cel::CompilerBuilder& compiler_builder,
      const google::protobuf::DescriptorPool& descriptor_pool) override {
    return compiler_builder.AddLibrary(
        cel::extensions::ProtoExtCompilerLibrary());
  }
};

CEL_EXTENSION_MODULE(ext_proto, ExtProto);

}  // namespace cel_python
