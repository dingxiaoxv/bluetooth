#ifndef DM_JSON_PACKER_H
#define DM_JSON_PACKER_H


#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <list>


namespace dm {

class JsonPacker {
public:
  JsonPacker() { doc_.SetObject(); }

  JsonPacker& add(const std::string& key, const std::string& value) {
    // rapidjson does not copy the strings, so we need to keep them alive
    keys_.push_back(key);
    values_.push_back(value);
    doc_.AddMember(rapidjson::StringRef(keys_.back().c_str()),
                   rapidjson::StringRef(values_.back().c_str()),
                   doc_.GetAllocator());
    return *this;
  }

  JsonPacker& add(const std::string& key, int value) {
    keys_.push_back(key);
    doc_.AddMember(rapidjson::StringRef(keys_.back().c_str()),
                   value,
                   doc_.GetAllocator());
    return *this;
  }

  const std::string result() {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc_.Accept(writer);
    return buffer.GetString();
  }

private:
  rapidjson::Document doc_;
  // use list instead of vector since vector will realloc memory
  std::list<std::string> keys_;
  std::list<std::string> values_;
};

} // namespace dm


#endif // DM_JSON_PACKER_H