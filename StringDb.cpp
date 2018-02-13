#include "StringDb.h"


StringDb::StringDb()
{
   _strData.reserve( 512 );
   _strData.push_back( '\0' ); // First character should always be NULL
   stringIndex.reserve( 256 );
   for( auto& v : idsPerThreads )
   {
      v.reserve( 256 );
      v.push_back( 0 );
   }
}

void StringDb::addStringData( const std::vector<char>& inData, uint32_t threadIndex )
{
   // Update the idsPerThreads
   idsPerThreads[ threadIndex ].resize( idsPerThreads[ threadIndex ].size() + inData.size() );

   std::string curStr;
   curStr.reserve( 256 );
   size_t startStrPos = 0;
   for( size_t i = 0; i < inData.size(); ++i )
   {
      if( inData[i] == '\0' && i != startStrPos )
      {
         curStr.assign( &inData[ startStrPos ], i - startStrPos );
         uint32_t& value = stringIndex[ curStr ];
         if( value == 0 )
         {
            // String was not previously in the db, so add it
            value = _strData.size();
            for( char c : curStr )
               _strData.push_back( c );
         }

         // Update string index for the thread
         idsPerThreads[ threadIndex ][ startStrPos ] = value;

         startStrPos = i + 1;
      }
   }
}

const char* StringDb::getString( uint32_t stringIndex, uint32_t threadIndex ) const
{
   const uint32_t stringIndexForThread = idsPerThreads[ threadIndex ][ stringIndex ];
   return &_strData[ stringIndexForThread ];
}