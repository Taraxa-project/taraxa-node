// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2013-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.
#include "Log.h"

#ifdef __APPLE__
#include <pthread.h>
#endif

#include <boost/core/null_deleter.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/attributes/function.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>

#if defined(NDEBUG)
#include <boost/log/sinks/async_frontend.hpp>
template <class T>
using log_sink = boost::log::sinks::asynchronous_sink<T>;
#else
#include <boost/log/sinks/sync_frontend.hpp>
template <class T>
using log_sink = boost::log::sinks::synchronous_sink<T>;
#endif

namespace dev {
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(context, "Context", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", int)
BOOST_LOG_ATTRIBUTE_KEYWORD(threadName, "ThreadName", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

namespace {
/// Associate a name with each thread for nice logging.
struct ThreadLocalLogName {
  ThreadLocalLogName(std::string const& _name) { m_name.reset(new std::string(_name)); }
  boost::thread_specific_ptr<std::string> m_name;
};

ThreadLocalLogName g_logThreadName("main");

auto const g_timestampFormatter =
    (boost::log::expressions::stream << EthViolet
                                     << boost::log::expressions::format_date_time(timestamp, "%m-%d %H:%M:%S")
                                     << EthReset " ");
}  // namespace

std::string getThreadName() {
#if defined(__GLIBC__) || defined(__APPLE__)
  char buffer[128];
  pthread_getname_np(pthread_self(), buffer, 127);
  buffer[127] = 0;
  return buffer;
#else
  return g_logThreadName.m_name.get() ? *g_logThreadName.m_name.get() : "<unknown>";
#endif
}

void setThreadName(std::string const& _n) {
#if defined(__GLIBC__)
  pthread_setname_np(pthread_self(), _n.c_str());
#elif defined(__APPLE__)
  pthread_setname_np(_n.c_str());
#else
  g_logThreadName.m_name.reset(new std::string(_n));
#endif
}

}  // namespace dev
