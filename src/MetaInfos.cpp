#include "yaodaq/MetaInfos.hpp"

#include <nlohmann/json.hpp>

std::string yaodaq::MetaInfos::m_raw{};

void yaodaq::MetaInfos::getMetaInfos()
{
  nlohmann::json j;
  j["meta"]["platform"]  = nlohmann::json::meta()["platform"];
  j["meta"]["compiler"]  = nlohmann::json::meta()["compiler"];
  j["meta"]["libraries"] = nlohmann::json::array();
  // json
  nlohmann::json nlohmann;
  nlohmann["name"]    = "nlohmann";
  nlohmann["version"] = nlohmann::json::meta()["version"];
  j["meta"]["libraries"].push_back( nlohmann );
  m_raw = j.dump();
}
