#!/usr/bin/env python3
"""
List on-screen macOS windows to help debug M1-Panner overlay video-window detection.

Mirrors WindowUtil.mm: same CGWindowList options, sharing-state filter, and
substring matching against known DAW video window titles and owners.
"""

import argparse
import sys
from typing import Optional

try:
    from Quartz import (
        CGWindowListCopyWindowInfo,
        kCGWindowListOptionOnScreenOnly,
        kCGWindowListExcludeDesktopElements,
        kCGNullWindowID,
        kCGWindowName,
        kCGWindowOwnerName,
        kCGWindowBounds,
        kCGWindowSharingState,
        kCGWindowSharingNone,
        kCGWindowIsOnscreen,
        kCGWindowNumber,
        kCGWindowLayer,
    )
except ImportError:
    print(
        "Missing dependency: pyobjc-framework-Quartz\n"
        "Install with: pip3 install pyobjc-framework-Quartz",
        file=sys.stderr,
    )
    sys.exit(1)

# Keep in sync with WindowUtil.mm / WindowUtil.cpp
VIDEO_PLAYER_NAMES = [
    "Avid Video Engine",
    "Video Engine",
    "Video",
    "Video Player",
    "FL Studio Video Player",
    "Logic Pro Video",
    "Studio One Video Player",
    "Cubase Video Player",
]

VIDEO_PLAYER_OWNER_NAMES = [
    "Avid Video Engine",
]

MIN_VIDEO_WINDOW_WIDTH = 100
MIN_VIDEO_WINDOW_HEIGHT = 100


def matches_video_player(window_name: str) -> Optional[str]:
    for candidate in VIDEO_PLAYER_NAMES:
        if candidate in window_name:
            return f"title:{candidate}"
    return None


def matches_video_owner(owner_name: str) -> Optional[str]:
    for candidate in VIDEO_PLAYER_OWNER_NAMES:
        if candidate in owner_name:
            return f"owner:{candidate}"
    return None


def get_window_list():
    options = kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements
    return CGWindowListCopyWindowInfo(options, kCGNullWindowID) or []


def parse_bounds(bounds_dict):
    if not bounds_dict:
        return None
    x = bounds_dict.get("X", 0)
    y = bounds_dict.get("Y", 0)
    width = bounds_dict.get("Width", 0)
    height = bounds_dict.get("Height", 0)
    return x, y, width, height


def has_usable_video_bounds(bounds) -> bool:
    if not bounds:
        return False

    _, _, width, height = bounds
    return width >= MIN_VIDEO_WINDOW_WIDTH and height >= MIN_VIDEO_WINDOW_HEIGHT


def passes_plugin_filters(entry) -> bool:
    sharing_state = entry.get(kCGWindowSharingState, kCGWindowSharingNone)
    if sharing_state == kCGWindowSharingNone:
        return False

    is_visible = entry.get(kCGWindowIsOnscreen, 0)
    return float(is_visible) == 1.0


def collect_windows(args):
    windows = []
    for entry in get_window_list():
        owner = entry.get(kCGWindowOwnerName) or ""
        title = entry.get(kCGWindowName) or ""
        bounds = parse_bounds(entry.get(kCGWindowBounds))
        plugin_visible = passes_plugin_filters(entry)
        video_match = None
        if has_usable_video_bounds(bounds):
            video_match = (matches_video_player(title) if title else None) or matches_video_owner(owner)

        haystack = f"{owner} {title}".lower()
        if args.grep and args.grep.lower() not in haystack:
            continue

        if args.video_only and not video_match:
            continue

        if not args.all and not plugin_visible:
            continue

        windows.append(
            {
                "owner": owner,
                "title": title,
                "bounds": bounds,
                "window_id": entry.get(kCGWindowNumber, ""),
                "layer": entry.get(kCGWindowLayer, ""),
                "plugin_visible": plugin_visible,
                "video_match": video_match,
            }
        )

    return windows


def format_bounds(bounds):
    if not bounds:
        return "n/a"
    x, y, width, height = bounds
    return f"{int(x)},{int(y)} {int(width)}x{int(height)}"


def print_windows(windows, args):
    if not windows:
        print("No windows matched the current filters.")
        return

    print(f"\nFound {len(windows)} window(s)")
    print("-" * 120)
    print(
        f"{'Match':<24} | {'Application':<24} | {'Window Title':<32} | "
        f"{'Bounds (x,y wxh)':<22} | {'ID':<8} | {'Layer':<5}"
    )
    print("-" * 120)

    for window in windows:
        match = window["video_match"] or ""
        if match and args.highlight:
            match = f"* {match}"

        owner = window["owner"] or "(no app)"
        title = window["title"] or "(untitled)"
        bounds = format_bounds(window["bounds"])
        window_id = window["window_id"]
        layer = window["layer"]

        print(
            f"{match:<24} | {owner[:24]:<24} | {title[:32]:<32} | "
            f"{bounds:<22} | {str(window_id):<8} | {str(layer):<5}"
        )

    video_matches = [w for w in windows if w["video_match"]]
    if video_matches:
        print("\nOverlay would track (first match wins, same as plugin):")
        for window in video_matches[:1]:
            bounds = window["bounds"]
            if bounds:
                x, y, width, height = bounds
                overlay_x = x
                overlay_y = y + 15
                overlay_w = width
                overlay_h = height - 15
                print(
                    f"  app={window['owner']!r}, title={window['title']!r} -> "
                    f"x={overlay_x:.0f}, y={overlay_y:.0f}, "
                    f"w={overlay_w:.0f}, h={overlay_h:.0f}"
                )
    elif not args.video_only:
        print(
            "\nNo known video window title or owner matches found. "
            "Try --all or --grep to inspect more windows."
        )


def main():
    parser = argparse.ArgumentParser(
        description="List macOS windows for M1-Panner overlay debugging."
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Include windows hidden from the plugin (sharing disabled or off-screen)",
    )
    parser.add_argument(
        "--video-only",
        action="store_true",
        help="Only show windows whose title or owner matches a known DAW video player name",
    )
    parser.add_argument(
        "--grep",
        metavar="TEXT",
        help="Only show windows whose app or title contains TEXT (case-insensitive)",
    )
    parser.add_argument(
        "--no-highlight",
        action="store_true",
        help="Do not mark known video player matches with *",
    )
    args = parser.parse_args()
    args.highlight = not args.no_highlight

    print("Known video window title substrings:")
    print("  " + ", ".join(f'"{name}"' for name in VIDEO_PLAYER_NAMES))
    print("Known video window owner substrings:")
    print("  " + ", ".join(f'"{name}"' for name in VIDEO_PLAYER_OWNER_NAMES))

    windows = collect_windows(args)
    print_windows(windows, args)


if __name__ == "__main__":
    main()
