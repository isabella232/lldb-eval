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

#include "ast.h"
#include "expr_gen.h"

#include "lldb/API/SBBreakpoint.h"
#include "lldb/API/SBDebugger.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBTarget.h"
#include "lldb/API/SBThread.h"
#include "lldb/API/SBValue.h"

#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <utility>

#include <libgen.h>

constexpr char VAR[] = "x";
constexpr char TARGET_TRIPLE[] = "x86_64-linux-gnu";

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s executable-path\n", argv[0]);
    return 1;
  }

  if (*argv[1] == '\0') {
    fprintf(stderr, "Empty string passed for the executable path\n");
    return 1;
  }

  const auto* exe_path = argv[1];
  std::string dirname_buf = exe_path;
  std::string basename_buf = exe_path;

  // `dirname()` and `basename()` need not be reentrant, hence the copying
  std::string exe_dir = dirname(&dirname_buf[0]);
  std::string exe_name = basename(&basename_buf[0]);

  std::random_device rd;
  auto seed = rd();
  printf("Seed for this run is: %u\n", seed);

  const char* ARGV[] = { exe_name.c_str(), nullptr };

  lldb::SBDebugger::Initialize();
  {
    auto debugger = lldb::SBDebugger::Create();
    debugger.SetAsync(false);

    auto target = debugger.CreateTargetWithFileAndTargetTriple(exe_path,
                                                               TARGET_TRIPLE);
    auto bp = target.BreakpointCreateByName("break_here", exe_name.c_str());
    auto proc = target.LaunchSimple(ARGV, nullptr, exe_dir.c_str());
    auto thread = proc.GetSelectedThread();

    auto frame = thread.SetSelectedFrame(1);
    auto var_value = frame.GetValueForVariablePath(VAR);
    printf("Value of variable `%s` is: %s\n", VAR, var_value.GetValue());

    eval_fuzzer::ExprGenerator gen(seed);
    for (int i = 0; i < 20; i++) {
      auto gen_expr = gen.generate();
      auto str = stringify_expr(gen_expr);

      auto expr_value = frame.EvaluateExpression(str.c_str());
      printf("Evaluation of expression `%s` yields: %s\n", str.c_str(), expr_value.GetValue());
    }
    thread.Resume();
  }
  lldb::SBDebugger::Terminate();

  return 0;
}
