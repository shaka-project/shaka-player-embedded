# :warning: DEPRECATED :warning:
Due to lack of internal funding and external maintainers, it is with great sadness that I announce the official shutdown of Shaka Player Embedded. The repo will remain available in archived form, and you may fork it and continue development under the terms of the Apache License if you wish.

I'll wait to archive the repo until the end of March, 2024, so that this announcement can be discussed on https://github.com/shaka-project/shaka-player-embedded/issues/248.  After that deadline, I will archive the repo, putting it in a readonly state that will prevent further updates to issues.


# Shaka Player Embedded

Shaka Player Embedded is a framework that runs [Shaka Player][] in native (C++)
apps.  This gives a cross-platform interface to Shaka Player allowing native
apps to use the player.  Your native apps can now be built with the same
features, behavior, and API as your Shaka-based web apps.

In addition to the C++ API, we provide a high-level wrapper for iOS called
`ShakaPlayer` and `ShakaPlayerView` which can be used from either Objective-C or
Swift.

Documentation: <https://shaka-project.github.io/shaka-player-embedded/>
Tutorials: <https://shaka-project.github.io/shaka-player-embedded/usergroup0.html>
Releases: <https://github.com/shaka-project/shaka-player-embedded/releases>

[Shaka Player]: https://www.github.com/shaka-project/shaka-player


## Platform support

We only support iOS at this time, but other platforms could be added.

Because many on our team use Linux, there is a Linux port included to make it
easier to work on non-iOS-specific features.  Linux is not, however, a
first-class target platform at this time.


## API/ABI compatibility

This project follows [semantic versioning][], meaning we maintain backwards
compatibility with all minor releases, including ABI.  This means you can drop
in a newer version of the compiled library and not have to recompile your main
app (you'll need to re-sign it due to Apple requirements).

Minor releases (e.g. v1.1) will add new features in a reverse-compatible way,
but major releases (e.g. v2.0) may break any compatibility.  We'll mark
deprecated features with compiler attributes to give you warnings about features
that will be removed later.  This may break API compatibility if you compile
with `-Werror`, but you can suppress the warnings.  Features will only be
removed in major releases.

[semantic versioning]: https://semver.org/


## Announcements & Issues

If you are interested in release and survey announcements, please join our
mailing list: https://groups.google.com/forum/#!forum/shaka-player-users

This is a very low-volume list that only admins of the project may post to.

For issues or to start a discussion, please use github issues:
https://github.com/shaka-project/shaka-player-embedded/issues


## Widevine support

Widevine support requires the Widevine CDM for iOS, which must be obtained
separately [from Widevine](http://www.widevine.com/contact), under license.
The Widevine CDM is not open-source.

Adding Widevine support requires compiling from source; you cannot use the
pre-built versions.  Follow the instructions in the Widevine CDM repo for how
to build it.

This requires the use of at least v15.2.3 of the Widevine CDM.


## Setting up for development

1. The source is managed by Git at
<https://www.github.com/shaka-project/shaka-player-embedded>.  You will need these
tools available on your development system:

* Git v1.7.5+
* Python v2.7
  * Requires installing package `enum34`
* autoconf 2.57+
  * m4
  * automake 1.7+
  * perl

This does not require a C++ compiler as it uses a pre-compiled version of
clang downloaded from Google Cloud Storage.  However, it does require a C++11
compatible C++ library (installed with most compilers, e.g. gcc).

2. Install Chromium depot tools, which contains ninja and other required tools.
See <https://www.chromium.org/developers/how-tos/install-depot-tools> for
details.

3. Get the source

```shell
git clone https://github.com/shaka-project/shaka-player-embedded.git
cd shaka-player-embedded
```

We use git submodules to manage third-party dependencies.  You can run
`git submodule update` (or pass `--recurse-submodules` to `git clone`) to
download all the submodules; or you can wait and have the `configure` script
download only those you need.

4. Configure and Build

You need to run `configure` to setup configuration settings.  You can pass
--help to see some of the options you can use.  We don't support in-tree builds;
you need to run `configure` in another directory.

Once `configure` is done, you can build using `build.py`.  There is a Makefile
that will run the script for you if you prefer.

```shell
mkdir foo
cd foo
../configure
make
```

5. Copy the shared library

Once it is built, you can use the resulting shared library.

On iOS, there will be a `ShakaPlayerEmbedded.framework` folder that you use;
there is also a `ShakaPlayerEmbedded.FFmpeg.framework` bundle that you need to
include in your app, but you shouldn't use it directly since we don't maintain
ABI for it.

On other platforms, there is a `libshaka-player-embedded.dylib` or a
`libshaka-player-embedded.so` file you can use.  Like iOS, there is a
`libshaka-player-embedded.ffmpeg.*` file you need to include too.

If you are using a custom `--eme-impl`, you need to copy the respective
shared libraries too, if needed.


## Running the checks

If you intend to send a pull-request, you need to ensure the code complies with
style guidelines.  This is done with the `shaka/tools/presubmit.py` script,
which can be run with `make check`.

To run all the checks, you also have to have `clang-tidy` installed.  It needs
to be installed on `PATH`, or you can pass `--clang-tidy` to the script to tell
it where to find the binary.
