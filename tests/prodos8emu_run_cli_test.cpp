#include <sys/wait.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

struct CommandResult {
  int         exit_code = -1;
  std::string output;
};

static std::string shell_quote(const std::string& text) {
  std::string quoted = "'";
  for (char ch : text) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted += ch;
    }
  }
  quoted += "'";
  return quoted;
}

static CommandResult run_command_capture(const fs::path&                 executable,
                                         const std::vector<std::string>& args) {
  CommandResult result;

  std::string command = shell_quote(executable.string());
  for (const std::string& arg : args) {
    command += " ";
    command += shell_quote(arg);
  }
  command += " 2>&1";

  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) {
    result.output = "Failed to spawn subprocess";
    return result;
  }

  std::array<char, 256> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    result.output += buffer.data();
  }

  int status = pclose(pipe);
  if (status == -1) {
    result.output += "\nFailed to read subprocess exit status";
    return result;
  }

  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  }

  return result;
}

static CommandResult run_command_capture_in_dir(const fs::path&                 working_dir,
                                                const fs::path&                 executable,
                                                const std::vector<std::string>& args) {
  CommandResult result;

  std::string command = "cd ";
  command += shell_quote(working_dir.string());
  command += " && ";
  command += shell_quote(executable.string());
  for (const std::string& arg : args) {
    command += " ";
    command += shell_quote(arg);
  }
  command += " 2>&1";

  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) {
    result.output = "Failed to spawn subprocess";
    return result;
  }

  std::array<char, 256> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    result.output += buffer.data();
  }

  int status = pclose(pipe);
  if (status == -1) {
    result.output += "\nFailed to read subprocess exit status";
    return result;
  }

  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  }

  return result;
}

static fs::path create_test_temp_dir() {
  fs::path          base    = fs::temp_directory_path();
  std::string       pattern = (base / "prodos8emu_run_cli_test_XXXXXX").string();
  std::vector<char> writable(pattern.begin(), pattern.end());
  writable.push_back('\0');

  char* created = mkdtemp(writable.data());
  if (created == nullptr) {
    return {};
  }

  return fs::path(created);
}

static bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

static fs::path resolve_runner_path(const char* argv0) {
  fs::path test_exe_path = fs::absolute(fs::path(argv0));
  fs::path test_exe_dir  = test_exe_path.parent_path();

  fs::path candidate = test_exe_dir / "prodos8emu_run";
  if (fs::exists(candidate)) {
    return candidate;
  }

  candidate = fs::current_path() / "prodos8emu_run";
  if (fs::exists(candidate)) {
    return candidate;
  }

  // Return the most likely location for debugging if the binary is missing.
  return test_exe_dir / "prodos8emu_run";
}

static void runner_help_lists_jsr_rts_trace_flag(const fs::path& runner_path, int& failures) {
  std::cout << "Test 1: runner_help_lists_jsr_rts_trace_flag\n";

  CommandResult result = run_command_capture(runner_path, {"--help"});

  bool ok = result.exit_code == 0 && contains(result.output, "--jsr-rts-trace");
  if (!ok) {
    std::cerr << "FAIL: Help output must list --jsr-rts-trace (exit=" << result.exit_code << ")\n";
    std::cerr << "Output:\n" << result.output << "\n";
    failures++;
  } else {
    std::cout << "PASS: runner_help_lists_jsr_rts_trace_flag\n";
  }
}

static void runner_help_lists_disassembly_trace_flag(const fs::path& runner_path, int& failures) {
  std::cout << "Test 2: runner_help_lists_disassembly_trace_flag\n";

  CommandResult result = run_command_capture(runner_path, {"--help"});

  bool ok = result.exit_code == 0 && contains(result.output, "--disassembly-trace");
  if (!ok) {
    std::cerr << "FAIL: Help output must list --disassembly-trace (exit=" << result.exit_code
              << ")\n";
    std::cerr << "Output:\n" << result.output << "\n";
    failures++;
  } else {
    std::cout << "PASS: runner_help_lists_disassembly_trace_flag\n";
  }
}

static void runner_rejects_unknown_option_contract_stable(const fs::path& runner_path,
                                                          int&            failures) {
  std::cout << "Test 3: runner_rejects_unknown_option_contract_stable\n";

  const std::string unknown_option = "--not-a-real-runner-option";
  CommandResult     result         = run_command_capture(runner_path, {unknown_option});

  bool ok = result.exit_code != 0 &&
            contains(result.output, "Error: Unknown option: " + unknown_option) &&
            contains(result.output, "Usage:");

  if (!ok) {
    std::cerr << "FAIL: Unknown option contract changed (exit=" << result.exit_code << ")\n";
    std::cerr << "Output:\n" << result.output << "\n";
    failures++;
  } else {
    std::cout << "PASS: runner_rejects_unknown_option_contract_stable\n";
  }
}

static void runner_accepts_jsr_rts_trace_flag(const fs::path& runner_path, int& failures) {
  std::cout << "Test 4: runner_accepts_jsr_rts_trace_flag\n";

  CommandResult result = run_command_capture(runner_path, {"--jsr-rts-trace", "--help"});

  bool ok = result.exit_code == 0 && contains(result.output, "--jsr-rts-trace") &&
            !contains(result.output, "Unknown option");

  if (!ok) {
    std::cerr << "FAIL: --jsr-rts-trace should be accepted by the runner (exit=" << result.exit_code
              << ")\n";
    std::cerr << "Output:\n" << result.output << "\n";
    failures++;
  } else {
    std::cout << "PASS: runner_accepts_jsr_rts_trace_flag\n";
  }
}

static void runner_accepts_disassembly_trace_flag(const fs::path& runner_path, int& failures) {
  std::cout << "Test 5: runner_accepts_disassembly_trace_flag\n";

  CommandResult result = run_command_capture(runner_path, {"--disassembly-trace", "--help"});

  bool ok = result.exit_code == 0 && contains(result.output, "--disassembly-trace") &&
            !contains(result.output, "Unknown option");

  if (!ok) {
    std::cerr << "FAIL: --disassembly-trace should be accepted by the runner (exit="
              << result.exit_code << ")\n";
    std::cerr << "Output:\n" << result.output << "\n";
    failures++;
  } else {
    std::cout << "PASS: runner_accepts_disassembly_trace_flag\n";
  }
}

static void runner_disassembly_trace_creates_dedicated_log_without_debug(
    const fs::path& runner_path, int& failures) {
  std::cout << "Test 6: runner_disassembly_trace_creates_dedicated_log_without_debug\n";

  const fs::path temp_dir = create_test_temp_dir();
  if (temp_dir.empty()) {
    std::cerr << "FAIL: could not create temp directory for disassembly trace test\n";
    failures++;
    return;
  }

  CommandResult result = run_command_capture_in_dir(
      temp_dir, runner_path, {"--disassembly-trace", "missing.rom", "missing.system"});

  const bool disassembly_log_exists = fs::exists(temp_dir / "prodos8emu_disassembly_trace.log");
  const bool mli_log_exists         = fs::exists(temp_dir / "prodos8emu_mli.log");
  const bool cout_log_exists        = fs::exists(temp_dir / "prodos8emu_cout.log");
  const bool trace_log_exists       = fs::exists(temp_dir / "prodos8emu_trace.log");

  const bool ok = result.exit_code != 0 && disassembly_log_exists && !mli_log_exists &&
                  !cout_log_exists && !trace_log_exists;

  if (!ok) {
    std::cerr << "FAIL: --disassembly-trace must create prodos8emu_disassembly_trace.log "
                 "without creating debug logs (exit="
              << result.exit_code << ")\n";
    std::cerr << "  disassembly_log_exists=" << (disassembly_log_exists ? "yes" : "no") << "\n";
    std::cerr << "  mli_log_exists=" << (mli_log_exists ? "yes" : "no") << "\n";
    std::cerr << "  cout_log_exists=" << (cout_log_exists ? "yes" : "no") << "\n";
    std::cerr << "  trace_log_exists=" << (trace_log_exists ? "yes" : "no") << "\n";
    std::cerr << "Output:\n" << result.output << "\n";
    failures++;
  } else {
    std::cout << "PASS: runner_disassembly_trace_creates_dedicated_log_without_debug\n";
  }

  std::error_code remove_ec;
  fs::remove_all(temp_dir, remove_ec);
}

int main(int argc, char* argv[]) {
  int failures = 0;

  if (argc < 1) {
    std::cerr << "FAIL: argc must be >= 1\n";
    return EXIT_FAILURE;
  }

  const fs::path runner_path = resolve_runner_path(argv[0]);
  if (!fs::exists(runner_path)) {
    std::cerr << "FAIL: prodos8emu_run binary not found at " << runner_path << "\n";
    return EXIT_FAILURE;
  }

  runner_help_lists_jsr_rts_trace_flag(runner_path, failures);
  runner_help_lists_disassembly_trace_flag(runner_path, failures);
  runner_rejects_unknown_option_contract_stable(runner_path, failures);
  runner_accepts_jsr_rts_trace_flag(runner_path, failures);
  runner_accepts_disassembly_trace_flag(runner_path, failures);
  runner_disassembly_trace_creates_dedicated_log_without_debug(runner_path, failures);

  if (failures == 0) {
    std::cout << "\nAll tests passed!\n";
    return EXIT_SUCCESS;
  }

  std::cerr << "\n" << failures << " test(s) failed!\n";
  return EXIT_FAILURE;
}
