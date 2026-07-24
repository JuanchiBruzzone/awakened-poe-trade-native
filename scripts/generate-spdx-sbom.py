#!/usr/bin/env python3
"""Generate a deterministic SPDX 2.3 file inventory for an AppDir."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import pathlib
import subprocess


def checksum(path: pathlib.Path, algorithm: str) -> str:
    digest = hashlib.new(algorithm)
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def git_value(root: pathlib.Path, *arguments: str) -> str:
    return subprocess.check_output(
        [
            "git",
            "-c",
            f"safe.directory={root}",
            "-C",
            str(root),
            *arguments,
        ],
        text=True,
    ).strip()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate an SPDX 2.3 inventory for a packaged native AppDir."
    )
    parser.add_argument("--root", required=True, type=pathlib.Path)
    parser.add_argument("--artifact", required=True, type=pathlib.Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()

    app_dir = args.root.resolve()
    artifact = args.artifact.resolve()
    repository = pathlib.Path(__file__).resolve().parents[1]
    if not app_dir.is_dir():
        parser.error(f"AppDir does not exist: {app_dir}")
    if not artifact.is_file():
        parser.error(f"artifact does not exist: {artifact}")

    commit = git_value(repository, "rev-parse", "HEAD")
    created = git_value(repository, "show", "-s", "--format=%cI", "HEAD")
    created = (
        dt.datetime.fromisoformat(created)
        .astimezone(dt.timezone.utc)
        .strftime("%Y-%m-%dT%H:%M:%SZ")
    )
    owner_repository = os.environ.get(
        "GITHUB_REPOSITORY", "JuanchiBruzzone/awakened-poe-trade-native"
    )
    namespace = (
        f"https://github.com/{owner_repository}/spdx/"
        f"{args.version}/{commit}"
    )

    files: list[dict[str, object]] = []
    verification_hashes: list[str] = []
    has_files: list[str] = []
    for index, path in enumerate(
        sorted(
            item
            for item in app_dir.rglob("*")
            if item.is_file() and not item.is_symlink()
        ),
        start=1,
    ):
        relative = path.relative_to(app_dir).as_posix()
        sha1 = checksum(path, "sha1")
        sha256 = checksum(path, "sha256")
        spdx_id = f"SPDXRef-File-{index}"
        verification_hashes.append(sha1)
        has_files.append(spdx_id)
        files.append(
            {
                "fileName": f"./{relative}",
                "SPDXID": spdx_id,
                "checksums": [
                    {"algorithm": "SHA1", "checksumValue": sha1},
                    {"algorithm": "SHA256", "checksumValue": sha256},
                ],
                "licenseConcluded": "NOASSERTION",
                "copyrightText": "NOASSERTION",
            }
        )

    verification = hashlib.sha1(
        "".join(sorted(verification_hashes)).encode(), usedforsecurity=False
    ).hexdigest()
    artifact_sha256 = checksum(artifact, "sha256")
    package_id = "SPDXRef-Package-AwakenedPoETradeNative"
    document = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"Awakened-PoE-Trade-Native-{args.version}-x86_64",
        "documentNamespace": namespace,
        "creationInfo": {
            "created": created,
            "creators": [
                "Tool: awakened-poe-trade-native/scripts/generate-spdx-sbom.py"
            ],
        },
        "documentDescribes": [package_id],
        "packages": [
            {
                "name": "Awakened PoE Trade Native",
                "SPDXID": package_id,
                "versionInfo": args.version,
                "downloadLocation": (
                    f"https://github.com/{owner_repository}/releases/download/"
                    f"v{args.version}/{artifact.name}"
                ),
                "filesAnalyzed": True,
                "packageVerificationCode": {
                    "packageVerificationCodeValue": verification
                },
                "checksums": [
                    {"algorithm": "SHA256", "checksumValue": artifact_sha256}
                ],
                "licenseConcluded": "MIT",
                "licenseDeclared": "MIT",
                "copyrightText": "See LICENSE and upstream attribution.",
                "hasFiles": has_files,
                "externalRefs": [
                    {
                        "referenceCategory": "PACKAGE-MANAGER",
                        "referenceType": "purl",
                        "referenceLocator": (
                            "pkg:github/JuanchiBruzzone/"
                            f"awakened-poe-trade-native@v{args.version}"
                        ),
                    }
                ],
            }
        ],
        "files": files,
        "relationships": [
            {
                "spdxElementId": "SPDXRef-DOCUMENT",
                "relationshipType": "DESCRIBES",
                "relatedSpdxElement": package_id,
            }
        ],
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(
        f"Wrote {args.output} with {len(files)} files; "
        f"AppImage SHA-256 {artifact_sha256}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
