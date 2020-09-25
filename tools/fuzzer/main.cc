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

#include "tools/cpp/runfiles/runfiles.h"

#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <utility>

using bazel::tools::cpp::runfiles::Runfiles;

constexpr char VAR[] = "x";

constexpr char LLDB_SERVER_KEY[] = "llvm_project_local/bin/lldb-server";
constexpr char EXECUTABLE_PATH_KEY[] = "lldb_eval/testdata/fuzzer_binary";

int main(int argc, char** argv) {
  // No need to use argc when running via Bazel
  (void) argc;

  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], &error));
  if (runfiles == nullptr) {
    fprintf(stderr, "Could not launch the fuzzer: %s\n", error.c_str());
    return 1;
  }

#ifndef _WIN32
  std::string lldb_server = runfiles->Rlocation(LLDB_SERVER_KEY);
  setenv("LLDB_DEBUGSERVER_PATH", lldb_server.c_str(), 0);
#endif // !_WIN32

  std::string exe_path = runfiles->Rlocation(EXECUTABLE_PATH_KEY);
  std::string dirname_buf = exe_path;

  std::random_device rd;
  auto seed = rd();
  printf("Seed for this run is: %u\n", seed);

  const char* ARGV[] = { exe_path.c_str(), nullptr };

  lldb::SBDebugger::Initialize();
  {
    auto debugger = lldb::SBDebugger::Create();
    debugger.SetAsync(false);

    auto target = debugger.CreateTarget(exe_path.c_str());
    auto bp = target.BreakpointCreateByName("break_here", exe_path.c_str());
    // Test program does not perform any I/O, so current directory doesn't
    // matter.
    auto proc = target.LaunchSimple(ARGV, nullptr, ".");
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
