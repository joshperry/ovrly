#include "logging.h"

#ifdef OS_WIN
#include <spdlog/sinks/msvc_sink.h>
#endif
//#include "spdlog/cfg/env.h"

namespace ovrly{ namespace logging{

    void init() {
      std::vector<spdlog::sink_ptr> sinks;
#ifdef OS_WIN
      sinks.push_back(std::make_shared<logger::sinks::msvc_sink_mt>());
#else
      sinks.push_back(std::make_shared<logger::sinks::ansicolor_stdout_sink_mt>());
#endif
      auto logr = std::make_shared<logger::logger>("OVRLY", begin(sinks), end(sinks));
      logger::set_default_logger(logr);

      logger::set_pattern("[%m%d/%H%M%S.%e:%l:%@] [%n] [t%t] %v");

#ifndef NDEBUG
      logger::set_level(logger::level::debug);
#endif
    }

}} // namespace
