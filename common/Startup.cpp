#include "common/Startup.h"
#include "common/Utils.h"
#include "common/BlockAllocator.h"
#include "Hop.h"

#include <string>

namespace hop
{

static void setExecInfo( LaunchOptions& lo, char* fullProcessPath, char** argv, bool startExec )
{
   lo.fullProcessPath = fullProcessPath;
   lo.processName     = fullProcessPath;
   lo.args            = argv;
   lo.startExec       = startExec;

   std::string fullPathStr( fullProcessPath );
   size_t lastSeparator = fullPathStr.find_last_of( "/\\" );
   if ( lastSeparator != std::string::npos )
   {
      lo.processName = &fullProcessPath[++lastSeparator];
   }
}

void printUsage( const char* progname )
{
   printf(
       "Usage : %s [OPTION] <process name>\n\n OPTIONS:\n"
       "\t-o output path for saved file\n"
       "\t-e Launch specified executable with its arguments and start recording\n"
       "\t-v Display version info and exit\n\t-h Show usage\n",
       progname );
}

LaunchOptions parseArgs( int argc, char* argv[] )
{
   LaunchOptions lo{nullptr, nullptr, nullptr, nullptr, false};

   // Invalid argument count
   if ( argc == 1 )
      return lo;

   int i = 0; // Skip first arguments since it is not relevent
   while( argv[++i] )
   {
      if( argv[i][0] == '-' )
      {
         switch( argv[i][1] )
         {
            default:
               break;
            case 'v':
               printf( "hop version %.2f \n", HOP_VERSION );
               exit( 0 );
            case 'h':
               printUsage( argv[0] );
               exit( 0 );
            case 'o' :
               if( !argv[++i] )
               {
                  fprintf( stderr, "Missing output file name\n" );
                  exit( -1 );
               }
               lo.saveFilePath = argv[i];
               break;
            case 'e':
               if( !argv[++i] )
               {
                  fprintf( stderr, "Missing executable name\n" );
                  exit( -1 );
               }
               setExecInfo( lo, argv[i], &argv[i], true );
               return lo;
         }
      }
      else
      {
         setExecInfo( lo, argv[i], &argv[i], false );
         return lo;
      }
   }
   return lo;
}


bool verifyPlatform()
{
   if ( !hop::supportsRDTSCP() )
   {
      printf(
          "This platform does not seem to support RDTSCP. Hop will not be "
          "able to work properly.\n" );
      return false;
   }

   if ( !hop::supportsConstantTSC() )
   {
      printf(
          "This platform does not seem to support Invariant TSC. Hop will be "
          "able to run, but no precision on the measurement are guaranteed.\n" );
   }
   return true;
}

void initializeBlockAllocator()
{
   hop::block_allocator::initialize( 512 );
}

void terminateBlockAllocator()
{
   hop::block_allocator::terminate();
}


}