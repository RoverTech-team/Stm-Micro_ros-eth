"""Raw mono8 video over UDP for the /images page.

Covers the "NCAP VFR0 feed" source: firmware streams grayscale luma as
self-describing UDP datagrams (one per row block) that this viewer reassembles
into frames, converts to JPEG, and serves through the same snapshot shape as
the TCP/HTTP socket viewer so the frontend polls both uniformly.

Wire format (see NCAP firmware/video_udp.c):
    meta: 'V' 'F' 'M' '0' | HTTP-style preamble text
    data: 'V' 'F' 'R' '0' | u32 seq | u16 width | u16 height
                          | u16 row_offset | u16 rows | u32 payload_len | luma...

Subscribe with ``udp://host:port`` (host selects the local interface to bind;
use 0.0.0.0 when the sender is remote).
"""

from __future__ import annotations

import base64
import socket
import struct
import threading
import time
from datetime import datetime
from typing import Dict, Optional, Tuple

try:
    from PIL import Image as PilImage
except Exception:  # pragma: no cover
    PilImage = None

try:
    from image_stream import encode_jpeg, raw_to_rgb_array
except Exception:  # pragma: no cover - imaging helpers optional at import
    raw_to_rgb_array = None
    encode_jpeg = None


VFR_MAGIC = b"VFR0"
VFM_MAGIC = b"VFM0"
VFR_HDR_LEN = 20
VFM_HDR_LEN = 4

RECV_TIMEOUT_SEC = 1.0
MAX_FRAME_BYTES = 2 * 1024 * 1024
MAX_PENDING_SEQS = 8


def parse_vfr_datagram(data: bytes) -> Tuple[str, dict, bytes]:
    """Split one datagram into (kind, fields, payload).

    kind is "meta", "frame", or "unknown". Raises ValueError on truncation.
    """
    if len(data) >= VFM_HDR_LEN and data[:4] == VFM_MAGIC:
        return "meta", {}, bytes(data[VFM_HDR_LEN:])
    if len(data) < VFR_HDR_LEN or data[:4] != VFR_MAGIC:
        return "unknown", {}, b""
    if len(data) < VFR_HDR_LEN:
        raise ValueError("truncated VFR header")
    seq, width, height, row, rows, plen = struct.unpack(">IHHHHI", data[4:VFR_HDR_LEN])
    payload = bytes(data[VFR_HDR_LEN:VFR_HDR_LEN + plen])
    if len(payload) != plen:
        raise ValueError(f"short VFR payload {len(payload)} != {plen}")
    return "frame", {
        "seq": seq, "width": width, "height": height,
        "row": row, "rows": rows, "plen": plen,
    }, payload


class VfrUdpViewer:
    """Background-thread UDP receiver reassembling VFR0 luma into JPEG."""

    def __init__(self, feed_id: str, host: str = "0.0.0.0", port: int = 0,
                 max_width: int = 640, quality: int = 80):
        self.feed_id = feed_id
        self.host = host or "0.0.0.0"
        self.port = int(port)
        self.path = "/"
        self.scheme = "udp"
        self.max_width = int(max_width or 640)
        self.quality = int(quality or 80)
        self._lock = threading.RLock()
        self._stop = threading.Event()
        self._latest_jpeg: Optional[bytes] = None
        self._width = 0
        self._height = 0
        self._encoding = "mono8"
        self._frames_received = 0
        self._last_error: Optional[str] = f"Binding UDP {self.host}:{self.port}…"
        self._connected = False
        self._last_frame_at: Optional[str] = None
        self._frames: Dict[int, dict] = {}
        self._sock: Optional[socket.socket] = None
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    # -- internals -----------------------------------------------------
    def _store_frame(self, width: int, height: int, luma: bytes):
        try:
            if raw_to_rgb_array is None or encode_jpeg is None:
                raise RuntimeError("Image decoding requires numpy and Pillow")
            rgb = raw_to_rgb_array(bytes(luma), int(height), int(width), "mono8")
            jpeg = encode_jpeg(rgb, max_width=self.max_width, quality=self.quality)
            with self._lock:
                self._latest_jpeg = jpeg
                self._width = int(width)
                self._height = int(height)
                self._encoding = "mono8"
                self._frames_received += 1
                self._connected = True
                self._last_error = None
                self._last_frame_at = datetime.utcnow().isoformat() + "Z"
        except Exception as exc:
            with self._lock:
                self._last_error = str(exc)

    def _ingest(self, fields: dict, payload: bytes):
        seq = fields["seq"]
        width, height, row, rows = fields["width"], fields["height"], fields["row"], fields["rows"]
        if width <= 0 or height <= 0 or width * height > MAX_FRAME_BYTES:
            return
        with self._lock:
            entry = self._frames.get(seq)
            if entry is None:
                if len(self._frames) >= MAX_PENDING_SEQS:
                    # Drop the oldest incomplete frame to stay bounded.
                    self._frames.pop(min(self._frames))
                entry = {"width": width, "height": height, "blocks": {}}
                self._frames[seq] = entry
            if row not in entry["blocks"]:
                entry["blocks"][row] = (rows, payload)
            covered = sum(r for (r, _p) in entry["blocks"].values())
            if covered >= height:
                img = bytearray(width * height)
                for row_off, (r, p) in sorted(entry["blocks"].items()):
                    for i in range(r):
                        dst = (row_off + i) * width
                        src = i * width
                        take = min(width, len(p) - src, len(img) - dst)
                        if take <= 0:
                            break
                        img[dst:dst + take] = p[src:src + take]
                del self._frames[seq]
                # Outside the lock for the (potentially slow) JPEG encode.
                pending = (width, height, bytes(img))
            else:
                return
        self._store_frame(*pending)

    def _run(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind((self.host, self.port))
            # Report the real port back when 0 (ephemeral) was requested.
            with self._lock:
                self.port = sock.getsockname()[1]
            sock.settimeout(RECV_TIMEOUT_SEC)
        except Exception as exc:
            with self._lock:
                self._last_error = f"UDP bind {self.host}:{self.port} failed: {exc}"
            return
        self._sock = sock
        with self._lock:
            self._connected = True
            if self._frames_received == 0:
                self._last_error = None
        try:
            while not self._stop.is_set():
                try:
                    data, _ = sock.recvfrom(65535)
                except socket.timeout:
                    continue
                except OSError:
                    break
                try:
                    kind, fields, payload = parse_vfr_datagram(data)
                except ValueError:
                    continue
                if kind == "frame":
                    self._ingest(fields, payload)
                # meta datagrams are informational; nothing to store.
        finally:
            try:
                sock.close()
            except Exception:
                pass
            with self._lock:
                self._connected = False

    # -- public (same shape as SocketImageViewer) ----------------------
    def status(self) -> Dict[str, object]:
        with self._lock:
            return {
                "feed_id": self.feed_id,
                "scheme": self.scheme,
                "host": self.host,
                "port": self.port,
                "path": self.path,
                "connected": self._connected,
                "frames_received": self._frames_received,
                "last_error": self._last_error,
                "last_frame_at": self._last_frame_at,
            }

    def snapshot(self, max_width: Optional[int] = None):
        with self._lock:
            if self._latest_jpeg is None:
                return None
            jpeg = self._latest_jpeg
            payload = {
                "feed_id": self.feed_id,
                "source": "socket",
                "scheme": self.scheme,
                "host": self.host,
                "port": self.port,
                "path": self.path,
                "width": self._width,
                "height": self._height,
                "encoding": "mono8/socket",
                "frames_received": self._frames_received,
                "connected": self._connected,
                "last_error": self._last_error,
                "last_frame_at": self._last_frame_at,
                "captured_at": datetime.utcnow().isoformat() + "Z",
            }
        target = int(max_width or self.max_width or 0)
        if target and PilImage is not None:
            try:
                with PilImage.open(__import__("io").BytesIO(jpeg)) as img:
                    img = img.convert("RGB")
                    if img.size[0] > target:
                        new_h = max(1, int(img.size[1] * target / img.size[0]))
                        img = img.resize((target, new_h), PilImage.BILINEAR)
                        out = __import__("io").BytesIO()
                        img.save(out, format="JPEG", quality=self.quality)
                        jpeg = out.getvalue()
            except Exception:
                pass
        payload["jpeg_base64"] = base64.b64encode(jpeg).decode("ascii")
        payload["mime"] = "image/jpeg"
        return payload

    def shutdown(self):
        self._stop.set()
        sock, self._sock = self._sock, None
        if sock is not None:
            try:
                sock.close()
            except Exception:
                pass
