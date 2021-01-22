#!/bin/bash
#
# Copyright 2010 The Goma Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# Simple test scripts for sanity check. Runs against production
# servers.
#
# Run this like:
#  % GOMA_RPC_EXTRA_PARAMS="?${USERNAME}_$cell" ./test/simpletry.sh out/Debug
# in order to test your personal canary with binaries in out/Debug.
# If the binary directory isn't specified, out/Release will be used.
#
#  % ./test/simpletry.sh -w
# will wait after all tests finished, so you could investigate
# outputs or compiler proxy status page.
#
# % ./test/simpletry.sh -k
# will kill running compiler_proxy before test to make sure compiler_proxy
# is actually invoked for the test only.
# Without -k, it will try own compiler_proxy (isolated with GOMA_* flags)
#
# By default, it will allocate port 8100 (or later)
# You can set port number with -p option.
# % ./test/simpletry.sh -p 8200
#
# with -d dumpfile option, you'll get task.json and task's ExecReq in
# dumpfile (tgz format)
# % ./test/simpletry.sh -d /tmp/simpletry.tgz
#
# If CLANG_PATH is specified, and $CLANG_PATH/clang and $CLANG_PATH/clang++
# exists, it will test with clang and clang++.
# Note that it doesn't support old clang that doesn't support -dumpmachine.
#

test_dir=$(cd $(dirname $0); pwd)
goma_top_dir=${test_dir}/..
tmpdir=$(mktemp -d /tmp/tmp.XXXXXXXX)
chmod 0700 $tmpdir

. $test_dir/gomatest.sh

is_color=0
if tput init && test -t 1; then
  is_color=1
fi
function test_term() {
  test "$is_color" = 1
}
function tput_reset() {
  test_term && tput sgr0
  return 0
}
function echo_title() {
  if test_term; then
    tput bold; tput setaf 4
  fi
  echo "$@"
  tput_reset
}
function echo_bold() {
  test_term && tput bold
  echo "$@"
  tput_reset
}
function echo_ok() {
  if test_term; then
    tput bold; tput setaf 2
  fi
  echo "$@"
  tput_reset
}
function echo_known_fail() {
  if test_term; then
    tput setab 1
  fi
  echo "$@"
  tput_reset
}
function echo_fail() {
  if test_term; then
    tput bold; tput setaf 1
  fi
  echo "$@"
  tput_reset
}
function echo_warn() {
  if test_term; then
   tput bold; tput setaf 5
  fi
  echo "$@"
  tput_reset
}

function at_exit() {
  # cleanup function.
  rm -f a.out a.out2 out.o out2.o out_plain.o has_include.o
  rm -f test/compile_error.o
  rm -f test/compile_error*.out test/compile_error*.err
  rm -f cmd_out cmd_err
  stop_compiler_proxy

  $goma_top_dir/client/diagnose_goma_log.py \
    --show-errors --show-warnings --show-known-warnings-threshold=0 \
    --fail-tasks-threshold=2 \
    || true
  if [ -n "${GLOG_log_dir:-}" ]; then
    echo "Gomacc logs:"
    cat ${GLOG_log_dir}/gomacc.* || true
  fi
  rm -rf $tmpdir
  tput_reset
}

function is_cros_gcc() {
  local compiler=$1

  local version=$($compiler --version)
  case "$version" in
    *_cos_*)
      echo "yes"
      ;;
    *)
      echo "no"
      ;;
  esac
}

function wait_no_active_tasks() {
  while [ -z "$("${goma_bin_dir}/goma_ctl.py" jsonstatus| \
    grep '"num_active_tasks":0')" ]
  do
    sleep 1
    echo "waiting for active tasks become 0"
  done
}

# keep the list of failed tests in an array
FAIL=()
KNOWN_FAIL=()

function fail() {
  local testname="$1"
  case "$testname" in
  FAIL_*)
      echo_known_fail "FAIL"
      KNOWN_FAIL+=($testname);;
  *)
      echo_fail "FAIL"
      FAIL+=($testname);;
  esac
}

function ok() {
  echo_ok "OK"
}

function assert_success() {
  local cmd="$1"
  if eval $cmd; then
    return
  else
    echo_fail "FAIL in $cmd"
    exit 1
  fi
}

function dump_request() {
  local cmd="$1"
  if [ "$FLAGS_dump" = "" ]; then
    return
  fi
  set -- $cmd
  cmd=$1
  case "$cmd" in
  "$GOMA_CC"|"$GOMA_CXX"|"$GOMACC")
    echo "[dump:$TASK_ID]"
    httpfetch 127.0.0.1 "$GOMA_COMPILER_PROXY_PORT" \
     "/api/taskz?id=$TASK_ID&dump=req" post > /dev/null
    if [ -d $GOMA_TMP_DIR/task_request_$TASK_ID ]; then
      httpfetch 127.0.0.1 "$GOMA_COMPILER_PROXY_PORT" \
       "/api/taskz?id=$TASK_ID" post \
         > $GOMA_TMP_DIR/task_request_$TASK_ID/task.json
    fi
    TASK_ID=$((TASK_ID+1))
    ;;
   *)
    echo "[nodump]";;
  esac
}

function expect_success() {
  local testname="$1"
  local cmd="$2"
  echo_bold -n "TEST: "
  echo -n "${testname}..."
  if [ -z "$cmd" ]; then
    fail $testname
    echo_bold "cmd: must not be empty string."
    exit 1
  fi
  if eval $cmd >$tmpdir/cmd_out 2>$tmpdir/cmd_err; then
    ok
    if [ -s $tmpdir/cmd_err ]; then
      echo_bold "cmd: $cmd"
      cat $tmpdir/cmd_err
    fi
  else
    fail $testname
    echo_bold "cmd: $cmd"
    cat $tmpdir/cmd_out
    cat $tmpdir/cmd_err
  fi
  dump_request "$cmd"
  rm -f cmd_out cmd_err
}

function expect_failure() {
  local testname="$1"
  local cmd="$2"
  echo_bold -n "TEST: "
  echo -n "${testname}..."
  if eval $cmd >cmd_out 2>cmd_err; then
    fail $testname
    echo_bold "cmd: $cmd"
    cat cmd_out
    cat cmd_err
  else
    ok
  fi
  dump_request "$cmd"
  rm -f cmd_out cmd_err
}

# Note: all code in this script is expected to be executed from $goma_top_dir.
cd $goma_top_dir

FLAGS_wait=0
FLAGS_kill=0
FLAGS_port=8100
FLAGS_dump=
while getopts kwp:d: opt; do
 case $opt in
 k) FLAGS_kill=1 ;;
 w) FLAGS_wait=1 ;;
 p) FLAGS_port="$OPTARG";;
 d) FLAGS_dump="$OPTARG";;
 ?) echo "Usage: $0 [-w] [-k] [-p port] [-d tgz] [goma_dir]\n" >&2; exit 1;;
 esac
done
shift $(($OPTIND - 1))

set_goma_dirs "$1"

# Flags for gomacc
export GOMA_STORE_ONLY=true
export GOMA_DUMP=true
export GOMA_RETRY=false
export GOMA_FALLBACK=false
export GOMA_USE_LOCAL=false
export GOMA_START_COMPILER_PROXY=false
export GOMA_STORE_LOCAL_RUN_OUTPUT=true
export GOMA_ENABLE_REMOTE_LINK=true
export GOMA_HERMETIC=error
export GOMA_FALLBACK_INPUT_FILES=""

GOMACC=$goma_bin_dir/gomacc

# on buildslave:/b/build/slave/$builddir/build/client
# api key can be found at /b/build/goma/goma.key
if [ -d "$goma_top_dir/../../../../goma" ]; then
  bot_goma_dir=$(cd "$goma_top_dir/../../../../goma"; pwd)
  GOMA_API_KEY_FILE=${GOMA_API_KEY_FILE:-$bot_goma_dir/goma.key}
fi
if [ -n "$GOMA_SERVICE_ACCOUNT_JSON_FILE" ]; then
  echo "Use GOMA_SERVICE_ACCOUNT_JSON_FILE=$GOMA_SERVICE_ACCOUNT_JSON_FILE"
  unset GOMA_API_KEY_FILE
elif [ -f "$GOMA_API_KEY_FILE" ]; then
  echo "Use GOMA_API_KEY_FILE=$GOMA_API_KEY_FILE"
  export GOMA_API_KEY_FILE
elif [ -n "$GOMA_API_KEY_FILE" ]; then
  echo "GOMA_API_KEY_FILE $GOMA_API_KEY_FILE not found." >&2
  unset GOMA_API_KEY_FILE
fi

if [ "$GOMATEST_USE_RUNNING_COMPILER_PROXY" = ""  ]; then
  # --exec_compiler_proxy is deprecated. Use GOMA_COMPILER_PROXY_BINARY instead.
  if ! [ -x ${GOMA_COMPILER_PROXY_BINARY} ]; then
    echo "compiler_proxy($GOMA_COMPILER_PROXY_BINARY) is not executable" >&2
    exit 1
  fi
  echo "Starting $GOMA_COMPILER_PROXY_BINARY..."

  trap at_exit exit sighup sigpipe
  export GOMA_COMPILER_PROXY_PORT=$FLAGS_port

  if [ "$FLAGS_kill" = 1 ]; then
    echo Kill any remaining compiler proxy
    killall compiler_proxy
  else
    echo "GOMA_TMP_DIR: $tmpdir"
    export GOMA_TMP_DIR=$tmpdir
    export TMPDIR=$tmpdir
    export GLOG_log_dir=$tmpdir
    export GOMA_DEPS_CACHE_FILE=deps_cache
    export GOMA_COMPILER_PROXY_SOCKET_NAME=$tmpdir/goma.ipc
    export GOMA_GOMACC_LOCK_FILENAME=$tmpdir/gomacc.lock
    export GOMA_COMPILER_PROXY_LOCK_FILENAME=$tmpdir/goma_compiler_proxy.lock
    # Test uses SSL by default.
    export GOMA_USE_SSL=true
    export GOMA_SERVER_PORT=443
    # Test uses ?prod by default.
    export GOMA_RPC_EXTRA_PARAMS="?prod"
    if [ "$(uname)" = "Linux" ]; then
      export GOMA_ARBITRARY_TOOLCHAIN_SUPPORT="true"
    fi
  fi
  expect_failure "gomacc_port_no_compiler_proxy" "$GOMACC port"
  (cd /tmp && ${GOMA_COMPILER_PROXY_BINARY} & )
  update_compiler_proxy_port $(dirname $GOMA_COMPILER_PROXY_BINARY) 10
  watch_healthz localhost ${GOMA_COMPILER_PROXY_PORT} /healthz \
    ${GOMA_COMPILER_PROXY_BINARY}

  expect_success "gomacc_port" "$GOMACC port"
  expect_success "gomacc_port_500" \
    "for i in $(seq 0 500); do $GOMACC port > /dev/null & done; wait"
fi

if [ "$CLANG_PATH" = "" ]; then
  clang_path="$goma_top_dir/third_party/llvm-build/Release+Asserts/bin"
  if [ -d "$clang_path" ]; then
     if "$clang_path/clang" -v; then
       CLANG_PATH="$clang_path"
     else
       echo "clang is not runnable, disable clang test" 1>&2
     fi
  fi
fi

if [ -n "${GLOG_log_dir:-}" ]; then
  echo "removing gomacc logs."
  rm -f "${GLOG_log_dir}/gomacc.*"
fi

# if build env doesn't not use hermetic gcc,
# set HERMETIC_GCC=FAIL_ for workaround.
HERMETIC_GCC=

DEFAULT_CC=clang
DEFAULT_CXX=clang++
export PATH=$CLANG_PATH:$PATH
if [ "$(uname)" = "Darwin" ]; then
  # Should set SDKROOT if we use non system clang.
  export SDKROOT="$("$goma_top_dir"/third_party/chromium_build/mac/find_sdk.py \
    --print_sdk_path 10.7 | head -1)"
  echo "SDKROOT=${SDKROOT}"
fi

CC=${CC:-$DEFAULT_CC}
CXX=${CXX:-$DEFAULT_CXX}

LOCAL_CC=$(command -v ${CC})
LOCAL_CXX=$(command -v ${CXX})
LOCAL_CXX_DIR=$(dirname ${LOCAL_CXX})
GOMA_CC=${goma_bin_dir}/${CC}
GOMA_CXX=${goma_bin_dir}/${CXX}

# Build determinism is broken on ChromeOS gcc, and since ChromeOS uses
# clang as a default compiler (b/31105358), I do not think we need to
# guarantee build determinism for it.  (b/64499036)
if [[ "$CC" =~ ^g(cc|\+\+)$ && "$(is_cros_gcc $CC)" = "yes" ]]; then
  HERMETIC_GCC="FAIL_"
fi

echo_title "CC=${CC} CXX=${CXX}"
echo_title "LOCAL CC=${LOCAL_CC} CXX=${LOCAL_CXX}"
echo_title "GOMA CC=${GOMA_CC} CXX=${GOMA_CXX}"

${GOMACC} --goma-verify-command ${LOCAL_CC} -v
TASK_ID=1

function print_macho() {
  # Normalizes the filename so that it can be used for diff
  local obj=$1
  objdump -p -s -r -t $obj | sed -e "s|$obj|MACHO_FILE|g"
}

function objcmp() {
 local want=$1
 local got=$2
 if command -v readelf > /dev/null 2>&1; then
    readelf --headers $want > $want.elf
    readelf --headers $got > $got.elf
    diff -u $want.elf $got.elf
    rm -f $want.elf $got.elf
 fi
 if [ "$(uname)" = "Darwin" ]; then
    print_macho $want > $want.macho
    print_macho $got > $got.macho
    diff -u $want.macho $got.macho
    rm -f $want.macho $got.macho
 fi
 cmp $want $got
}

# Emulate gomacc cancel and confirm the compiler_proxy won't crash.
# (crbug.com/904532)
expect_success "gomacc_cancel" \
  "GOMA_DIR=${goma_bin_dir} python ${test_dir}/gomacc_close.py"

CFLAGS=
CXXFLAGS=
if [ "$(uname)" = "Darwin" ]; then
  # Make the build more deterministic
  CFLAGS+=" -mmacosx-version-min=10.10.0"
  # TODO: Have goma client send SDKSettings.json to the server
  CFLAGS+=" -isysroot $goma_top_dir/build/mac_files/xcode_binaries/Contents/\
Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
  CFLAGS+=" -Xclang -target-sdk-version=10.14.0"

  CXXFLAGS+=${CFLAGS}
fi

echo_title "CFLAGS=${CFLAGS}"
echo_title "CXXFLAGS=${CXXFLAGS}"

GOMA_FALLBACK=true
expect_success "${CC}_hello_fallback" "${GOMA_CC} ${CFLAGS} \
  test/hello.c -c -o out.o"
GOMA_USE_LOCAL=true
expect_success "${CC}_hello_fallback_use_local" \
    "${GOMA_CC} ${CFLAGS} test/hello.c -c -o out.o"

MAYBE_FAIL=
if [ "$(uname)" = "Linux" ]; then
MAYBE_FAIL="FAIL_"
fi
GOMA_FALLBACK=false
wait_no_active_tasks
expect_success "${MAYBE_FAIL}${CC}_hello_use_local" "${GOMA_CC} ${CFLAGS} \
  test/hello.c -c -o out.o"
GOMA_USE_LOCAL=false

GOMA_FALLBACK_INPUT_FILES="test/hello.c"
curl -s http://localhost:$GOMA_COMPILER_PROXY_PORT/statz | grep fallback > stat_before.txt
expect_success "${CC}_hello_enforce_fallback" \
  "${GOMA_CC} ${CFLAGS} test/hello.c -c -o out.o"
curl -s http://localhost:$GOMA_COMPILER_PROXY_PORT/statz | grep fallback > stat_after.txt
expect_failure "check_fallback" "diff -u stat_before.txt stat_after.txt"
GOMA_FALLBACK_INPUT_FILES=""

rm -f test/compile_error.{out,err} test/compile_error_fallback.{out,err}
expect_failure "${CXX}_compile_error.cc" \
  "${LOCAL_CXX} ${CXXFLAGS} test/compile_error.cc -c -o test/compile_error.o \
    > test/compile_error.out 2> test/compile_error.err"

GOMA_FALLBACK=true  # run local when remote failed.
GOMA_USE_LOCAL=false  # don't run local when idle.
expect_failure "${CXX}_fail_fallback" \
  "${GOMA_CXX} ${CXXFLAGS} test/compile_error.cc -c -o test/compile_error.o \
  > test/compile_error_fallback.out 2> test/compile_error_fallback.err"

expect_success "compile_error_out" \
  "cmp test/compile_error.out test/compile_error_fallback.out"
expect_success "compile_error_err" \
  "cmp test/compile_error.err test/compile_error_fallback.err"

if [ "$(uname)" = "Darwin" ]; then
  rm -f out.o
  expect_success "${CXX}_multi_arch_fallback" \
   "${GOMA_CXX} ${CXXFLAGS} -arch i386 -arch x86_64 -c -o out.o test/hello.c"
  rm -f out.o
fi

expect_failure "gomacc_gomacc" \
  "${GOMACC} ${GOMA_CC} ${CFLAGS} -c -o out.o test/hello.c"
rm -f out.o
expect_success "gomacc_path_gomacc" \
  "PATH=${goma_bin_dir}:$PATH \
   ${GOMACC} ${CC} ${CFLAGS} -c -o out.o \
   test/hello.c"
rm -f out.o

expect_failure "disabled_true_masquerade_gcc" \
  "GOMA_DISABLED=1 \
   ${GOMA_CC} ${CFLAGS} -c -o out.o test/hello.c"
rm -f out.o

curl -s http://localhost:$GOMA_COMPILER_PROXY_PORT/statz | grep request > stat_before.txt
expect_success "disabled_true_gomacc_local_path_gcc" \
  "GOMA_DISABLED=1 \
   ${GOMACC} ${LOCAL_CC} ${CFLAGS} -c -o out.o test/hello.c"
curl -s http://localhost:$GOMA_COMPILER_PROXY_PORT/statz | grep request > stat_after.txt
expect_success "disabled_true_gomacc_local_path_gcc_not_delivered" \
   "cmp stat_before.txt stat_after.txt"
diff -u stat_before.txt stat_after.txt

rm -f out.o
rm -f stat_before.txt
rm -f stat_after.txt

expect_success "disabled_true_gomacc_masquerade_gcc" \
  "GOMA_DISABLED=1 \
   PATH=${goma_bin_dir}:$PATH \
   ${GOMACC} ${CC} ${CFLAGS} -c -o out.o test/hello.c"
rm -f out.o

expect_success "disabled_true_gomacc_gcc_in_local_path" \
  "GOMA_DISABLED=1 \
   PATH=$(dirname ${LOCAL_CC}) \
   ${GOMACC} ${CC} ${CFLAGS} -c -o out.o test/hello.c"
rm -f out.o

GOMA_FALLBACK=true
GOMA_USE_LOCAL=true
wait_no_active_tasks
expect_success "${CXX}_compile_with_umask_local" \
  "(umask 777; ${GOMACC} ${LOCAL_CC} ${CFLAGS} -o out.o -c test/hello.c)"
expect_success "${CXX}_expected_umask_local" \
  "[ \"$(ls -l out.o | awk '{ print $1}')\" = \"----------\" ]"
rm -f out.o

# TODO
# write a test to send a compiler binary to the backend.

curl --dump-header header.out \
  -X POST --data-binary @${test_dir}/badreq.bin \
  -H 'Content-Type: binary/x-protocol-buffer' \
  http://localhost:${GOMA_COMPILER_PROXY_PORT}/e
expect_success "access_rejected" \
  "head -1 header.out | grep -q 'HTTP/1.1 401 Unauthorized'"
rm -f header.out

if [ -n "${GLOG_log_dir:-}" ]; then
  # Smoke test to confirm gomacc does not create logs.
  # I know there are several tests that make gomacc to write logs but should
  # not be so much.
  expect_success "smoke_test_gomacc_does_not_create_logs_much" \
    "[ \"$(echo ${GLOG_log_dir}/gomacc.* | wc -w)\" -lt "20" ]"
fi

# Gomacc should write log to GLOG_log_dir.
mkdir -p "$tmpdir/gomacc_test"
expect_success "gomacc_should succeed_with_write_log_flag" \
    "GOMA_GOMACC_WRITE_LOG_FOR_TESTING=true \
     GLOG_log_dir=${tmpdir}/gomacc_test ${GOMACC}"
expect_success "gomacc_should_create_log_file" \
  "[ \"$(echo ${tmpdir}/gomacc_test/gomacc.* | wc -w)\" -eq "2" ]"

expect_success "dont_verify_or_abort_when_supposed_to_fallback" \
    "GOMA_VERIFY_OUTPUT=true ${GOMA_CC} ${CFLAGS} test/empty_assembler.s -c \
    -o empty_assembler.o"

if [ "${#FAIL[@]}" -ne 0 ]; then
  echo_fail "Failed tests: ${FAIL[@]}"
fi
if [ "${#KNOWN_FAIL[@]}" -ne 0 ]; then
  echo_known_fail "Known failed tests: ${KNOWN_FAIL[@]}"
fi
if [ "${#FAIL[@]}" -eq 0 -a "${#KNOWN_FAIL[@]}" -eq 0 ]; then
  echo_ok "All tests passed: $CC $CXX"
fi

if [ "$FLAGS_dump" != "" ]; then
   (cd $GOMA_TMP_DIR && tar zcf $FLAGS_dump task_request_*)
   echo "task dump in $FLAGS_dump"
fi

if [ "$FLAGS_wait" = "1" ]; then
  echo -n "Ready to finish? "
  read
fi
echo exit "${#FAIL[@]} # ${CC} ${CXX}"
exit "${#FAIL[@]}"
