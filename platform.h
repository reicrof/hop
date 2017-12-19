#ifndef VDBG_PLATFORM_H_
#define VDBG_PLATFORM_H_

#define VDBG_CONSTEXPR constexpr
#define VDBG_NOEXCEPT noexcept
#define VDBG_STATIC_ASSERT static_assert
#define VDBG_GET_THREAD_ID() std::this_thread::get_id()

#endif // VDBG_PLATFORM_H_