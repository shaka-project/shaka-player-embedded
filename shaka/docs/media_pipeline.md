# Media Pipeline Design

This document describes how the core media pipeline works and how an app can
replace pieces of it to implement them differently.


## Core types

First is the frame types; these are used to store encoded
([EncodedFrame][]) or decoded ([DecodedFrame][]) frames.  Each object
represents a single frame and should maintain the lifetime of any pointers
returned; the data pointers must be static and are valid so long as the frame is
alive.  To allow for shared lifetime management, all frames are stored in a
`std::shared_ptr<T>`.  Apps should subclass these to store their own data and
ensure the frame data remains alive for the life of the object.

The [EncodedFrame][] objects are created by the [Demuxer][]; the
[DecodedFrame][] objects are created by the [Decoder][] (if using
[DefaultMediaPlayer][]).  The library-provided types don't care about the
specific implementation of the [EncodedFrame][] and [DecodedFrame][] types, so
apps are free to create subclasses and still use other library-provided
defaults.

Each frame has a [StreamInfo][] that is used to tell what stream it comes from.
Frames coming from the same stream will have pointers to the same [StreamInfo][]
object.  The [StreamInfo][] instance can be used to initialize decoders or
renderers.

Buffers of frames are stored in the [Stream][] type.  There is
[ElementaryStream][] and [DecodedStream][] for the [EncodedFrame][] and
[DecodedFrame][] types respectively.  Alternatively apps can use
`Stream<T, OrderByDts>` to store a collection of their own types.  A Stream
stores the frames in time order (PTS or DTS based on the `OrderByDts`
parameter).  Streams only store one kind of frame (i.e. video-only or
audio-only).


## MediaPlayer

The [MediaPlayer][] is the top-level handler for all media interactions.  It is
responsible for loading and playing content.  The library provides the
[DefaultMediaPlayer][] which handles timeline tracking but provides other
plugins allowing apps to replace the [Decoder][] or [Renderer][].  The
[MediaPlayer][] operates in two modes, MSE and src= mode.

In MSE mode, data is given to a [Demuxer][] and placed into an
[ElementaryStream][].  These streams are given to the [MediaPlayer][].  Then the
[MediaPlayer][] will handle reading, decoding, and rendering those frames.

In src= mode, the [MediaPlayer][] is given a URL to read from.  It is expected
to read that file to get the data.  Then it will play it similar to MSE mode.

When apps create a [Player](@ref shaka::Player) instance, they will need to pass
in the [MediaPlayer][] instance they want to play with.  This will act similar
to the `<video>` element given in JavaScript.


## Demuxer

The [Demuxer][] is used when playing MSE content.  This type is responsible for
reading the bytes from the stream and producing [EncodedFrame][] objects.  This
type should do so synchronously and will be called on a background thread.  This
is registered by creating an implementation of [DemuxerFactory][] and giving it
to [DemuxerFactory::SetFactory](@ref shaka::media::DemuxerFactory::SetFactory).
Passing `nullptr` to this method will cause the built-in [Demuxer][] to be used.

One [Demuxer][] instance is created per input stream by the core library.  For
example, one [Demuxer][] instance will handle video and the other will handle
audio.  Changing the [DemuxerFactory][] will not change any existing [Demuxer][]
instances that are already being used.


## Decoder

The [Decoder][] is only used if using the [DefaultMediaPlayer][].  If providing
your own [MediaPlayer][] implementation, you don't need to use/provide a
[Decoder][].  The [Decoder][] is used to read [EncodedFrame][] objects and
decode them into [DecodedFrame][] objects.  A [Decoder][] is only used to decode
one kind of media, but may be required to decode different input streams.  It
should automatically check the [StreamInfo][] and re-initialize as required.

Custom [Decoder][] implementations are given to
[DefaultMediaPlayer::SetDecoders](@ref shaka::media::DefaultMediaPlayer::SetDecoders).
Passing `nullptr` to either will cause the built-in decoders to be used.


## Renderer

The [Renderer][] is only used if using the [DefaultMediaPlayer][].  If providing
your own [MediaPlayer][] implementation, you don't need to use/provide a
[Renderer][].  They are also only used for MSE playback.  The [Renderer][] is
used to read [DecodedFrame][] objects and either show them on the screen or play
on an audio device.  This is expected to, on a background thread, pull frames
and update the data as needed.  This can be done as part of audio callbacks, or
with an explicit background thread.  The [Renderer][] will be given the
[MediaPlayer][] instance so it can read the current time, playback rate, etc
that are needed to show the content.

On Desktop, we provide an
[SdlAudioRenderer][], [SdlManualVideoRenderer][], and [SdlThreadVideoRenderer][]
types for use with SDL.  On iOS, we don't expose the iOS renderers; we expect
iOS users to use the [ShakaPlayerView](@ref ShakaPlayerView).

[Renderer][] instances are given to the [DefaultMediaPlayer][] constructor.


## Encryption support

Encryption support comes through the [Eme Implementation][] and
[ImplementationFactory](@ref shaka::eme::ImplementationFactory) types.  Once an
EME instance is created, it is given to the [MediaPlayer][].  If using the
[DefaultMediaPlayer][], it will be given to the [Decoder][] and the [Decoder][]
will decrypt and decode the frame as needed.  In this case, it is assumed the
frames are decoded into the clear.  This allows supporting secure
decrypt-decode, but doesn't allow for secure rendering.

Apps can use secure rendering by providing their own [MediaPlayer][] instance.
Since this case doesn't allow clear frames in memory, the [DefaultMediaPlayer][]
cannot handle it.  You'll need to handle all the media playback.


## Removing defaults

When running `./configure`, you can disable default media components you don't
need when providing your own replacements.  When these are used, the app will
need to provide their own overrides or playback won't work.

- `--no-media-player`: Removes the [DefaultMediaPlayer][].  This also implies
                       `--no-decoder`, `--no-sdl-audio`, and `--no-sdl-video`.
- `--no-demuxer`: Removes the default [Demuxer][] implementation.
- `--no-decoder`: Removes the default [Decoder][] implementation.
- `--no-sdl-audio`: Removes the [SdlAudioRenderer][].
- `--no-sdl-video`: Removes the [SdlManualVideoRenderer][] and
                    [SdlThreadVideoRenderer][].  This is the default on iOS.


[DecodedFrame]: @ref shaka::media::DecodedFrame
[DecodedStream]: @ref shaka::media::DecodedStream
[Decoder]: @ref shaka::media::Decoder
[DefaultMediaPlayer]: @ref shaka::media::DefaultMediaPlayer
[Demuxer]: @ref shaka::media::Demuxer
[DemuxerFactory]: @ref shaka::media::DemuxerFactory
[ElementaryStream]: @ref shaka::media::ElementaryStream
[Eme Implementation]: @ref shaka::eme::Implementation
[EncodedFrame]: @ref shaka::media::EncodedFrame
[MediaPlayer]: @ref shaka::media::MediaPlayer
[Player]: @ref shaka::media::Player
[Renderer]: @ref shaka::media::Renderer
[SdlAudioRenderer]: @ref shaka::media::SdlAudioRenderer
[SdlManualVideoRenderer]: @ref shaka::media::SdlManualVideoRenderer
[SdlThreadVideoRenderer]: @ref shaka::media::SdlThreadVideoRenderer
[Stream]: @ref shaka::media::Stream
[StreamInfo]: @ref shaka::media::StreamInfo
