#if defined(HAVE_LLVM)

#include <memory>
#include <utility>

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "llvm_propeller_options.pb.h"
#include "llvm_propeller_options_builder.h"
#include "llvm_propeller_profile_computer.h"
#include "llvm_propeller_program_cfg.h"
#include "llvm_propeller_program_cfg_proto_builder.h"
#include "third_party/abseil/absl/flags/flag.h"
#include "third_party/abseil/absl/log/check.h"
#include "third_party/abseil/absl/status/statusor.h"
#include "third_party/abseil/absl/flags/parse.h"
#include "third_party/abseil/absl/flags/usage.h"

DEFINE_string(binary, "a.out", "Binary file name");
DEFINE_string(profile, "perf.data", "Input profile file name");
DEFINE_string(propeller_protobuf_out, "",
              "Instruct propeller to output a protobuf file which can reused "
              "as testdata input for the code layout algorithm.");
DEFINE_string(profiled_binary_name, "",
              "Name specified to compare against perf mmap_events. Same as the "
              "option in create_llvm_prof.");
DEFINE_bool(ignore_build_id, false,
            "Ignore build id, use file name to match data in perfdata file. "
            "Same as the option in create_llvm_prof.");

using ::devtools_crosstool_autofdo::ProgramCfg;
using ::devtools_crosstool_autofdo::ProgramCfgProtoBuilder;
using ::devtools_crosstool_autofdo::PropellerOptions;
using ::devtools_crosstool_autofdo::PropellerOptionsBuilder;
using ::devtools_crosstool_autofdo::PropellerProfileComputer;

// This binary is used to generate text protobuf format files for use as
// testdata for codelayout tests. Example usage:
// $ blaze run -c opt :llvm_propeller_generate_testdata -- \
// --binary=`pwd`/testdata/propeller_sample.bin \
// --profiled_binary_name="propeller_sample.bin" \
// --profile=`pwd`/testdata/propeller_sample.perfdata \
// --propeller_protobuf_out=$HOME/tmp/propeller_sample.pb
int main(int argc, char **argv) {
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);

  if (absl::GetFlag(FLAGS_propeller_protobuf_out).empty()) {
    LOG(ERROR) << "Path to output protobuf must be specified.";
    return 1;
  }

  PropellerOptions options(
      PropellerOptionsBuilder()
          .SetBinaryName(absl::GetFlag(FLAGS_binary))
          .AddPerfNames(absl::GetFlag(FLAGS_profile))
          .SetProfiledBinaryName(absl::GetFlag(FLAGS_profiled_binary_name))
          .SetIgnoreBuildId(absl::GetFlag(FLAGS_ignore_build_id)));

  absl::StatusOr<std::unique_ptr<PropellerProfileComputer>> profile_computer =
      PropellerProfileComputer::Create(options);
  CHECK_OK(profile_computer);

  absl::StatusOr<std::unique_ptr<ProgramCfg>> program_cfg =
      (*profile_computer)->GetProgramCfg();
  if (!program_cfg.ok()) {
    LOG(ERROR) << "Could not create cfs for whole program.";
    return 1;
  }

  ProgramCfgProtoBuilder program_cfg_proto_builder;
  if (!program_cfg_proto_builder.InitWriter(
          absl::GetFlag(FLAGS_propeller_protobuf_out))) {
    LOG(ERROR) << "Could not initialize protobuf writer.";
    return 1;
  }
  program_cfg_proto_builder.AddCfgs(
      std::move(**std::move(program_cfg)).release_cfgs_by_index());
  if (!program_cfg_proto_builder.WriteAndClose()) {
    LOG(ERROR) << "Could not write testdata protobuf file.";
    return 1;
  }
  return 0;
}

#else
#include <stdio.h>
#include "third_party/abseil/absl/flags/parse.h"
#include "third_party/abseil/absl/flags/usage.h"
int main(int argc, char **argv) {
  fprintf(stderr,
          "ERROR: LLVM support was not enabled in this configuration.\nPlease "
          "configure and rebuild with:\n\n$ ./configure "
          "--with-llvm=<path-to-llvm-config>\n\n");
  return -1;
}
#endif  // HAVE_LLVM }
