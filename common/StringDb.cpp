#include "StringDb.h"
#include "common/Utils.h"

#include <cassert>
#include <cstring>

static uint32_t alignOn( uint32_t val, uint32_t alignment )
{
   return (( val + alignment-1) & ~(alignment-1));
}

namespace hop
{

StringDb::StringDb()
{
   _strData.reserve( 512 );
   _stringIndices.reserve( 256 );
   clear();
}

bool StringDb::empty() const
{
   return _stringIndices.empty();
}

void StringDb::clear()
{
   _strData.clear();
   _stringIndices.clear();
}

void StringDb::addStringData( const char* inData, size_t count )
{
   using namespace hop;
   size_t i = 0;

   while ( i < count )
   {
      hop_str_ptr_t strPtr = *( (hop_str_ptr_t*)&inData[i] );
      i += sizeof( hop_str_ptr_t );

      auto& strIndex = _stringIndices[strPtr];
      const size_t stringLen = alignOn( strlen( &inData[i] ) + 1, 8 );
      // If not already in the db, add it
      if ( strIndex == 0 )
      {
         strIndex = _strData.size();
         _strData.resize( _strData.size() + stringLen );
         strcpy( &_strData[strIndex], &inData[i] );
      }

      i += stringLen;
   }
}

void StringDb::addStringData( const std::vector<char>& inData )
{
   addStringData( inData.data(), inData.size() );
}

size_t StringDb::getStringIndex( hop_str_ptr_t strId ) const
{
   // Early return on NULL
   if( strId == 0 ) { return 0; }

   const auto it = _stringIndices.find( strId );
   assert( it != _stringIndices.end() );
   return it->second;
}

size_t StringDb::sizeInBytes() const
{
   return _strData.size();
}

std::vector< size_t > StringDb::findStringIndexMatching( const char* substrToFind ) const noexcept
{
   std::vector< size_t > indices;
   indices.reserve( 64 );

   const int subStrLen = strlen( substrToFind );

   size_t i = 0;
   while( i < _strData.size() )
   {
      const int entryLength = strlen( &_strData[i] );
      if( findSubstrNoCase( &_strData[i], entryLength, substrToFind, subStrLen ) != -1 )
      {
         indices.push_back( i );
      }
      i += alignOn( entryLength + 1, 8 );
   }
   return indices;
}

static constexpr size_t keySize = sizeof( hop_str_ptr_t );
static constexpr size_t valueSize = sizeof( size_t );
static constexpr size_t mapEntrySize = keySize + valueSize;

size_t serializedSize( const StringDb& strDb )
{
   const uint32_t entryCount = (uint32_t)strDb._stringIndices.size();
   const uint32_t dataSize = (uint32_t)strDb._strData.size();

   return 2 * sizeof( uint32_t ) + entryCount * mapEntrySize + dataSize;
}

size_t serialize( const StringDb& strDb, char* data )
{
   const uint32_t entryCount = (uint32_t)strDb._stringIndices.size();
   const uint32_t dataSize = (uint32_t)strDb._strData.size();

   size_t i = 0;
   // Copy the map entry size and the data array size first
   memcpy( &data[i], &entryCount, sizeof( uint32_t ) );
   i += sizeof( uint32_t );
   memcpy( &data[i], &dataSize, sizeof( uint32_t ) );
   i += sizeof( uint32_t );

   // Then copy the map entries
   for( const auto& entry : strDb._stringIndices )
   {
      memcpy( &data[i], &entry.first, keySize );
      memcpy( &data[i+keySize], &entry.second, valueSize );
      i += mapEntrySize;
   }

   // Finally, copy the char data
   memcpy( &data[i], strDb._strData.data(), strDb._strData.size() );
   i += strDb._strData.size();

   assert( i == serializedSize(strDb ) );

   return i;
}

size_t deserialize( const char* data, StringDb& strDb )
{
   constexpr size_t keySize = sizeof( decltype(strDb._stringIndices)::key_type );
   constexpr size_t valueSize = sizeof( decltype(strDb._stringIndices)::mapped_type );
   constexpr size_t mapEntrySize = keySize + valueSize;


   const uint32_t entryCount = *(uint32_t*)&data[0];
   const uint32_t dataSize = *(uint32_t*)&data[sizeof( uint32_t )];
   strDb._stringIndices.reserve( entryCount );
   strDb._strData.resize( dataSize );

   // Read the map data
   const size_t mapStart = 2 * sizeof( uint32_t );
   for( uint32_t i = 0; i < entryCount; ++i )
   {
      hop_str_ptr_t key = *(hop_str_ptr_t*)&data[2*sizeof( uint32_t ) + i * mapEntrySize ];
      size_t value = *(size_t*)&data[(2*sizeof( uint32_t ) + i * mapEntrySize) + keySize ];
      strDb._stringIndices[ key ] = value;
   }

   // Read the data
   const size_t dataStart = mapStart + entryCount * mapEntrySize;
   memcpy( strDb._strData.data(), &data[ dataStart ], dataSize );

   return 2 * sizeof( uint32_t ) + dataSize + entryCount * mapEntrySize;
}

}