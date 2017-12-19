#ifndef VDBG_CLIENT_H_
#define VDBG_CLIENT_H_

#include <platform.h>

#if !defined( VDBG_ENABLED )

#define VDBG_PROF_FUNC()
#define VDBG_PROF_MEMBER_FUNC()
#define VDBG_PROF_FUNC_WITH_GROUP( x )
#define VDBG_PROF_MEMBER_FUNC_WITH_GROUP( x )

#else

#include <message.h>
#include <thread>

// Create a new profiling trace for a free function
//#define VDBG_PROF_FUNC() vdbg::details::ProfGuard vdbgProfGuard( __func__, NULL, 0 )
#define VDBG_PROF_FUNC() VDBG_PROF_GUARD_VAR(__LINE__,( __func__, NULL, 0 ))
// Create a new profiling trace for a member function
#define VDBG_PROF_MEMBER_FUNC() \
   VDBG_PROF_GUARD_VAR(__LINE__,(__func__, typeid( this ).name(), 0 ))
// Create a new profiling trace for a free function that falls under category x
#define VDBG_PROF_FUNC_WITH_GROUP( x ) VDBG_PROF_GUARD_VAR(__LINE__,(( __func__, NULL, x ))
// Create a new profiling trace for a member function that falls under category x
#define VDBG_PROF_MEMBER_FUNC_WITH_GROUP( x ) \
   VDBG_PROF_GUARD_VAR(__LINE__,(( __func__, typeid( this ).name(), x ))

#define VDBG_COMBINE( X,Y ) X##Y
#define VDBG_PROF_GUARD_VAR( LINE, ARGS ) vdbg::details::ProfGuard COMBINE(vdbgProfGuard,LINE) ARGS

namespace vdbg
{
namespace details
{
static constexpr int MAX_THREAD_NB = 64;
class ClientProfiler
{
  public:
   class Impl;
   static Impl* Get( std::thread::id tId );
   static void StartProfile( Impl* );
   static void EndProfile(
       Impl*,
       const char* name,
       const char* classStr,
       vdbg::TimeStamp start,
       vdbg::TimeStamp end,
       uint8_t group );

  private:
   static size_t threadsId[MAX_THREAD_NB];
   static ClientProfiler::Impl* clientProfilers[MAX_THREAD_NB];
};

class ProfGuard
{
  public:
   ProfGuard( const char* name, const char* classStr, uint8_t groupId ) VDBG_NOEXCEPT
       : start( vdbg::getTimeStamp() ),
         fctName( name ),
         className( classStr ),
         impl( ClientProfiler::Get( VDBG_GET_THREAD_ID() ) ),
         group( groupId )
   {
      ClientProfiler::StartProfile( impl );
   }
   ~ProfGuard()
   {
      end = vdbg::getTimeStamp();
      ClientProfiler::EndProfile( impl, fctName, className, start, end, group );
   }

  private:
   vdbg::TimeStamp start, end;
   const char *className, *fctName;
   ClientProfiler::Impl* impl;
   uint8_t group;
};

}  // namespace details
}  // namespace vdbg

#endif  // !defined(VDBG_ENABLED)

#endif  // VDBG_CLIENT_H_