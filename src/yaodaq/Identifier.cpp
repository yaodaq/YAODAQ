/**
\copyright Copyright 2022 flagarde
*/

#include "yaodaq/Identifier.hpp"

#include "yaodaq/Exception.hpp"
#include "yaodaq/Key.hpp"
#include "yaodaq/StatusCode.hpp"

#include <fmt/color.h>
#include <magic_enum.hpp>
#include <string>
#include <vector>

namespace yaodaq
{

bool Identifier::empty() const
{
  if( get() == Identifier().get() ) return true;
  else
    return false;
}

Identifier::Identifier( const std::string& type, const std::string& name ) : m_Type( type ), m_Name( name ) {}

void Identifier::generateKey( const Domain& domain, const Class& c_lass, const Family& family ) { m_Key = Key( domain, c_lass, family ); }

std::string Identifier::getDomain() const { return static_cast<std::string>( magic_enum::enum_name( magic_enum::enum_cast<Domain>( m_Key.getDomain() ).value() ) ); }

std::string Identifier::getClass() const { return static_cast<std::string>( magic_enum::enum_name( magic_enum::enum_cast<Class>( m_Key.getClass() ).value() ) ); }

std::string Identifier::getFamily() const { return static_cast<std::string>( magic_enum::enum_name( magic_enum::enum_cast<Family>( m_Key.getFamily() ).value() ) ); }

std::string Identifier::getType() const { return m_Type; }

std::string Identifier::getName() const { return m_Name; }

Key Identifier::getKey() const { return m_Key; }

std::string Identifier::get() const { return fmt::format( "{0}/{1}/{2}/{3}/{4}", getDomain(), getClass(), getFamily(), getType(), getName() ); }

Identifier Identifier::parse( const std::string& id )
{
  std::vector<std::string> result;
  std::string              tmp        = id;
  std::string              separator  = "/";
  std::size_t              second_pos = tmp.find( separator );
  while( second_pos != std::string::npos )
  {
    if( 0 != second_pos )
    {
      std::string word = tmp.substr( 0, second_pos - 0 );
      result.push_back( word );
    }
    else
      result.push_back( "" );
    tmp        = tmp.substr( second_pos + separator.length() );
    second_pos = tmp.find( separator );
    if( second_pos == std::string::npos ) result.push_back( tmp );
  }
  if( result.size() == 5 )
  {
    Identifier identifier( result[3], result[4] );
    identifier.generateKey( magic_enum::enum_cast<Domain>( result[0] ).value(), magic_enum::enum_cast<Class>( result[1] ).value(), magic_enum::enum_cast<Family>( result[2] ).value() );
    return identifier;
  }
  else
  {
    throw Exception( StatusCode::WRONG_NUMBER_PARAMETERS, "Number of parameters in key should be 5 (Domain/Class/Family/Type/Name) !" );
  }
}

}  // namespace yaodaq
