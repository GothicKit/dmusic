# DirectMusic — A re-implementation.

This project aims to re-implement Microsoft's long-deprecated DirectMusic API available in early Direct3D and DirectX
versions. It is currently under heavy development at this time and might be unstable for some use-cases.

## Example

Here's how you play back a segment. This example works on POSIX only since it uses `<sys/stat.h>` for the file resolver.
On Windows, you simply need to replace `dm_resolve_file` with a Windows-compatible implementation.

```c
#include <dmusic.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void* dm_resolve_file(void* ctx, char const* name, size_t* len);

int main(int argc, char** argv) {
	Dm_setLoggerDefault(DmLogLevel_INFO);

	// 1. Create a new DmLoader. The loader is responsible for loading and caching DirectMusic files using a
	//    user-defined callback function called a "resolver". You really only ever need one for your application.

	DmLoader* loader = NULL;
	DmResult rv = DmLoader_create(&loader, DmLoader_DEFAULT | DmLoader_DOWNLOAD);
	if (rv != DmResult_SUCCESS) {
		puts("Creating the loader failed\n");
		return rv;
	}

	// 2. Register a resolver with the loader. A resolver is simply a function which gets a context pointer,
	//    a filename and returns a memory buffer and its length as an output parameter. The context pointer
	//    is user-defined, here it's just a path string. You can return NULL from a resolver to indicate that
	//    the file was not found.

	rv = DmLoader_addResolver(loader, dm_resolve_file, "/path/to/your/music/folder");
	if (rv != DmResult_SUCCESS) {
		puts("Adding the resolver failed\n");
		return rv;
	}

	// 3. Use the loader to obtain a segment. The loader will call your resolvers in order to read in the
	//    file, and it will then perform some internal magic to load the segment. Since we set the
	//    DmLoader_DOWNLOAD option when constructing the loader, we don't need to call DmSegment_download
	//    afterward. Otherwise, you do have to call it.

	DmSegment* segment = NULL;
	rv = DmLoader_getSegment(loader, "YourSegment.sgt", &segment);
	if (rv != DmResult_SUCCESS) {
		puts("Getting the segment failed\n");
		return rv;
	}

	// 4. Create a new performance. The performance represents your main playback device. It handles all
	//    the DirectMusic magic needed to produce music from your segments. You typically only need one
	//    performance for your application. The second parameter here is the sample rate, defaulted to
	//    44100 Hz.

	DmPerformance* performance = NULL;
	rv = DmPerformance_create(&performance, 44100);
	if (rv != DmResult_SUCCESS) {
		puts("Creating the performance failed\n");
		return rv;
	}

	// 5. Instruct the performance to play a segment. This will set up the performance's internals so that
	//    the following call to DmPerformance_renderPcm will start producing music. The performance renders
	//    music on-demand, so as long as you don't call DmPerformance_renderPcm, you can consider playback to
	//    be paused. To stop playing music, you can pass NULL as the segment parameter.
	//
	//    The second parameter here is the timing. It tells the performance at which boundary to start playing
	//    the new segment as to not interrupt the flow of music. The options are "instant", which ignores all
	//    that and immediately plays the segment, "grid" which plays the segment at the next possible beat
	//    subdivision, "beat" which plays the segment at the next beat and "measure" which plays it at the next
	//    measure boundary.
	//
	//    The performance also supports transitions. To play those, use DmPerformance_playTransition and see
	//    its inline documentation for more information.

	rv = DmPerformance_playSegment(performance, segment, DmTiming_MEASURE);
	if (rv != DmResult_SUCCESS) {
		puts("Playing the segment failed\n");
		return rv;
	}

	size_t len = 1000000;
	float* pcm = malloc(sizeof *pcm * len);

	// 6. Finally, render some PCM! This will instruct the performance to start processing the underlying
	//    DirectMusic messages and render the resulting PCM to the output buffer. In this case it will
	//    render 1000000 stereo samples which is 500000 samples per channel.
	//
	//    This will advance the internal clock for as many ticks as required to render the requested number
	//    of samples. No more, no less.

	rv = DmPerformance_renderPcm(performance, pcm, len, DmRender_FLOAT | DmRender_STEREO);
	if (rv != DmResult_SUCCESS) {
		puts("Playing the PCM failed\n");
		return rv;
	}

	// 6.1. Write out the PCM data to some place where we can access it later. This could also just be some
	//      audio output device or another library.

	FILE* fp = fopen("output.pcm", "w");
	if (fp == NULL) {
		puts("Opening the output file failed\n");
		return -1;
	}

	(void) fwrite(pcm, sizeof *pcm, len, fp);
	(void) fclose(fp);

	// 7. Don't forget to clean up after yourself.
	free(pcm);

	DmSegment_release(segment);
	DmPerformance_release(performance);
	DmLoader_release(loader);
	return 0;
}

static void* dm_resolve_file(void* ctx, char const* name, size_t* len) {
	char const* root = ctx;

	// 1. Concat `root` and `name` to produce the final path. If a slash is missing
	//    at the end of `root`, add it.
	size_t root_len = strlen(root);
	size_t name_len = strlen(name);

	int miss_sep = root[root_len - 1] != '/';

	char* path = malloc(root_len + name_len + 1 + miss_sep);
	memcpy(path, root, root_len);
	memcpy(path + root_len + miss_sep, name, name_len);

	if (miss_sep) {
		path[root_len] = '/';
	}

	path[root_len + name_len + miss_sep] = '\0';

	// 2. Check if the file we want to open actually exists. If it doesn't, return NULL.
	struct stat st;
	if (stat(path, &st) != 0) {
		free(path);
		return NULL;
	}

	// 3. Read in data from the file.
	FILE* fp = fopen(path, "re");
	if (fp == NULL) {
		free(path);
		return NULL;
	}

	void* bytes = malloc((size_t) st.st_size);
	*len = fread(bytes, 1, (size_t) st.st_size, fp);

	(void) fclose(fp);
	free(path);

	return bytes;
}
```

More examples can be found in the `examples/` folder.

## Contact

If you have any questions, or you just want to say hi, you can reach me via e-mail ([`me@lmichaelis.de`](mailto:me@lmichaelis.de))
or on Discord either via DM but preferably in the Gothic VR and GMC Discords (`@lmichaelis`).
