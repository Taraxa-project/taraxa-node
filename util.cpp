#include "util.hpp"
#include <cxxabi.h>
#include <dlfcn.h>
#include <errno.h>
namespace taraxa {

boost::property_tree::ptree strToJson(const std::string &str) {
  std::stringstream iss(str);
  boost::property_tree::ptree doc;
  try {
    boost::property_tree::read_json(iss, doc);
  } catch (std::exception &e) {
    std::cerr << "Property tree read error: " << e.what() << std::endl;
  }
  return doc;
}

boost::property_tree::ptree loadJsonFile(std::string json_file_name) {
  try {
    // read file as string
    std::ifstream json_file(json_file_name);
    std::string json_str;

    json_file.seekg(0, std::ios::end);
    json_str.reserve(json_file.tellg());
    json_file.seekg(0, std::ios::beg);

    json_str.assign((std::istreambuf_iterator<char>(json_file)),
                    std::istreambuf_iterator<char>());

    return strToJson(json_str);

  } catch (std::invalid_argument const &err) {
    throw;
  } catch (std::exception const &err) {
    throw std::invalid_argument(__FILE__ "(" + std::to_string(__LINE__) +
                                ") "
                                "Couldn't load Json from file " +
                                json_file_name + ": " + err.what());
  }
}

void thisThreadSleepForSeconds(unsigned sec) {
  std::this_thread::sleep_for(std::chrono::seconds(sec));
}

void thisThreadSleepForMilliSeconds(unsigned millisec) {
  std::this_thread::sleep_for(std::chrono::milliseconds(millisec));
}

void thisThreadSleepForMicroSeconds(unsigned microsec) {
  std::this_thread::sleep_for(std::chrono::microseconds(microsec));
}

unsigned long getCurrentTimeMilliSeconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void Subject::subscribe(std::shared_ptr<Observer> obs) { viewers_.insert(obs); }

void Subject::unsubscribe(std::shared_ptr<Observer> obs) {
  viewers_.erase(obs);
}

void Subject::notify() {
  for (auto const &v : viewers_) {
    v->update();
  }
}

Observer::Observer(std::shared_ptr<Subject> sub) : subject_(sub) {
  subject_->subscribe(shared_from_this());
}

Observer::~Observer() { subject_->unsubscribe(shared_from_this()); }

}  // namespace taraxa

void abortHandler(int sig) {
  const char *name = NULL;
  switch (sig) {
    case SIGABRT:
      name = "SIGABRT";
      break;
    case SIGSEGV:
      name = "SIGSEGV";
      break;
    case SIGILL:
      name = "SIGILL";
      break;
    case SIGFPE:
      name = "SIGFPE";
      break;
  }
  if (name) {
    std::cerr << "Caught signal " << sig << " (" << name << ")" << std::endl;
  } else {
    std::cerr << "Caught signal " << sig << std::endl;
  }
  printStackTrace();
  exit(sig);
}

static inline void printStackTrace() {
  std::cerr << "Stack Trace: " << std::endl;
  uint max_frames = 63;
  // storage array for stack trace address data
  void *addrlist[max_frames + 1];

  // retrieve current stack addresses
  auto addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void *));

  if (addrlen == 0) {
    return;
  }

  // create readable strings to each frame.
  char **symbollist = backtrace_symbols(addrlist, addrlen);
  FILE *out = stderr;
  size_t funcnamesize = 1024;
  char funcname[1024];

  // print the stack trace.
  for (auto i = 4; i < addrlen; i++) {
    char *begin_name = NULL;
    char *begin_offset = NULL;
    char *end_offset = NULL;
#ifdef DARWIN
    // OSX style stack trace
    for (char *p = symbollist[i]; *p; ++p) {
      if ((*p == '_') && (*(p - 1) == ' '))
        begin_name = p - 1;
      else if (*p == '+')
        begin_offset = p - 1;
    }

    if (begin_name && begin_offset && (begin_name < begin_offset)) {
      *begin_name++ = '\0';
      *begin_offset++ = '\0';

      // mangled name is now in [begin_name, begin_offset) and caller
      // offset in [begin_offset, end_offset). now apply
      // __cxa_demangle():
      int status;
      char *ret =
          abi::__cxa_demangle(begin_name, &funcname[0], &funcnamesize, &status);
      if (status == 0) {
        funcname = ret;  // use possibly realloc()-ed string
        fprintf(out, "  %-30s %-40s %s\n", symbollist[i], funcname,
                begin_offset);
      } else {
        // demangling failed. Output function name as a C function with
        // no arguments.
        fprintf(out, "  %-30s %-38s() %s\n", symbollist[i], begin_name,
                begin_offset);
      }

#else   // !DARWIN - but is posix
        // not OSX style
        // ./module(function+0x15c) [0x8048a6d]
    for (char *p = symbollist[i]; *p; ++p) {
      if (*p == '(')
        begin_name = p;
      else if (*p == '+')
        begin_offset = p;
      else if (*p == ')' && (begin_offset || begin_name))
        end_offset = p;
    }

    if (begin_name && end_offset && (begin_name < end_offset)) {
      *begin_name++ = '\0';
      *end_offset++ = '\0';
      if (begin_offset) *begin_offset++ = '\0';

      // mangled name is now in [begin_name, begin_offset) and caller
      // offset in [begin_offset, end_offset). now apply
      // __cxa_demangle():

      int status = 0;
      char *ret =
          abi::__cxa_demangle(begin_name, funcname, &funcnamesize, &status);
      char *fname = begin_name;
      if (status == 0) fname = ret;

      if (begin_offset) {
        fprintf(out, "  %-30s ( %-40s  + %-6s) %s\n", symbollist[i], fname,
                begin_offset, end_offset);
      } else {
        fprintf(out, "  %-30s ( %-40s    %-6s) %s\n", symbollist[i], fname, "",
                end_offset);
      }
#endif  // !DARWIN - but is posix
    } else {
      // couldn't parse the line? print the whole line.
      fprintf(out, "  %-40s\n", symbollist[i]);
    }
  }
  free(symbollist);
}
