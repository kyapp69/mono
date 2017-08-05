/*
 * Licensed to the .NET Foundation under one or more agreements.
 * The .NET Foundation licenses this file to you under the MIT license.
 * See the LICENSE file in the project root for more information.
 */

#ifndef __MONO_PROFILER_H__
#define __MONO_PROFILER_H__

#include <mono/metadata/appdomain.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/object.h>

MONO_BEGIN_DECLS

/*
 * This value will be incremented whenever breaking changes to the profiler API
 * are made. This macro is intended for use in profiler modules that wish to
 * support older versions of the profiler API.
 */
#define MONO_PROFILER_API_VERSION 2

/*
 * Loads a profiler module based on the specified description. The description
 * can be of the form "name:args" or just "name". For example, "log:sample" and
 * "log" will both load "libmono-profiler-log.so". The description is passed to
 * the module after it has been loaded. If the specified module has already
 * been loaded, this function has no effect.
 *
 * A module called foo should declare an entry point like so:
 *
 * void mono_profiler_init_foo (const char *desc)
 * {
 * }
 *
 * This function is not async safe.
 */
MONO_API void mono_profiler_load (const char *desc);

typedef struct _MonoProfiler MonoProfiler;
typedef struct _MonoProfilerDesc *MonoProfilerHandle;

/*
 * Installs a profiler and returns a handle for it. The handle is used with the
 * other functions in the profiler API (e.g. for setting up callbacks). The
 * given structure pointer will be passed to all callbacks from the profiler
 * API. It can be NULL.
 *
 * This function may only be called from your profiler's init function.
 *
 * Example usage:
 *
 * struct _MonoProfiler {
 * 	int my_stuff;
 * 	// ...
 * };
 *
 * MonoProfiler *prof = calloc (1, sizeof (MonoProfiler));
 * MonoProfilerHandle handle = mono_profiler_create (prof);
 * mono_profiler_set_shutdown_callback (handle, my_shutdown_cb);
 *
 * This function is not async safe.
 */
MONO_API MonoProfilerHandle mono_profiler_create (MonoProfiler *prof);

/*
 * Enables support for code coverage instrumentation. At the moment, this means
 * enabling the debug info subsystem. If you do not call this function, you
 * will not be able to use mono_profiler_get_coverage_data. Returns TRUE if
 * code coverage support was enabled, or FALSE if the function was called too
 * late for this to be possible.
 *
 * This function may only be called from your profiler's init function.
 *
 * This function is not async safe.
 */
MONO_API mono_bool mono_profiler_enable_coverage (void);

typedef mono_bool (*MonoProfilerCoverageFilterCallback) (MonoProfiler *prof, MonoMethod *method);

/*
 * Sets a code coverage filter function. The profiler API will invoke filter
 * functions from all installed profilers. If any of them return TRUE, then the
 * given method will be instrumented for coverage analysis. All filters are
 * guaranteed to be called at least once per method, even if an earlier filter
 * has already returned TRUE.
 *
 * Note that filter functions must be installed before a method is compiled in
 * order to have any effect, i.e. you should register your filter function in
 * your profiler's init function.
 *
 * This function is async safe.
 */
MONO_API void mono_profiler_set_coverage_filter_callback (MonoProfilerHandle handle, MonoProfilerCoverageFilterCallback cb);

typedef struct {
	MonoMethod *method;
	uint32_t il_offset;
	uint32_t counter;
	const char *file_name;
	uint32_t line;
	uint32_t column;
} MonoProfilerCoverageData;

typedef void (*MonoProfilerCoverageCallback) (MonoProfiler *prof, const MonoProfilerCoverageData *data);

/*
 * Retrieves all coverage data for the specified method and invokes the given
 * callback for each entry. Source location information will only be filled out
 * if the given method has debug info available. Returns TRUE if the given
 * method was instrumented for code coverage; otherwise, FALSE.
 *
 * Please note that the structure passed to the callback is only valid for the
 * duration of the callback.
 *
 * This function is not async safe.
 */
MONO_API mono_bool mono_profiler_get_coverage_data (MonoProfilerHandle handle, MonoMethod *method, MonoProfilerCoverageCallback cb);

typedef enum {
	/*
	 * Do not perform sampling. Will make the sampling thread sleep until the
	 * sampling mode is changed to one of the below modes.
	 */
	MONO_PROFILER_SAMPLE_MODE_NONE = 0,
	/*
	 * Try to base sampling frequency on process activity. Falls back to
	 * MONO_PROFILER_SAMPLE_MODE_REAL if such a clock is not available.
	 */
	MONO_PROFILER_SAMPLE_MODE_PROCESS = 1,
	/*
	 * Base sampling frequency on wall clock time. Uses a monotonic clock when
	 * available (all major platforms).
	 */
	MONO_PROFILER_SAMPLE_MODE_REAL = 2,
} MonoProfilerSampleMode;

/*
 * Enables the sampling thread. You must call this function if you intend to use
 * statistical sampling; mono_profiler_set_sample_mode will have no effect if
 * this function has not been called. The first profiler to call this function
 * will get ownership over sampling settings (mode and frequency) so that no
 * other profiler can change those settings. Returns TRUE if the sampling
 * thread was enabled, or FALSE if the function was called too late for this
 * to be possible.
 *
 * Note that you still need to call mono_profiler_set_sample_mode with a mode
 * other than MONO_PROFILER_SAMPLE_MODE_NONE to actually start sampling.
 *
 * This function may only be called from your profiler's init function.
 *
 * This function is not async safe.
 */
MONO_API mono_bool mono_profiler_enable_sampling (MonoProfilerHandle handle);

/*
 * Sets the sampling mode and frequency (in Hz). The frequency must be a
 * positive number. If the calling profiler has ownership over sampling
 * settings, the settings will be changed and this function will return TRUE;
 * otherwise, it returns FALSE without changing any settings.
 *
 * This function is async safe.
 */
MONO_API mono_bool mono_profiler_set_sample_mode (MonoProfilerHandle handle, MonoProfilerSampleMode mode, uint32_t freq);

/*
 * Retrieves the current sampling mode and/or frequency (in Hz). Returns TRUE if
 * the calling profiler is allowed to change the sampling settings; otherwise,
 * FALSE.
 *
 * This function is async safe.
 */
MONO_API mono_bool mono_profiler_get_sample_mode (MonoProfilerHandle handle, MonoProfilerSampleMode *mode, uint32_t *freq);

/*
 * Enables instrumentation of GC allocations. This is necessary so that managed
 * allocators can be instrumented with a call into the profiler API. Allocations
 * will not be reported unless this function is called. Returns TRUE if
 * allocation instrumentation was enabled, or FALSE if the function was called
 * too late for this to be possible.
 *
 * This function may only be called from your profiler's init function.
 *
 * This function is not async safe.
 */
MONO_API mono_bool mono_profiler_enable_allocations (void);

typedef enum {
	/* Do not instrument calls. */
	MONO_PROFILER_CALL_INSTRUMENTATION_NONE = 0,
	/* Instrument method prologues. */
	MONO_PROFILER_CALL_INSTRUMENTATION_PROLOGUE = 1 << 1,
	/* Also capture a call context for prologues. */
	MONO_PROFILER_CALL_INSTRUMENTATION_PROLOGUE_CONTEXT = 1 << 2,
	/* Instrument method epilogues. */
	MONO_PROFILER_CALL_INSTRUMENTATION_EPILOGUE = 1 << 3,
	/* Also capture a call context for epilogues. */
	MONO_PROFILER_CALL_INSTRUMENTATION_EPILOGUE_CONTEXT = 1 << 4,
} MonoProfilerCallInstrumentationFlags;

typedef MonoProfilerCallInstrumentationFlags (*MonoProfilerCallInstrumentationFilterCallback) (MonoProfiler *prof, MonoMethod *method);

/*
 * Sets a call instrumentation filter function. The profiler API will invoke
 * filter functions from all installed profilers. If any of them return flags
 * other than MONO_PROFILER_CALL_INSTRUMENTATION_NONE, then the given method
 * will be instrumented as requested. All filters are guaranteed to be called
 * exactly once per method, even if earlier filters have already specified all
 * flags.
 *
 * Note that filter functions must be installed before a method is compiled in
 * order to have any effect, i.e. you should register your filter function in
 * your profiler's init function. Also, if you want to instrument a method
 * that's going to be AOT-compiled, you must attach your profiler and install a
 * call instrumentation filter function at AOT time. This can be done in
 * exactly the same way as you would normally, i.e. by passing the --profile
 * option on the command line, by calling mono_profiler_load, or simply by
 * using the profiler API as an embedder.
 *
 * Keep in mind that indiscriminate method instrumentation is extremely heavy
 * and will slow down most applications to a crawl. Consider using sampling
 * instead if it would work for your use case.
 *
 * This function is async safe.
 */
MONO_API void mono_profiler_set_call_instrumentation_filter_callback (MonoProfilerHandle handle, MonoProfilerCallInstrumentationFilterCallback cb);

/*
 * Enables support for retrieving stack frame data from a call context. At the
 * moment, this means enabling the debug info subsystem. If you do not call
 * this function, you will not be able to use the call context introspection
 * functions (they will simply return NULL). Returns TRUE if call context
 * introspection was enabled, or FALSE if the function was called too late for
 * this to be possible.
 *
 * Please note: Mono's LLVM backend does not support this feature. This means
 * that methods with call context instrumentation will be handled by Mono's
 * JIT even in LLVM mode. There is also a special case when Mono is compiling
 * in LLVM-only mode: Since LLVM does not provide a way to implement call
 * contexts, a NULL context will always be passed to enter/leave events even
 * though this method returns TRUE.
 *
 * This function may only be called from your profiler's init function.
 *
 * This function is not async safe.
 */
MONO_API mono_bool mono_profiler_enable_call_context_introspection (void);

typedef struct _MonoProfilerCallContext MonoProfilerCallContext;

/*
 * Given a valid call context from an enter/leave event, retrieves a pointer to
 * the this reference for the method. Returns NULL if none exists (i.e. it's a
 * static method) or if call context introspection was not enabled.
 *
 * The buffer returned by this function must be freed with
 * mono_profiler_call_context_free_buffer.
 *
 * Please note that a call context is only valid for the duration of the
 * enter/leave callback it was passed to.
 *
 * This function is not async safe.
 */
MONO_API void *mono_profiler_call_context_get_this (MonoProfilerCallContext *context);

/*
 * Given a valid call context from an enter/leave event, retrieves a pointer to
 * the method argument at the given position. Returns NULL if position is out
 * of bounds or if call context introspection was not enabled.
 *
 * The buffer returned by this function must be freed with
 * mono_profiler_call_context_free_buffer.
 *
 * Please note that a call context is only valid for the duration of the
 * enter/leave callback it was passed to.
 *
 * This function is not async safe.
 */
MONO_API void *mono_profiler_call_context_get_argument (MonoProfilerCallContext *context, uint32_t position);

/*
 * Given a valid call context from an enter/leave event, retrieves a pointer to
 * the local variable at the given position. Returns NULL if position is out of
 * bounds or if call context introspection was not enabled.
 *
 * The buffer returned by this function must be freed with
 * mono_profiler_call_context_free_buffer.
 *
 * Please note that a call context is only valid for the duration of the
 * enter/leave callback it was passed to.
 *
 * This function is not async safe.
 */
MONO_API void *mono_profiler_call_context_get_local (MonoProfilerCallContext *context, uint32_t position);

/*
 * Given a valid call context from an enter/leave event, retrieves a pointer to
 * return value of a method. Returns NULL if the method has no return value
 * (i.e. it returns void), if the leave event was the result of a tail call, if
 * the function is called on a context from an enter event, or if call context
 * introspection was not enabled.
 *
 * The buffer returned by this function must be freed with
 * mono_profiler_call_context_free_buffer.
 *
 * Please note that a call context is only valid for the duration of the
 * enter/leave callback it was passed to.
 *
 * This function is not async safe.
 */
MONO_API void *mono_profiler_call_context_get_result (MonoProfilerCallContext *context);

/*
 * Frees a buffer returned by one of the call context introspection functions.
 * Passing a NULL buffer is allowed, which makes this function a no-op.
 *
 * This function is not async safe.
 */
MONO_API void mono_profiler_call_context_free_buffer (void *buffer);

#ifdef MONO_PROFILER_UNSTABLE_GC_ROOTS
typedef enum {
	/* Upper 2 bytes. */
	MONO_PROFILER_GC_ROOT_PINNING = 1 << 8,
	MONO_PROFILER_GC_ROOT_WEAKREF = 2 << 8,
	MONO_PROFILER_GC_ROOT_INTERIOR = 4 << 8,

	/* Lower 2 bytes (flags). */
	MONO_PROFILER_GC_ROOT_STACK = 1 << 0,
	MONO_PROFILER_GC_ROOT_FINALIZER = 1 << 1,
	MONO_PROFILER_GC_ROOT_HANDLE = 1 << 2,
	MONO_PROFILER_GC_ROOT_OTHER = 1 << 3,
	MONO_PROFILER_GC_ROOT_MISC = 1 << 4,

	MONO_PROFILER_GC_ROOT_TYPEMASK = 0xff,
} MonoProfilerGCRootType;
#endif

typedef enum {
	/* data = MonoMethod *method */
	MONO_PROFILER_CODE_BUFFER_METHOD = 0,
	MONO_PROFILER_CODE_BUFFER_METHOD_TRAMPOLINE = 1,
	MONO_PROFILER_CODE_BUFFER_UNBOX_TRAMPOLINE = 2,
	MONO_PROFILER_CODE_BUFFER_IMT_TRAMPOLINE = 3,
	MONO_PROFILER_CODE_BUFFER_GENERICS_TRAMPOLINE = 4,
	/* data = const char *name */
	MONO_PROFILER_CODE_BUFFER_SPECIFIC_TRAMPOLINE = 5,
	MONO_PROFILER_CODE_BUFFER_HELPER = 6,
	MONO_PROFILER_CODE_BUFFER_MONITOR = 7,
	MONO_PROFILER_CODE_BUFFER_DELEGATE_INVOKE = 8,
	MONO_PROFILER_CODE_BUFFER_EXCEPTION_HANDLING = 9,
} MonoProfilerCodeBufferType;

typedef enum {
	MONO_GC_EVENT_PRE_STOP_WORLD = 6,
	/* When this event arrives, the GC and suspend locks are acquired. */
	MONO_GC_EVENT_PRE_STOP_WORLD_LOCKED = 10,
	MONO_GC_EVENT_POST_STOP_WORLD = 7,
	MONO_GC_EVENT_START = 0,
	MONO_GC_EVENT_END = 5,
	MONO_GC_EVENT_PRE_START_WORLD = 8,
	/* When this event arrives, the GC and suspend locks are released. */
	MONO_GC_EVENT_POST_START_WORLD_UNLOCKED = 11,
	MONO_GC_EVENT_POST_START_WORLD = 9,
} MonoProfilerGCEvent;

/*
 * The macros below will generate the majority of the callback API. Refer to
 * mono/metadata/profiler-events.h for a list of callbacks. They are expanded
 * like so:
 *
 * typedef void (*MonoProfilerRuntimeInitializedCallback (MonoProfiler *prof);
 * MONO_API void mono_profiler_set_runtime_initialized_callback (MonoProfiler *prof, MonoProfilerRuntimeInitializedCallback cb);
 *
 * typedef void (*MonoProfilerRuntimeShutdownCallback (MonoProfiler *prof);
 * MONO_API void mono_profiler_set_runtime_shutdown_callback (MonoProfiler *prof, MonoProfilerRuntimeShutdownCallback cb);
 *
 * typedef void (*MonoProfilerContextLoadedCallback (MonoProfiler *prof);
 * MONO_API void mono_profiler_set_context_loaded_callback (MonoProfiler *prof, MonoProfilerContextLoadedCallback cb);
 *
 * typedef void (*MonoProfilerContextUnloadedCallback (MonoProfiler *prof);
 * MONO_API void mono_profiler_set_context_unloaded_callback (MonoProfiler *prof, MonoProfilerContextUnloadedCallback cb);
 *
 * Etc.
 *
 * To remove a callback, pass NULL instead of a valid function pointer.
 * Callbacks can be changed at any point, but note that doing so is inherently
 * racy with respect to threads that aren't suspended, i.e. you may still see a
 * call from another thread right after you change a callback.
 *
 * These functions are async safe.
 */

#define _MONO_PROFILER_EVENT(type, ...) \
	typedef void (*MonoProfiler ## type ## Callback) (__VA_ARGS__);
#define MONO_PROFILER_EVENT_0(name, type) \
		_MONO_PROFILER_EVENT(type, MonoProfiler *prof)
#define MONO_PROFILER_EVENT_1(name, type, arg1_type, arg1_name) \
		_MONO_PROFILER_EVENT(type, MonoProfiler *prof, arg1_type arg1_name)
#define MONO_PROFILER_EVENT_2(name, type, arg1_type, arg1_name, arg2_type, arg2_name) \
		_MONO_PROFILER_EVENT(type, MonoProfiler *prof, arg1_type arg1_name, arg2_type arg2_name)
#define MONO_PROFILER_EVENT_3(name, type, arg1_type, arg1_name, arg2_type, arg2_name, arg3_type, arg3_name) \
		_MONO_PROFILER_EVENT(type, MonoProfiler *prof, arg1_type arg1_name, arg2_type arg2_name, arg3_type arg3_name)
#define MONO_PROFILER_EVENT_4(name, type, arg1_type, arg1_name, arg2_type, arg2_name, arg3_type, arg3_name, arg4_type, arg4_name) \
		_MONO_PROFILER_EVENT(type, MonoProfiler *prof, arg1_type arg1_name, arg2_type arg2_name, arg3_type arg3_name, arg4_type arg4_name)
#include <mono/metadata/profiler-events.h>
#undef MONO_PROFILER_EVENT_0
#undef MONO_PROFILER_EVENT_1
#undef MONO_PROFILER_EVENT_2
#undef MONO_PROFILER_EVENT_3
#undef MONO_PROFILER_EVENT_4
#undef _MONO_PROFILER_EVENT

#define _MONO_PROFILER_EVENT(name, type) \
	MONO_API void mono_profiler_set_ ## name ## _callback (MonoProfilerHandle handle, MonoProfiler ## type ## Callback cb);
#define MONO_PROFILER_EVENT_0(name, type) \
	_MONO_PROFILER_EVENT(name, type)
#define MONO_PROFILER_EVENT_1(name, type, arg1_type, arg1_name) \
	_MONO_PROFILER_EVENT(name, type)
#define MONO_PROFILER_EVENT_2(name, type, arg1_type, arg1_name, arg2_type, arg2_name) \
	_MONO_PROFILER_EVENT(name, type)
#define MONO_PROFILER_EVENT_3(name, type, arg1_type, arg1_name, arg2_type, arg2_name, arg3_type, arg3_name) \
	_MONO_PROFILER_EVENT(name, type)
#define MONO_PROFILER_EVENT_4(name, type, arg1_type, arg1_name, arg2_type, arg2_name, arg3_type, arg3_name, arg4_type, arg4_name) \
	_MONO_PROFILER_EVENT(name, type)
#include <mono/metadata/profiler-events.h>
#undef MONO_PROFILER_EVENT_0
#undef MONO_PROFILER_EVENT_1
#undef MONO_PROFILER_EVENT_2
#undef MONO_PROFILER_EVENT_3
#undef MONO_PROFILER_EVENT_4
#undef _MONO_PROFILER_EVENT

MONO_END_DECLS

#endif // __MONO_PROFILER_H__
