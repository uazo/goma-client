// Copyright 2012 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "gomacc_argv.h"

#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

#include <string>
#include <vector>

#include "ioutil.h"
#include "mypath.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

namespace devtools_goma {

#ifndef _WIN32
TEST(GomaccArgvTest, BuildGomaccArgvMasqueradeGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gcc", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(3, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("gcc", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_TRUE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvMasqueradeClang) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"/gomadir/clang", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(3, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("clang", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_TRUE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependBaseGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "gcc", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("gcc", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvFullPathPrependBaseGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"/gomadir/gomacc", "gcc", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("gcc", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependPathGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "path/gcc", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("path/gcc", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_EQ("path/gcc", local_command_path);
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependFullPathGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "/usr/bin/gcc", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("/usr/bin/gcc", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_EQ("/usr/bin/gcc", local_command_path);
}

TEST(GomaccArgvTest, BuildGomaccArgvFullPathPrependPathGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"/gomadir/gomacc", "path/gcc", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("path/gcc", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_EQ("path/gcc", local_command_path);
}

TEST(GomaccArgvTest, BuildGomaccArgvFullPathPrependFullPathGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"/gomadir/gomacc", "/usr/bin/gcc", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("/usr/bin/gcc", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_EQ("/usr/bin/gcc", local_command_path);
}

TEST(GomaccArgvTest, BuildGomaccArgvMasqueradeVerifyCommandGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gcc", "--goma-verify-command", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(4U, args.size());
  EXPECT_EQ("gcc", args[0]);
  EXPECT_EQ("--goma-verify-command", args[1]);
  EXPECT_EQ("-c", args[2]);
  EXPECT_EQ("hello.c", args[3]);
  EXPECT_TRUE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependVerifyCommandGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "--goma-verify-command",
                        "gcc", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(5, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("gcc", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_EQ("all", verify_command);
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependVerifyCommandVersionGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "--goma-verify-command=version",
                        "gcc", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(5, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("gcc", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_EQ("version", verify_command);
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependVerifyCommandChecksumFullPathGcc) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "--goma-verify-command=checksum",
                        "/usr/bin/gcc", "-c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(5, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("/usr/bin/gcc", args[0]);
  EXPECT_EQ("-c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_EQ("checksum", verify_command);
  EXPECT_EQ("/usr/bin/gcc", local_command_path);
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependFlag) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "-c", "hello.c"};

  EXPECT_FALSE(BuildGomaccArgv(3, argv,
                               &args, &masquerade_mode,
                               &verify_command, &local_command_path));
}

TEST(GomaccArgvTest, BuildGomaccArgvMasqueradeNoCompiler) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"echo", "test"};

  EXPECT_TRUE(BuildGomaccArgv(2, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(2U, args.size());
  EXPECT_EQ("echo", args[0]);
  EXPECT_EQ("test", args[1]);
  EXPECT_TRUE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvMasqueradeFullPathNoCompiler) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"/gomadir/echo", "test"};

  EXPECT_TRUE(BuildGomaccArgv(2, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(2U, args.size());
  EXPECT_EQ("echo", args[0]);
  EXPECT_EQ("test", args[1]);
  EXPECT_TRUE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependBaseNoCompiler) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "echo", "test"};

  EXPECT_TRUE(BuildGomaccArgv(3, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(2U, args.size());
  EXPECT_EQ("echo", args[0]);
  EXPECT_EQ("test", args[1]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependoCompiler) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "/bin/echo", "test"};

  EXPECT_TRUE(BuildGomaccArgv(3, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(2U, args.size());
  EXPECT_EQ("/bin/echo", args[0]);
  EXPECT_EQ("test", args[1]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_EQ("/bin/echo", local_command_path);
}

#else  // _WIN32
TEST(GomaccArgvTest, BuildGomaccArgvMasqueradeCl) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"c:\\gomadir\\cl.exe", "/c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(3, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("cl.exe", args[0]);
  EXPECT_EQ("/c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_TRUE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependBaseCl) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc.exe", "cl", "/c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("cl", args[0]);
  EXPECT_EQ("/c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvFullPathPrependBaseCl) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"c:\\gomadir\\gomacc.exe", "cl", "/c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("cl", args[0]);
  EXPECT_EQ("/c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_TRUE(local_command_path.empty());
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependPathCl) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "path\\cl", "/c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("path\\cl", args[0]);
  EXPECT_EQ("/c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_EQ("path\\cl", local_command_path);
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependFullPathCl) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "c:\\vc\\bin\\cl", "/c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("c:\\vc\\bin\\cl", args[0]);
  EXPECT_EQ("/c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_EQ("c:\\vc\\bin\\cl", local_command_path);
}

TEST(GomaccArgvTest, BuildGomaccArgvFullPathPrependPathCl) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"c:\\gomadir\\gomacc", "path\\cl", "/c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("path\\cl", args[0]);
  EXPECT_EQ("/c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_EQ("path\\cl", local_command_path);
}

TEST(GomaccArgvTest, BuildGomaccArgvFullPathPrependFullPathCl) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"c:\\gomadir\\gomacc",
                        "c:\\vc\\bin\\cl", "/c", "hello.c"};

  EXPECT_TRUE(BuildGomaccArgv(4, argv,
                              &args, &masquerade_mode,
                              &verify_command, &local_command_path));
  EXPECT_EQ(3U, args.size());
  EXPECT_EQ("c:\\vc\\bin\\cl", args[0]);
  EXPECT_EQ("/c", args[1]);
  EXPECT_EQ("hello.c", args[2]);
  EXPECT_FALSE(masquerade_mode);
  EXPECT_TRUE(verify_command.empty());
  EXPECT_EQ("c:\\vc\\bin\\cl", local_command_path);
}

TEST(GomaccArgvTest, BuildGomaccArgvPrependNoCl) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc", "/c", "hello.c"};

  EXPECT_FALSE(BuildGomaccArgv(3, argv,
                               &args, &masquerade_mode,
                               &verify_command, &local_command_path));
}
#endif  // _WIN32

TEST(GomaccArgvTest, BuildGomaccArgvNoCompiler) {
  std::vector<std::string> args;
  bool masquerade_mode;
  std::string verify_command;
  std::string local_command_path;
  const char* argv[] = {"gomacc"};

  EXPECT_FALSE(BuildGomaccArgv(1, argv,
                               &args, &masquerade_mode,
                               &verify_command, &local_command_path));
}

#ifdef _WIN32

TEST(GomaccArgvTest, BuildArgsForInput) {
  std::vector<std::string> args_no_input;
  args_no_input.push_back("/c");
  args_no_input.push_back("/DFOO=\"foo.h\"");
  args_no_input.push_back("/Ic:\\vc\\include");
  args_no_input.push_back("/Fo..\\obj\\");
  args_no_input.push_back("/Fdfoo.pdb");
  args_no_input.push_back("/MP");

  std::string cmdline = BuildArgsForInput(args_no_input, "foo.cpp");
  EXPECT_EQ("\"/c\" \"/DFOO=\\\"foo.h\\\"\" \"/Ic:\\vc\\include\" "
            "\"/Fo..\\obj\\\\\" \"/Fdfoo.pdb\" \"/MP\" \"foo.cpp\"", cmdline);
}

TEST(GomaccArgvTest, EscapeWinArg) {
  EXPECT_EQ("\"foo\"", EscapeWinArg("foo"));
  EXPECT_EQ("\"foo\\bar\"", EscapeWinArg("foo\\bar"));
  EXPECT_EQ("\"foo bar\"", EscapeWinArg("foo bar"));
  EXPECT_EQ("\"foo=\\\"bar\\\"\"", EscapeWinArg("foo=\"bar\""));
  EXPECT_EQ("\"foo\\\\\"", EscapeWinArg("foo\\"));
  EXPECT_EQ("\"foo\\\\\\\"", EscapeWinArg("foo\\\\"));
}

#endif

}  // namespace devtools_goma
