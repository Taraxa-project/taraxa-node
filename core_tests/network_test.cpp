#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include <vector>
#include <atomic>
#include <iostream>
#include "network.hpp"
namespace taraxa {

TEST (UdpBuffer, one_buffer){
	UdpBuffer buffer (1, 512);
	auto buf1 (buffer.allocate());
	ASSERT_NE(nullptr, buf1);
	buffer.enqueue(buf1);
	auto buf2 (buffer.dequeue());
	ASSERT_EQ(buf1, buf2);
	buffer.release (buf2);
	auto buf3 (buffer.allocate());
	ASSERT_EQ (buf1, buf3);
}

TEST (UdpBuffer, two_buffers){
	UdpBuffer buffer (2, 512);
	auto buf1 (buffer.allocate());
	ASSERT_NE (nullptr, buf1);
	auto buf2 (buffer.allocate());
	ASSERT_NE (nullptr, buf2);
	ASSERT_NE (buf1, buf2);
	buffer.enqueue(buf2);
	buffer.enqueue(buf1);
	auto buf3 (buffer.dequeue());
	ASSERT_EQ (buf2, buf3);
	auto buf4 (buffer.dequeue());
	ASSERT_EQ (buf1, buf4);
	buffer.release(buf3);
	buffer.release(buf4);
	auto buf5 (buffer.allocate());
	ASSERT_EQ (buf2, buf5);
	auto buf6 (buffer.allocate());
	ASSERT_EQ (buf1, buf6);
}

TEST (UdpBuffer, one_buffer_multithreaded){
	UdpBuffer buffer(1, 512);
	boost::thread thread ([&buffer](){
		bool done (false);
		while (!done){
			auto item (buffer.dequeue());
			done = (item == nullptr);
			if (item!=nullptr){
				buffer.release(item);
			}
		}
	});
	auto buf1 (buffer.allocate());
	ASSERT_NE (nullptr, buf1);
	buffer.enqueue(buf1);
	auto buf2 (buffer.allocate());
	ASSERT_EQ (buf1, buf2);
	buffer.stop();
	auto buf3 (buffer.allocate());
	ASSERT_EQ (nullptr, buf3);
	auto buf4 (buffer.allocate());
	ASSERT_EQ (nullptr, buf4);
	thread.join();
}
//TODO: 
// Sometimes the consumer will miss a package, not knowing why yet

TEST (UdpBuffer, multi_buffers_multithreaded){
	UdpBuffer buffer (128, 512);
	//buffer.setVerbose(true);
	std::vector<boost::thread> producer_threads;
	std::vector<boost::thread> consumer_threads;

	std::atomic_int consumed (0);

	// consumer
	for (auto i = 0; i < 8; ++i){
		consumer_threads.push_back(boost::thread([&buffer, &consumed](){
			bool done (false);
			while (!done){
				auto item (buffer.dequeue());
				done = (item == nullptr);
				if (!done){
					consumed++;
					// buffer.print("c= "+std::to_string(item->sz));
					//std::cout<<item->sz<<std::endl;
					buffer.release(item);
				}
			}
		}));
	}

	// producer
	std::atomic_int produced (0);
	for (auto i = 0; i < 4; ++i){
		producer_threads.push_back(boost::thread([&buffer, &produced](){
			bool done = false;
			while(!done){
				auto item (buffer.allocate());
				done = (item == nullptr);
				if (!done){
					int p = produced.fetch_add(1);
					if (p>=6000){
						buffer.stop();
						break;
					}
					item->sz=p;
					buffer.enqueue(item);
				//	buffer.print("p= "+std::to_string(p));					
				}
			}
		}));
	}
	for (auto &thread: producer_threads){
		thread.join();
	}
	for (auto &thread: consumer_threads){
		thread.join();
	}
	
	ASSERT_GT (produced, 5999);
	ASSERT_EQ (consumed, 6000);
}

TEST(Network, udp_receive){
	boost::asio::io_context context;
	std::shared_ptr<Network> nw1 (new taraxa::Network(context, 3333));
	std::shared_ptr<Network> nw2 (new taraxa::Network(context, 3334));


	std::cout<<"Network 1 is set"<<std::endl;
	std::cout<<"Network 2 is set"<<std::endl;
	nw1->stop();
	nw2->stop();
}

}  // namespace taraxa

int main(int argc, char** argv){
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}