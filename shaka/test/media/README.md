# Media Test File

These are test files used to test demuxing and decoding.  These are taken from
the first 10 seconds of Sintel.

https://durian.blender.org/

All encrypted assets are encrypted using [Shaka Packager][] (possibly modified),
using the following key:

Key ID: abba271e8bcf552bbd2e86a434a9a5d9
Key: 69eaa802a6763af979e8d1940fb88392

[Shaka Packager]: https://github.com/google/shaka-packager

- `clear_high.mp4`: 426x182 video-only, fragmented, single-file asset.
- `clear_low.mp4`: 256x110 video-only, NOT FRAGMENTED, used as the source asset
  of other assets.
- `clear_low.webm`: Same as `clear_low.mp4`, but transcoded to VP9.  The frames
  should have the same decoded hash despite being a different codec.
- `clear_low_frag_init.mp4` and `clear_low_frag_seg1.mp4`: Fragmented,
  multi-file versions of `clear_low.mp4`.
- `encrypted_low.mp4`: `clear_low.mp4`, encrypted with the `cenc` scheme, using
  subsample encryption (with only 1 subsample).  Created with an unmodified
  packager.
- `encrypted_low.webm`: Same as `encrypted_low.mp4` except as VP9 in WebM.
- `encrypted_low.cenc.mp4`: `clear_low.mp4`, encrypted with the `cenc` scheme,
  see below.
- `encrypted_low_cens.mp4`: `clear_low.mp4`, encrypted with the `cens` scheme,
  see below.
- `encrypted_low.cbc1.mp4`: `clear_low.mp4`, encrypted with the `cbc1` scheme,
  see below.
- `encrypted_low_cbcs.mp4`: `clear_low.mp4`, encrypted with the `cbcs` scheme,
  see below.
- `hash_file.txt`: A text file containing the hashes of `clear_low.mp4`.  Since
  the encrypted files contain the same content, the decrypted frames will have
  the same hashes.  Each line represents the hexadecimal SHA256 of the decoded
  frame in ARGB pixel format.


### Special encrypted content

The four `encrypted_low_*.mp4` assets were encrypted with a modified Shaka
Packager for the purposes of testing certain use cases for the encryption.
These files themselves may not be spec compliant (and are against
recommendations), but they test use cases that are valid in other contexts.

These assets are all encrypted with subsample encryption that has been modified
to have multiple subsamples per sample.  Normally, there are rules for when
the subsamples need to appear; but these have subsamples such that their
positioning tests edge cases.  They each have the following cases (except when
not applicable):
- A single subsample.
- Two subsamples where the first is exactly an AES block size.
- Two subsamples where the first is exactly two AES block sizes.
- Two subsamples where the first is the pattern length.

The assets that have patterns use the pattern (2, 8) to test the cases where a
partial encrypted pattern is seen.
