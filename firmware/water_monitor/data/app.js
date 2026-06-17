// ===== Pomocné funkce =====
const $ = (id) => document.getElementById(id);

function toast(msg, type = '') {
  const t = $('toast');
  t.textContent = msg;
  t.className = 'toast show ' + type;
  setTimeout(() => t.classList.remove('show'), 3000);
}

async function fetchJSON(url, opts = {}) {
  try {
    const r = await fetch(url, opts);
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    return await r.json();
  } catch (err) {
    console.error('Fetch error:', url, err);
    throw err;
  }
}

function formatUptime(s) {
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m ${s % 60}s`;
}

// ===== Záložky =====
document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    btn.classList.add('active');
    $('tab-' + btn.dataset.tab).classList.add('active');
    
    // Načti data pro záložku
    if (btn.dataset.tab === 'config') loadConfig();
    if (btn.dataset.tab === 'calibration') loadCalibration();
  });
});

// ===== Dashboard - live data =====
async function updateDashboard() {
  try {
    const s = await fetchJSON('/api/status');
    
    // Status badges
    const wifiBadge = $('badge-wifi');
    wifiBadge.textContent = `WiFi: ${s.wifi_connected ? 'OK' : 'OFF'}`;
    wifiBadge.className = 'badge ' + (s.wifi_connected ? 'ok' : 'error');
    
    const mqttBadge = $('badge-mqtt');
    mqttBadge.textContent = `MQTT: ${s.mqtt_connected ? 'OK' : 'OFF'}`;
    mqttBadge.className = 'badge ' + (s.mqtt_connected ? 'ok' : 'error');
    
    // Flow
    $('flow-lpm').textContent = s.flow_lpm.toFixed(2);
    $('total-volume').textContent = s.total_volume_l.toFixed(1);
    
    // Tank
    if (s.tank_valid) {
      $('tank-fill').style.height = s.tank_fill_percent.toFixed(1) + '%';
      $('tank-percent').textContent = s.tank_fill_percent.toFixed(1) + '%';
      $('tank-volume').textContent = s.tank_volume_l.toFixed(0);
      $('tank-level').textContent = s.tank_level_cm.toFixed(1);
      $('tank-distance').textContent = s.tank_distance_cm.toFixed(1);
    } else {
      $('tank-percent').textContent = '--';
      $('tank-volume').textContent = '--';
    }
    
    // System info
    $('sys-fw').textContent = s.fw_version;
    $('sys-uptime').textContent = formatUptime(s.uptime_s);
    $('sys-heap').textContent = `${Math.round(s.free_heap / 1024)} KB`;
    $('sys-sta-ip').textContent = s.wifi_ip;
    $('sys-sta-ssid').textContent = s.wifi_ssid;
    $('sys-rssi').textContent = `${s.wifi_rssi} dBm`;
    $('sys-ap-ip').textContent = s.ap_ip;
    
    // Auto-fill calibration current
    if (s.tank_valid) {
      $('cal-current-level').textContent = s.tank_level_cm.toFixed(1);
      $('cal-current-volume').textContent = s.tank_volume_l.toFixed(0);
    }
  } catch (err) {
    console.error('Dashboard update failed');
  }
}

setInterval(updateDashboard, 2000);
updateDashboard();

// ===== Konfigurace =====
async function loadConfig() {
  try {
    const c = await fetchJSON('/api/config');
    $('cfg-wifi-ssid').value     = c.wifi_ssid || '';
    $('cfg-wifi-password').value = '';
    $('cfg-mqtt-server').value   = c.mqtt_server || '';
    $('cfg-mqtt-port').value     = c.mqtt_port || 1883;
    $('cfg-mqtt-user').value     = c.mqtt_user || '';
    $('cfg-mqtt-password').value = '';
    $('cfg-mqtt-client-id').value = c.mqtt_client_id || '';
    $('cfg-device-name').value   = c.device_name || '';
    $('cfg-device-id').value     = c.device_id || '';
    $('cfg-tank-full').value     = c.tank_distance_full_cm || 40;
    $('cfg-tank-empty').value    = c.tank_distance_empty_cm || 213;
    $('cfg-tank-max').value      = c.tank_max_volume_l || 5300;
  } catch (err) {
    toast('Nelze načíst konfiguraci', 'error');
  }
}

$('btn-save-config').addEventListener('click', async () => {
  const body = {
    wifi_ssid:     $('cfg-wifi-ssid').value,
    wifi_password: $('cfg-wifi-password').value,
    mqtt_server:   $('cfg-mqtt-server').value,
    mqtt_port:     parseInt($('cfg-mqtt-port').value),
    mqtt_user:     $('cfg-mqtt-user').value,
    mqtt_password: $('cfg-mqtt-password').value,
    mqtt_client_id: $('cfg-mqtt-client-id').value,
    device_name:   $('cfg-device-name').value,
    device_id:     $('cfg-device-id').value,
    tank_distance_full_cm:  parseFloat($('cfg-tank-full').value),
    tank_distance_empty_cm: parseFloat($('cfg-tank-empty').value),
    tank_max_volume_l:      parseFloat($('cfg-tank-max').value),
  };
  
  try {
    await fetchJSON('/api/config', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(body)
    });
    toast('Konfigurace uložena. Doporučen restart.', 'success');
  } catch (err) {
    toast('Uložení selhalo', 'error');
  }
});

$('btn-scan-wifi').addEventListener('click', async () => {
  toast('Skenuji WiFi sítě...');
  try {
    const r = await fetchJSON('/api/wifi/scan');
    const sel = $('cfg-wifi-list');
    sel.innerHTML = '<option value="">-- vybrat ze seznamu --</option>';
    r.networks.sort((a, b) => b.rssi - a.rssi).forEach(n => {
      const opt = document.createElement('option');
      opt.value = n.ssid;
      opt.textContent = `${n.ssid} (${n.rssi} dBm)${n.secure ? ' 🔒' : ''}`;
      sel.appendChild(opt);
    });
    sel.style.display = 'block';
    sel.addEventListener('change', () => {
      if (sel.value) $('cfg-wifi-ssid').value = sel.value;
    });
    toast(`Nalezeno ${r.networks.length} sítí`, 'success');
  } catch (err) {
    toast('Sken selhal', 'error');
  }
});

// ===== Kalibrace =====
async function loadCalibration() {
  try {
    const r = await fetchJSON('/api/calibration');
    const tbody = $('cal-tbody');
    tbody.innerHTML = '';
    
    r.points.forEach((p, i) => {
      const tr = document.createElement('tr');
      tr.innerHTML = `
        <td>${i + 1}</td>
        <td>${p.level_cm.toFixed(1)}</td>
        <td>${p.volume_l.toFixed(0)}</td>
        <td><button class="btn-danger" data-idx="${p.index}">🗑️</button></td>
      `;
      tbody.appendChild(tr);
    });
    
    tbody.querySelectorAll('button').forEach(btn => {
      btn.addEventListener('click', async () => {
        if (!confirm('Smazat tento bod?')) return;
        try {
          await fetchJSON('/api/calibration/remove', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({index: parseInt(btn.dataset.idx)})
          });
          loadCalibration();
          toast('Bod smazán', 'success');
        } catch (err) {
          toast('Mazání selhalo', 'error');
        }
      });
    });
  } catch (err) {
    toast('Nelze načíst kalibraci', 'error');
  }
}

$('btn-use-current').addEventListener('click', () => {
  const cur = $('cal-current-level').textContent;
  if (cur && cur !== '--') {
    $('cal-new-level').value = cur;
  }
});

$('btn-add-point').addEventListener('click', async () => {
  const level = parseFloat($('cal-new-level').value);
  const volume = parseFloat($('cal-new-volume').value);
  
  if (isNaN(level) || isNaN(volume)) {
    toast('Vyplň oba údaje', 'error');
    return;
  }
  
  try {
    await fetchJSON('/api/calibration/add', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({level_cm: level, volume_l: volume})
    });
    $('cal-new-level').value = '';
    $('cal-new-volume').value = '';
    loadCalibration();
    toast('Bod přidán', 'success');
  } catch (err) {
    toast('Přidání selhalo', 'error');
  }
});

$('btn-reset-cal').addEventListener('click', async () => {
  if (!confirm('Smazat všechny body a resetovat na lineární?')) return;
  try {
    await fetchJSON('/api/calibration/reset', {method: 'POST'});
    loadCalibration();
    toast('Kalibrace resetována', 'success');
  } catch (err) {
    toast('Reset selhal', 'error');
  }
});

// ===== OTA Update =====
(function () {
  const dropzone  = $('ota-dropzone');
  const fileInput = $('ota-file');
  const fileLabel = $('ota-file-label');
  const btnUpload = $('btn-ota-upload');
  const progressWrap = $('ota-progress-wrap');
  const progressFill = $('ota-progress-fill');
  const progressText = $('ota-progress-text');
  const statusEl  = $('ota-status');

  let selectedFile = null;

  function setStatus(type, msg) {
    statusEl.className = 'ota-status ' + type;
    statusEl.textContent = msg;
  }

  function clearStatus() {
    statusEl.className = 'ota-status';
    statusEl.textContent = '';
  }

  function selectFile(file) {
    if (!file || !file.name.endsWith('.bin')) {
      toast('Očekává se soubor .bin', 'error');
      return;
    }
    selectedFile = file;
    fileLabel.textContent = `📦 ${file.name}  (${(file.size / 1024).toFixed(1)} KB)`;
    fileLabel.className = 'ready';
    btnUpload.disabled = false;
    progressWrap.style.display = 'none';
    progressFill.style.width = '0%';
    progressText.textContent = '';
    clearStatus();
  }

  dropzone.addEventListener('click', () => fileInput.click());

  dropzone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropzone.classList.add('drag-over');
  });
  dropzone.addEventListener('dragleave', () => dropzone.classList.remove('drag-over'));
  dropzone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropzone.classList.remove('drag-over');
    selectFile(e.dataTransfer.files[0]);
  });

  fileInput.addEventListener('change', () => {
    if (fileInput.files[0]) selectFile(fileInput.files[0]);
  });

  btnUpload.addEventListener('click', () => {
    if (!selectedFile) return;
    if (!confirm(`Nahrát ${selectedFile.name}?\nZařízení se po nahrání automaticky restartuje.`)) return;

    const formData = new FormData();
    formData.append('firmware', selectedFile, selectedFile.name);

    const xhr = new XMLHttpRequest();

    xhr.upload.addEventListener('progress', (e) => {
      if (!e.lengthComputable) return;
      const pct = Math.round((e.loaded / e.total) * 100);
      progressFill.style.width = pct + '%';
      progressText.textContent =
        `${pct}%  —  ${(e.loaded / 1024).toFixed(0)} / ${(e.total / 1024).toFixed(0)} KB`;
    });

    xhr.addEventListener('load', () => {
      if (xhr.status === 200) {
        progressFill.style.width = '100%';
        progressText.textContent = '100%  —  hotovo';
        setStatus('success', '✅ Firmware nahrán. Čekám na restart zařízení…');
        btnUpload.disabled = true;

        // Po rebootu ESP vrátí odpověď a teprve pak restartuje,
        // takže počkáme 4s a pak pollujeme /api/status dokud zařízení neodpoví.
        setTimeout(() => {
          setStatus('info', '🔄 Zařízení se restartuje, čekám…');
          let attempts = 0;
          const poll = setInterval(() => {
            attempts++;
            fetch('/api/status', {cache: 'no-store'})
              .then((r) => {
                if (r.ok) {
                  clearInterval(poll);
                  setStatus('success', '✅ Zařízení je zpět online! Znovu načítám stránku…');
                  setTimeout(() => location.reload(), 1500);
                }
              })
              .catch(() => {
                if (attempts >= 30) {
                  clearInterval(poll);
                  setStatus('error', '⚠️ Zařízení se nevrátilo do 60 s. Zkontroluj ručně.');
                }
              });
          }, 2000);
        }, 4000);
      } else {
        setStatus('error', `❌ Chyba HTTP ${xhr.status}: ${xhr.responseText}`);
        btnUpload.disabled = false;
      }
    });

    xhr.addEventListener('error', () => {
      setStatus('error', '❌ Chyba přenosu. Zkontroluj připojení a zkus znovu.');
      btnUpload.disabled = false;
    });

    progressWrap.style.display = 'block';
    progressFill.style.width = '0%';
    progressText.textContent = '0%';
    setStatus('info', '📤 Nahrávám firmware…');
    btnUpload.disabled = true;

    xhr.open('POST', '/update');
    xhr.send(formData);
  });
}());

// ===== Restart =====
$('btn-restart').addEventListener('click', async () => {
  if (!confirm('Opravdu restartovat ESP?')) return;
  try {
    await fetchJSON('/api/restart', {method: 'POST'});
    toast('Restartuji...', 'success');
    setTimeout(() => location.reload(), 5000);
  } catch (err) {
    toast('Restart selhal', 'error');
  }
});
