/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_PYTHON_STATUS_MACROS_H_
#define THIRD_PARTY_CEL_PYTHON_STATUS_MACROS_H_

#include "absl/base/optimization.h"
#include "absl/status/status.h"

#define CEL_PYTHON_RETURN_IF_ERROR(expr)  \
  {                                       \
    absl::Status __CEL_STATUS__ = (expr); \
    if (!__CEL_STATUS__.ok()) {           \
      return __CEL_STATUS__;              \
    }                                     \
  }

#define CEL_PYTHON_ASSIGN_OR_RETURN_(statusor, lhs, expr) \
  auto statusor = (expr);                                 \
  if (ABSL_PREDICT_FALSE(!statusor.ok())) {               \
    return statusor.status();                             \
  }                                                       \
  lhs = std::move(statusor).value()

#define CEL_PYTHON_ASSIGN_OR_RETURN_CONCAT_HELPER_(x, y) x##y
#define CEL_PYTHON_ASSIGN_OR_RETURN_CONCAT_(x, y) \
  CEL_PYTHON_ASSIGN_OR_RETURN_CONCAT_HELPER_(x, y)

#define CEL_PYTHON_ASSIGN_OR_RETURN(lval, expr)                             \
  CEL_PYTHON_ASSIGN_OR_RETURN_(                                             \
      CEL_PYTHON_ASSIGN_OR_RETURN_CONCAT_(status_or_value, __LINE__), lval, \
      expr)

#endif  // THIRD_PARTY_CEL_PYTHON_STATUS_MACROS_H_
