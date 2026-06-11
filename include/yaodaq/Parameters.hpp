#pragma once
#include "yaodaq/Export.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace yaodaq
{

class Parameters
{
public:
  using int_list    = std::vector<std::int64_t>;
  using double_list = std::vector<double>;
  using string_list = std::vector<std::string>;

  using parameter = std::variant<bool, std::int64_t, double, std::string, int_list, double_list, string_list>;
  using value_ref = std::reference_wrapper<const parameter>;

private:
  mutable std::shared_mutex                  m_mutex;
  std::unordered_map<std::string, parameter> m_params;

public:
  Parameters() = default;
  Parameters( const Parameters& other )
  {
    std::shared_lock lock( other.m_mutex );
    m_params = other.m_params;
  }
  Parameters& operator=( const Parameters& other )
  {
    if( this == &other ) return *this;

    std::shared_lock lock_other( other.m_mutex );
    std::unique_lock lock_this( m_mutex );

    m_params = other.m_params;

    return *this;
  }
  Parameters( Parameters&& other ) noexcept
  {
    std::unique_lock lock( other.m_mutex );
    m_params = std::move( other.m_params );
  }
  Parameters& operator=( Parameters&& other ) noexcept
  {
    if( this == &other ) return *this;

    std::unique_lock lock_other( other.m_mutex );
    std::unique_lock lock_this( m_mutex );

    m_params = std::move( other.m_params );

    return *this;
  }
  template<typename T> Parameters& set( const std::string_view key, T&& value )
  {
    std::unique_lock lock( m_mutex );
    m_params.insert_or_assign( std::string( key ), parameter( std::forward<T>( value ) ) );
    return *this;
  }

  Parameters& set( const std::string_view key, parameter value )
  {
    std::unique_lock lock( m_mutex );
    m_params.insert_or_assign( std::string( key ), std::move( value ) );
    return *this;
  }

  bool contains( const std::string_view key ) const
  {
    std::shared_lock lock( m_mutex );
    return m_params.find( key.data() ) != m_params.end();
  }

  std::size_t size() const
  {
    std::shared_lock lock( m_mutex );
    return m_params.size();
  }

  std::optional<value_ref> get( const std::string_view key ) const
  {
    std::shared_lock lock( m_mutex );
    auto             it = m_params.find( key.data() );
    if( it == m_params.end() ) return std::nullopt;
    return std::cref( it->second );
  }

  template<typename T> std::optional<T> get_as( const std::string_view key ) const
  {
    std::shared_lock lock( m_mutex );
    auto             it = m_params.find( key.data() );
    if( it == m_params.end() ) return std::nullopt;
    if( auto ptr = std::get_if<T>( &it->second ) ) return *ptr;
    return std::nullopt;
  }

  template<typename T> class getter
  {
  public:
    getter( std::optional<T> v ) : value( std::move( v ) ) {}
    T or_value( T fallback ) && { return value ? *std::move( value ) : std::move( fallback ); }

    T or_throw( const std::string& msg = "Parameter missing or wrong type" ) &&
    {
      if( !value ) throw std::runtime_error( msg );
      return *std::move( value );
    }

    bool has_value() const { return value.has_value(); }

    const std::optional<T>& raw() const { return value; }

  private:
    std::optional<T> value;
  };

  template<typename T> getter<T> get_as_f( const std::string_view key ) const { return getter<T>( get_as<T>( key ) ); }

  template<typename Visitor> auto visit( const std::string_view key, Visitor&& vis ) const -> std::optional<std::invoke_result_t<Visitor, const parameter&>>
  {
    std::shared_lock lock( m_mutex );
    auto             it = m_params.find( key.data() );
    if( it == m_params.end() ) return std::nullopt;
    return vis( it->second );
  }
  template<typename Visitor> void visit_all( Visitor&& vis ) const
  {
    std::shared_lock lock( m_mutex );

    for( const auto& [key, value]: m_params ) { vis( key, value ); }
  }
  bool erase( const std::string_view key )
  {
    std::unique_lock lock( m_mutex );
    return m_params.erase( key.data() ) > 0;
  }

  void clear()
  {
    std::unique_lock lock( m_mutex );
    m_params.clear();
  }
};

}  // namespace yaodaq
