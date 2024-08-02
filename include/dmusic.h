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

#ifndef DM_STATIC
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
#else
	#define DMAPI DM_EXTERN
	#define DMINT DM_EXTERN
#endif

/// \endcond

/// \defgroup DmCommonGroup Common
/// \addtogroup DmCommonGroup
/// \{

/// \brief Possible operation result values.
typedef enum DmResult {
	/// \brief The operation completed successfully.
	DmResult_SUCCESS = 0,

	/// \brief An invalid argument was provided.
	DmResult_INVALID_ARGUMENT,

	/// \brief The operation could not be completed because the system is in an invalid state.
	DmResult_INVALID_STATE,

	/// \brief A memory allocation failed.
	DmResult_MEMORY_EXHAUSTED,

	/// \brief A resource was not found.
	DmResult_NOT_FOUND,

	/// \brief A resource file could not be parsed.
	DmResult_FILE_CORRUPT,

	/// \brief A mutex could not be locked.
	DmResult_MUTEX_ERROR,
} DmResult;

/// \brief Contains a 128-bit *GUID* (aka *UUID*) value.
///
/// GUIDs are used in *DirectMusic Segments*, *Styles*, *Bands* and *Downloadable Sound* files as a way to uniquely
/// identify distinct objects. For some of these objects, these GUIDs are exposed by their APIs. Many of these GUIDs
/// could be manually set by the composer although generally, they are generated randomly by *DirectMusic Producer*
/// upon object creation.
///
/// \attention Do not alter GUIDs retrieved using the library APIs directly. Doing this can lead to cache misses which
///            result in the underlying file being re-loaded the next time the object is used, causing higher memory
///            usage and a spike in processing times. It can also result in broken playback (e.g. missing instruments).
/// \see https://en.wikipedia.org/wiki/Universally_unique_identifier for more in-depth information about GUIDs.
typedef struct DmGuid {
	/// \brief The bytes representing the GUID's value.
	uint8_t data[16];
} DmGuid;

/// \brief Convert the given GUID to a string.
///
/// The output string is generated in 8-4-4-4-12 format and thus needs 36 chars (excl. nul-terminator) to be fully
/// converted. The number of chars actually converted can be obtained from the return value.
///
/// \param slf[in] The GUID to convert to a string.
/// \param out[out] The output buffer to write the chars to.
/// \param len The maximum number of bytes available in the \p out buffer.
/// \return The number of chars actually written to the \p out buffer.
DMAPI size_t DmGuid_toString(DmGuid const* slf, char* out, size_t len);

/// \brief A `malloc`-like memory allocation function.
///
/// Allocates \p len bytes of contiguous memory and returns a pointer to the first byte allocated. May return NULL
/// if memory allocation fails for any reason. In this case, function which relies on the allocation will fail with
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
/// \param ptr[in] A pointer to free, previously returned by the corresponding #DmMemoryAlloc function or `NULL`.
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

/// \brief A `rand_r`-like random number generation function.
///
/// Generates a random number between 0 and UINT32_MAX. Functions implementing this interface must be
/// thread-safe. The function is not required to produce different numbers upon invocation and it does
/// not need to be cryptographically secure. Calling a function implementing this interface should be
/// inexpensive.
///
/// \param ctx[in] An arbitrary pointer provided when calling #Dm_setRandomNumberGenerator.
/// \return A random number between 0 and UINT32_MAX.
/// \see Dm_setRandomNumberGenerator
typedef uint32_t DmRng(void* ctx);

/// \brief Set the random number generator to use internally.
///
/// The given random number generator is sampled every time the library requires a random number. This includes
/// but is not limited to:
///
///  * Selecting the next pattern to be played,
///  * selecting the next note/curve variation and
///  * applying random note offsets
///
/// \param rng[in] A pointer to a function to use as a random number generator or `NULL`
///                to reset to the default random number generator.
/// \param ctx[in] An arbitrary pointer passed to \p rng on every invocation.
///
/// \see DmRng Requirements for the random number generator
DMAPI void Dm_setRandomNumberGenerator(DmRng* rng, void* ctx);

/// \brief The set of message levels supported by the logging facilities.
typedef enum DmLogLevel {
	/// \brief The log message indicates a fatal error.
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

/// \brief Set the log level of the library.
/// \param lvl The log level to set.
DMAPI void Dm_setLoggerLevel(DmLogLevel lvl);

/// \}

/// \defgroup DmSegmentGroup Segment
/// \brief Structures and functions related to DirectMusic Segments.
///
/// ## Overview
///
/// Segments are the heart of all DirectMusic scores. They contain information about how to arrange the musical piece
/// including, among other things, the tempo, which bands to use and how to integrate with the style. Exported segments
/// usually have the file extension `.sgt` while [DirectMusic Producer][dm-producer] uses `.sgp` instead.
///
/// This library only supports what is known as [Style-based Segments][style-based-segments] at the moment. This means,
/// that only _style_, _chord_, _command_, _band_ and _tempo_ tracks are supported. All other tracks are ignored.
///
/// ### What exactly is a track?
///
/// DirectMusic is a dynamic system which selects and plays back music in real time. To do this, the composer specifies
/// at least one set of distinct musical patterns known as a [Style][dm-style] and at least one set of instruments
/// called a [Band][dm-band] as well as a set of tempos, commands and chords to be played back in real time. These
/// components are all separated into so-called [Tracks][dm-track] in which they are saved by timestamp. During
/// playback, DirectMusic will process each track whenever its timestamp is reached and perform the action associated
/// with it.
///
/// For example, a composer could create a segment which contains a tempo track with one entry per measure (the musical
/// unit). During playback, the playback engine will then change the tempo at each measure to the value set by the
/// composer. This applies to all five supported tracks, so a composer could, for example, also include band changes
/// mid-playback.
///
/// ### Bands and band tracks
///
/// DirectMusic Bands are collections of instruments each assigned to a [Performance Channel][dm-performance-channel].
/// Instruments are backed by [Downloadable Sound][dls] (_"DLS"_) files which contain wave-tables for one or more
/// instruments, very similar to [SoundFont][sf2] files. At runtime, bands are loaded an unloaded according to the
/// band track. It specifies which band should be active which timestamps.
///
/// ### Styles and style tracks
///
/// DirectMusic Styles contain the actual notes to be played by the instruments specified in the band. Each instrument
/// is assigned one or more _parts_ each containing one set of notes and possibly random variations. Parts are then
/// combined into _patterns_, which contain multiple parts to be played at the same time. At runtime, the playback
/// engine will select a pattern according to the command track of the segment which it will then start playing. To do
/// this, the playback engine takes the notes of each _part_ referenced by the _pattern_ and assigns the notes to be
/// played to each referenced instrument while taking into account the currently playing chord selected by the chord
/// track and choosing a variation either, randomly, sequentially or otherwise as specified by the composer.
///
/// ### Why do I need to download a segment?
///
/// Styles the Downloadable Sounds referenced by the segment are stored in separate files. For the playback engine to
/// have access to the data contained within, it needs to open and load the contents of these files. To avoid doing
/// this during playback, since opening and loading files can take a few milliseconds (enough to notice playback
/// stuttering), these files must be loaded before submitting the segment for playback. This should be done in a
/// separate thread than the actual music rendering (see #DmPerformance_renderPcm).
///
/// ### I've loaded a segment, how do I play the damn thing?
///
/// Playback is done using a #DmPerformance object. See the documentation for \ref DmPerformanceGroup and
/// #DmPerformance_renderPcm for more details.
///
/// [dm-producer]: https://www.vgmpf.com/Wiki/index.php?title=DirectMusic_Producer
/// [dm-style]: https://documentation.help/DirectMusic/styles.htm
/// [dm-band]: https://documentation.help/DirectMusic/usingbands.htm
/// [dm-track]: https://documentation.help/DirectMusic/directmusictracks.htm
/// [dm-performance-channel]: https://documentation.help/DirectMusic/performancechannels.htm
/// [style-based-segments]: https://documentation.help/DirectMusic/stylebasedsegments.htm

/// \ingroup DmSegmentGroup
/// \brief Represents a DirectMusic Segment.
typedef struct DmSegment DmSegment;

/// \defgroup DmLoaderGroup Loader
/// \brief TODO

/// \ingroup DmLoaderGroup
/// \brief Represents a DirectMusic Loader.
typedef struct DmLoader DmLoader;

/// \defgroup DmPerformanceGroup Performance
/// \brief TODO

/// \ingroup DmPerformanceGroup
/// \brief Represents a DirectMusic Performance.
typedef struct DmPerformance DmPerformance;

/// \addtogroup DmSegmentGroup
/// \{

/// \brief Add one to the reference count of a segment.
/// \param slf[in] The segment to retain.
/// \return The same segment as was given in \p slf or `NULL` if \p slf is `NULL`.
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

/// \brief Get the GUID of the given segment.
/// \warning The returned pointer is only valid for as long as a strong reference to the segment is held.
/// \param slf[in] The segment to get the GUID of.
/// \return A read-only pointer to the segment's GUID or `NULL` if \p slf is `NULL`.
DMAPI DmGuid const* DmSegment_getGuid(DmSegment const* slf);

/// \brief Get the name of the given segment.
/// \note The returned pointer is only valid for as long as a strong reference to the segment is held.
/// \param slf[in] The segment to get the name of.
/// \return A read-only pointer to the segment's name in UTF-8 or `NULL` if \p slf is `NULL`.
DMAPI char const* DmSegment_getName(DmSegment const* slf);

/// \brief Get the length of the given segment in seconds.
///
/// The number of PCM samples required to render `n` seconds of the segment can be calculated like this:
///     \f$n_{samples} = n \cdot x_{rate} \cdot n_{channels}\f$
/// where \f$x_{rate}\f$ is the sample rate to use (usually 44100 Hz) and \f$n_{channels}\f$ is the number
/// of PCM output channels (1 for mono and 2 for stereo PCM).
///
/// \param slf[in] The segment to get the length of.
/// \return The number of seconds one repeat of the segment takes.
DMAPI double DmSegment_getLength(DmSegment const* slf);

/// \brief Get the number of times the segment repeats.
/// \param slf[in] The segment to get the number of repeats of.
/// \return The number of times the segment repeats or `0` if \p slf is `NULL`.
DMAPI uint32_t DmSegment_getRepeats(DmSegment const* slf);

/// \}

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
/// Gets a segment from the loader's cache or loads the segment using the resolvers added to the loader. If the
/// requested segment is found in neither the loader, nor by any resolver, an error is issued.
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

/// \addtogroup DmPerformanceGroup
/// \{

typedef enum DmRenderOptions {
	/// \brief Render format flag to request rendering of `int16_t` samples
	DmRender_SHORT = 1 << 0,

	/// \brief Render format flag to request rendering of `float` samples
	DmRender_FLOAT = 1 << 1,

	/// \brief Render flags to request stereo PCM rendering.
	DmRender_STEREO = 1 << 2,
} DmRenderOptions;

typedef enum DmTiming {
	/// \brief Timing flag indicating start at the next possible tick.
	DmTiming_INSTANT = 1,

	/// \brief Timing flag indicating start at the next possible grid boundary.
	DmTiming_GRID = 2,

	/// \brief Timing flag indicating start at the next possible beat boundary.
	DmTiming_BEAT = 3,

	/// \brief Timing flag indicating start at the next possible measure boundary.
	DmTiming_MEASURE = 4,
} DmTiming;

/// \brief Embellishment types for choosing transition patterns.
typedef enum DmEmbellishmentType {
	/// \brief Don't choose a pattern.
	DmEmbellishment_NONE = 0,

	/// \brief Only choose patterns with the default 'groove' embellishment.
	DmEmbellishment_GROOVE = 1,

	/// \brief Only choose patterns with the 'fill' embellishment.
	DmEmbellishment_FILL = 2,

	/// \brief Only choose patterns with the 'intro' embellishment.
	DmEmbellishment_INTRO = 3,

	/// \brief Only choose patterns with the 'break' embellishment.
	DmEmbellishment_BREAK = 4,

	/// \brief Only choose patterns with the 'end' embellishment.
	DmEmbellishment_END = 5,

	/// \brief Choose two patterns, one with the 'end' embellishment from the playing segment and one with
	///        the 'intro' embellishment from the new segment and play them back-to-back.
	/// \todo This is not yet implemented.
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
/// playing segment and starts playing the next one. To play a transition between the two segments, use
/// #DmPerformance_playTransition.
///
/// \note The segment will always start playing strictly after the last call to #DmPerformance_renderPcm since that
///       function advances the internal clock. This means, if you have already rendered ten seconds worth of PCM
///       using #DmPerformance_renderPcm, the transition can only audibly be heard after these ten seconds of PCM
///       have been played.
///
/// \param slf[in] The performance to play the segment in.
/// \param sgt[in] The segment to play or `NULL` to simply stop the playing segment.
/// \param timing The timing bounding to start playing the segment at.
///
/// \return #DmResult_SUCCESS if the operation completed and an error code if it did not.
/// \retval #DmResult_INVALID_ARGUMENT \p slf was `NULL`.
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
/// \param sgt[in] The segment to transition to or `NULL` to transition to silence.
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
