// Copyright 2010 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "env_flags.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <set>
#include <string>
#include <sstream>

#include "absl/strings/numbers.h"
#include "client/util.h"
#include "glog/logging.h"

struct GomaAutoConfigurer {
  GomaAutoConfigurer(std::string (*GetConfiguredValue)(void),
                     void (*SetConfiguredValue)(void))
      : GetConfiguredValue(GetConfiguredValue),
        SetConfiguredValue(SetConfiguredValue) {}

  std::string (*GetConfiguredValue)(void);
  void (*SetConfiguredValue)(void);
};

static std::set<std::string>* g_env_flag_names;
typedef std::map<std::string, GomaAutoConfigurer> AutoConfigurerMap;
static AutoConfigurerMap* g_autoconfigurers;

void RegisterEnvFlag(const char* name) {
  if (!g_env_flag_names) {
    g_env_flag_names = new std::set<std::string>;
  }
  if (!g_env_flag_names->insert(name).second) {
    fprintf(stderr, "%s has registered twice\n", name);
    exit(1);
  }
}

void RegisterEnvAutoConfFlag(const char* name,
                             std::string (*GetConfiguredValue)(),
                             void (*SetConfiguredValue)()) {
  if (!g_autoconfigurers) {
    g_autoconfigurers = new AutoConfigurerMap;
  }

  GomaAutoConfigurer configurer(GetConfiguredValue, SetConfiguredValue);

  if (!g_autoconfigurers->insert(make_pair(std::string(name), configurer))
           .second) {
    fprintf(stderr, "%s has registered twice for autoconf\n", name);
    exit(1);
  }
}

void CheckFlagNames(const char** envp) {
  bool ok = true;
  for (int i = 0; envp[i]; i++) {
    if (strncmp(envp[i], "GOMA_", 5)) {
      continue;
    }
    const char* name_end = strchr(envp[i], '=');
    assert(name_end);
    const std::string name(envp[i] + 5, name_end - envp[i] - 5);
    if (!g_env_flag_names->count(name)) {
      fprintf(stderr, "%s: unknown GOMA_ parameter\n", envp[i]);
      ok = false;
    }
  }
  if (!ok) {
    exit(1);
  }
}

void AutoConfigureFlags(const char** envp) {
  std::set<std::string> goma_set_params;

  for (int i = 0; envp[i]; i++) {
    if (strncmp(envp[i], "GOMA_", 5))
      continue;

    const char* name_end = strchr(envp[i], '=');
    assert(name_end);
    const std::string name(envp[i] + 5, name_end - envp[i] - 5);
    goma_set_params.insert(name);
  }

  for (const auto& it : *g_autoconfigurers) {
    if (goma_set_params.count(it.first))
      continue;
    it.second.SetConfiguredValue();
  }
}

void DumpEnvFlag(std::ostringstream* ss) {
  if (g_env_flag_names == nullptr)
    return;

  for (const auto& iter : *g_env_flag_names) {
    const std::string name = "GOMA_" + iter;
    char* v = nullptr;
#ifdef _WIN32
    _dupenv_s(&v, nullptr, name.c_str());
#else
    v = getenv(name.c_str());
#endif
    if (v != nullptr) {
      (*ss) << name << "=" << v << std::endl;
    } else if (g_autoconfigurers->count(iter)) {
      (*ss) << name << "="
            << g_autoconfigurers->find(iter)->second.GetConfiguredValue()
            << " (auto configured)" << std::endl;
    }
  }
}

std::string GOMA_EnvToString(const char* envname, const char* dflt) {
  return devtools_goma::GetEnv(envname).value_or(dflt);
}

bool GOMA_EnvToBool(const char* envname, bool dflt) {
  const absl::optional<std::string> env = devtools_goma::GetEnv(envname);
  if (!env) {
    return dflt;
  }
  bool result = 0;
  if (!absl::SimpleAtob(*env, &result)) {
    LOG(FATAL) << envname << "=" << *env << " is invalid value for bool flag."
               << " Specify true or false.";
  }
  return result;
}

int GOMA_EnvToInt(const char* envname, int dflt) {
  const absl::optional<std::string> env = devtools_goma::GetEnv(envname);
  if (!env) {
    return dflt;
  }
  int result = 0;
  if (!absl::SimpleAtoi(*env, &result)) {
    LOG(FATAL) << envname << "=" << *env
               << " is invalid value for integer flag."
               << " Specify number as a base-10 integer.";
  }
  return result;
}
