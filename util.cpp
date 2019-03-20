#include "util.hpp"

namespace taraxa {

boost::property_tree::ptree strToJson(std::string str) {
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
