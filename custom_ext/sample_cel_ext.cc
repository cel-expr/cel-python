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

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "runtime/function_adapter.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "py_cel/cel_extension.h"
#include "py_cel/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

// A sample CEL extension that demonstrates how to define a custom CEL extension
// in C++.  Please note the `CEL_EXTENSION_MODULE` macro invocation at the
// bottom of this file, which is needed to make the extension discoverable in
// Python.
namespace cel_ext_translate {

using ::cel::MakeFunctionDecl;
using ::cel::MakeMemberOverloadDecl;
using ::cel::MakeOverloadDecl;
using ::cel::StringType;
using ::cel::StringValue;

namespace {

// A native implementation of the CEL extension function. Primitive types such
// as int, double can be used directly. The descriptor pool, message factory and
// arena are provided by CEL and can be used to create complex types such as
// StringValue, ListValue, MapValue, or protocol buffer messages.
static absl::StatusOr<StringValue> Translate(
    const StringValue& text, const StringValue& from_lang,
    const StringValue& to_lang, const google::protobuf::DescriptorPool* absl_nonnull,
    google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull arena) {
  if (from_lang.ToString() != "en" || to_lang.ToString() != "es") {
    return absl::InvalidArgumentError(
        "We can only translate from 'en' to 'es'");
  }
  if (text.ToString() != "Hello, world!") {
    return absl::InvalidArgumentError("Come on, this is just 'Hello, world!'");
  }
  return StringValue::From("¡Hola Mundo!", arena);
}

}  // namespace

class SampleCelExtension : public cel_python::CelExtension {
 public:
  explicit SampleCelExtension()
      : cel_python::CelExtension("sample.cel.cpp.ext") {}

  absl::Status ConfigureCompiler(
      cel::CompilerBuilder& compiler_builder,
      const google::protobuf::DescriptorPool& descriptor_pool) override {
    PY_CEL_ASSIGN_OR_RETURN(
        auto func_translate,
        MakeFunctionDecl("translate",
                         MakeMemberOverloadDecl("translate_inst",
                                                /*return_type=*/StringType(),
                                                /*target=*/StringType(),
                                                /*from_lang=*/StringType(),
                                                /*to_lang=*/StringType())));
    PY_CEL_RETURN_IF_ERROR(
        compiler_builder.GetCheckerBuilder().AddFunction(func_translate));

    PY_CEL_ASSIGN_OR_RETURN(
        auto func_translate_late,
        MakeFunctionDecl("translate_late",
                         MakeOverloadDecl("late_bound_translation",
                                          /*return_type=*/StringType(),
                                          /*text=*/StringType())));
    PY_CEL_RETURN_IF_ERROR(
        compiler_builder.GetCheckerBuilder().AddFunction(func_translate_late));
    return absl::OkStatus();
  }

  absl::Status ConfigureRuntime(cel::RuntimeBuilder& runtime_builder,
                                const cel::RuntimeOptions& opts) override {
    using TranslateFunctionAdapter =
        ::cel::TernaryFunctionAdapter<absl::StatusOr<StringValue>,
                                      const StringValue&, const StringValue&,
                                      const StringValue&>;
    auto status = TranslateFunctionAdapter::RegisterMemberOverload(
        "translate", &Translate, runtime_builder.function_registry());
    PY_CEL_RETURN_IF_ERROR(status);

    // Register a lazy function that will be bound at evaluation time.
    using TranslateLateFunctionAdapter =
        ::cel::UnaryFunctionAdapter<absl::StatusOr<StringValue>,
                                    const StringValue&>;
    PY_CEL_RETURN_IF_ERROR(
        runtime_builder.function_registry().RegisterLazyFunction(
            TranslateLateFunctionAdapter::CreateDescriptor(
                "translate_late",
                /*receiver_style=*/false)));
    return absl::OkStatus();
  }
};

// Register the extension module. This is needed to make the extension
// discoverable in Python.
CEL_EXTENSION_MODULE(sample_cel_ext_cc, SampleCelExtension);

}  // namespace cel_ext_translate
