import argparse
import base64
import tempfile
import unittest
from pathlib import Path

from tools import screenshot_review


class ScreenshotReviewTest(unittest.TestCase):
    def test_build_user_content_embeds_prompt_and_image(self):
        image_bytes = None
        with tempfile.TemporaryDirectory() as tmpdir:
            image_path = Path(tmpdir) / "sample.png"
            image_bytes = base64.b64decode(
                "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAAC0lEQVR4nGNgYAAAAAMAASsJTYQAAAAASUVORK5CYII="
            )
            image_path.write_bytes(image_bytes)

            content = screenshot_review.build_user_content([image_path], "Inspect this")

        self.assertEqual("text", content[0]["type"])
        self.assertEqual("Inspect this", content[0]["text"])
        self.assertEqual("Screenshot: sample.png", content[1]["text"])
        self.assertTrue(
            content[2]["image_url"]["url"].startswith("data:image/png;base64,")
        )

        encoded = content[2]["image_url"]["url"].split(",", 1)[1]
        self.assertEqual(image_bytes, base64.b64decode(encoded))

    def test_build_request_payload_uses_model_and_messages(self):
        args = argparse.Namespace(
            model="google/gemini-3-flash-preview",
            prompt="Prompt",
            system_prompt="System",
            temperature=0.2,
            max_tokens=321,
        )

        with tempfile.TemporaryDirectory() as tmpdir:
            image_path = Path(tmpdir) / "sample.png"
            image_path.write_bytes(b"png")
            payload = screenshot_review.build_request_payload(args, [image_path])

        self.assertEqual("google/gemini-3-flash-preview", payload["model"])
        self.assertEqual(0.2, payload["temperature"])
        self.assertEqual(321, payload["max_tokens"])
        self.assertEqual("system", payload["messages"][0]["role"])
        self.assertEqual("System", payload["messages"][0]["content"])
        self.assertEqual("user", payload["messages"][1]["role"])

    def test_parse_response_text_accepts_string_content(self):
        payload = {
            "choices": [
                {
                    "message": {
                        "content": "Detected menu bar clipping.",
                    }
                }
            ]
        }

        self.assertEqual(
            "Detected menu bar clipping.",
            screenshot_review.parse_response_text(payload),
        )

    def test_parse_response_text_accepts_chunked_content(self):
        payload = {
            "choices": [
                {
                    "message": {
                        "content": [
                            {"type": "text", "text": "Issue one."},
                            {"type": "text", "text": "Issue two."},
                        ]
                    }
                }
            ]
        }

        self.assertEqual(
            "Issue one.\nIssue two.", screenshot_review.parse_response_text(payload)
        )

    def test_validate_image_paths_rejects_missing_file(self):
        with self.assertRaises(FileNotFoundError):
            screenshot_review.validate_image_paths(["does-not-exist.png"])


if __name__ == "__main__":
    unittest.main()
