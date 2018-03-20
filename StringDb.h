#ifndef STRING_DB_H_
#define STRING_DB_H_

#include "Hop.h"

#include <vector>
#include <unordered_map>

namespace hop
{

class StringDb
{
public:
   StringDb();
   bool empty() const;
   void clear();
   void addStringData( const std::vector<char>& inData );
   void addStringData( const char* inData, size_t count );
   size_t getStringIndex( hop::TStrPtr_t strId ) const;
   size_t sizeInBytes() const noexcept;
   inline const char* getString( size_t index ) const
   {
      return &_strData[ index ];
   }
   std::vector< size_t > findStringIndexMatching( const char* ) const noexcept;

   friend size_t serializedSize( const StringDb& strDb );
   friend size_t serialize( const StringDb& strDb, char* data );
   friend size_t deserialize( const char* data, StringDb& strDb );

private:
   std::unordered_map< hop::TStrPtr_t, size_t > _stringIndices;
   std::vector< char > _strData;
};

} // namespace hop

#endif // STRING_DB_H_