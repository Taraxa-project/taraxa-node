#include <libweb3jsonrpc/ModularServer.h>

#include "common/thread_pool.hpp"
#include "network/http_server.hpp"
#include "network/ws_server.hpp"
#include "plugin/plugin.hpp"

namespace taraxa::net {
class TaraxaFace;
class NetFace;
class EthFace;
class TestFace;
class DebugFace;
}  // namespace taraxa::net

namespace taraxa::plugin {

class Rpc : public Plugin {
 public:
  explicit Rpc(std::shared_ptr<AppBase> app) : Plugin(app) {}

  std::string name() const override { return "RPC"; }
  std::string description() const override { return "Includes http, ws and graphql APIs"; }

  void init(const boost::program_options::variables_map& options) override;
  void addOptions(boost::program_options::options_description& command_line_options) override;

  void start() override;
  void shutdown() override;

 private:
  using JsonRpcServer = ModularServer<net::TaraxaFace, net::NetFace, net::EthFace, net::TestFace, net::DebugFace>;

  // should be destroyed after all components, since they may depend on it through unsafe pointers
  std::shared_ptr<util::ThreadPool> rpc_thread_pool_;
  std::shared_ptr<util::ThreadPool> graphql_thread_pool_;

  std::shared_ptr<net::HttpServer> jsonrpc_http_;
  std::shared_ptr<net::HttpServer> graphql_http_;
  std::shared_ptr<net::WsServer> jsonrpc_ws_;
  std::shared_ptr<net::WsServer> graphql_ws_;
  std::unique_ptr<JsonRpcServer> jsonrpc_api_;

  uint32_t threads_;
  bool enable_test_rpc_ = false;
  bool enable_debug_ = false;
};

}  // namespace taraxa::plugin
