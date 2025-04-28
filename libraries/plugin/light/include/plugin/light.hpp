#include "common/thread_pool.hpp"
#include "logger/logging.hpp"
#include "plugin/plugin.hpp"

namespace taraxa::net {}  // namespace taraxa::net

namespace taraxa::plugin {

class Light : public Plugin {
 public:
  explicit Light(std::shared_ptr<AppBase> app);

  std::string name() const override { return "light"; }
  std::string description() const override { return "Light node plugin"; }

  void init(const boost::program_options::variables_map& options) override;
  void addOptions(boost::program_options::options_description& command_line_options) override;

  void start() override;
  void shutdown() override;

 private:
  /**
   * @brief Clears light node history
   */
  void clearLightNodeHistory();
  void clearPeriod(PbftPeriod period);
  void clearHistory(PbftPeriod end_period, uint64_t dag_level_to_keep, PbftPeriod last_block_number);
  void pruneStateDb();

  static constexpr uint64_t periods_to_keep_non_block_data_ = 1000;
  std::shared_ptr<util::ThreadPool> cleanup_pool_ = std::make_shared<util::ThreadPool>(1);
  uint32_t history_ = 0;
  uint64_t min_light_node_history_ = 0;
  bool state_db_pruning_;

  logger::Logger logger_;
};

}  // namespace taraxa::plugin
