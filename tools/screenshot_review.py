#!/usr/bin/env python3

import argparse
import base64
import json
import mimetypes
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Iterable, List, Sequence


DEFAULT_MODEL = "google/gemini-3-flash-preview"
DEFAULT_PROMPT = (
    "Review these Amiga application screenshots. Identify visible rendering, layout, "
    "text, clipping, input, or menu issues. Explain what looks wrong, what appears "
    "correct, and suggest the most likely subsystem involved inside lxa."
)
DEFAULT_SYSTEM_PROMPT = (
    "You are reviewing screenshots of Amiga applications running under lxa. "
    "Focus on concrete visual evidence. Mention coordinates or regions when helpful. "
    "Distinguish clearly between confirmed observations and hypotheses."
)
OPENROUTER_URL = "https://openrouter.ai/api/v1/chat/completions"


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Ask an OpenRouter vision model to review one or more screenshots."
    )
    parser.add_argument("images", nargs="+", help="Screenshot paths to review")
    parser.add_argument(
        "--model",
        default=DEFAULT_MODEL,
        help=f"OpenRouter model name (default: {DEFAULT_MODEL})",
    )
    parser.add_argument(
        "--prompt",
        default=DEFAULT_PROMPT,
        help="User prompt sent alongside the screenshots",
    )
    parser.add_argument(
        "--system-prompt",
        default=DEFAULT_SYSTEM_PROMPT,
        help="System prompt used for the screenshot review",
    )
    parser.add_argument(
        "--max-tokens",
        type=int,
        default=1200,
        help="Maximum completion tokens to request",
    )
    parser.add_argument(
        "--temperature",
        type=float,
        default=0.1,
        help="Sampling temperature",
    )
    parser.add_argument(
        "--output",
        choices=("text", "json"),
        default="text",
        help="Output format",
    )
    return parser.parse_args(argv)


def guess_mime_type(path: Path) -> str:
    mime_type, _ = mimetypes.guess_type(str(path))
    if mime_type:
        return mime_type
    return "application/octet-stream"


def encode_image(path: Path) -> str:
    with path.open("rb") as handle:
        return base64.b64encode(handle.read()).decode("ascii")


def build_user_content(image_paths: Iterable[Path], prompt: str) -> List[dict]:
    content: List[dict[str, object]] = [{"type": "text", "text": prompt}]

    for image_path in image_paths:
        content.append(
            {
                "type": "text",
                "text": f"Screenshot: {image_path.name}",
            }
        )
        content.append(
            {
                "type": "image_url",
                "image_url": {
                    "url": (
                        f"data:{guess_mime_type(image_path)};base64,"
                        f"{encode_image(image_path)}"
                    )
                },
            }
        )

    return content


def build_request_payload(
    args: argparse.Namespace, image_paths: Sequence[Path]
) -> dict:
    return {
        "model": args.model,
        "temperature": args.temperature,
        "max_tokens": args.max_tokens,
        "messages": [
            {"role": "system", "content": args.system_prompt},
            {
                "role": "user",
                "content": build_user_content(image_paths, args.prompt),
            },
        ],
    }


def extract_text_content(message_content):
    if isinstance(message_content, str):
        return message_content

    if isinstance(message_content, list):
        parts = []
        for item in message_content:
            if isinstance(item, dict) and item.get("type") == "text":
                parts.append(item.get("text", ""))
        return "\n".join(part for part in parts if part)

    return ""


def parse_response_text(response_payload: dict) -> str:
    choices = response_payload.get("choices") or []
    if not choices:
        raise ValueError("OpenRouter response did not contain any choices")

    message = choices[0].get("message") or {}
    text = extract_text_content(message.get("content"))
    if not text:
        raise ValueError("OpenRouter response did not contain text content")

    return text


def call_openrouter(payload: dict, api_key: str) -> dict:
    request = urllib.request.Request(
        OPENROUTER_URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
            "HTTP-Referer": "https://github.com/guenterbartsch/lxa",
            "X-Title": "lxa screenshot review",
        },
        method="POST",
    )

    with urllib.request.urlopen(request, timeout=120) as response:
        return json.loads(response.read().decode("utf-8"))


def validate_image_paths(image_args: Sequence[str]) -> List[Path]:
    image_paths = []

    for image_arg in image_args:
        image_path = Path(image_arg)
        if not image_path.is_file():
            raise FileNotFoundError(f"Screenshot not found: {image_path}")
        image_paths.append(image_path)

    return image_paths


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)

    api_key = os.environ.get("OPENROUTER_API_KEY")
    if not api_key:
        print("OPENROUTER_API_KEY is not set", file=sys.stderr)
        return 2

    try:
        image_paths = validate_image_paths(args.images)
        payload = build_request_payload(args, image_paths)
        response_payload = call_openrouter(payload, api_key)
        if args.output == "json":
            print(json.dumps(response_payload, indent=2, sort_keys=True))
        else:
            print(parse_response_text(response_payload))
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        print(f"OpenRouter request failed: HTTP {exc.code}: {body}", file=sys.stderr)
        return 1
    except urllib.error.URLError as exc:
        print(f"OpenRouter request failed: {exc}", file=sys.stderr)
        return 1
    except ValueError as exc:
        print(f"Invalid response: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
