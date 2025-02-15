#pragma once
#include <boost/program_options.hpp>
#include <memory>

#include "common/app_base.hpp"

namespace taraxa {

class Plugin {
 public:
  explicit Plugin(std::shared_ptr<AppBase>& a) : _app(a) {}
  virtual ~Plugin() = default;

  std::shared_ptr<AppBase> app() const { return _app.lock(); }

  virtual std::string name() const = 0;

  virtual std::string description() const = 0;

  virtual void start() = 0;

  virtual void shutdown() = 0;

  virtual void init(const boost::program_options::variables_map& options) = 0;

  virtual void addOptions(boost::program_options::options_description& command_line_options) = 0;

 protected:
  std::weak_ptr<AppBase> _app;
};

}  // namespace taraxa