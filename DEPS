# Copyright 2019 Google LLC.

vars = {
     "chromium_git": "https://chromium.googlesource.com",
}

deps = {
     # protobuf > 3.9.1
     # TODO: use released proto including
     # https://github.com/protocolbuffers/protobuf/blob/ee4f2492ea4e7ff120f68a792af870ee30435aa5/src/google/protobuf/io/zero_copy_stream.h#L122
     # Note: When you update protobuf, you will need to update
     # test/goma_data.pb.{h,cc}. Copying them from your output directory should
     # work.
     "client/third_party/protobuf/protobuf":
     "https://github.com/google/protobuf.git@7bff8393cab939bfbb9b5c69b3fe76b4d83c41ee",

     # google-glog v0.4.0
     "client/third_party/glog":
     "https://github.com/google/glog.git@96a2f23dca4cc7180821ca5f32e526314395d26a",

     # googletest 1.8.1
     "client/third_party/gtest":
     Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' +
         '2fe3bd994b3189899d93f1d5a881e725e046fdc2',

     # zlib
     "client/third_party/zlib":
     "https://chromium.googlesource.com/chromium/src/third_party/zlib@1337da5314a9716c0653301cceeb835d17fd7ea4",

     # xz v5.2.0
     "client/third_party/xz":
     "https://goma.googlesource.com/xz.git@fbafe6dd0892b04fdef601580f2c5b0e3745655b",

     # jsoncpp
     "client/third_party/jsoncpp/source":
     Var("chromium_git") + '/external/github.com/open-source-parsers/jsoncpp.git@f572e8e42e22cfcf5ab0aea26574f408943edfa4', # from svn 248

     # chrome's tools/clang
     "client/tools/clang":
     "https://chromium.googlesource.com/chromium/src/tools/clang.git",

     # chrome's deps/third_party/boringssl
     "client/third_party/boringssl/src":
     "https://boringssl.googlesource.com/boringssl@706da620b248615b709e71b36a31312f87a2c692",

     # google-breakpad
     "client/third_party/breakpad/breakpad":
     Var("chromium_git") + "/breakpad/breakpad.git@" +
         "a0e078365d0515f4ffdfc3389d92b2c062f62132",

     # lss
     "client/third_party/lss":
     Var("chromium_git") + "/linux-syscall-support.git@" +
         "a89bf7903f3169e6bc7b8efc10a73a7571de21cf",

     # nasm
     "client/third_party/nasm":
     Var("chromium_git") + "/chromium/deps/nasm.git@" +
         "ae8e4ca1c64c861de419c93385a0fc66a39141e2",

     # chromium's buildtools containing libc++, libc++abi, clang_format and gn.
     "client/buildtools":
     Var("chromium_git") + "/chromium/src/buildtools@" +
         "74cfb57006f83cfe050817526db359d5c8a11628",

     # libFuzzer
     "client/third_party/libFuzzer/src":
     Var("chromium_git") +
         "/chromium/llvm-project/compiler-rt/lib/fuzzer.git@" +
         "b9f51dc8c98065df0c8da13c051046f5bab833db",

     # libprotobuf-mutator
     "client/third_party/libprotobuf-mutator/src":
     Var("chromium_git") +
         "/external/github.com/google/libprotobuf-mutator.git@" +
         "439e81f8f4847ec6e2bf11b3aa634a5d8485633d",

     # abseil
     "client/third_party/abseil/src":
     "https://github.com/abseil/abseil-cpp.git@3e2e9b5557e76d098de4b8a2a659125b98ca519b",

     # google benchmark v1.4.1
     "client/third_party/benchmark/src":
     "https://github.com/google/benchmark.git@e776aa0275e293707b6a0901e0e8d8a8a3679508",

     # Jinja2 template engine v2.10
     "client/third_party/jinja2":
     "https://github.com/pallets/jinja.git@78d2f672149e5b9b7d539c575d2c1bfc12db67a9",

     # Markupsafe module v1.0
     "client/third_party/markupsafe":
     "https://github.com/pallets/markupsafe.git@d2a40c41dd1930345628ea9412d97e159f828157",

     # depot_tools
     'client/third_party/depot_tools':
     Var('chromium_git') + '/chromium/tools/depot_tools.git',

     # gflags 2.2.1
     "client/third_party/gflags/src":
     "https://github.com/gflags/gflags.git@46f73f88b18aee341538c0dfc22b1710a6abedef",

     # subprocess32 3.5.3
     "client/third_party/subprocess32":
     "https://github.com/google/python-subprocess32@" +
     "0a814da4a033875880534fd488770e2d97febe2f",

     # libyaml dist-0.2.2
     "client/third_party/libyaml/src":
     "https://github.com/yaml/libyaml.git@d407f6b1cccbf83ee182144f39689babcb220bd6",

     # chromium's build.
     "client/third_party/chromium_build":
     "https://chromium.googlesource.com/chromium/src/build/@3fe260c8e2f6ccb84e1e43ab04b321328fb8998c",
}

hooks = [
     # Download to make Linux Goma client linked with an old libc.
     {
       'name': 'sysroot_x64',
       'pattern': '.',
       'condition': 'checkout_linux',
       'action': [
         'python',
         ('client/third_party/chromium_build/linux/sysroot_scripts/'
          'install-sysroot.py'),
          '--arch=x64',
       ],
     },

     # Update the Windows toolchain if necessary. Must run before 'clang' below.
     {
       'name': 'win_toolchain',
       'pattern': '.',
       'action': ['python', 'client/build/vs_toolchain.py', 'update'],
     },
     {
       "name": "clang",
       "pattern": ".",
       "action": ["python", "client/tools/clang/scripts/update.py"],
     },

     # Pull binutils for linux, it is used for simpletry test.
     {
       "name": "binutils",
       "pattern": ".",
       "action": [
         "python",
         "client/test/third_party/binutils/download.py",
       ],
     },

     {
       # Update LASTCHANGE.
       'name': 'lastchange',
       'pattern': '.',
       'action': ['python', 'client/build/util/lastchange.py',
                  '-o', 'client/build/util/LASTCHANGE'],
     },

     # Pull clang-format binaries using checked-in hashes.
     {
         'name': 'clang_format_win',
         'pattern': '.',
         'condition': 'host_os == "win"',
         'action': [ 'download_from_google_storage',
                     '--no_resume',
                     '--no_auth',
                     '--bucket', 'chromium-clang-format',
                     '-s', 'client/buildtools/win/clang-format.exe.sha1',
         ],
     },
     {
         'name': 'clang_format_mac',
         'pattern': '.',
         'condition': 'host_os == "mac"',
         'action': [ 'download_from_google_storage',
                     '--no_resume',
                     '--no_auth',
                     '--bucket', 'chromium-clang-format',
                     '-s', 'client/buildtools/mac/clang-format.sha1',
         ],
     },
     {
         'name': 'clang_format_linux',
         'pattern': '.',
         'condition': 'host_os == "linux"',
         'action': [ 'download_from_google_storage',
                     '--no_resume',
                     '--no_auth',
                     '--bucket', 'chromium-clang-format',
                     '-s', 'client/buildtools/linux64/clang-format.sha1',
         ],
     },
     # Update the Mac toolchain if necessary.
     {
       'name': 'mac_toolchain',
       'pattern': '.',
       'condition': 'checkout_ios or checkout_mac',
       'action': ['python', 'client/build/mac_toolchain.py'],
     },

     # Ensure that the DEPS'd "depot_tools" has its self-update capability
     # disabled.
     {
       'name': 'disable_depot_tools_selfupdate',
       'pattern': '.',
       'action': [
         'python',
         'client/third_party/depot_tools/update_depot_tools_toggle.py',
         '--disable',
       ],
     },
]

recursedeps = [
  # buildtools provides clang_format, libc++, and libc++abi
  'client/buildtools',
]
