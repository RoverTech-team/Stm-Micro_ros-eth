"""Multi-feed image streaming helpers for the dashboard.

Supports sensor_msgs/Image (raw) and sensor_msgs/CompressedImage, keeping only
the latest JPEG per topic so the /images page can poll multiple tiles at once.
Raw decoding is dependency-light: numpy + Pillow (both already available in
dev and via apt/pip in the ROS image).
"""

import base64
import io
import threading
from datetime import datetime

try:
    import numpy as np
except Exception:  # pragma: no cover - numpy is expected in prod/dev
    np = None

try:
    from PIL import Image as PilImage
except Exception:  # pragma: no cover
    PilImage = None


IMAGE_MSG_TYPES = (
    "sensor_msgs/msg/Image",
    "sensor_msgs/msg/CompressedImage",
)

# Default downscale cap so 4/6/9-tile layouts stay usable on LAN.
DEFAULT_MAX_WIDTH = 640
DEFAULT_JPEG_QUALITY = 80


def _require_imaging():
    if np is None or PilImage is None:
        raise RuntimeError("Image decoding requires numpy and Pillow")


def raw_to_rgb_array(data: bytes, height: int, width: int, encoding: str):
    """Convert a raw sensor_msgs/Image buffer into an RGB uint8 array."""
    _require_imaging()
    enc = (encoding or "").lower()
    buf = np.frombuffer(data, dtype=np.uint8)

    if enc in ("rgb8",):
        arr = buf.reshape((height, width, 3))
        return arr.copy()
    if enc in ("bgr8",):
        arr = buf.reshape((height, width, 3))
        return arr[:, :, ::-1].copy()
    if enc in ("rgba8",):
        arr = buf.reshape((height, width, 4))[:, :, :3]
        return arr.copy()
    if enc in ("bgra8",):
        arr = buf.reshape((height, width, 4))[:, :, :3][:, :, ::-1].copy()
        return arr.copy()
    if enc in ("mono8", "8uc1"):
        arr = buf.reshape((height, width))
        return np.stack([arr, arr, arr], axis=-1)
    if enc in ("mono16", "16uc1"):
        raw16 = np.frombuffer(data, dtype=np.uint16).reshape((height, width))
        # Normalize 16-bit to 8-bit for display.
        lo, hi = float(raw16.min()), float(raw16.max())
        span = max(hi - lo, 1.0)
        scaled = (((raw16.astype(np.float32) - lo) / span) * 255.0).astype(np.uint8)
        return np.stack([scaled, scaled, scaled], axis=-1)
    if enc in ("bayer_rggb8", "bayer_bggr8", "bayer_gbrg8", "bayer_grbg8"):
        # Cheap demosaic: replicate mono channel; full debayer is overkill here.
        arr = buf.reshape((height, width))
        return np.stack([arr, arr, arr], axis=-1)
    raise ValueError(f"Unsupported image encoding '{encoding}'")


def encode_jpeg(rgb_array, max_width: int = DEFAULT_MAX_WIDTH, quality: int = DEFAULT_JPEG_QUALITY) -> bytes:
    """Encode an RGB uint8 array to JPEG bytes, downscaling to max_width."""
    _require_imaging()
    img = PilImage.fromarray(rgb_array, mode="RGB")
    width, height = img.size
    if max_width and width > max_width:
        new_height = max(1, int(height * max_width / width))
        img = img.resize((max_width, new_height), PilImage.BILINEAR)
    buffer = io.BytesIO()
    img.save(buffer, format="JPEG", quality=int(quality))
    return buffer.getvalue()


def normalize_compressed_blob(blob: bytes, fmt: str, max_width: int = DEFAULT_MAX_WIDTH) -> bytes:
    """Normalize a CompressedImage blob to JPEG bytes.

    Already-JPEG blobs pass through (optionally downscaled); PNG blobs are
    re-encoded to JPEG to keep the frontend single-format (<img> data URL).
    """
    _require_imaging()
    fmt = (fmt or "jpeg").lower()
    if fmt == "jpeg" or fmt == "jpg":
        if not max_width:
            return bytes(blob)
        img = PilImage.open(io.BytesIO(blob)).convert("RGB")
        width, _ = img.size
        if width <= max_width:
            return bytes(blob)
        return encode_jpeg(np.asarray(img), max_width=max_width)
    # png or unknown -> decode then JPEG-encode
    img = PilImage.open(io.BytesIO(blob)).convert("RGB")
    return encode_jpeg(np.asarray(img), max_width=max_width)


class ImageViewer:
    """Keep the latest JPEG frame for one image topic (raw or compressed)."""

    def __init__(self, node, topic_name: str, topic_type: str,
                 max_width: int = DEFAULT_MAX_WIDTH, quality: int = DEFAULT_JPEG_QUALITY):
        self.node = node
        self.topic_name = topic_name
        self.topic_type = topic_type
        self.max_width = max_width
        self.quality = quality
        self._lock = threading.RLock()
        self._latest_jpeg: bytes | None = None
        self._width = 0
        self._height = 0
        self._encoding = ""
        self._frame_id = ""
        self._stamp = None
        self._frames_received = 0
        self._last_error = None
        self._sub = None
        self._connect()

    def _connect(self):
        from rclpy.qos import qos_profile_sensor_data

        if self.topic_type.endswith("CompressedImage"):
            from sensor_msgs.msg import CompressedImage

            self._sub = self.node.create_subscription(
                CompressedImage, self.topic_name, self._on_compressed, qos_profile_sensor_data
            )
        else:
            from sensor_msgs.msg import Image

            self._sub = self.node.create_subscription(
                Image, self.topic_name, self._on_image, qos_profile_sensor_data
            )

    def _on_image(self, msg):
        try:
            rgb = raw_to_rgb_array(bytes(msg.data), int(msg.height), int(msg.width), str(msg.encoding))
            jpeg = encode_jpeg(rgb, max_width=self.max_width, quality=self.quality)
            with self._lock:
                self._latest_jpeg = jpeg
                self._width = int(msg.width)
                self._height = int(msg.height)
                self._encoding = str(msg.encoding)
                try:
                    self._frame_id = str(msg.header.frame_id)
                    self._stamp = f"{msg.header.stamp.sec}.{msg.header.stamp.nanosec:09d}"
                except Exception:
                    pass
                self._frames_received += 1
                self._last_error = None
        except Exception as exc:
            try:
                self.node.get_logger().error(f"Image decode failed on {self.topic_name}: {exc}")
            except Exception:
                pass
            with self._lock:
                self._last_error = str(exc)

    def _on_compressed(self, msg):
        try:
            blob = normalize_compressed_blob(bytes(msg.data), str(msg.format), max_width=self.max_width)
            with self._lock:
                self._latest_jpeg = blob
                self._encoding = f"compressed/{msg.format}"
                try:
                    self._frame_id = str(msg.header.frame_id)
                    self._stamp = f"{msg.header.stamp.sec}.{msg.header.stamp.nanosec:09d}"
                except Exception:
                    pass
                self._frames_received += 1
                self._last_error = None
        except Exception as exc:
            try:
                self.node.get_logger().error(f"Compressed image decode failed on {self.topic_name}: {exc}")
            except Exception:
                pass
            with self._lock:
                self._last_error = str(exc)

    def snapshot(self, max_width: int | None = None):
        """Return a JSON-safe snapshot (JPEG as base64). Re-scales on demand."""
        with self._lock:
            if self._latest_jpeg is None:
                return None
            jpeg = self._latest_jpeg
            payload = {
                "topic_name": self.topic_name,
                "topic_type": self.topic_type,
                "width": self._width,
                "height": self._height,
                "encoding": self._encoding,
                "frame_id": self._frame_id,
                "stamp": self._stamp,
                "frames_received": self._frames_received,
                "last_error": self._last_error,
                "captured_at": datetime.utcnow().isoformat() + "Z",
            }
        # Optional client-side downscale request: decode + re-encode only if smaller.
        if max_width and max_width > 0 and max_width != self.max_width:
            try:
                img = PilImage.open(io.BytesIO(jpeg)).convert("RGB")
                if img.size[0] > max_width:
                    jpeg = encode_jpeg(np.asarray(img), max_width=max_width, quality=self.quality)
            except Exception:
                pass
        payload["jpeg_base64"] = base64.b64encode(jpeg).decode("ascii")
        payload["mime"] = "image/jpeg"
        return payload

    def shutdown(self):
        try:
            self.node.destroy_subscription(self._sub)
        except Exception:
            pass
