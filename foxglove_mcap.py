#
# Replays a Foxglove MCAP file over a local Foxglove WebSocket server.
#
# Usage:
#  $ python3 foxglove_mcap.py rd03d_logs.mcap
#
# On Foxglove connection WebSocket: ws://localhost:8765

import argparse
import shutil
import struct
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

import foxglove

MCAP_MAGIC = b"\x89MCAP0\r\n"

OP_SCHEMA = 0x03
OP_CHANNEL = 0x04
OP_MESSAGE = 0x05
OP_CHUNK = 0x06


@dataclass
class SchemaRecord:
    schema_id: int
    name: str
    encoding: str
    data: bytes


@dataclass
class ChannelRecord:
    channel_id: int
    schema_id: int
    topic: str
    message_encoding: str
    metadata: dict[str, str]


@dataclass
class MessageRecord:
    channel_id: int
    sequence: int
    log_time: int
    publish_time: int
    data: bytes


class McapFormatError(RuntimeError):
    pass


class McapReader:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.data = path.read_bytes()
        self.schemas: dict[int, SchemaRecord] = {}
        self.channels: dict[int, ChannelRecord] = {}
        self._scan_summary_records()

    def _scan_summary_records(self) -> None:
        for opcode, content in self._iter_top_level_records():
            if opcode == OP_SCHEMA:
                schema = self._parse_schema(content)
                self.schemas[schema.schema_id] = schema
            elif opcode == OP_CHANNEL:
                channel = self._parse_channel(content)
                self.channels[channel.channel_id] = channel

    def _iter_top_level_records(self) -> Iterator[tuple[int, bytes]]:
        if not self.data.startswith(MCAP_MAGIC) or not self.data.endswith(MCAP_MAGIC):
            raise McapFormatError(f"{self.path} is not an MCAP file")

        yield from self._iter_records(self.data, len(MCAP_MAGIC), len(self.data) - len(MCAP_MAGIC))

    def _iter_chunk_records(self, content: bytes) -> Iterator[tuple[int, bytes]]:
        offset = 0
        offset += 8  # message_start_time
        offset += 8  # message_end_time
        uncompressed_size = self._read_u64(content, offset)
        offset += 8
        offset += 4  # uncompressed_crc
        compression, offset = self._read_string(content, offset)
        compressed_records, offset = self._read_prefixed_bytes64(content, offset)
        if offset != len(content):
            raise McapFormatError("chunk record has trailing bytes")

        records = self._decompress(compression, compressed_records)
        if len(records) != uncompressed_size:
            raise McapFormatError(
                f"chunk decompressed to {len(records)} bytes, expected {uncompressed_size}"
            )
        yield from self._iter_records(records, 0, len(records))

    def _decompress(self, compression: str, data: bytes) -> bytes:
        if compression == "":
            return data
        if compression == "zstd":
            try:
                import zstandard  # type: ignore

                return zstandard.ZstdDecompressor().decompress(data)
            except ModuleNotFoundError:
                zstd_path = shutil.which("zstd")
                if zstd_path is None:
                    raise McapFormatError(
                        "MCAP uses zstd compression, but neither the Python "
                        "'zstandard' package nor the 'zstd' command is available"
                    ) from None
                return subprocess.check_output([zstd_path, "-d", "--stdout"], input=data)
        raise McapFormatError(f"unsupported MCAP chunk compression: {compression!r}")

    def iter_messages(self) -> Iterator[MessageRecord]:
        for opcode, content in self._iter_top_level_records():
            if opcode == OP_CHUNK:
                records = self._iter_chunk_records(content)
            else:
                records = iter([(opcode, content)])

            for inner_opcode, inner_content in records:
                if inner_opcode == OP_SCHEMA:
                    schema = self._parse_schema(inner_content)
                    self.schemas[schema.schema_id] = schema
                elif inner_opcode == OP_CHANNEL:
                    channel = self._parse_channel(inner_content)
                    self.channels[channel.channel_id] = channel
                elif inner_opcode == OP_MESSAGE:
                    yield self._parse_message(inner_content)

    def _iter_records(self, data: bytes, offset: int, end: int) -> Iterator[tuple[int, bytes]]:
        while offset < end:
            if offset + 9 > end:
                raise McapFormatError("truncated MCAP record header")

            opcode = data[offset]
            length = self._read_u64(data, offset + 1)
            offset += 9

            record_end = offset + length
            if record_end > end:
                raise McapFormatError("truncated MCAP record content")

            yield opcode, data[offset:record_end]
            offset = record_end

    def _parse_schema(self, content: bytes) -> SchemaRecord:
        offset = 0
        schema_id = self._read_u16(content, offset)
        offset += 2
        name, offset = self._read_string(content, offset)
        encoding, offset = self._read_string(content, offset)
        data, offset = self._read_prefixed_bytes32(content, offset)
        if offset != len(content):
            raise McapFormatError("schema record has trailing bytes")
        return SchemaRecord(schema_id, name, encoding, data)

    def _parse_channel(self, content: bytes) -> ChannelRecord:
        offset = 0
        channel_id = self._read_u16(content, offset)
        offset += 2
        schema_id = self._read_u16(content, offset)
        offset += 2
        topic, offset = self._read_string(content, offset)
        message_encoding, offset = self._read_string(content, offset)
        metadata, offset = self._read_metadata(content, offset)
        if offset != len(content):
            raise McapFormatError("channel record has trailing bytes")
        return ChannelRecord(channel_id, schema_id, topic, message_encoding, metadata)

    def _parse_message(self, content: bytes) -> MessageRecord:
        if len(content) < 22:
            raise McapFormatError("message record is too short")
        channel_id = self._read_u16(content, 0)
        sequence = self._read_u32(content, 2)
        log_time = self._read_u64(content, 6)
        publish_time = self._read_u64(content, 14)
        return MessageRecord(channel_id, sequence, log_time, publish_time, content[22:])

    def _read_metadata(self, data: bytes, offset: int) -> tuple[dict[str, str], int]:
        byte_count = self._read_u32(data, offset)
        offset += 4
        end = offset + byte_count
        values: dict[str, str] = {}
        while offset < end:
            key, offset = self._read_string(data, offset)
            value, offset = self._read_string(data, offset)
            values[key] = value
        if offset != end:
            raise McapFormatError("metadata map length mismatch")
        return values, offset

    def _read_string(self, data: bytes, offset: int) -> tuple[str, int]:
        raw, offset = self._read_prefixed_bytes32(data, offset)
        return raw.decode("utf-8"), offset

    def _read_prefixed_bytes32(self, data: bytes, offset: int) -> tuple[bytes, int]:
        length = self._read_u32(data, offset)
        offset += 4
        end = offset + length
        if end > len(data):
            raise McapFormatError("truncated length-prefixed bytes")
        return data[offset:end], end

    def _read_prefixed_bytes64(self, data: bytes, offset: int) -> tuple[bytes, int]:
        length = self._read_u64(data, offset)
        offset += 8
        end = offset + length
        if end > len(data):
            raise McapFormatError("truncated length-prefixed bytes")
        return data[offset:end], end

    def _read_u16(self, data: bytes, offset: int) -> int:
        return struct.unpack_from("<H", data, offset)[0]

    def _read_u32(self, data: bytes, offset: int) -> int:
        return struct.unpack_from("<I", data, offset)[0]

    def _read_u64(self, data: bytes, offset: int) -> int:
        return struct.unpack_from("<Q", data, offset)[0]


def create_channel(channel: ChannelRecord, schemas: dict[int, SchemaRecord]) -> foxglove.Channel:
    schema = schemas.get(channel.schema_id)
    foxglove_schema = None
    if schema is not None:
        foxglove_schema = foxglove.Schema(
            name=schema.name,
            encoding=schema.encoding,
            data=schema.data,
        )

    return foxglove.Channel(
        channel.topic,
        schema=foxglove_schema,
        message_encoding=channel.message_encoding,
        metadata=channel.metadata,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Replay an MCAP file to Foxglove over WebSocket")
    parser.add_argument("mcap_file", type=Path, help="MCAP file to replay")
    parser.add_argument("--host", default="127.0.0.1", help="Foxglove WebSocket bind host")
    parser.add_argument("--port", type=int, default=8765, help="Foxglove WebSocket port")
    parser.add_argument("--rate", type=float, default=1.0, help="playback speed multiplier")
    parser.add_argument("--start-delay", type=float, default=2.0, help="seconds to wait before replay starts")
    parser.add_argument("--no-delay", action="store_true", help="send all messages as fast as possible")
    parser.add_argument("--loop", action="store_true", help="replay the MCAP repeatedly")
    args = parser.parse_args()

    if args.rate <= 0:
        parser.error("--rate must be greater than 0")
    if not args.mcap_file.exists():
        parser.error(f"{args.mcap_file} does not exist")

    reader = McapReader(args.mcap_file)
    channels: dict[int, foxglove.Channel] = {}
    for channel_id, channel in sorted(reader.channels.items()):
        channels[channel_id] = create_channel(channel, reader.schemas)

    foxglove.start_server(
        name="MCAP replay",
        host=args.host,
        port=args.port,
    )

    print(f"Foxglove server started: ws://{args.host}:{args.port}")
    print(f"Replaying {args.mcap_file}")
    if reader.channels:
        print("Topics:")
        for channel in sorted(reader.channels.values(), key=lambda item: item.channel_id):
            print(f"  {channel.topic} ({channel.message_encoding})")
    if args.start_delay > 0:
        print(f"Starting in {args.start_delay:g}s...")
        time.sleep(args.start_delay)
    print("Press Ctrl-C to exit")

    try:
        while True:
            count = replay_once(reader, channels, args.no_delay, args.rate)
            print(f"Replay complete: {count} messages sent")
            if not args.loop:
                break
    except KeyboardInterrupt:
        print("\nCtrl-C received, exiting...")


def replay_once(
    reader: McapReader,
    channels: dict[int, foxglove.Channel],
    no_delay: bool,
    rate: float,
) -> int:
    previous_log_time: int | None = None
    count = 0

    for message in reader.iter_messages():
        channel = channels.get(message.channel_id)
        if channel is None:
            channel_record = reader.channels.get(message.channel_id)
            if channel_record is None:
                raise McapFormatError(f"message references unknown channel {message.channel_id}")
            channel = create_channel(channel_record, reader.schemas)
            channels[message.channel_id] = channel

        if not no_delay and previous_log_time is not None:
            delay = (message.log_time - previous_log_time) / 1_000_000_000.0 / rate
            if delay > 0:
                time.sleep(delay)

        channel.log(message.data, log_time=message.log_time)
        previous_log_time = message.log_time
        count += 1

    return count


if __name__ == "__main__":
    try:
        main()
    except McapFormatError as exc:
        print(f"foxglove_mcap.py: {exc}", file=sys.stderr)
        sys.exit(1)
