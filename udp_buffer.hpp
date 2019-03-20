/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2019-01-17 15:16:02
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-01-17 15:31:14
 */

#ifndef UDP_BUFFER_HPP
#define UDP_BUFFER_HPP
#include <boost/circular_buffer.hpp>
#include "types.hpp"

namespace taraxa {

struct UdpData {
  uint8_t *buffer;
  size_t sz;
  end_point_udp_t ep;
};

/**
 * A circular buffer for servicing UDP datagrams.
 * This container follows a producer/consumer model where the operating system
 * is producing data in to buffers which are serviced by internal threads. If
 * buffers are not serviced fast enough they're internally dropped. This
 * container has a maximum space to hold N (count) buffers of M (sz) size and
 * will allocate them in round-robin order. All public methods are thread-safe
 */

class UdpBuffer {
 public:
  UdpBuffer(size_t count, size_t sz);

  // Return a buffer where UDP data can be put
  // Function will block if there are no free buffers
  // Return nullptr if the container has stopped
  UdpData *allocate();

  // Return a buffer to the freelist after is has been serviced
  void release(UdpData *data);

  // Queue a buffer that has been filled with UDP data and notify servicing
  // threads
  void enqueue(UdpData *data);

  // Return a buffer that has been filled with UDP data
  // Function will block until a buffer has been added
  // Return nullptr only if the container has stopped and job_qu is empty
  UdpData *dequeue();

  // Stop container and notify waiting threads
  // Once the container stopped, cannot allocate,
  // but can consume remaining jobs in job_qu until all jobs are dequeued and
  // relased them back to free_qu
  void stop();
  bool isStopped();
  void print(const std::string &str);
  void setVerbose(bool verbose);

 private:
  std::mutex mutex_;
  std::mutex mutex_for_print_;
  std::condition_variable condition_free_qu_;
  std::condition_variable condition_job_qu_;

  boost::circular_buffer<UdpData *> free_qu_;
  boost::circular_buffer<UdpData *> job_qu_;
  std::vector<uint8_t> mem_pool_;
  std::vector<UdpData> entries_;
  bool stopped_;
  bool verbose_;
};

}  // namespace taraxa

#endif
