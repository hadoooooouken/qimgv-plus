#!/usr/bin/env python3
"""
check_deps.py - Upstream dependency version checker for qimgv-plus.

Fetches latest git release tags directly from upstream repositories via
'git ls-remote' (no GitHub token or API rate limits required) and compares
them against the versions configured in build_scripts/setup-deps.ps1.

Usage:
    python build_scripts/check_deps.py
    python build_scripts/check_deps.py --markdown
    python build_scripts/check_deps.py --json
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# Pre-release / unstable pattern keywords to filter out non-stable tags
UNSTABLE_KEYWORDS_PATTERN = re.compile(
    r"[a-z]*(?:rc|alpha|beta|dev|pre|preview|kernel|esr|test|draft)",
    re.IGNORECASE,
)

# Semantic version extraction pattern (handles vX.Y.Z, nX.Y.Z, Release-X.Y.Z, X.Y.Z)
VERSION_TAG_PATTERN = re.compile(
    r"^(?:v|n|Release-)?(\d+(?:\.\d+)+)(.*)$",
    re.IGNORECASE,
)

# Fallback dependency definitions if setup-deps.ps1 cannot be found or parsed
DEFAULT_DEPS = [
    {"name": "Imath", "url": "https://github.com/AcademySoftwareFoundation/Imath.git", "current": "v3.2.3", "patched": True},
    {"name": "openexr", "url": "https://github.com/AcademySoftwareFoundation/openexr.git", "current": "v3.5.0", "patched": True},
    {"name": "libavif", "url": "https://github.com/AOMediaCodec/libavif.git", "current": "v1.4.2", "patched": True},
    {"name": "libjxl", "url": "https://github.com/libjxl/libjxl.git", "current": "v0.12.0", "patched": False},
    {"name": "jxrlib", "url": "https://github.com/4creators/jxrlib.git", "current": "v2019.10.9", "patched": True},
    {"name": "LibRaw", "url": "https://github.com/LibRaw/LibRaw.git", "current": "0.22.2", "patched": True},
    {"name": "ffmpeg", "url": "https://git.ffmpeg.org/ffmpeg.git", "current": "n9.0.1", "patched": False},
    {"name": "kimageformats", "url": "https://invent.kde.org/frameworks/kimageformats.git", "current": "v6.26.0", "patched": True},
    {"name": "openjpeg", "url": "https://github.com/uclouvain/openjpeg.git", "current": "v2.5.4", "patched": True},
    {"name": "OpenJPH", "url": "https://github.com/aous72/OpenJPH.git", "current": "0.32.0", "patched": True},
    {"name": "libdeflate", "url": "https://github.com/ebiggers/libdeflate.git", "current": "v1.26", "patched": True},
    {"name": "zlib-ng", "url": "https://github.com/zlib-ng/zlib-ng.git", "current": "2.3.3", "patched": False},
    {"name": "libjpeg-turbo", "url": "https://github.com/libjpeg-turbo/libjpeg-turbo.git", "current": "3.2.0", "patched": False},
    {"name": "libspng", "url": "https://github.com/randy408/libspng.git", "current": "v0.7.4", "patched": False},
    {"name": "libtiff", "url": "https://gitlab.com/libtiff/libtiff.git", "current": "v4.7.2", "patched": False},
    {"name": "zstd", "url": "https://github.com/facebook/zstd.git", "current": "v1.5.7", "patched": False},
    {"name": "exiv2", "url": "https://github.com/Exiv2/exiv2.git", "current": "0.28.9", "patched": False},
]

# ANSI color escape codes
COLOR_RESET = "\033[0m"
COLOR_BOLD = "\033[1m"
COLOR_GREEN = "\033[32m"
COLOR_YELLOW = "\033[33m"
COLOR_CYAN = "\033[36m"
COLOR_RED = "\033[31m"
COLOR_GRAY = "\033[90m"


@dataclass
class DepCheckResult:
    name: str
    current: str
    latest: str
    status: str
    patched: bool
    branch_latest: Optional[str]
    url: str


def parse_tag(tag: str) -> Optional[Tuple[Tuple[int, ...], str]]:
    """
    Parses a git tag into an integer tuple representation and raw tag string.
    Filters out unstable pre-releases (rc, alpha, beta, dev, preview, etc.).
    """
    m = VERSION_TAG_PATTERN.match(tag)
    if not m:
        return None
    ver_str, suffix = m.groups()
    if suffix and UNSTABLE_KEYWORDS_PATTERN.search(suffix):
        return None
    try:
        numbers = tuple(int(x) for x in ver_str.split("."))
        return numbers, tag
    except ValueError:
        return None


def fetch_remote_tags(url: str, timeout: int = 30) -> List[str]:
    """
    Queries git tags directly from the remote repository using git ls-remote.
    """
    cmd = ["git", "ls-remote", "--tags", "--refs", url]
    try:
        res = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
        if res.returncode != 0:
            return []
    except (subprocess.SubprocessError, OSError):
        return []

    tags: List[str] = []
    prefix = "refs/tags/"
    for line in res.stdout.strip().splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1].startswith(prefix):
            tags.append(parts[1][len(prefix):])
    return tags


def parse_setup_deps_script(ps1_path: Path) -> List[Dict[str, object]]:
    """
    Extracts Git dependencies, prebuilts, and patch info from setup-deps.ps1.
    """
    if not ps1_path.is_file():
        return DEFAULT_DEPS

    try:
        content = ps1_path.read_text(encoding="utf-8")
    except OSError:
        return DEFAULT_DEPS

    # Parse patched status
    patched_deps = set()
    single_patch_pattern = re.compile(r'@\{\s*Dep\s*=\s*"([^"]+)";\s*Patch\s*=', re.IGNORECASE)
    for m in single_patch_pattern.finditer(content):
        patched_deps.add(m.group(1).lower())

    series_patch_pattern = re.compile(r'@\{\s*Dep\s*=\s*"([^"]+)";\s*PatchDir\s*=', re.IGNORECASE)
    for m in series_patch_pattern.finditer(content):
        patched_deps.add(m.group(1).lower())

    # Parse Git dependencies
    git_deps: List[Dict[str, object]] = []
    dep_pattern = re.compile(
        r'@\{\s*Name\s*=\s*"([^"]+)";\s*Url\s*=\s*"([^"]+)";\s*Tag\s*=\s*"([^"]+)"',
        re.IGNORECASE,
    )
    for m in dep_pattern.finditer(content):
        name, url, tag = m.groups()
        git_deps.append({
            "name": name,
            "url": url,
            "current": tag,
            "patched": name.lower() in patched_deps,
        })

    # Parse Exiv2 prebuilt release
    exiv2_match = re.search(
        r'\$EXIV2_URL\s*=\s*"https://github\.com/Exiv2/exiv2/releases/download/v?([^/]+)/',
        content,
    )
    if exiv2_match:
        exiv2_ver = exiv2_match.group(1)
        git_deps.append({
            "name": "exiv2",
            "url": "https://github.com/Exiv2/exiv2.git",
            "current": exiv2_ver,
            "patched": False,
        })

    return git_deps if git_deps else DEFAULT_DEPS


def check_single_dependency(dep: Dict[str, object], timeout: int) -> DepCheckResult:
    """
    Checks an individual dependency against upstream tags.
    """
    name = str(dep["name"])
    url = str(dep["url"])
    current = str(dep["current"])
    patched = bool(dep.get("patched", False))

    tags = fetch_remote_tags(url, timeout=timeout)
    parsed_tags = []
    for t in tags:
        parsed = parse_tag(t)
        if parsed is not None:
            parsed_tags.append(parsed)

    if not parsed_tags:
        return DepCheckResult(
            name=name,
            current=current,
            latest="Unknown",
            status="Failed to fetch",
            patched=patched,
            branch_latest=None,
            url=url,
        )

    parsed_tags.sort(key=lambda item: item[0])
    latest_numbers, latest_tag = parsed_tags[-1]

    curr_parsed = parse_tag(current)
    curr_numbers = curr_parsed[0] if curr_parsed else ()

    # Find the latest tag in the same major/minor branch (if applicable)
    branch_latest_tag: Optional[str] = None
    if len(curr_numbers) >= 2:
        branch_key = curr_numbers[:2]
        same_branch = [item for item in parsed_tags if len(item[0]) >= 2 and item[0][:2] == branch_key]
        if same_branch:
            same_branch.sort(key=lambda item: item[0])
            branch_latest_tag = same_branch[-1][1]

    if curr_numbers == latest_numbers:
        status = "Up-to-date"
    elif curr_numbers < latest_numbers:
        status = "Update available"
    else:
        status = "Ahead of upstream"

    return DepCheckResult(
        name=name,
        current=current,
        latest=latest_tag,
        status=status,
        patched=patched,
        branch_latest=branch_latest_tag,
        url=url,
    )


def format_status_terminal(status: str, use_color: bool) -> str:
    """
    Applies ANSI color formatting to status strings.
    """
    if not use_color:
        return status

    if status == "Up-to-date":
        return f"{COLOR_GREEN}{status}{COLOR_RESET}"
    if status == "Update available":
        return f"{COLOR_YELLOW}{status}{COLOR_RESET}"
    if status == "Ahead of upstream":
        return f"{COLOR_CYAN}{status}{COLOR_RESET}"
    return f"{COLOR_RED}{status}{COLOR_RESET}"


def print_terminal_table(results: List[DepCheckResult], use_color: bool) -> None:
    """
    Renders an aligned CLI table for terminal view.
    """
    col_lib = max(len("Library"), max(len(r.name) for r in results)) + 2
    col_cur = max(len("Current"), max(len(r.current) for r in results)) + 2
    col_lat = max(len("Latest Upstream"), max(len(r.latest) for r in results)) + 2
    col_pat = len("Patched") + 2
    col_sta = len("Update available") + 2

    header = (
        f"{'Library':<{col_lib}} | "
        f"{'Current':<{col_cur}} | "
        f"{'Latest Upstream':<{col_lat}} | "
        f"{'Patched':<{col_pat}} | "
        f"{'Status':<{col_sta}}"
    )
    separator = "-" * len(header)

    if use_color:
        print(f"\n{COLOR_BOLD}{header}{COLOR_RESET}")
    else:
        print(f"\n{header}")
    print(separator)

    for r in results:
        patched_str = "Yes" if r.patched else "No"
        status_colored = format_status_terminal(r.status, use_color)
        status_padding = " " * (col_sta - len(r.status))

        # Show note if latest tag differs from branch latest
        notes = ""
        if r.status == "Update available" and r.branch_latest and r.branch_latest != r.latest:
            branch_note = f" (same branch: {r.branch_latest})"
            notes = f"{COLOR_GRAY}{branch_note}{COLOR_RESET}" if use_color else branch_note

        row = (
            f"{r.name:<{col_lib}} | "
            f"{r.current:<{col_cur}} | "
            f"{r.latest:<{col_lat}} | "
            f"{patched_str:<{col_pat}} | "
            f"{status_colored}{status_padding}{notes}"
        )
        print(row)

    print(separator)

    # Summary
    total = len(results)
    up_to_date = sum(1 for r in results if r.status == "Up-to-date")
    updates = sum(1 for r in results if r.status == "Update available")
    failed = sum(1 for r in results if r.status not in ("Up-to-date", "Update available", "Ahead of upstream"))

    summary_text = f"Total: {total} | Up-to-date: {up_to_date} | Updates available: {updates}"
    if failed > 0:
        summary_text += f" | Failed checks: {failed}"

    if use_color:
        print(f"{COLOR_BOLD}{summary_text}{COLOR_RESET}\n")
    else:
        print(f"{summary_text}\n")


def print_markdown_table(results: List[DepCheckResult]) -> None:
    """
    Renders a GitHub-flavored Markdown table.
    """
    print("| Library | Current | Latest Upstream | Patched | Status | Notes |")
    print("|---------|:-------:|:---------------:|:-------:|:------:|-------|")
    for r in results:
        patched_str = "Yes" if r.patched else "No"
        note = ""
        if r.status == "Update available" and r.branch_latest and r.branch_latest != r.latest:
            note = f"Latest in branch: `{r.branch_latest}`"
        print(f"| [{r.name}]({r.url}) | `{r.current}` | `{r.latest}` | {patched_str} | **{r.status}** | {note} |")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check qimgv-plus dependency versions against upstream Git repositories."
    )
    parser.add_argument(
        "--markdown", "-m",
        action="store_true",
        help="Print results formatted as a Markdown table.",
    )
    parser.add_argument(
        "--json", "-j",
        action="store_true",
        help="Output raw JSON data.",
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable ANSI colors in terminal output.",
    )
    parser.add_argument(
        "--workers", "-w",
        type=int,
        default=8,
        help="Number of concurrent worker threads (default: 8).",
    )
    parser.add_argument(
        "--timeout", "-t",
        type=int,
        default=30,
        help="Timeout in seconds for remote git queries (default: 30).",
    )

    args = parser.parse_args()

    # Determine paths relative to script location
    script_dir = Path(__file__).resolve().parent
    setup_deps_file = script_dir / "setup-deps.ps1"
    if not setup_deps_file.is_file():
        # Check parent directory fallback
        setup_deps_file = script_dir.parent / "build_scripts" / "setup-deps.ps1"

    deps = parse_setup_deps_script(setup_deps_file)

    if not args.json and not args.markdown:
        print(f"Checking {len(deps)} upstream repositories in parallel (timeout {args.timeout}s)...", flush=True)

    with ThreadPoolExecutor(max_workers=max(1, args.workers)) as executor:
        futures = [executor.submit(check_single_dependency, dep, args.timeout) for dep in deps]
        results = [f.result() for f in futures]

    if args.json:
        data = [asdict(r) for r in results]
        print(json.dumps(data, indent=2))
        return 0

    if args.markdown:
        print_markdown_table(results)
        return 0

    use_color = not args.no_color and sys.stdout.isatty()
    print_terminal_table(results, use_color=use_color)
    return 0


if __name__ == "__main__":
    sys.exit(main())
