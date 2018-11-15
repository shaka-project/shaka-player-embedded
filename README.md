# Shaka Player Embedded

Shaka Player Embedded is a framework that runs [Shaka Player][] in native (C++)
apps.  This gives a cross-platform interface to Shaka Player allowing native
apps to use the player.  Your native apps can now be built with the same
features, behavior, and API as your Shaka-based web apps.

In addition to the C++ API, we provide a high-level wrapper for iOS called
ShakaPlayerView which can be used from either Objective-C or Swift.

[Shaka Player]: https://www.github.com/google/shaka-player


## Platform support

We only support iOS at this time, but other platforms could be added.

Because many on our team use Linux, there is a Linux port included to make it
easier to work on non-iOS-specific features.  Linux is not, however, a
first-class target platform at this time.


## Release status

This project is currently in an open beta.  We would love to have your feedback
on what we've built, but we are not yet feature-complete for v1.0.  As such,
ABI compatibility is not yet guaranteed.


## Announcements & Issues

If you are interested in release and survey announcements, please join our
mailing list: https://groups.google.com/forum/#!forum/shaka-player-users

This is a very low-volume list that only admins of the project may post to.

For issues or to start a discussion, please use github issues:
https://github.com/google/shaka-player-embedded/issues


## Widevine support

Widevine support requires the Widevine CDM for iOS, which must be obtained
separately [from Widevine](http://www.widevine.com/contact), under license.
The Widevine CDM is not open-source.

We don't have an exact release date yet, but the Widevine CDM for iOS should be
available by the time we tag v1.0 of Shaka Player Embedded.


## Setting up for development

1. The source is managed by Git at
<https://www.github.com/google/shaka-player-embedded>.  You will need these
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
git clone https://github.com/google/shaka-player-embedded.git
cd shaka-player-embedded
```

We use git submodules to manage third-party dependencies.  You can run
`git submodule update` (or pass `--recurse-submodules` to `git clone`) to
download all the submodules; or you can wait and have the `configure` script
download only those you need.

4. Configure and Build

You need to run `configure` to setup configuration settings.  You can pass
--help to see some of the options you can use.  We don't support in-tree builds;
you can either run `configure` in another directory or use the --config-name
parameter.  If you use --config-name, it will create a directory in the source
root to hold the data.

Once `configure` is done, you can build using `build.py`.  There is a Makefile
that will run the script for you if you prefer.  If you used --config-name,
you'll need to use that option in `build.py` (the Makefile will do this for
you).

```shell
./configure --config-name Debug
make
```

Or

```shell
mkdir foo
cd foo
../configure
make
```


## Running the checks

If you intend to send a pull-request, you need to ensure the code complies with
style guidelines.  This is done with the `shaka/tools/presubmit.py` script,
which can be run with `make check`.

To run all the checks, you also have to have `clang-tidy` installed.  It needs
to be installed on `PATH`, or you can pass `--clang-tidy` to the script to tell
it where to find the binary.
