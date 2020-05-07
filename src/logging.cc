#include "logging.h"

#include "spdlog/sinks/msvc_sink.h"
//#include "spdlog/cfg/env.h"

namespace ovrly{ namespace logging{

    void init() {
      auto sink = std::make_shared<logger::sinks::msvc_sink_mt>();
      auto logr = std::make_shared<logger::logger>("OVRLY", sink);
      logger::set_default_logger(logr);

      logger::set_pattern("[%m%d/%H%M%S.%e:%l:%@] [%n] [t%t] %v");

#ifdef _DEBUG
      logger::set_level(logger::level::debug);
#endif
    }

}} // namespace
