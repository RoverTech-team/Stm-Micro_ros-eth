"""Raw socket image feeds for the /images page.

Covers the "raw feed from socket" source: a TCP socket (or HTTP MJPEG URL)
that carries concatenated JPEG frames. The viewer scans the byte stream for
JPEG SOI (FFD8) / EOI (FFD9) markers so it works with:

- raw TCP JPEG streams:  ``tcp://192.168.0.10:8080`` / ``socket://host:port``
- MJPEG over HTTP:       ``http://192.168.0.10:8080/video``

Snapshots use the same JSON shape as the ROS image viewer
(``jpeg_base64`` + ``mime``) so the frontend can poll both sources uniformly.
"""

from __future__ import annotations

import base64
import http.client
import io
import re
import socket
import threading
import time
import urllib.parse
from datetime import datetime
from typing import Dict, Optional, Tuple

try:
    from PIL import Image as PilImage
except Exception:  # pragma: no cover
    PilImage = None


SOCKET_VIEW_MAX = 9
BUFFER_CAP = 4 * 1024 * 1024
RECONNECT_DELAY_SEC = 2.0
CONNECT_TIMEOUT_SEC = 5.0
READ_TIMEOUT_SEC = 10.0

_HOST_RE = re.compile(r"^[A-Za-z0-9]([A-Za-z0-9.\-]{0,253}[A-Za-z0-9])?$")

SUPPORTED_SCHEMES = ("tcp", "socket", "http", "udp")


def parse_socket_source(value: str = "", *, host: str = "", port=None, path: str = "/",
                         scheme: str = "tcp") -> Tuple[str, Dict[str, object]]:
    """Normalize a socket feed reference into (feed_id, config).

    Accepts either a single address string (``tcp://host:port/path``,
    ``host:port/path``, ``http://host:port/path``) or discrete fields.
    Returns a canonical feed_id like ``tcp://host:port/path`` plus config.
    Raises ValueError on invalid input.
    """
    raw = (value or "").strip()
    cfg_scheme = (scheme or "tcp").strip().lower()
    cfg_host = (host or "").strip()
    cfg_path = (path or "/").strip() or "/"
    cfg_port = port

    if raw:
        # Allow bare "host:port/path" by prepending the requested scheme.
        candidate = raw if "://" in raw else f"{cfg_scheme}://{raw}"
        parsed = urllib.parse.urlparse(candidate)
        netloc_scheme = (parsed.scheme or cfg_scheme).lower()
        if netloc_scheme not in SUPPORTED_SCHEMES:
            raise ValueError(f"Unsupported socket scheme '{parsed.scheme}'. Use tcp:// or http://")
        if not parsed.hostname:
            raise ValueError("Socket address needs a host, e.g. tcp://192.168.0.10:8080")
        cfg_host = parsed.hostname
        if parsed.port is not None:
            cfg_port = parsed.port
        cfg_scheme = netloc_scheme
        if parsed.path:
            cfg_path = parsed.path
    else:
        cfg_scheme = cfg_scheme.lower()
        if cfg_scheme not in SUPPORTED_SCHEMES:
            raise ValueError(f"Unsupported socket scheme '{scheme}'. Use tcp or http")

    if not cfg_host or len(cfg_host) > 255 or not _HOST_RE.match(cfg_host):
        raise ValueError("Socket host is invalid (use an IP or hostname, max 255 chars)")

    try:
        port_int = int(cfg_port) if str(cfg_port).strip() != "" else None
    except (TypeError, ValueError):
        raise ValueError("Socket port must be an integer 1-65535")
    if port_int is None or not 1 <= port_int <= 65535:
        raise ValueError("Socket port must be an integer 1-65535")

    if not cfg_path.startswith("/"):
        cfg_path = "/" + cfg_path
    if len(cfg_path) > 512:
        raise ValueError("Socket path is too long (max 512 chars)")

    if cfg_scheme == "http":
        canonical_scheme = "http"
    elif cfg_scheme == "udp":
        canonical_scheme = "udp"
    else:
        canonical_scheme = "tcp"
    feed_id = f"{canonical_scheme}://{cfg_host}:{port_int}{cfg_path}"
    if cfg_path == "/":
        feed_id = f"{canonical_scheme}://{cfg_host}:{port_int}/"
    return feed_id, {
        "scheme": canonical_scheme,
        "host": cfg_host,
        "port": port_int,
        "path": cfg_path,
    }


def extract_jpegs(buffer: bytearray) -> Tuple[list, bytearray]:
    """Pull complete JPEG frames (SOI..EOI) off the front of buffer.

    Returns (frames, remainder). Keeps a trailing partial frame buffered.
    Caps scanned garbage so a non-JPEG stream cannot grow memory unboundedly.
    """
    frames: list = []
    start = buffer.find(b"\xff\xd8")
    if start < 0:
        # No frame start: keep only the last byte (could be a split FFD8).
        return frames, buffer[-1:] if len(buffer) else bytearray()
    if start > 0:
        del buffer[:start]
    while True:
        end = buffer.find(b"\xff\xd9", 2)
        if end < 0:
            break
        frame = bytes(buffer[: end + 2])
        if len(frame) >= 128:  # skip noise / tiny fragments
            frames.append(frame)
        del buffer[: end + 2]
        nxt = buffer.find(b"\xff\xd8")
        if nxt < 0:
            buffer.clear()
            break
        if nxt > 0:
            del buffer[:nxt]
    if len(buffer) > BUFFER_CAP:
        # Stream is not yielding frames; drop oldest to stay bounded.
        del buffer[: len(buffer) - BUFFER_CAP]
    return frames, buffer


class SocketImageViewer:
    """Background-thread TCP/HTTP JPEG tap keeping the latest frame."""

    def __init__(self, feed_id: str, host: str, port: int, path: str = "/",
                 scheme: str = "tcp", max_width: int = 640, quality: int = 80):
        self.feed_id = feed_id
        self.host = host
        self.port = int(port)
        self.path = path or "/"
        self.scheme = scheme or "tcp"
        self.max_width = int(max_width or 640)
        self.quality = int(quality or 80)
        self._lock = threading.RLock()
        self._stop = threading.Event()
        self._latest_jpeg: Optional[bytes] = None
        self._width = 0
        self._height = 0
        self._frames_received = 0
        self._last_error: Optional[str] = f"Connecting to {host}:{port}…"
        self._connected = False
        self._last_frame_at: Optional[str] = None
        self._sock: Optional[socket.socket] = None
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    # -- internals -----------------------------------------------------
    def _store_frame(self, frame: bytes):
        try:
            width, height = 0, 0
            if PilImage is not None:
                try:
                    with PilImage.open(io.BytesIO(frame)) as img:
                        width, height = img.size
                except Exception:
                    pass
            with self._lock:
                self._latest_jpeg = frame
                self._width = width
                self._height = height
                self._frames_received += 1
                self._connected = True
                self._last_error = None
                self._last_frame_at = datetime.utcnow().isoformat() + "Z"
        except Exception:
            pass

    def _close_sock(self):
        sock, self._sock = self._sock, None
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass
            try:
                sock.close()
            except Exception:
                pass

    def _run(self):
        buf = bytearray()
        while not self._stop.is_set():
            try:
                if self.scheme == "http":
                    self._pump_http(buf)
                else:
                    self._pump_tcp(buf)
            except Exception as exc:
                with self._lock:
                    self._connected = False
                    self._last_error = f"{type(exc).__name__}: {exc}"
                self._close_sock()
                if self._stop.wait(RECONNECT_DELAY_SEC):
                    break
                with self._lock:
                    if self._latest_jpeg is None:
                        self._last_error = f"Retrying {self.host}:{self.port}… ({self._last_error})"

    def _pump_tcp(self, buf: bytearray):
        sock = socket.create_connection((self.host, self.port), timeout=CONNECT_TIMEOUT_SEC)
        sock.settimeout(READ_TIMEOUT_SEC)
        self._sock = sock
        with self._lock:
            self._connected = True
            if self._last_error and self._frames_received == 0:
                self._last_error = None
        try:
            while not self._stop.is_set():
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    with self._lock:
                        self._last_error = f"No data from {self.host}:{self.port} for {READ_TIMEOUT_SEC}s"
                    break
                if not chunk:
                    with self._lock:
                        self._last_error = f"Socket {self.host}:{self.port} closed by peer"
                    break
                buf.extend(chunk)
                frames, _ = extract_jpegs(buf)
                for frame in frames:
                    self._store_frame(frame)
        finally:
            self._close_sock()

    def _pump_http(self, buf: bytearray):
        conn = http.client.HTTPConnection(self.host, self.port, timeout=CONNECT_TIMEOUT_SEC)
        try:
            conn.request("GET", self.path or "/")
            resp = conn.getresponse()
            if resp.status != 200:
                raise ConnectionError(f"HTTP {resp.status} {resp.reason} for {self.path}")
            resp.fp.raw._sock.settimeout(READ_TIMEOUT_SEC) if hasattr(resp.fp, "raw") else None
            with self._lock:
                self._connected = True
                if self._frames_received == 0:
                    self._last_error = None
            while not self._stop.is_set():
                chunk = resp.read(65536)
                if not chunk:
                    with self._lock:
                        self._last_error = f"HTTP stream {self.host}:{self.port}{self.path} ended"
                    break
                buf.extend(chunk)
                frames, _ = extract_jpegs(buf)
                for frame in frames:
                    self._store_frame(frame)
        finally:
            try:
                conn.close()
            except Exception:
                pass

    # -- public --------------------------------------------------------
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
                "encoding": "jpeg/socket",
                "frames_received": self._frames_received,
                "connected": self._connected,
                "last_error": self._last_error,
                "last_frame_at": self._last_frame_at,
                "captured_at": datetime.utcnow().isoformat() + "Z",
            }
        target = int(max_width or self.max_width or 0)
        if target and PilImage is not None:
            try:
                with PilImage.open(io.BytesIO(jpeg)) as img:
                    img = img.convert("RGB")
                    if img.size[0] > target:
                        new_h = max(1, int(img.size[1] * target / img.size[0]))
                        img = img.resize((target, new_h), PilImage.BILINEAR)
                        out = io.BytesIO()
                        img.save(out, format="JPEG", quality=self.quality)
                        jpeg = out.getvalue()
            except Exception:
                pass
        payload["jpeg_base64"] = base64.b64encode(jpeg).decode("ascii")
        payload["mime"] = "image/jpeg"
        return payload

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

    def shutdown(self):
        self._stop.set()
        self._close_sock()
