#include "Utils.h"

namespace hop
{

static uint64_t estimateCpuFreqMhz()
{
   using namespace std::chrono;
   uint32_t cpu;
   volatile uint64_t dummy = 0;
   // Do a quick warmup first
   for( int i = 0; i < 1000; ++i ) { ++dummy; hop::rdtscp( cpu ); }

   // Start timer and get current cycle count
   const auto startTime = high_resolution_clock::now();
   const uint64_t startCycleCount = hop::rdtscp( cpu );

   // Make the cpu work hard
   for( int i = 0; i < 2000000; ++i ) { dummy += i; }

   // Stop timer and get end cycle count
   const uint64_t endCycleCount = hop::rdtscp( cpu );
   const auto endTime = high_resolution_clock::now();

   const uint64_t deltaCycles = endCycleCount - startCycleCount;
   const auto deltaTimeNs = duration_cast<nanoseconds>( endTime - startTime );

   double countPerSec = duration<double>( seconds( 1 ) ) / deltaTimeNs;
   return deltaCycles * countPerSec;
}

uint64_t getCpuFreqHz()
{
   static uint64_t cpuFreq = 0;
   if( cpuFreq == 0 )
   {
      cpuFreq = estimateCpuFreqMhz();
   }

   return cpuFreq;
}

int formatNanosDurationToDisplay( uint64_t duration, char* str, size_t strSize )
{
   if ( duration < 1000 )
   {
      return snprintf( str, strSize, "%" PRIu64 " ns", duration );
   }
   else if ( duration < 1000000 )
   {
      return snprintf( str, strSize, "%.3f us", duration * 0.001f );
   }
   else if ( duration < 1000000000 )
   {
      return snprintf( str, strSize, "%.3f ms", duration * 0.000001f );
   }
   else
   {
      return snprintf( str, strSize, "%.3f s", duration * 0.000000001f );
   }
}

void formatNanosTimepointToDisplay(int64_t timepoint, uint64_t totalNanosInScreen, char* str, size_t strSize)
{
   if (totalNanosInScreen < 1000)
   {
      snprintf(str, strSize, "%" PRId64 " ns", timepoint);
   }
   else if (totalNanosInScreen < 1000000)
   {
      snprintf(str, strSize, "%.3f us", timepoint * 0.001f);
   }
   else if (totalNanosInScreen < 1000000000)
   {
      snprintf(str, strSize, "%.3f ms", timepoint * 0.000001f);
   }
   else
   {
      snprintf(str, strSize, "%.3f s", timepoint * 0.000000001f);
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

} // namespace hop