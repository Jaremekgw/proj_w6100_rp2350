#pragma once

/* MS72SF1 datasheet v1.0.0, sections 7 and 8.2.
 * Integers and IEEE-754 float32 values are little-endian. LENGTH includes
 * the eight-byte header. No checksum is documented.
 */
#define MS72SF1_HEADER_SIZE       8u
#define MS72SF1_FIXED_FRAME_SIZE 32u
#define MS72SF1_TRACK_SIZE       32u
#define MS72SF1_MAX_TRACKS       10u
#define MS72SF1_MAX_FRAME_SIZE   (MS72SF1_FIXED_FRAME_SIZE + MS72SF1_MAX_TRACKS * MS72SF1_TRACK_SIZE)
#define MS72SF1_POINT_TLV         1u
#define MS72SF1_TRACK_TLV         2u
