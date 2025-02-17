#pragma once

#if defined(__COUNTER__) && (__COUNTER__ + 1 == __COUNTER__ + 0)
#	define UNUSED(type) type _unused##__COUNTER__
#else
#	define UNUSED(type) type _unused##__LINE__
#endif
