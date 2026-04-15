#pragma once
#include <string>
namespace yaodaq
{

// Get the compiler version etc ....
class MetaInfos
{
public:
  explicit MetaInfos()
  {
    if( m_raw.empty() ) getMetaInfos();
  }
  static std::string raw()
  {
    if( m_raw.empty() ) getMetaInfos();
    return m_raw;
  }

private:
  static void        getMetaInfos();
  static std::string m_raw;
};

}  // namespace yaodaq
