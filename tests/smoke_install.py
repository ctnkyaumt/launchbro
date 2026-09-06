"""Exercise the built Windows updater using an offline HTTP feed and ZIPs.

Usage: python tests/smoke_install.py bin/64/launchbro.exe
No browser is launched, and no shortcuts or registry associations are created.
"""
import hashlib
import http.server
import io
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import zipfile


def main():
    launcher = Path(sys.argv[1]).resolve()
    browser = launcher.read_bytes()  # Valid PE/version resource; never executed.
    payload = b"Browser support file: extraction must preserve these bytes."
    responses = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            body = responses.get(self.path)
            self.send_response(200 if body is not None else 404)
            self.send_header("Content-Length", str(len(body or b"")))
            self.end_headers()
            self.wfile.write(body or b"")

        def log_message(self, *_args):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base_url = f"http://127.0.0.1:{server.server_port}"
    scratch = Path(__file__).resolve().parents[1] / "temp"
    scratch.mkdir(exist_ok=True)

    try:
        for prefix in ("", "portable-browser/"):
            with tempfile.TemporaryDirectory(prefix="install-test-", dir=scratch) as tmp:
                folder = Path(tmp)
                shutil.copy2(launcher, folder / "launchbro.exe")
                archive = io.BytesIO()
                with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as out:
                    out.writestr(prefix + "chrome.exe", browser)
                    out.writestr(prefix + "locales/test.pak", payload)
                responses["/browser.zip"] = archive.getvalue()
                responses["/metadata"] = (
                    f"version=999.0.0.0;download={base_url}/browser.zip;timestamp=1"
                ).encode()
                (folder / "launchbro.ini").write_text(
                    "[launchbro]\n"
                    "ChromiumType=ungoogled-chromium\n"
                    "ChromiumArchitecture=64\n"
                    "ChromiumDirectory=.\\bin\n"
                    "ChromiumAutoDownload=false\n"
                    "ChromiumCheckPeriod=0\n"
                    "ChromiumRunAtEnd=false\n"
                    "PatchRegistryProfile=false\n"
                    "CreateShortcut=false\n"
                    "AutoCheckUpdates=false\n"
                    "ChromiumBringToFront=false\n"
                    f"ChromiumUpdateUrl={base_url}/metadata\n",
                    encoding="utf-8",
                )
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = 0
                process = subprocess.Popen(
                    [str(folder / "launchbro.exe"), "-wait", "-nowow64"],
                    cwd=folder, startupinfo=startup,
                )
                try:
                    process.wait(timeout=45)
                    assert process.returncode == 0, process.returncode
                    if not (folder / "64/bin/chrome.exe").exists():
                        print("Files after launch:", list(folder.rglob("*")), flush=True)
                        for log in folder.glob("*.log"):
                            print(log.read_bytes().decode("utf-16-le", errors="replace"), flush=True)
                    assert (folder / "64/bin/chrome.exe").read_bytes() == browser
                    assert (folder / "64/bin/locales/test.pak").read_bytes() == payload
                    assert not (folder / "64/bin.new").exists()
                    assert not (folder / "64/bin.old").exists()
                    print(f"PASS first install, layout={prefix or 'archive root'}, "
                          f"browser SHA256={hashlib.sha256(browser).hexdigest()}")
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()
                    time.sleep(0.1)
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
