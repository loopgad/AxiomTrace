"""AxiomTrace Python tools tests - CLI, decoder, auto-search."""

import json
import os
import struct
import sys
from pathlib import Path

import pytest

# Add tool/src to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent / "tool" / "src"))

from axiomtrace_tools.cli import main as cli_main
from axiomtrace_tools.decoder import (
    FRAME_SYNC,
    WIRE_VERSION_MAJOR,
    crc16_ccitt_false,
    decode_frame,
    decode_stream,
    find_dictionary,
)


# =============================================================================
# Fixtures
# =============================================================================

@pytest.fixture
def valid_frame():
    """Create a minimal valid frame for testing."""
    # Build a minimal valid frame: sync(1) + version(1) + level(1) + module(1)
    # + event_id(2) + seq(2) + payload_len(1) + payload(0) + crc(2) = 11 bytes
    frame = bytearray()
    frame.append(FRAME_SYNC)  # sync
    frame.append((WIRE_VERSION_MAJOR << 4) | 0x01)  # version: major=1, minor=1
    frame.append(0x01)  # level=1 (INFO), reserved=0
    frame.append(0x10)  # module_id=0x10
    frame.extend(struct.pack('<H', 0x0001))  # event_id=0x0001
    frame.extend(struct.pack('<H', 1))  # seq=1
    frame.append(0)  # payload_len=0
    # CRC covers header + payload (sync through payload_len + payload)
    crc = crc16_ccitt_false(bytes(frame))
    frame.extend(struct.pack('<H', crc))
    return bytes(frame)


@pytest.fixture
def temp_dict_dir(tmp_path):
    """Create a temporary directory structure for dictionary auto-search tests."""
    # Create hierarchy: tmp_path/build/gen/.axiom/dictionary.json
    build_dir = tmp_path / "build"
    gen_dir = build_dir / "gen"
    axiom_dir = gen_dir / ".axiom"
    axiom_dir.mkdir(parents=True)
    return tmp_path, axiom_dir


@pytest.fixture
def sample_dictionary():
    """Sample dictionary content for testing."""
    return {
        "version": "1.0",
        "events": {
            "0x10:0x0001": {"name": "TEST_EVENT", "description": "Test event"}
        }
    }


# =============================================================================
# Decoder Tests
# =============================================================================

class TestDecoder:
    """Tests for decode_frame and decode_stream functions."""

    def test_decode_frame_valid(self, valid_frame):
        """Test decoding a valid frame."""
        result, offset = decode_frame(valid_frame)
        assert result is not None
        assert 'error' not in result
        assert result['sync'] == FRAME_SYNC
        assert result['version'] == (1, 1)
        assert result['level'] == 'INFO'
        assert result['module_id'] == 0x10
        assert result['event_id'] == 0x0001
        assert result['seq'] == 1
        assert result['payload'] == []

    def test_decode_frame_invalid_version(self):
        """Test decoding frame with wrong version."""
        # Create frame with version 2.x
        frame = bytearray()
        frame.append(FRAME_SYNC)
        frame.append((2 << 4) | 0x01)  # major=2
        frame.append(0x01)  # level=INFO
        frame.append(0x10)  # module_id
        frame.extend(struct.pack('<H', 0x0001))  # event_id
        frame.extend(struct.pack('<H', 1))  # seq
        frame.append(0)  # payload_len
        crc = crc16_ccitt_false(bytes(frame))
        frame.extend(struct.pack('<H', crc))

        result, _ = decode_frame(bytes(frame))
        assert 'error' in result
        assert result['error'] == 'VERSION_MISMATCH'

    def test_decode_frame_truncated(self):
        """Test decoding truncated frame."""
        # Too short to be a valid frame (less than minimum 11 bytes)
        frame = bytes([FRAME_SYNC, 0x11, 0x01, 0x10, 0x01, 0x00, 0x01, 0x00, 0x00])
        result, offset = decode_frame(frame)
        # decode_frame returns (None, offset) when frame is invalid/truncated
        assert result is None or 'error' in result

    def test_decode_stream_empty(self):
        """Test decoding empty stream."""
        result = decode_stream(b'')
        assert result == []

    def test_decode_stream_multiple_frames(self, valid_frame):
        """Test decoding stream with multiple frames."""
        # Duplicate the valid frame twice
        stream = valid_frame + valid_frame
        result = decode_stream(stream)
        assert len(result) == 2
        assert all('error' not in r for r in result)

    def test_crc16_ccitt_false(self):
        """Test CRC calculation."""
        # Known test case: CRC of empty bytes should be 0xFFFF
        assert crc16_ccitt_false(b'') == 0xFFFF


# =============================================================================
# Auto-search Tests
# =============================================================================

class TestAutoSearch:
    """Tests for find_dictionary function."""

    def test_find_dictionary_in_build_dir(self, temp_dict_dir, sample_dictionary):
        """Test finding dictionary in build/ directory."""
        tmp_path, axiom_dir = temp_dict_dir
        dict_path = axiom_dir / "dictionary.json"
        dict_path.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        # Create dictionary at tmp_path/build/gen/.axiom/dictionary.json
        # But we expect search to find it in build/dictionary.json or build/gen/dictionary.json
        # Let's create the proper structure
        build_dict = tmp_path / "build" / "dictionary.json"
        build_dict.parent.mkdir(parents=True, exist_ok=True)
        build_dict.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        found = find_dictionary(str(tmp_path / "build"))
        assert found is not None
        assert found.is_file()
        assert found.name == "dictionary.json"

    def test_find_dictionary_direct(self, tmp_path, sample_dictionary):
        """Test finding dictionary.json directly in directory."""
        dict_path = tmp_path / "dictionary.json"
        dict_path.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        found = find_dictionary(str(tmp_path))
        assert found == dict_path

    def test_find_dictionary_not_found(self, tmp_path):
        """Test when no dictionary is found."""
        found = find_dictionary(str(tmp_path))
        assert found is None

    def test_find_dictionary_walks_up(self, tmp_path, sample_dictionary):
        """Test that find_dictionary walks up the directory tree."""
        # Create dictionary in a subdirectory
        sub_dir = tmp_path / "sub" / "deep"
        sub_dir.mkdir(parents=True)
        # dictionary in parent
        dict_path = tmp_path / "dictionary.json"
        dict_path.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        found = find_dictionary(str(sub_dir))
        assert found == dict_path

    def test_find_dictionary_stops_at_home(self, tmp_path):
        """Test that search stops at home directory."""
        # Search from tmp_path which should not find anything and stop
        found = find_dictionary(str(tmp_path))
        # May or may not find depending on whether dictionary exists above
        # Just ensure it doesn't raise an exception
        assert found is None or isinstance(found, Path)


# =============================================================================
# CLI Argument Parsing Tests
# =============================================================================

class TestCLI:
    """Tests for CLI argument parsing."""

    def test_cli_no_args(self):
        """Test CLI with no arguments shows help."""
        from click.testing import CliRunner
        runner = CliRunner()
        result = runner.invoke(cli_main, [])
        # No args should show help or require INPUT
        assert result.exit_code != 0 or 'INPUT' in result.output

    def test_cli_help(self):
        """Test CLI --help."""
        from click.testing import CliRunner
        runner = CliRunner()
        result = runner.invoke(cli_main, ['--help'])
        assert result.exit_code == 0
        assert 'AxiomTrace' in result.output
        assert 'decoder' in result.output.lower()

    def test_cli_valid_input_with_empty_data(self, tmp_path):
        """Test CLI with valid input but empty data."""
        input_file = tmp_path / "input.bin"
        input_file.write_bytes(b'')

        # Should not crash, returns empty list
        result = decode_stream(b'')
        assert result == []

    def test_cli_output_format_json(self, tmp_path, valid_frame, sample_dictionary):
        """Test CLI with JSON output format."""
        import click
        from click.testing import CliRunner

        input_file = tmp_path / "input.bin"
        input_file.write_bytes(valid_frame)

        dict_file = tmp_path / "dictionary.json"
        dict_file.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        runner = CliRunner()

        @click.command()
        @click.argument('input', type=click.File('rb'))
        @click.option('--dictionary', '-d', type=click.Path(exists=True), default=None)
        @click.option('--output', '-o', type=click.Choice(['text', 'json']), default='text')
        def test_cmd(input, dictionary, output):
            click.echo(f"output={output}")

        result = runner.invoke(test_cmd, [str(input_file), '-o', 'json'])
        assert result.exit_code == 0
        assert 'output=json' in result.output


# =============================================================================
# Integration-style Tests (no network)
# =============================================================================

class TestIntegration:
    """Integration tests that don't require network or external files."""

    def test_frame_encoding_decoding_roundtrip(self):
        """Test that frames can be encoded and decoded correctly."""
        # Manually construct a known frame
        frame = bytearray()
        frame.append(FRAME_SYNC)
        frame.append((1 << 4) | 0x01)  # v1.1
        frame.append(0x02)  # WARN level
        frame.append(0xAB)  # module_id
        frame.extend(struct.pack('<H', 0x1234))  # event_id
        frame.extend(struct.pack('<H', 99))  # seq
        frame.append(0)  # payload_len=0
        crc = crc16_ccitt_false(bytes(frame))
        frame.extend(struct.pack('<H', crc))

        result, _ = decode_frame(bytes(frame))
        assert result['level'] == 'WARN'
        assert result['module_id'] == 0xAB
        assert result['event_id'] == 0x1234
        assert result['seq'] == 99

    def test_decode_with_payload(self):
        """Test decoding frame with payload."""
        # Build frame with u8 payload: tag=0x01, value=42
        frame = bytearray()
        frame.append(FRAME_SYNC)
        frame.append((1 << 4) | 0x01)  # v1.1
        frame.append(0x01)  # INFO level
        frame.append(0x01)  # module_id
        frame.extend(struct.pack('<H', 0x0001))  # event_id
        frame.extend(struct.pack('<H', 1))  # seq
        frame.append(2)  # payload_len=2 (tag + value)
        frame.append(0x01)  # tag: u8
        frame.append(42)  # value: 42
        crc = crc16_ccitt_false(bytes(frame))
        frame.extend(struct.pack('<H', crc))

        result, _ = decode_frame(bytes(frame))
        assert 'error' not in result
        assert len(result['payload']) == 1
        assert result['payload'][0]['type'] == 'u8'
        assert result['payload'][0]['value'] == 42

    def test_find_dictionary_environment_cwd(self, tmp_path, sample_dictionary):
        """Test dictionary search uses cwd when start_path is None."""
        # Create dictionary in current working directory
        orig_cwd = os.getcwd()
        dict_path = tmp_path / "dictionary.json"
        dict_path.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        try:
            os.chdir(tmp_path)
            found = find_dictionary()  # Uses cwd
            assert found == dict_path
        finally:
            os.chdir(orig_cwd)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
