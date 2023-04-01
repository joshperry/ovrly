#pragma once

#ifndef SPDLOG_FMT_EXTERNAL
#define SPDLOG_FMT_EXTERNAL
#endif
#include <spdlog/spdlog.h>

namespace ovrly{

  namespace logger = spdlog;

  namespace logging {

      void init();

  }
} //namespace
