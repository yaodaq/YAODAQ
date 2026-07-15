#pragma once
#include <string>
#include <cstdint>
#include <cstddef>
#include "fmt/format.h"

namespace yaodaq
{

std::string progressBar(std::uint64_t current, std::uint64_t max, std::size_t width = 50)
{
  if(max == (std::numeric_limits<std::uint64_t>::max)()) return fmt::format("{}/∞",current);
  const double ratio = static_cast<double>(current) / static_cast<double>(max);
  const std::size_t filled = static_cast<std::size_t>(ratio * width);
  std::string bar;
  bar.reserve(width);
  for (std::size_t i = 0; i < width; ++i)
  {
    if(i < filled) bar += "█";
    else bar += "░";
  }
  const int percent = static_cast<int>(ratio * 100.0);
  return fmt::format("[{}] {:3}% ({}/{})",bar,percent,current,max);
}

}