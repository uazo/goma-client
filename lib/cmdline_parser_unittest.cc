// Copyright 2012 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "lib/cmdline_parser.h"

#include "glog/logging.h"
#include "gtest/gtest.h"

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvSimple) {
  const std::string input = "a b c";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b");
  expected_argv.push_back("c");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvWithQuote) {
  const std::string input = "a \"b \" \'c \'";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b ");
  expected_argv.push_back("c ");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvWithQuoteInDifferentQuote) {
  const std::string input = "a \"b \' \" \'c \" \'";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b \' ");
  expected_argv.push_back("c \" ");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvNoCloseQuoteAfterBackslash) {
  const std::string input = "a \"b \\\" \" \'c \\\'";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b \" ");
  expected_argv.push_back("c \\");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvKeepNonEscapeInDQuote) {
  const std::string input = "a \"b \\c \" \"d\\?e\" f";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b \\c ");
  expected_argv.push_back("d\\?e");
  expected_argv.push_back("f");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvConjunctSpaceWithBackslash) {
  const std::string input = "a b\\ c d";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b c");
  expected_argv.push_back("d");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvKeepCharAfterBackslashAsIs) {
  const std::string input = "a b\\c d";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("bc");
  expected_argv.push_back("d");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvBackslashAfterBackslash) {
  const std::string input = "a b\\\\c d";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b\\c");
  expected_argv.push_back("d");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvIgnoreEndlAfterBackslash) {
  const std::string input = "a b\\\nc d";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("bc");
  expected_argv.push_back("d");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvConjunctCharAfterBackslash) {
  const std::string input = "a b\\ \"c \" \"d \"\\ e f\\ \' g \'\\ h i";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b c ");
  expected_argv.push_back("d  e");
  expected_argv.push_back("f  g  h");
  expected_argv.push_back("i");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvBackslashEndlInQuote) {
  const std::string input = "a \"b\\\nc\" \'d\\\ne\' f";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("bc");
  expected_argv.push_back("d\\\ne");
  expected_argv.push_back("f");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvSingleBackslashInQuote) {
  const std::string input = "a \"b\\c\" \'d\\e\' f";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b\\c");
  expected_argv.push_back("d\\e");
  expected_argv.push_back("f");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvDoubleBackslashesInQuote) {
  const std::string input = "a \"b\\\\c\" \'d\\\\e\' f";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b\\c");
  expected_argv.push_back("d\\\\e");
  expected_argv.push_back("f");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvTripleBackslashesInQuote) {
  const std::string input = "a \"b\\\\\\c\" \'d\\\\\\e\' f";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b\\\\c");
  expected_argv.push_back("d\\\\\\e");
  expected_argv.push_back("f");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParsePosixCommandLineToArgvReturnFalseForUnfinished) {
  const std::string open_single_quote = "\"";
  const std::string open_double_quote = "\'";
  const std::string open_backslash = "\\";
  std::vector<std::string> argv;
  EXPECT_FALSE(devtools_goma::ParsePosixCommandLineToArgv(open_single_quote,
                                                          &argv));
  EXPECT_FALSE(devtools_goma::ParsePosixCommandLineToArgv(open_double_quote,
                                                          &argv));
  EXPECT_FALSE(devtools_goma::ParsePosixCommandLineToArgv(open_backslash,
                                                          &argv));
}

TEST(CmdlineParserTest, ParsePosixCommandLineShouldKeepOriginalArgv) {
  const std::string input = "a b";
  std::vector<std::string> argv;

  argv.push_back("0");
  argv.push_back("1");
  EXPECT_TRUE(devtools_goma::ParsePosixCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("0");
  expected_argv.push_back("1");
  expected_argv.push_back("a");
  expected_argv.push_back("b");

  EXPECT_EQ(expected_argv, argv);
}

// All test vectors for ParseWinCommandLineToArgv are come from:
// Results of Parsing Command Lines in
// http://msdn.microsoft.com/en-us/library/windows/desktop/17w5ykft(v=vs.85).aspx
//
// Note:
// In the document argv[3] is always capitailzed but I thought it typo.
TEST(CmdlineParserTest, ParseWinCommandLineToArgvRule1) {
  const std::string input = "\"abc\" d e";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParseWinCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("abc");
  expected_argv.push_back("d");
  expected_argv.push_back("e");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParseWinCommandLineToArgvRule2) {
  const std::string input = "a\\\\\\b d\"e f\"g h";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParseWinCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a\\\\\\b");
  expected_argv.push_back("de fg");
  expected_argv.push_back("h");

  EXPECT_EQ(expected_argv, argv);
}


TEST(CmdlineParserTest, ParseWinCommandLineToArgvRule3) {
  const std::string input = "a\\\\\\\"b c d";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParseWinCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a\\\"b");
  expected_argv.push_back("c");
  expected_argv.push_back("d");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParseWinCommandLineToArgvRule4) {
  const std::string input = "a\\\\\\\\\"b c\" d e";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParseWinCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a\\\\b c");
  expected_argv.push_back("d");
  expected_argv.push_back("e");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParseWinCommandLineToArgvRule5) {
  const std::string input = " \t\n\r";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParseWinCommandLineToArgv(input, &argv));
  EXPECT_EQ(0U, argv.size());
}

TEST(CmdlineParserTest, ParseWinCommandLineToArgvRule6) {
  const std::string input = "  \n a \r  b \t  c  ";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParseWinCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("a");
  expected_argv.push_back("b");
  expected_argv.push_back("c");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParseWinCommandLineToArgvRule7) {
  const std::string input = " \n \" a \" b\t\n\t \"c \"\n\t\" d\t\" ";
  std::vector<std::string> argv;
  EXPECT_TRUE(devtools_goma::ParseWinCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back(" a ");
  expected_argv.push_back("b");
  expected_argv.push_back("c ");
  expected_argv.push_back(" d\t");

  EXPECT_EQ(expected_argv, argv);
}

TEST(CmdlineParserTest, ParseWinCommandLineToArgvReturnFalseWithoutEndQuote) {
  const std::string input = "\"";
  std::vector<std::string> argv;
  EXPECT_FALSE(devtools_goma::ParseWinCommandLineToArgv(input, &argv));
}

TEST(CmdlineParserTest, ParseWinCommandLineToArgvShouldKeepOriginalArgv) {
  const std::string input = "a b";
  std::vector<std::string> argv;

  argv.push_back("0");
  argv.push_back("1");
  EXPECT_TRUE(devtools_goma::ParseWinCommandLineToArgv(input, &argv));

  std::vector<std::string> expected_argv;
  expected_argv.push_back("0");
  expected_argv.push_back("1");
  expected_argv.push_back("a");
  expected_argv.push_back("b");

  EXPECT_EQ(expected_argv, argv);
}
