#include "StringDb.h"

#include <cassert>
#include <cstring>

namespace hop
{

StringDb::StringDb()
{
   _strData.reserve( 512 );
   _strData.push_back( '\0' );  // First character should always be NULL
   _stringIndices.reserve( 256 );
}

bool StringDb::empty() const
{
	return _stringIndices.empty();
}

void StringDb::addStringData( const char* inData, size_t count )
{
   using namespace hop;
   size_t i = 0;

   // Check if the first entry is NULL.
   TStrPtr_t firstStrPtr = *( (TStrPtr_t*)&inData[0] );
   if ( firstStrPtr == 0 )
   {
      i += sizeof( TStrPtr_t );
   }

   while ( i < count )
   {
      TStrPtr_t strPtr = *( (TStrPtr_t*)&inData[i] );
      i += sizeof( TStrPtr_t );

      auto& strIndex = _stringIndices[strPtr];
      const size_t stringLen = strlen( &inData[i] );
      // If not already in the db, add it
      if ( strIndex == 0 )
      {
         strIndex = _strData.size();
         _strData.resize( _strData.size() + stringLen + 1 );
         strcpy( &_strData[strIndex], &inData[i] );
      }

      i += stringLen + 1;
   }
}

void StringDb::addStringData( const std::vector<char>& inData )
{
   addStringData( inData.data(), inData.size() );
}

size_t StringDb::getStringIndex( hop::TStrPtr_t strId ) const
{
   // Early return on NULL
   if( strId == 0 ) { return 0; }

   const auto it = _stringIndices.find( strId );
   assert( it != _stringIndices.end() );
   return it->second;
}

void StringDb::formatTraceName(size_t classNameIndex, size_t fctNameIndex, char* nameArray, size_t size) const
{
	if (classNameIndex > 0)
	{
		// We do have a class name. Prepend it to the string
		snprintf(
			nameArray,
			size,
			"%s::%s",
			getString(classNameIndex),
			getString(fctNameIndex) );
	}
	else
	{
		// No class name. Ignore it
		snprintf(
			nameArray,
			size,
			"%s",
			getString(fctNameIndex));
	}
}

}