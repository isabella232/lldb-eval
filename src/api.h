/*
 * Copyright 2020 Google LLC
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

#ifndef LLDB_EVAL_API_H_
#define LLDB_EVAL_API_H_

#include "defines.h"
#include "lldb/API/SBFrame.h"
#include "lldb/API/SBValue.h"
#include "lldb/API/SBError.h"

namespace lldb_eval {

LLDB_EVAL_API
lldb::SBValue EvaluateExpression(lldb::SBFrame frame, const char* expression,
                                 lldb::SBError& error);

}  // namespace lldb_eval

#endif  // LLDB_EVAL_API_H_
