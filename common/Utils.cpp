#include "common/Utils.h"

#include "common/platform/Platform.h"

#include <cstring>

namespace hop
{

bool supportsRDTSCP()
{
   int reg[4];
   hop::cpuid( reg, 0x80000001 );
   return reg[3] & (1 << 27);
}

bool supportsConstantTSC()
{
   int reg[4];
   hop::cpuid( reg, 0x80000007 );
   return reg[3] & (1 << 8);
}

int formatCyclesDurationToDisplay(
    uint64_t duration,
    char* str,
    size_t strSize,
    bool asCycles,
    float cpuFreqGHz )
{
   if( asCycles )
   {
      return snprintf( str, strSize, "%" PRIu64 " cycles", duration );
   }
   else
   {
      const auto durationInNs = hop::cyclesToNanos( duration, cpuFreqGHz );
      if( durationInNs < 1000 )
      {
         return snprintf( str, strSize, "%" PRIu64 " ns", durationInNs );
      }
      else if( durationInNs < 1000000 )
      {
         return snprintf( str, strSize, "%.3f us", durationInNs * 0.001f );
      }
      else if( durationInNs < 1000000000 )
      {
         return snprintf( str, strSize, "%.3f ms", durationInNs * 0.000001f );
      }
      else
      {
         return snprintf( str, strSize, "%.3f s", durationInNs * 0.000000001f );
      }
   }
}

int formatCyclesTimepointToDisplay(
    int64_t timepoint,
    uint64_t totalCyclesInScreen,
    char* str,
    size_t strSize,
    bool asCycles,
    float cpuFreqGHz )
{
   if( asCycles )
   {
      return snprintf( str, strSize, "%" PRId64 " cycles", timepoint );
   }
   else
   {
      const auto timepointInNs = hop::cyclesToNanos( timepoint, cpuFreqGHz );
      const auto nanosInScreen = hop::cyclesToNanos( totalCyclesInScreen, cpuFreqGHz );
      if( nanosInScreen < 1000 )
      {
         return snprintf( str, strSize, "%" PRId64 " ns", timepointInNs );
      }
      else if( nanosInScreen < 1000000 )
      {
         return snprintf( str, strSize, "%.3f us", timepointInNs * 0.001f );
      }
      else if( nanosInScreen < 1000000000 )
      {
         return snprintf( str, strSize, "%.3f ms", timepointInNs * 0.000001f );
      }
      else
      {
         return snprintf( str, strSize, "%.3f s", timepointInNs * 0.000000001f );
      }
   }
}

void formatSizeInBytesToDisplay( size_t sizeInBytes, char* str, size_t strSize )
{
   if (sizeInBytes < 1000)
   {
      snprintf(str, strSize, "%d B", (int)sizeInBytes);
   }
   else if (sizeInBytes < 1000000)
   {
      snprintf(str, strSize, "%.3f kB", sizeInBytes / 1000.0f);
   }
   else if (sizeInBytes < 1000000000)
   {
      snprintf(str, strSize, "%.3f MB",  sizeInBytes / 1000000.0f);
   }
   else
   {
      snprintf(str, strSize, "%.3f GB",  sizeInBytes / 1000000000.0f);
   }
}

static bool noCaseCmp( char lhs, char rhs )
{
   return std::toupper( lhs ) == std::toupper( rhs );
}

int findSubstrNoCase(
    const char* haystack,
    uint32_t haystackSize,
    const char* needle,
    uint32_t needleSize )
{
   const auto it =
       std::search( haystack, haystack + haystackSize, needle, needle + needleSize, noCaseCmp );
   if( it != haystack + haystackSize )
   {
      return it - haystack;
   }
   else
   {
      return -1;  // not found
   }
}

static bool isNumber( const char* str )
{
   const size_t length = strlen( str );
   return std::all_of( str, str + length, ::isdigit );
}

int getPIDFromString( const char* str )
{
   int pid = -1;
   if( isNumber( str ) )
   {
      pid = strtol( str, nullptr, 10 );
   }

   return pid;
}

} // namespace hop