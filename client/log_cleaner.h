// Copyright 2011 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_LOG_CLEANER_H_
#define DEVTOOLS_GOMA_CLIENT_LOG_CLEANER_H_

#include <set>
#include <string>
#include <vector>

#include "absl/time/time.h"
#include "basictypes.h"

namespace devtools_goma {

class LogCleaner {
 public:
  LogCleaner();
  ~LogCleaner();

  // Adds log's basename to be cleaned.
  void AddLogBasename(const std::string& basename);

  // Cleans log files older than |time|.
  void CleanOldLogs(absl::Time time);

 private:
  friend class LogCleanerTest;
  void FindOldLogsInDir(const std::string& log_dir,
                        absl::Time time,
                        std::set<std::string>* old_logs);
  bool IsMyLogFile(const std::string& name) const;

  std::vector<std::string> basenames_;

  DISALLOW_COPY_AND_ASSIGN(LogCleaner);
};
}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_LOG_CLEANER_H_
