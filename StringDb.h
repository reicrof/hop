#ifndef STRING_DB_H_
#define STRING_DB_H_

#include "Hop.h"

#include <vector>
#include <array>
#include <unordered_map>
#include <string>

class StringDb
{
public:
   StringDb();
   void addStringData( const std::vector<char>& data, uint32_t threadIndex );
   const char* getString( uint32_t stringIndex, uint32_t threadIndex ) const;

private:
   std::unordered_map< std::string, uint32_t > stringIndex;
   std::array< std::vector< uint32_t >, MAX_THREAD_NB > idsPerThreads;
   std::vector< char > _strData;
};

#endif // STRING_DB_H_