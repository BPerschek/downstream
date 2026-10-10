"""Convert the three retained CC0 Kenney cues with Blender's bundled aud.

Run from any directory:
  blender.exe --background --factory-startup --python-exit-code 1 --python convert_audio.py

No normalization, trimming, mixing, or pitch changes are applied. Source OGG
files and the unchanged vendor license are retained in third_party/kenney-interface.
"""

import array
import hashlib
import json
import math
from pathlib import Path
import sys
import wave

import aud
import bpy


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = Path(__file__).resolve().parent
VENDOR = ROOT / "third_party" / "kenney-interface"
SOURCE_URL = "https://kenney.nl/assets/interface-sounds"
DOWNLOAD_URL = (
    "https://kenney.nl/media/pages/assets/interface-sounds/"
    "fa43c1dd4d-1677589452/kenney_interface-sounds.zip"
)
ARCHIVE_SHA256 = "f2193d072726d6758a5f7871b2dcc54dcce0d5c35c6f0a62f92549b327c81232"
CHOICES = (
    ("build_confirm", "confirmation_001.ogg", "S_SettlementBuild.wav", 0.50),
    ("error", "error_004.ogg", "S_SettlementError.wav", 0.45),
    ("cargo_docking", "switch_001.ogg", "S_SettlementCargo.wav", 0.60),
)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative(path):
    return path.relative_to(ROOT).as_posix()


def inspect_pcm(path):
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        rate = wav.getframerate()
        frames = wav.getnframes()
        sample_width = wav.getsampwidth()
        compression = wav.getcomptype()
        payload = wav.readframes(frames)
    assert compression == "NONE", (path, compression)
    assert sample_width == 2, (path, sample_width)
    assert channels in (1, 2), (path, channels)
    assert frames > 0 and len(payload) == frames * channels * sample_width
    samples = array.array("h", payload)
    if sys.byteorder != "little":
        samples.byteswap()
    peak = max(abs(value) for value in samples) / 32768.0
    rms = math.sqrt(sum(value * value for value in samples) / len(samples)) / 32768.0
    clipped_samples = sum(value in (-32768, 32767) for value in samples)
    assert peak > 0.001 and clipped_samples == 0, (path, peak, clipped_samples)
    assert 0.05 <= frames / rate <= 1.5, (path, frames / rate)
    return {
        "sample_rate_hz": rate,
        "channels": channels,
        "sample_width_bits": sample_width * 8,
        "frames": frames,
        "duration_seconds": round(frames / rate, 6),
        "peak_dbfs": round(20 * math.log10(peak), 3),
        "rms_dbfs": round(20 * math.log10(rms), 3),
        "clipped_samples": clipped_samples,
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def main():
    files = []
    for usage, source_name, output_name, volume in CHOICES:
        source = VENDOR / "Audio" / source_name
        target = OUTPUT / output_name
        sound = aud.Sound(str(source))
        rate, channels = sound.specs
        sound.write(
            str(target), int(rate), channels, aud.FORMAT_S16,
            aud.CONTAINER_WAV, aud.CODEC_PCM, 0, 4096,
        )
        metrics = inspect_pcm(target)
        files.append({
            "usage": usage,
            "source_archive_member": "Audio/" + source_name,
            "source_path": relative(source),
            "source_sha256": sha256(source),
            "source_bytes": source.stat().st_size,
            "output_path": relative(target),
            "suggested_volume_multiplier": volume,
            **metrics,
        })
    manifest = {
        "pack": "Kenney Interface Sounds",
        "pack_version": "1.0",
        "creator": "Kenney",
        "source_page_url": SOURCE_URL,
        "download_url": DOWNLOAD_URL,
        "acquired_date": "2026-10-09",
        "archive_filename": "kenney_interface-sounds.zip",
        "archive_bytes": 834536,
        "archive_sha256": ARCHIVE_SHA256,
        "archive_retained": False,
        "license": "CC0 1.0 Universal",
        "license_url": "https://creativecommons.org/publicdomain/zero/1.0/",
        "vendor_license_path": relative(VENDOR / "License.txt"),
        "vendor_license_sha256": sha256(VENDOR / "License.txt"),
        "conversion": {
            "tool": "Blender bundled aud.Sound.write",
            "blender_version": bpy.app.version_string,
            "format": "RIFF WAV, signed 16-bit PCM, original sample rate and channel count",
            "changes": "Container/codec conversion only; no normalization, trimming, mixing or pitch changes",
            "script": relative(Path(__file__).resolve()),
        },
        "verification": "All outputs reopened with Python wave; PCM format, duration, non-silence and zero clipped samples checked",
        "files": files,
    }
    for destination in (OUTPUT / "manifest.json", VENDOR / "manifest.json"):
        destination.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
