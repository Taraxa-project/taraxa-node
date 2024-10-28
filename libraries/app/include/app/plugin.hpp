#include "app/app.hpp"

namespace taraxa {

class Plugin {
 public:
  explicit Plugin(App& a) : _app(a) {}
  virtual ~Plugin() = default;

  App& app() const { return _app; }

  virtual std::string name() const = 0;

  virtual std::string description() const = 0;

  virtual void startup() = 0;

  virtual void shutdown() = 0;

  virtual void initialize(const boost::program_options::variables_map& options) = 0;

  virtual void addOptions(boost::program_options::options_description& command_line_options) = 0;

 protected:
  App& _app;
};

}  // namespace taraxa