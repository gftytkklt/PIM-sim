#!/usr/bin/env python3
"""Check publishable files and local links in public documentation.

Uses the existing working tree without staging or modifying files. Deleted files
are excluded; new files follow .gitignore, matching the next `git add -A` scope.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]


def git_paths(*args):
    result = subprocess.run(["git", "-C", str(ROOT), *args], check=True, capture_output=True)
    return {Path(p.decode()) for p in result.stdout.split(b"\0") if p}


def public_files():
    return {p for p in git_paths("ls-files", "-z", "--cached", "--others", "--exclude-standard")
            if (ROOT / p).is_file() or (ROOT / p).is_symlink()}


def check():
    files = public_files()
    problems = []
    ignored_tracked = git_paths("ls-files", "-z", "--cached", "--ignored", "--exclude-standard")
    for path in sorted(ignored_tracked & files):
        problems.append(f"Ignored file is still tracked: {path}")
    local_prefixes = ("docs/development/", "docs/reference/", "experiments/records/", ".opencode/",
                      ".venv/", "scripts/local/", "tests/local/")
    for path in sorted(files):
        if str(path).startswith(local_prefixes) or str(path) == "AGENTS.md":
            problems.append(f"Local-only file appears in publishable set: {path}")
    docs = [p for p in files if p.suffix == ".md" and
            (str(p) == "README.md" or str(p).startswith(("docs/", "experiments/", "artifacts/")))]
    for document in sorted(docs):
        text = (ROOT / document).read_text()
        # Public Markdown uses inline relative links; fenced examples are excluded.
        text = re.sub(r"```.*?```", "", text, flags=re.S)
        for match in re.finditer(r"!?\[[^\]]*\]\((<[^>]+>|[^\s)]+)(?:\s+\"[^\"]*\")?\)", text):
            destination = match.group(1).strip("<>")
            url = urlsplit(destination)
            if url.scheme or not url.path:
                continue
            target = (ROOT / document.parent / unquote(url.path)).resolve()
            try:
                relative = target.relative_to(ROOT)
            except ValueError:
                problems.append(f"{document}: link outside repository: {destination}")
                continue
            if relative not in files:
                if not target.is_dir() or not any(relative in f.parents for f in files):
                    problems.append(f"{document}: link target will not be published: {destination}")
        if "Document.pdf" in text:
            problems.append(f"{document}: public instructions depend on a local design PDF")
    return files, docs, problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="print the publishable file list")
    args = parser.parse_args()
    files, docs, problems = check()
    if args.list:
        print("\n".join(str(p) for p in sorted(files)))
    for problem in problems:
        print(problem, file=sys.stderr)
    print(f"Checked {len(files)} publishable files and {len(docs)} public Markdown documents; "
          f"{len(problems)} problem(s).", file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
