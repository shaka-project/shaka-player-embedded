# FFmpeg Encryption

This document describes how the encryption info works in FFmpeg.  This is for
future Shaka Embedded developers who want to add support for different kinds
of encryption info to FFmpeg in the future. See
[libavutil/encryption_info.h][1].

[1]: https://github.com/FFmpeg/FFmpeg/blob/master/libavutil/encryption_info.h

There are two kinds of encryption info: (1) frame-specific encryption info
stored as `AVEncryptionInfo`, and (2) encryption initialization info (e.g.
`pssh` data) stored as `AVEncryptionInitInfo` on the `AVStream`.

Also note, in FFmpeg, the encoded (demuxed) frames are called packets
(`AVPacket`), while the decoded frames are called frames (`AVFrame`).  This will
still use "frame" to refer to an `AVPacket`.


## Side data

The encryption info is stored as side data on either an `AVPacket` or an
`AVStream`.  The side-data itself is binary data.  `encryption_info.h` defines
types and methods that can be used independent of container formats.  You need
to use those methods to convert between the binary format and the generic
object.

If you add a side-data to an object that already has a side-data of that type,
the old value is replaced.  So if there are multiple values (e.g. for multiple
`pssh` support), you'll need to get the old value, parse it, and combine it with
the new info you are adding.


## Frame-specific encryption info

First, you'll need to update the demuxer to parse the encryption info.  This
will likely involve editing the internal state to include this info.  It would
be convenient if you used an `AVEncryptionInfo` object now, but you don't have
to.

Next, you need to find where the demuxer creates the frames (`AVPacket`).  This
is usually named `*_read_packet`.  In this method, before the frame is returned
to the caller, you need to add the side-data to it.  If you haven't already,
create an `AVEncryptionInfo` object and fill it with the required info.  Then
do something like:

```c
// Convert the object to a serialized binary format.
size_t side_data_size;
uint8_t *side_data =
    av_encryption_info_add_side_data(encryption_info, &side_data_size);
if (!side_data)
  return AVERROR(ENOMEM);

// Add the data to the packet; this takes ownership of |side_data|.
int ret = av_packet_add_side_data(
    &pkt, AV_PKT_DATA_ENCRYPTION_INFO, side_data, side_data_size);
if (ret < 0) {
  av_free(side_data);
  return ret;
}
// Remember to free |encryption_info|.
```


## Encryption initialization info

Like before, you'll need to update the demuxer to parse whatever initialization
info is required.  It should be put into a new `AVEncryptionInitInfo` struct.
Some demuxers support multiple initialization infos, so before setting the
stream's side-data, you'll need to determine if there is any existing side-data.

```c
// Find any existing side-data.
size_t old_side_data_size;
uint8_t *old_side_data = av_stream_get_side_data(
    stream, AV_PKT_DATA_ENCRYPTION_INIT_INFO, &old_side_data_size);
if (old_side_data) {
  // Convert the side-data to a struct and add it to the liked list.
  AVEncryptionInitInfo *old_init_info = av_encryption_init_info_get_side_data(
      old_side_data, old_side_data_size);
  if (old_init_info) {
    init_info->next = old_init_info;
  } else {
    return AVERROR(ENOMEM);
  }
}

// Convert the object to a serialized binary format.
size_t side_data_size;
uint8_t *side_data = av_encryption_init_info_add_side_data(
    init_info, &side_data_size);
if (!side_data)
  return AVERROR(ENOMEM);

// Add the data to the stream; this takes ownership of |side_data|.
int ret = av_stream_add_side_data(
    stream, AV_PKT_DATA_ENCRYPTION_INIT_INFO, side_data, side_data_size);
if (ret < 0) {
  av_free(side_data);
  return ret;
}
// Remember to free |init_info|.  Note that av_encryption_init_info_free will
// free child members.
```
