// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "java/java_compiler_info_builder.h"

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "base/path.h"
#include "counterz.h"
#include "env_flags.h"
#include "glog/logging.h"
#include "glog/stl_logging.h"
#include "ioutil.h"
#include "util.h"

GOMA_DECLARE_bool(SEND_COMPILER_BINARY_AS_INPUT);

namespace devtools_goma {

namespace {

void AddJavaLibraries(const std::string& compiler_path,
                      const std::string& cwd,
                      CompilerInfoData* data) {
  // TODO: Verify that this is the smallest set of files needed to
  // run javac.
  const auto lib_dir = file::JoinPath(file::Dirname(compiler_path), "../lib");
  constexpr absl::string_view kJavaLibs[] = {
      "jfr/default.jfc", "jfr/profile.jfc",

      "jli/libjli.so",

      "security/blacklisted.certs", "security/cacerts",
      "security/default.policy", "security/public_suffix_list.dat",

      "server/libjsig.so", "server/libjvm.so", "server/Xusage.txt",

      "libattach.so", "libawt_headless.so", "libawt.so", "libawt_xawt.so",
      "libdt_socket.so", "libextnet.so", "libfontmanager.so",
      "libinstrument.so", "libj2gss.so", "libj2pcsc.so", "libj2pkcs11.so",
      "libjaas.so", "libjavajpeg.so", "libjava.so", "libjawt.so", "libjdwp.so",
      "libjimage.so", "libjsig.so", "libjsound.so", "liblcms.so",
      "libmanagement_agent.so", "libmanagement_ext.so", "libmanagement.so",
      "libmlib_image.so", "libnet.so", "libnio.so", "libprefs.so", "librmi.so",
      "libsaproc.so", "libsctp.so", "libsplashscreen.so", "libsunec.so",
      "libunpack.so", "libverify.so", "libzip.so",

      // Other files
      "classlist", "ct.sym", "jexec", "jrt-fs.jar", "jvm.cfg", "modules",
      "psfontj2d.properties", "psfont.properties.ja", "tzdb.dat",

      // Files that are excluded:
      // - src.zip
  };

  for (const auto lib_file : kJavaLibs) {
    JavacCompilerInfoBuilder::AddResourceAsExecutableBinary(
        file::JoinPath(lib_dir, lib_file), cwd, data);
  }
}

}  // namespace

void JavacCompilerInfoBuilder::SetLanguageExtension(
    CompilerInfoData* data) const {
  (void)data->mutable_javac();
}

void JavacCompilerInfoBuilder::SetTypeSpecificCompilerInfo(
    const CompilerFlags& flags,
    const std::string& local_compiler_path,
    const std::string& abs_local_compiler_path,
    const std::vector<std::string>& compiler_info_envs,
    CompilerInfoData* data) const {
  // TODO: Check for Python wrapper for javac, and set
  // |real_javac_path| to the wrapper path instead of |real_compiler_path|.
  const std::string real_javac_path = data->real_compiler_path();
  if (!GetJavacVersion(real_javac_path, compiler_info_envs, flags.cwd(),
                       data->mutable_version())) {
    AddErrorMessage("Failed to get java version for " + real_javac_path, data);
    LOG(ERROR) << data->error_message();
    return;
  }
  data->set_target("java");

  if (FLAGS_SEND_COMPILER_BINARY_AS_INPUT) {
    // TODO: Add Python wrapper if it is being used.
    AddResourceAsExecutableBinary(real_javac_path, flags.cwd(), data);

    // Add libs.
    AddJavaLibraries(real_javac_path, flags.cwd(), data);
  }
}

// static
bool JavacCompilerInfoBuilder::ParseJavacVersion(
    const std::string& version_info,
    std::string* version) {
  version->assign(
      std::string(absl::StripTrailingAsciiWhitespace(version_info)));
  static const char kJavac[] = "javac ";
  static const size_t kJavacLength = sizeof(kJavac) - 1;  // Removed '\0'.
  if (!absl::StartsWith(*version, kJavac)) {
    LOG(ERROR) << "Unable to parse javac -version output:"
               << *version;
    return false;
  }
  version->erase(0, kJavacLength);
  return true;
}

// static
bool JavacCompilerInfoBuilder::GetJavacVersion(
    const std::string& javac,
    const std::vector<std::string>& compiler_info_envs,
    const std::string& cwd,
    std::string* version) {
  std::vector<std::string> argv;
  argv.push_back(javac);
  argv.push_back("-version");
  std::vector<std::string> env(compiler_info_envs);
  env.push_back("LC_ALL=C");
  int32_t status = 0;
  std::string javac_out;
  {
    GOMA_COUNTERZ("ReadCommandOutput(version)");
    javac_out =
        ReadCommandOutput(javac, argv, env, cwd, MERGE_STDOUT_STDERR, &status);
  }
  bool ret = ParseJavacVersion(javac_out, version);
  LOG_IF(ERROR, status != 0)
      << "ReadCommandOutput exited with non zero status code."
      << " javac=" << javac
      << " status=" << status
      << " argv=" << argv
      << " env=" << env
      << " cwd=" << cwd;
  return ret;
}

void JavaCompilerInfoBuilder::SetLanguageExtension(
    CompilerInfoData* data) const {
  (void)data->mutable_java();
  LOG(ERROR) << "java is not supported";
}

void JavaCompilerInfoBuilder::SetTypeSpecificCompilerInfo(
    const CompilerFlags& flags,
    const std::string& local_compiler_path,
    const std::string& abs_local_compiler_path,
    const std::vector<std::string>& compiler_info_envs,
    CompilerInfoData* data) const {}

bool JavacCompilerInfoBuilder::AddResourceAsExecutableBinary(
    const std::string& resource_path,
    const std::string& cwd,
    CompilerInfoData* data) {
  CompilerInfoData::ResourceInfo r;
  if (!CompilerInfoBuilder::ResourceInfoFromPath(
          cwd, resource_path, CompilerInfoData::EXECUTABLE_BINARY, &r)) {
    CompilerInfoBuilder::AddErrorMessage(
        "failed to get resource info for " + resource_path, data);
    LOG(ERROR) << "failed to get resource info for " + resource_path;
    return false;
  }

  if (r.symlink_path().empty()) {
    // Not a symlink, add it as a resource directly.
    *data->add_resource() = std::move(r);
    return true;
  }
  // TODO: handle symlinks?
  return false;
}

}  // namespace devtools_goma
