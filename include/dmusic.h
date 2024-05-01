// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#pragma once
#include <stddef.h>
#include <stdint.h>

/// \cond DmHidden
#ifdef __cplusplus
	#define DM_EXTERN extern "C"
#else
	#define DM_EXTERN
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
	#ifdef DM_BUILD
		#ifdef __GNUC__
			#define DMAPI DM_EXTERN __attribute__((dllexport))
		#else
			#define DMAPI DM_EXTERN __declspec(dllexport)
		#endif
	#else
		#ifdef __GNUC__
			#define DMAPI DM_EXTERN __attribute__((dllimport))
		#else
			#define DMAPI DM_EXTERN __declspec(dllimport)
		#endif
	#endif
	#define DMINT
#else
	#define DMAPI DM_EXTERN __attribute__((visibility("default")))
	#define DMINT DM_EXTERN __attribute__((visibility("hidden")))
#endif
/// \endcond

/// \defgroup DmCommonGroup Common
/// \addtogroup DmCommonGroup
/// \{

typedef enum DmResult {
	DmResult_SUCCESS = 0,
	DmResult_INVALID_ARGUMENT,
	DmResult_INVALID_STATE,
	DmResult_MEMORY_EXHAUSTED,
	DmResult_NOT_FOUND,
	DmResult_FILE_CORRUPT,
	DmResult_MUTEX_ERROR,
} DmResult;

typedef struct DmGuid {
	uint8_t data[16];
} DmGuid;

/// \brief A `malloc`-like memory allocation function.
///
/// Allocates \p len bytes of contiguous memory and returns a pointer to the first byte allocated. May return NULL
/// if memory allocation fails for any reason. In this case, function which rely on allocation will failed with
/// #DmResult_MEMORY_EXHAUSTED to indicate memory allocation failure.
///
/// \warning Functions implementing this interface are required to be thread-safe.
///
/// \param ctx[in] An arbitrary pointer provided when calling #Dm_setHeapAllocator.
/// \param len[in] The number of bytes to allocate.
///
/// \return A pointer to the first byte of the allocated memory block or NULL.
/// \retval NULL Memory allocation failed.
///
/// \see Dm_setHeapAllocator Set the memory allocator for the library.
typedef void* DmMemoryAlloc(void* ctx, size_t len);

/// \brief A `free`-like memory de-allocation function.
///
/// De-allocates bytes starting a the memory location pointed to by \p ptr, which were previously allocated by
/// calling a corresponding #DmMemoryAlloc function. This function must not fail. If \p ptr is `NULL`, this
/// function must be a no-op.
///
/// \warning Functions implementing this interface are required to be thread-safe.
///
/// \param ctx[in] An arbitrary pointer provided when calling #Dm_setHeapAllocator.
/// \param ptr[in] A pointer to free, previously returned by the corresponding #DmMemoryAlloc function or NULL.
///
/// \see Dm_setHeapAllocator Set the memory allocator for the library.
typedef void DmMemoryFree(void* ctx, void* ptr);

/// \brief Set the memory allocator to use internally.
///
///	This function should be called before calling any other library functions since, calling it after any allocation
///	has been made internally will result in an error. It is guaranteed, however, that logging-related functions will
///	never allocate.
///
/// \warning This function is **not** thread safe.
///
/// \param alloc[in] A `malloc`-like function, which allocates memory. May not be NULL.
/// \param free[in] A `free`-like function, which free memory previously allocated using \p alloc. May not be NULL.
/// \param ctx[in] An arbitrary pointer passed to \p alloc and \p free on every invocation.
///
/// \return #DmResult_SUCCESS if the operation completed and an error code if it did not.
/// \retval #DmResult_INVALID_ARGUMENT Either \p alloc or \p free was `NULL`.
/// \retval #DmResult_INVALID_STATE The function was called after an allocation was already made.
///
/// \see DmMemoryAlloc Allocation function definition.
/// \see DmMemoryFree De-allocation function definition.
DMAPI DmResult Dm_setHeapAllocator(DmMemoryAlloc* alloc, DmMemoryFree* free, void* ctx);

/// \brief The set of message levels supported by the logging facilities.
typedef enum DmLogLevel {
	/// \brief The log message indicates an fatal error.
	DmLogLevel_FATAL = 10,

	/// \brief The log message indicates an error.
	DmLogLevel_ERROR = 20,

	/// \brief The log message indicates a warning.
	DmLogLevel_WARN = 30,

	/// \brief The log message is informational.
	DmLogLevel_INFO = 40,

	/// \brief The log message is a debug message.
	DmLogLevel_DEBUG = 50,

	/// \brief The log message is a tracing message.
	DmLogLevel_TRACE = 60,
} DmLogLevel;

typedef void DmLogHandler(void* ctx, DmLogLevel lvl, char const* msg);

/// \brief Set a callback to send log messages to.
///
/// Registers the given \p log function to be called whenever a log message is issued by the library.
/// The given \p ctx pointer is passed alongside the log message on every invocation. If \p log is set
/// to `NULL`, any existing log callback function is removed and logging is disabled.
///
/// \param lvl The log level to set for the library.
/// \param log[in] The callback function to invoke whenever log message is generated or `NULL`
/// \param ctx[in] An arbitrary pointer passed to \p log on every invocation.
///
/// \see DmLogHandler Log callback function definition.
/// \see Dm_setLoggerLevel Function to set the log level on its own.
DMAPI void Dm_setLogger(DmLogLevel lvl, DmLogHandler* log, void* ctx);

/// \brief Set a default logging function.
///
/// Registers a default log handler which outputs all log messages at or above the given level to the standard
/// error stream (`stderr`).
///
/// \param lvl The log level to set for the library.
/// \see Dm_setLoggerLevel Function to set the log level on its own.
DMAPI void Dm_setLoggerDefault(DmLogLevel lvl);

/// \brief Sets the log level of the library.
/// \param lvl The log level to set.
DMAPI void Dm_setLoggerLevel(DmLogLevel lvl);

/// \}

typedef struct DmSegment DmSegment;
typedef struct DmLoader DmLoader;
typedef struct DmPerformance DmPerformance;

/// \defgroup DmSegmentGroup Segment
/// \addtogroup DmSegmentGroup
/// \{

/// \brief Add one to the reference count of a segment.
/// \param slf[in] The segment to retain.
/// \return The same segment as was given in \p slf or `NULL` if \p slf was `NULL`.
DMAPI DmSegment* DmSegment_retain(DmSegment* slf);

/// \brief Subtract one from the reference count of a segment.
///
/// If a call to this function reduces the reference count to zero, it also de-allocates the segment and
/// releases any resources referenced by it.
///
/// \param slf[in] The segment to release.
DMAPI void DmSegment_release(DmSegment* slf);

/// \brief Download all resources needed by the segment.
///
/// In order to play a segment, its internal resources, like references to styles and bands need to be resolved and
/// downloaded. This is done by either calling #DmSegment_download manually or by providing the #DmLoader_DOWNLOAD flag
/// when creating the loader.
///
/// \param slf[in] The segment to download resources for.
/// \param loader[in] The loader to use for downloading resources
///
/// \return #DmResult_SUCCESS if the operation completed and an error code if it did not.
/// \retval #DmResult_INVALID_ARGUMENT Either \p slf or \p loader was `NULL`.
/// \retval #DmResult_NOT_FOUND An internal resource required by the segment was not found.
/// \retval #DmResult_MUTEX_ERROR An error occurred while trying to lock an internal mutex.
/// \retval #DmResult_MEMORY_EXHAUSTED A dynamic memory allocation failed.
DMAPI DmResult DmSegment_download(DmSegment* slf, DmLoader* loader);

/// \}

/// \defgroup DmLoaderGroup Loader
/// \addtogroup DmLoaderGroup
/// \{

/// \brief Configuration flags for DirectMusic Loaders.
/// \see DmLoader_create
typedef enum DmLoaderOptions {
	/// \brief Automatically download references.
	DmLoader_DOWNLOAD = 1U << 0U,

	/// \brief Default options for loader objects.
	DmLoader_DEFAULT = 0U,
} DmLoaderOptions;

/// \brief A function used to look up and read in DirectMusic objects by file name.
///
/// When called, a function implementing this interface should look up a DirectMusic data file
/// corresponding to the given \p file name and return the data contained within as a memory buffer.
/// When the function fails to find an appropriate file, it should return `NULL`.
///
/// The returned memory buffer's ownership will be transferred to the loader, so it must be able to be
/// de-allocated using `free` or (if set) the custom de-allocation function passed to #Dm_setHeapAllocator.
///
/// \param ctx[in] An arbitrary pointer provided when calling #DmLoader_addResolver.
/// \param file[in] The name of the file to look up.
/// \param len[out] The length of the returned memory buffer in bytes.
///
/// \return A memory buffer containing the file data or `NULL` if the lookup failed. **Ownership of this
///         buffer is transferred to the loader.**
typedef void* DmLoaderResolverCallback(void* ctx, char const* file, size_t* len);

/// \brief Create a new DirectMusic Loader object.
///
/// If the #DmLoader_DOWNLOAD option is defined, all references for objects retrieved for the loader
/// are automatically resolved and downloaded.
///
/// \param slf[out] A pointer to a variable in which to store the newly created loader.
/// \param opt A bitfield containing loader configuration flags.
///
/// \return #DmResult_SUCCESS if the operation completed and an error code if it did not.
/// \retval #DmResult_INVALID_ARGUMENT \p slf was `NULL`.
/// \retval #DmResult_MEMORY_EXHAUSTED A dynamic memory allocation failed.
DMAPI DmResult DmLoader_create(DmLoader** slf, DmLoaderOptions opt);

/// \brief Add one to the reference count of a loader.
/// \param slf[in] The loader to retain.
/// \return The same loader as was given in \p slf or `NULL` if \p slf was `NULL`.
DMAPI DmLoader* DmLoader_retain(DmLoader* slf);

/// \brief Subtract one from the reference count of a loader.
///
/// If a call to this function reduces the reference count to zero, it also de-allocates the loader and
/// releases any resources referenced by it.
///
/// \param slf[in] The loader to release.
DMAPI void DmLoader_release(DmLoader* slf);

/// \brief Add a resolver to the loader.
///
/// Resolvers are used to locate stored DirectMusic object by file name. Whenever the loader needs to
/// look up an object, it calls all resolvers in sequential order until one returns a match. If no match
/// is found, an error is issued and the object is not loaded.
///
/// \param slf[in] The loader to add a resolver to.
/// \param resolve[in] The callback function used to resolve a file using the new resolver.
/// \param ctx[in] An arbitrary pointer passed to \p resolve on every invocation.
///
/// \return #DmResult_SUCCESS if the operation completed and an error code if it did not.
/// \retval #DmResult_INVALID_ARGUMENT \p slf or \p resolve was `NULL`.
/// \retval #DmResult_MEMORY_EXHAUSTED A dynamic memory allocation failed.
/// \retval #DmResult_MUTEX_ERROR An error occurred while trying to lock an internal mutex.
DMAPI DmResult DmLoader_addResolver(DmLoader* slf, DmLoaderResolverCallback* resolve, void* ctx);

/// \brief Get a segment from the loader's cache or load it by file \p name.
///
/// Gets a segment from the loader's cache if it's enabled (see #DmLoader_CACHE) or loads the segment
/// using the resolvers added to the loader. If the requested segment is found in neither the loader,
/// nor by any resolver, an error is issued.
///
/// If the loader was created using the #DmLoader_DOWNLOAD option, this function automatically downloads
/// the segment by calling #DmSegment_download.
///
/// \param slf[in] The loader to load a segment from.
/// \param name[in] The file name of the segment to load.
/// \param segment[out] A pointer to a variable in which to store the segment.
///
/// \return #DmResult_SUCCESS if the operation completed and an error code if it did not.
/// \retval #DmResult_INVALID_ARGUMENT \p slf, \p name or \p segment was `NULL`.
/// \retval #DmResult_MEMORY_EXHAUSTED A dynamic memory allocation failed.
/// \retval #DmResult_NOT_FOUND No segment with the given name could be found.
/// \retval #DmResult_MUTEX_ERROR An error occurred while trying to lock an internal mutex.
///
/// \see #DmLoader_addResolver
DMAPI DmResult DmLoader_getSegment(DmLoader* slf, char const* name, DmSegment** segment);

/// \}

/// \defgroup DmPerformanceGroup Performance
/// \addtogroup DmPerformanceGroup
/// \{

typedef enum DmRenderOptions {
	DmRender_SHORT = 1 << 0,
	DmRender_FLOAT = 1 << 1,
	DmRender_STEREO = 1 << 2,
} DmRenderOptions;

typedef enum DmTiming {
	DmTiming_INSTANT = 1,
	DmTiming_GRID = 2,
	DmTiming_BEAT = 3,
	DmTiming_MEASURE = 4,
} DmTiming;

typedef enum DmEmbellishmentType {
	DmEmbellishment_NONE = 0,
	DmEmbellishment_GROOVE = 1,
	DmEmbellishment_FILL = 2,
	DmEmbellishment_INTRO = 3,
	DmEmbellishment_BREAK = 4,
	DmEmbellishment_END = 5,
	DmEmbellishment_END_AND_INTRO = 6,
} DmEmbellishmentType;

/// \brief Create a new DirectMusic Performance object.
///
/// \param slf[out] A pointer to a variable in which to store the newly created performance.
/// \param rate The sample rate for the synthesizer. Provide 0 to use the default (44100 Hz).
///
/// \returns #DmResult_SUCCESS if the operation completed and an error code if it did not.
/// \retval #DmResult_INVALID_ARGUMENT \p slf was `NULL`.
/// \retval #DmResult_MEMORY_EXHAUSTED A dynamic memory allocation failed.
DMAPI DmResult DmPerformance_create(DmPerformance** slf, uint32_t rate);

/// \brief Add one to the reference count of a performance.
/// \param slf[in] The performance to retain.
/// \return The same performance as was given in \p slf or `NULL` if \p slf was `NULL`.
DMAPI DmPerformance* DmPerformance_retain(DmPerformance* slf);

/// \brief Subtract one from the reference count of a performance.
///
/// If a call to this function reduces the reference count to zero, it also de-allocates the performance and
/// releases any resources referenced by it.
///
/// \param slf[in] The performance to release.
DMAPI void DmPerformance_release(DmPerformance* slf);

/// \brief Schedule a new segment to be played by the given performance.
///
/// The segment is played at the next timing boundary provided with \p timing. This function simply stops the currently
/// playing segment and starts playing the next one. To play a transition between the two segment, use
/// #DmPerformance_playTransition.
///
/// \note The segment will always start playing strictly after the last call to #DmPerformance_renderPcm since that
///       function advances the internal clock. This means, if you have already rendered ten seconds worth of PCM
///       using #DmPerformance_renderPcm, the transition can only audibly be heard after these ten seconds of PCM
///       have been played.
///
/// \param slf[in] The performance to play the segment in.
/// \param sgt[in] The segment to play.
/// \param timing The timing bounding to start playing the segment at.
///
/// \return #DmResult_SUCCESS if the operation completed and an error code if it did not.
/// \retval #DmResult_INVALID_ARGUMENT \p slf or \p sgt was `NULL`.
/// \retval #DmResult_MEMORY_EXHAUSTED A dynamic memory allocation failed.
/// \retval #DmResult_MUTEX_ERROR An error occurred while trying to lock an internal mutex.
///
/// \see DmPerformance_playTransition
DMAPI DmResult DmPerformance_playSegment(DmPerformance* slf, DmSegment* sgt, DmTiming timing);

/// \brief Schedule a new segment to play by the given performance with a transition.
///
/// Schedules a new transitional segment to be played, which first plays a transitional pattern from the currently
/// playing segment's style and then starts playing the given segment. This can be used to smoothly transition from
/// one segment to another.
///
/// The transitional pattern is selected by its \p embellishment type provided when calling the function. Only
/// embellishments matching the current groove level are considered.
///
/// \note The #DmEmbellishment_END_AND_INTRO embellishment is currently not implemented.
///
/// \param slf[in] The performance to play the transition in.
/// \param sgt[in] The segment to transition to.
/// \param embellishment The embellishment type to use for the transition.
/// \param timing The timing bounding to start playing the transition at.
///
/// \return #DmResult_SUCCESS if the operation completed and an error code if it did not.
/// \retval #DmResult_INVALID_ARGUMENT \p slf or \p sgt was `NULL`.
/// \retval #DmResult_MEMORY_EXHAUSTED A dynamic memory allocation failed.
/// \retval #DmResult_MUTEX_ERROR An error occurred while trying to lock an internal mutex.
///
/// \see DmPerformance_playSegment
DMAPI DmResult DmPerformance_playTransition(DmPerformance* slf,
                                            DmSegment* sgt,
                                            DmEmbellishmentType embellishment,
                                            DmTiming timing);

/// \brief Render a given number of PCM samples from a performance.
///
/// Since the performance is played "on demand", calling this function will advance the internal clock and perform all
/// musical operation for the rendered timeframe. If no segment is currently playing, the output will be set to zero
/// samples.
///
/// Using the \p opts parameter, you can control what data is output. The #DmRender_SHORT and #DmRender_FLOAT bits
/// indicate the format of the PCM data to output (either as int16_t or 32-bit float). All data is output as
/// host-endian. Setting the #DmRender_STEREO bit renders interleaved stereo samples.
///
/// \warning When rendering stereo audio, you must provide an output array with an even number of elements!
///
/// \param slf[in] The performance to render from.
/// \param buf[out] A buffer to render PCM into.
/// \param num The number of elements available in \p buf. This will be equal to the number of samples when rendering
///            mono PCM or 2 time the number of samples when rendering stereo.
/// \param opts A bitfield with options for the renderer.
///
/// \return #DmResult_SUCCESS if the operation completed and an error code if it did not.
/// \retval #DmResult_INVALID_ARGUMENT \p slf or \p buf was `NULL`, \p opts included multiple format specifiers
///                                    or \p opts included #DmRender_STEREO and \p num was not even.
/// \retval #DmResult_MEMORY_EXHAUSTED A dynamic memory allocation failed.
/// \retval #DmResult_MUTEX_ERROR An error occurred while trying to lock an internal mutex.
DMAPI DmResult DmPerformance_renderPcm(DmPerformance* slf, void* buf, size_t num, DmRenderOptions opts);

/// \brief Set the playback volume of a performance
/// \note This only affects the output created when calling #DmPerformance_renderPcm.
/// \param slf[in] The performance to set the volume of.
/// \param vol The new volume to set (between 0 and 1).
DMAPI void DmPerformance_setVolume(DmPerformance* slf, float vol);

/// \}
