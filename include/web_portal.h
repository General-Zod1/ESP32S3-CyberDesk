// ============================================================
//  web_portal.h  —  Cyberpunk Wallpaper & GIF Web Upload Portal
//                   Runs on Core 1 alongside WiFi & BLE
// ============================================================
#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "wallpaper_mgr.h"

// ── Cyberpunk Glassmorphic Web Portal HTML/CSS/JS ─────────
static const char _HTML_PORTAL_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>CYBERDESK // WALLPAPER PORTAL</title>
<style>
  :root {
    --bg: #090a0f;
    --panel: #121420;
    --border: #1e2238;
    --cyan: #00f0ff;
    --magenta: #ff007f;
    --green: #00ff66;
    --yellow: #ffe600;
    --text: #e0e6ed;
    --dim: #707b90;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
  body {
    background: var(--bg);
    color: var(--text);
    padding: 20px;
    display: flex;
    justify-content: center;
  }
  .container { max-width: 600px; width: 100%; }
  .header {
    text-align: center;
    margin-bottom: 24px;
    padding-bottom: 16px;
    border-bottom: 1px solid var(--border);
  }
  .header h1 {
    font-size: 24px;
    letter-spacing: 2px;
    color: var(--cyan);
    text-shadow: 0 0 10px rgba(0,240,255,0.4);
    margin-bottom: 6px;
  }
  .header p { color: var(--magenta); font-size: 13px; letter-spacing: 1px; }

  .card {
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 18px;
    margin-bottom: 18px;
    box-shadow: 0 4px 20px rgba(0,0,0,0.5);
  }
  .card h2 {
    font-size: 14px;
    letter-spacing: 1.5px;
    color: var(--cyan);
    margin-bottom: 12px;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }

  .stats-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
    font-size: 12px;
  }
  .stat-box {
    background: #0d0f17;
    padding: 10px;
    border-radius: 4px;
    border-left: 3px solid var(--cyan);
  }
  .stat-label { color: var(--dim); margin-bottom: 4px; }
  .stat-val { font-size: 15px; font-weight: bold; color: #fff; }

  .mode-btns {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr;
    gap: 8px;
    margin-top: 10px;
  }
  .btn {
    background: #181b2c;
    border: 1px solid var(--border);
    color: #fff;
    padding: 12px 10px;
    border-radius: 6px;
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 1px;
    cursor: pointer;
    transition: all 0.2s;
    text-align: center;
  }
  .btn:hover { border-color: var(--cyan); color: var(--cyan); }
  .btn.active {
    background: rgba(0,240,255,0.15);
    border-color: var(--cyan);
    color: var(--cyan);
    box-shadow: 0 0 12px rgba(0,240,255,0.3);
  }

  /* Upload drop zone */
  .dropzone {
    border: 2px dashed var(--border);
    border-radius: 8px;
    padding: 24px;
    text-align: center;
    background: #0d0f17;
    cursor: pointer;
    transition: all 0.2s;
  }
  .dropzone:hover, .dropzone.dragover {
    border-color: var(--magenta);
    background: rgba(255,0,127,0.05);
  }
  .dropzone input { display: none; }
  .dropzone-icon { font-size: 32px; margin-bottom: 8px; }
  .dropzone-text { font-size: 13px; color: var(--dim); }
  .dropzone-sub { font-size: 11px; color: #555; margin-top: 4px; }

  .progress-bar-bg {
    height: 8px;
    background: #181b2c;
    border-radius: 4px;
    margin-top: 12px;
    overflow: hidden;
    display: none;
  }
  .progress-bar {
    height: 100%;
    width: 0%;
    background: linear-gradient(90deg, var(--cyan), var(--magenta));
    transition: width 0.2s;
  }

  /* File list table */
  .file-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 10px;
    background: #0d0f17;
    border-radius: 6px;
    margin-bottom: 8px;
    font-size: 13px;
    border-left: 3px solid transparent;
  }
  .file-item.active { border-left-color: var(--green); }
  .file-info { display: flex; flex-direction: column; gap: 2px; }
  .file-name { font-weight: bold; }
  .file-meta { font-size: 11px; color: var(--dim); }
  .file-actions { display: flex; gap: 6px; }
  .btn-sm {
    padding: 5px 10px;
    font-size: 11px;
    border-radius: 4px;
  }
  .btn-active {
    background: rgba(0,255,102,0.15);
    border: 1px solid var(--green);
    color: var(--green);
  }
  .btn-del {
    background: rgba(255,0,60,0.15);
    border: 1px solid #ff003c;
    color: #ff003c;
  }

  .toggle-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    font-size: 13px;
    margin-top: 10px;
  }
  .badge {
    background: var(--magenta);
    color: #fff;
    font-size: 10px;
    padding: 2px 6px;
    border-radius: 3px;
  }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>CYBERDESK v1.0</h1>
    <p>// WALLPAPER & GIF PORTAL //</p>
  </div>

  <!-- Intel -->
  <div class="card">
    <h2>SYSTEM INTEL <span id="status-tag" class="badge">ONLINE</span></h2>
    <div class="stats-grid">
      <div class="stat-box">
        <div class="stat-label">STORAGE FREE</div>
        <div class="stat-val" id="free-kb">-- KB</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">STORAGE USED</div>
        <div class="stat-val" id="used-kb">-- KB</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">ACTIVE MODE</div>
        <div class="stat-val" id="mode-name" style="color:var(--cyan)">--</div>
      </div>
      <div class="stat-box">
        <div class="stat-label">FREE RAM</div>
        <div class="stat-val" id="heap-kb">-- KB</div>
      </div>
    </div>
  </div>

  <!-- Mode Selector -->
  <div class="card">
    <h2>BACKGROUND MODE</h2>
    <div class="mode-btns">
      <button class="btn" id="btn-m0" onclick="setMode(0)">MATRIX RAIN</button>
      <button class="btn" id="btn-m1" onclick="setMode(1)">CUSTOM PIC</button>
      <button class="btn" id="btn-m2" onclick="setMode(2)">ANIMATED GIF</button>
    </div>
    <div class="toggle-row" style="margin-top: 14px;">
      <span>Clock HUD on Wallpaper:</span>
      <button class="btn btn-sm" id="btn-ovr" onclick="toggleOverlay()">TOGGLE</button>
    </div>
  </div>

  <!-- Uploader -->
  <div class="card">
    <h2>UPLOAD WALLPAPER / GIF</h2>
    <div class="dropzone" id="dropzone" onclick="document.getElementById('file-input').click()">
      <div class="dropzone-icon">🖼️</div>
      <div class="dropzone-text">Click or Drop <b>.JPG</b> or <b>.GIF</b> here</div>
      <div class="dropzone-sub">TFT 240x320 portrait recommended (auto-centered)</div>
      <input type="file" id="file-input" accept=".jpg,.jpeg,.gif">
    </div>
    <div class="progress-bar-bg" id="progress-bg">
      <div class="progress-bar" id="progress-bar"></div>
    </div>
  </div>

  <!-- Gallery -->
  <div class="card">
    <h2>WALLPAPER GALLERY <button class="btn btn-sm" onclick="fetchStatus()">REFRESH</button></h2>
    <div id="file-list">
      <div style="color:var(--dim); font-size:12px; text-align:center; padding:10px;">Loading files...</div>
    </div>
  </div>
</div>

<script>
async function fetchStatus() {
  try {
    const res = await fetch('/api/status');
    const data = await res.json();
    document.getElementById('free-kb').innerText = data.free_kb + ' KB';
    document.getElementById('used-kb').innerText = data.used_kb + ' KB';
    document.getElementById('mode-name').innerText = data.mode_name;
    document.getElementById('heap-kb').innerText = data.heap_kb + ' KB';

    [0, 1, 2].forEach(m => {
      document.getElementById('btn-m' + m).classList.toggle('active', data.mode === m);
    });

    document.getElementById('btn-ovr').innerText = data.clock_ovr ? 'SHOWN' : 'HIDDEN';
    document.getElementById('btn-ovr').style.borderColor = data.clock_ovr ? 'var(--cyan)' : 'var(--dim)';

    const listEl = document.getElementById('file-list');
    if (!data.files || data.files.length === 0) {
      listEl.innerHTML = '<div style="color:var(--dim); font-size:12px; text-align:center; padding:14px;">No files uploaded yet. Drag a .jpg or .gif above!</div>';
      return;
    }
    listEl.innerHTML = data.files.map(f => `
      <div class="file-item ${f.active ? 'active' : ''}">
        <div class="file-info">
          <div class="file-name">${f.name.replace('/', '')} ${f.active ? '<span class="badge" style="background:var(--green); color:#000;">ACTIVE</span>' : ''}</div>
          <div class="file-meta">${(f.size / 1024).toFixed(1)} KB</div>
        </div>
        <div class="file-actions">
          ${!f.active ? `<button class="btn btn-sm btn-active" onclick="selectFile('${f.name}')">SET</button>` : ''}
          <button class="btn btn-sm btn-del" onclick="deleteFile('${f.name}')">DEL</button>
        </div>
      </div>
    `).join('');
  } catch(e) {
    console.error(e);
  }
}

async function setMode(m) {
  const form = new FormData();
  form.append('mode', m);
  await fetch('/api/set_mode', { method: 'POST', body: form });
  fetchStatus();
}

async function selectFile(name) {
  const form = new FormData();
  form.append('file', name);
  await fetch('/api/set_file', { method: 'POST', body: form });
  fetchStatus();
}

async function deleteFile(name) {
  if (!confirm(`Delete ${name}?`)) return;
  const form = new FormData();
  form.append('file', name);
  await fetch('/api/delete', { method: 'POST', body: form });
  fetchStatus();
}

async function toggleOverlay() {
  await fetch('/api/toggle_overlay', { method: 'POST' });
  fetchStatus();
}

const fileInput = document.getElementById('file-input');
const dropzone  = document.getElementById('dropzone');
const progressBg = document.getElementById('progress-bg');
const progressBar = document.getElementById('progress-bar');

fileInput.addEventListener('change', () => {
  if (fileInput.files.length > 0) uploadFile(fileInput.files[0]);
});

dropzone.addEventListener('dragover', (e) => { e.preventDefault(); dropzone.classList.add('dragover'); });
dropzone.addEventListener('dragleave', () => { dropzone.classList.remove('dragover'); });
dropzone.addEventListener('drop', (e) => {
  e.preventDefault();
  dropzone.classList.remove('dragover');
  if (e.dataTransfer.files.length > 0) uploadFile(e.dataTransfer.files[0]);
});

function uploadFile(file) {
  const formData = new FormData();
  formData.append('file', file, file.name);

  progressBg.style.display = 'block';
  progressBar.style.width = '0%';

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/upload', true);

  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      const pct = Math.round((e.loaded / e.total) * 100);
      progressBar.style.width = pct + '%';
    }
  };

  xhr.onload = () => {
    progressBg.style.display = 'none';
    if (xhr.status === 200) {
      alert(`Uploaded and set ${file.name} as wallpaper!`);
      fetchStatus();
    } else {
      alert('Upload failed.');
    }
  };

  xhr.onerror = () => {
    progressBg.style.display = 'none';
    alert('Upload connection error.');
  };

  xhr.send(formData);
}

fetchStatus();
setInterval(fetchStatus, 4000);
</script>
</body>
</html>
)rawliteral";

class WebPortal {
public:
    void begin(WallpaperMgr* wallMgr) {
        _wall = wallMgr;

        // Root UI
        _server.on("/", HTTP_GET, [this]() {
            _server.send_P(200, "text/html", _HTML_PORTAL_PAGE);
        });

        // File upload endpoint (multi-part handler)
        _server.on("/upload", HTTP_POST, [this]() {
            _server.send(200, "application/json", "{\"status\":\"ok\"}");
        }, [this]() {
            _handleUploadChunk();
        });

        // API: Get current status & file list
        _server.on("/api/status", HTTP_GET, [this]() {
            _wall->refreshFileList();
            JsonDocument doc;
            doc["mode"]       = (uint8_t)_wall->getMode();
            doc["mode_name"]  = _wall->getModeName();
            doc["active_file"]= _wall->getActiveFile();
            doc["clock_ovr"]  = _wall->hasClockOverlay();
            doc["total_kb"]   = (uint32_t)(LittleFS.totalBytes() / 1024);
            doc["used_kb"]    = (uint32_t)(LittleFS.usedBytes() / 1024);
            doc["free_kb"]    = (uint32_t)((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024);
            doc["heap_kb"]    = (uint32_t)(ESP.getFreeHeap() / 1024);

            JsonArray arr = doc["files"].to<JsonArray>();
            for (const auto& f : _wall->getFiles()) {
                JsonObject item = arr.add<JsonObject>();
                item["name"] = f;
                File file = LittleFS.open(f, "r");
                item["size"] = file ? (uint32_t)file.size() : 0;
                if (file) file.close();
                item["active"] = (f == _wall->getActiveFile());
            }

            String out;
            serializeJson(doc, out);
            _server.send(200, "application/json", out);
        });

        // API: Set active mode
        _server.on("/api/set_mode", HTTP_POST, [this]() {
            if (_server.hasArg("mode")) {
                uint8_t m = _server.arg("mode").toInt();
                _wall->setMode((WallpaperMode)m);
                _server.send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                _server.send(400, "text/plain", "Missing mode");
            }
        });

        // API: Select active wallpaper file
        _server.on("/api/set_file", HTTP_POST, [this]() {
            if (_server.hasArg("file")) {
                String f = _server.arg("file");
                _wall->setActiveFile(f);
                _server.send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                _server.send(400, "text/plain", "Missing file");
            }
        });

        // API: Delete wallpaper
        _server.on("/api/delete", HTTP_POST, [this]() {
            if (_server.hasArg("file")) {
                String f = _server.arg("file");
                bool ok = _wall->deleteFile(f);
                _server.send(200, "application/json", ok ? "{\"status\":\"ok\"}" : "{\"status\":\"error\"}");
            } else {
                _server.send(400, "text/plain", "Missing file");
            }
        });

        // API: Toggle clock overlay
        _server.on("/api/toggle_overlay", HTTP_POST, [this]() {
            _wall->toggleClockOverlay();
            _server.send(200, "application/json", "{\"status\":\"ok\"}");
        });

        _server.begin();
        Serial.println("[WEB] Cyberpunk Wallpaper Portal started on port 80");
    }

    void handleClient() {
        _server.handleClient();
    }

private:
    WebServer     _server{80};
    WallpaperMgr* _wall = nullptr;
    File          _uploadFile;

    void _handleUploadChunk() {
        HTTPUpload& upload = _server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            Serial.printf("[WEB] Upload start: %s\n", filename.c_str());
            _uploadFile = LittleFS.open(filename, "w");
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (_uploadFile) {
                _uploadFile.write(upload.buf, upload.currentSize);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (_uploadFile) {
                _uploadFile.close();
                Serial.printf("[WEB] Upload finished: %u bytes\n", upload.totalSize);
                String filename = upload.filename;
                if (!filename.startsWith("/")) filename = "/" + filename;
                _wall->setActiveFile(filename);
            }
        }
    }
};
