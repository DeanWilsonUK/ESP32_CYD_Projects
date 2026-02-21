#!/usr/bin/env python3
"""
Movie Frame Extractor for ESP32-CYD Very Slow Movie Player
===========================================================
Extracts frames from a movie file, applies Floyd-Steinberg dithering,
and saves them as BMPs to an SD card for display on the ESP32-CYD.

Requirements:
    pip install ffmpeg-python pillow

Usage:
    python extract_frames.py --input my_movie.mkv --output /path/to/sd/card
    python extract_frames.py --input my_movie.mkv --output ./frames --increment 4
    python extract_frames.py --input my_movie.mkv --output ./frames --colour

Author: Adapted from TomWhitwell/SlowMovie for ESP32-CYD
"""

import os
import sys
import argparse
import ffmpeg
from PIL import Image, ImageEnhance

# ESP32-CYD display resolution (landscape)
DISPLAY_WIDTH = 320
DISPLAY_HEIGHT = 240


def get_frame_count(video_path):
    """Get the total number of frames in the video."""
    try:
        probe = ffmpeg.probe(video_path)
        video_stream = next(
            (s for s in probe['streams'] if s['codec_type'] == 'video'), None
        )
        if video_stream is None:
            print("Error: No video stream found in file.")
            sys.exit(1)

        # Try to get frame count directly
        if 'nb_frames' in video_stream:
            return int(video_stream['nb_frames'])

        # Fall back to calculating from duration and frame rate
        duration = float(probe['format']['duration'])
        fps_parts = video_stream['r_frame_rate'].split('/')
        fps = float(fps_parts[0]) / float(fps_parts[1])
        return int(duration * fps)

    except ffmpeg.Error as e:
        print(f"Error probing video: {e}")
        sys.exit(1)


def extract_frame(video_path, frame_number):
    """Extract a single frame from the video as a PIL Image."""
    try:
        out, _ = (
            ffmpeg
            .input(video_path)
            .filter('select', f'eq(n,{frame_number})')
            .output('pipe:', vframes=1, format='rawvideo', pix_fmt='rgb24')
            .run(capture_stdout=True, capture_stderr=True)
        )

        # Probe to get frame dimensions
        probe = ffmpeg.probe(video_path)
        video_stream = next(
            s for s in probe['streams'] if s['codec_type'] == 'video'
        )
        width = int(video_stream['width'])
        height = int(video_stream['height'])

        image = Image.frombytes('RGB', (width, height), out)
        return image

    except ffmpeg.Error as e:
        print(f"Error extracting frame {frame_number}: {e.stderr.decode()}")
        return None


def process_frame(image, colour_mode=False, contrast=1.0):
    """
    Resize, enhance, and dither a frame for the ESP32-CYD display.

    Args:
        image: PIL Image object
        colour_mode: If False, apply B&W Floyd-Steinberg dithering (VSMP style)
                     If True, apply colour dithering with limited palette
        contrast: contrast adjustment multiplier (1.0 = no change)
    """
    # Resize to display resolution, maintaining aspect ratio with letterboxing
    image.thumbnail((DISPLAY_WIDTH, DISPLAY_HEIGHT), Image.LANCZOS)

    # Create black background canvas
    canvas = Image.new('RGB', (DISPLAY_WIDTH, DISPLAY_HEIGHT), (0, 0, 0))

    # Centre the image on the canvas
    x_offset = (DISPLAY_WIDTH - image.width) // 2
    y_offset = (DISPLAY_HEIGHT - image.height) // 2
    canvas.paste(image, (x_offset, y_offset))
    image = canvas

    # Apply contrast enhancement
    if contrast != 1.0:
        image = ImageEnhance.Contrast(image).enhance(contrast)

    if colour_mode:
        # Colour dithering: reduce to limited palette (retro/pixel art look)
        # Using a simple web-safe-ish 64 colour palette
        image = image.convert('P', palette=Image.ADAPTIVE, colors=64, dither=Image.FLOYDSTEINBERG)
        image = image.convert('RGB')
    else:
        # Classic B&W Floyd-Steinberg dithering (faithful to original VSMP)
        image = image.convert('L')          # Convert to greyscale
        image = image.convert('1', dither=Image.FLOYDSTEINBERG)  # Dither to B&W
        image = image.convert('RGB')        # Convert back to RGB for BMP saving

    return image


def extract_frames(video_path, output_dir, increment=4, start_frame=0,
                   colour_mode=False, contrast=1.5):
    """
    Main extraction loop. Extracts every Nth frame from the video,
    processes and saves to output directory.

    Args:
        video_path:  path to the input video file
        output_dir:  directory to save frames (your SD card path)
        increment:   extract every Nth frame (default 4, same as SlowMovie)
        start_frame: frame number to start from (useful for resuming)
        colour_mode: False = B&W dithered, True = colour dithered
        contrast:    contrast enhancement (1.5 recommended for dithering)
    """
    if not os.path.exists(video_path):
        print(f"Error: Video file not found: {video_path}")
        sys.exit(1)

    os.makedirs(output_dir, exist_ok=True)

    # Create a 'frames' subdirectory on the SD card
    frames_dir = os.path.join(output_dir, 'frames')
    os.makedirs(frames_dir, exist_ok=True)

    print(f"Probing video file: {video_path}")
    total_frames = get_frame_count(video_path)
    print(f"Total frames: {total_frames}")

    # Calculate which frames we'll extract
    frames_to_extract = list(range(start_frame, total_frames, increment))
    total_to_extract = len(frames_to_extract)

    mode_label = "colour dithered" if colour_mode else "B&W Floyd-Steinberg"
    print(f"Extracting {total_to_extract} frames ({mode_label})")
    print(f"Increment: every {increment} frames")
    print(f"Output directory: {frames_dir}")
    print(f"Output resolution: {DISPLAY_WIDTH}x{DISPLAY_HEIGHT}")
    print()

    # Save a manifest file so the ESP32 knows how many frames there are
    manifest_path = os.path.join(output_dir, 'manifest.txt')

    extracted_count = 0
    for i, frame_number in enumerate(frames_to_extract):

        # Filename format: 00000.bmp, 00001.bmp, etc. (zero-padded for ordering)
        filename = f"{extracted_count:05d}.bmp"
        output_path = os.path.join(frames_dir, filename)

        # Skip already-extracted frames (allows resuming interrupted extraction)
        if os.path.exists(output_path):
            extracted_count += 1
            if i % 10 == 0:
                print(f"  Skipping {filename} (already exists)")
            continue

        # Progress update
        percent = (i / total_to_extract) * 100
        print(f"  [{percent:5.1f}%] Frame {frame_number:6d} → {filename}", end='\r')

        image = extract_frame(video_path, frame_number)
        if image is None:
            print(f"\n  Warning: Could not extract frame {frame_number}, skipping.")
            continue

        processed = process_frame(image, colour_mode=colour_mode, contrast=contrast)
        processed.save(output_path, 'BMP')

        extracted_count += 1

    print(f"\n\nDone! Extracted {extracted_count} frames to: {frames_dir}")

    # Write manifest
    with open(manifest_path, 'w') as f:
        f.write(f"frames={extracted_count}\n")
        f.write(f"width={DISPLAY_WIDTH}\n")
        f.write(f"height={DISPLAY_HEIGHT}\n")
        f.write(f"mode={'colour' if colour_mode else 'bw'}\n")

    print(f"Manifest written to: {manifest_path}")
    print()
    print("Next steps:")
    print("  1. Copy the entire output directory to your SD card root")
    print("  2. Insert SD card into ESP32-CYD")
    print("  3. Flash the ESP32 sketch")


def main():
    parser = argparse.ArgumentParser(
        description='Extract dithered frames from a movie for ESP32-CYD VSMP'
    )
    parser.add_argument(
        '-i', '--input',
        required=True,
        help='Path to the input video file (mp4, mkv, avi, etc.)'
    )
    parser.add_argument(
        '-o', '--output',
        required=True,
        help='Output directory (e.g. /Volumes/SD_CARD or D:\\SD_CARD or ./frames)'
    )
    parser.add_argument(
        '--increment',
        type=int,
        default=4,
        help='Extract every Nth frame (default: 4, same as SlowMovie default)'
    )
    parser.add_argument(
        '--start',
        type=int,
        default=0,
        help='Start from this frame number (default: 0)'
    )
    parser.add_argument(
        '--colour',
        action='store_true',
        help='Use colour dithering instead of B&W Floyd-Steinberg'
    )
    parser.add_argument(
        '--contrast',
        type=float,
        default=1.5,
        help='Contrast enhancement multiplier (default: 1.5, good for dithering)'
    )

    args = parser.parse_args()

    print("=" * 50)
    print("  ESP32-CYD Movie Frame Extractor")
    print("  Adapted from TomWhitwell/SlowMovie")
    print("=" * 50)
    print()

    extract_frames(
        video_path=args.input,
        output_dir=args.output,
        increment=args.increment,
        start_frame=args.start,
        colour_mode=args.colour,
        contrast=args.contrast
    )


if __name__ == '__main__':
    main()
