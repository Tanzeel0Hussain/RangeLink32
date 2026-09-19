#include <WebServer.h>
#include <Update.h>
#include <vector>
#include "qrcode.h"
#include "web_admin.h"
#include "wifi_manager.h"
#include "router_engine.h"
#include "storage.h"
#include "access_control.h"

namespace {
WebServer server(80);
bool restartPending = false;
unsigned long restartRequestedAt = 0;

String placementLabel(int32_t rssi) {
  if (rssi == -127) return "No upstream signal";
  if (rssi >= -60) return "Excellent position";
  if (rssi >= -70) return "Good position";
  if (rssi >= -80) return "Weak — move closer";
  return "Very weak — move closer";
}

void scheduleRestart() {
  restartPending = true;
  restartRequestedAt = millis();
}

bool requireAdmin() {
  const String user = getAdminUser();
  const String pass = getAdminPassword();

  if (server.authenticate(user.c_str(), pass.c_str())) {
    return true;
  }

  server.requestAuthentication();
  return false;
}

String renderPage() {
  const SystemState s = getSystemState();

  String html;
  html.reserve(14000);

  html += R"HTML(<!doctype html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RangeLink32 Admin</title>
<style>
:root{--bg:#07111d;--panel:#0d1d2d;--line:#20354a;--text:#eef7ff;--muted:#8ea7bd;--accent:#49d3ff;--ok:#42d392;--warn:#f6c453;--danger:#fb7185}
html[data-theme="light"]{--bg:#edf5fb;--panel:#fff;--line:#c9d9e6;--text:#102334;--muted:#5c7183;--accent:#0ca7d4;--ok:#168c61;--warn:#9a6900;--danger:#b72e49}
*{box-sizing:border-box}body{margin:0;background:linear-gradient(155deg,#06101b,#0b1d2e);font-family:system-ui,sans-serif;color:var(--text)}
.wrap{max-width:1100px;margin:auto;padding:20px}.topbar{display:flex;align-items:center;justify-content:space-between;gap:12px}.brand h1{margin:0;font-size:1.45rem}.brand p{margin:5px 0 0;color:var(--muted)}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-top:18px}
.card{background:rgba(13,29,45,.96);border:1px solid var(--line);border-radius:16px;padding:16px}
.k{font-size:.72rem;text-transform:uppercase;color:var(--muted);letter-spacing:.08em}.v{font-size:1.25rem;font-weight:800;margin-top:7px}
section{margin-top:14px}.btn{display:inline-block;border:0;border-radius:10px;padding:10px 13px;background:var(--accent);color:#041019;font-weight:800;cursor:pointer;text-decoration:none}
.btn.secondary{background:#132b40;color:var(--text);border:1px solid var(--line)}.btn.danger{background:#5a2030;color:#ffd5dd}
input,select{width:100%;background:#091623;border:1px solid var(--line);color:var(--text);border-radius:10px;padding:11px;margin:6px 0}
.row{display:flex;gap:12px;align-items:flex-start;justify-content:space-between;border-bottom:1px solid var(--line);padding:13px 0}.row:last-child{border-bottom:0}
.meta{color:var(--muted);font-size:.78rem;margin-top:4px}.actions{display:flex;gap:7px;flex-wrap:wrap}
.limit-row{display:grid;grid-template-columns:1fr 92px;gap:8px;align-items:center}.client-tools{display:grid;grid-template-columns:repeat(2,minmax(180px,1fr));gap:8px;margin-top:10px}.client-tools form{border:1px solid var(--line);border-radius:12px;padding:9px;background:#091623}
.client-tools label{display:block;color:var(--muted);font-size:.72rem;margin-top:4px}.client-tools .btn{width:100%;margin-top:5px}
small{color:var(--muted);line-height:1.5}@media(max-width:850px){.grid{grid-template-columns:1fr 1fr}}@media(max-width:430px){.grid{grid-template-columns:1fr}.row{align-items:flex-start;flex-direction:column}}
</style></head><body><main class="wrap">
<div class="topbar"><div class="brand"><h1>RangeLink32</h1><p>Smart ESP32 Wi-Fi Extender & Managed Gateway</p></div><button class="btn secondary" type="button" onclick="toggleTheme()">Theme</button></div>
)HTML";

  html += "<div class='grid'>";
  html += "<div class='card'><div class='k'>Upstream</div><div class='v'>" +
          String(s.upstreamConnected ? "Connected" : "Disconnected") + "</div></div>";
  html += "<div class='card'><div class='k'>Network</div><div class='v'>" +
          String(s.upstreamSsid.length() ? s.upstreamSsid : "—") + "</div></div>";
  html += "<div class='card'><div class='k'>Signal</div><div class='v'>" +
          String(s.upstreamRssi) + " dBm</div></div>";
  html += "<div class='card'><div class='k'>Clients</div><div class='v'>" +
          String(s.connectedClients) + "</div></div>";
  html += "<div class='card'><div class='k'>Internet</div><div class='v'>" +
          String(s.internetReachable ? "Online" : "Offline") + "</div></div>";
  html += "<div class='card'><div class='k'>Internet Forwarding</div><div class='v'>" +
          String(routerEngineReady() ? "NAPT On" : "Off") + "</div></div></div>";

  html += R"HTML(
<section class="card">
<h3>Nearby Wi-Fi</h3>
<div class="actions"><button class="btn secondary" onclick="scanNow()">Scan now</button></div>
<div id="networks"><small>Loading nearby networks…</small></div>
</section>

<section class="card">
<h3>Saved Networks & Failover</h3>
<div id="profiles"><small>Loading saved profiles…</small></div>
<p><small>Saved networks are retried automatically. After repeated failures RangeLink32 scans for another available saved profile and fails over automatically.</small></p>
</section>

<section class="card">
<h3>Connected & Known Devices</h3>
<div class="actions">
<form method="post" action="/access/mode">
<input type="hidden" name="mode" value="all">
<button class="btn secondary" type="submit">Allow all devices</button>
</form>
<form method="post" action="/access/mode">
<input type="hidden" name="mode" value="allowlist">
<button class="btn secondary" type="submit">Allowlisted devices only</button>
</form>
</div>
<p><small>Current access mode: <b>)HTML" +
          String(getAccessMode() == AccessMode::AllowlistOnly ? "Allowlisted only" : "Allow all") +
          R"HTML(</b>. Devices may remain connected to the RangeLink32 Wi-Fi while Internet forwarding is blocked. Local admin access and DHCP/ARP stay available.</small></p>
<div id="clients"><small>Loading client inventory…</small></div>
</section>

<section class="card">
<h3>Channel Analysis</h3>
<div id="channels"><small>Loading channel congestion…</small></div>
<p><small>Lower congestion values are generally better. RangeLink32 uses the upstream network's channel because ESP32U has a single 2.4 GHz radio.</small></p>
</section>

<section class="card">
<h3>Connect a Network</h3>
<form method="post" action="/connect">
<input id="ssid" name="ssid" placeholder="Wi-Fi name (SSID)" required>
<input name="password" type="password" placeholder="Wi-Fi password" minlength="8" required>
<button class="btn" type="submit">Connect & Save</button>
</form>
</section>

<section class="card">
<h3>Smart Placement Assistant</h3>
<p><b>)HTML" + placementLabel(s.upstreamRssi) + R"HTML(</b></p>
<p><small>Current upstream signal: )HTML" + String(s.upstreamRssi) + R"HTML( dBm. Place RangeLink32 where it still receives a stable router signal while remaining closer to the area you want to cover.</small></p>
</section>

<section class="card">
<h3>RangeLink32 Hotspot Settings</h3>
<form method="post" action="/settings/ap">
<input name="ssid" value=")HTML" + getApSsid() + R"HTML(" placeholder="RangeLink32 Wi-Fi name" required>
<input name="password" type="password" placeholder="New hotspot password (8+ characters)" minlength="8" required>
<button class="btn" type="submit">Save & Restart</button>
</form>
<p><small>Changing the hotspot settings restarts the ESP32. Reconnect using the new SSID/password and open <b>192.168.50.1</b>.</small></p>
</section>

<section class="card">
<h3>Wi-Fi QR Code</h3>
<p><small>Scan this QR code to join the RangeLink32 hotspot. It is generated locally by the ESP32; the Wi-Fi password is not sent to an external QR service.</small></p>
<img src="/qr.svg" alt="RangeLink32 Wi-Fi QR code" style="max-width:280px;width:100%;background:white;padding:10px;border-radius:12px">
</section>

<section class="card">
<h3>DNS Settings</h3>
<form method="post" action="/settings/dns">
<label><small>Custom downstream DNS IPv4 address. Leave blank to use 1.1.1.1.</small></label>
<input name="dns" value=")HTML" + getCustomDns() + R"HTML(" placeholder="e.g. 1.1.1.1">
<button class="btn" type="submit">Save DNS & Restart</button>
</form>
</section>

<section class="card">
<h3>Time & Scheduling</h3>
<form method="post" action="/settings/timezone">
<label><small>Timezone offset from UTC in minutes (Pakistan = 300)</small></label>
<input name="minutes" type="number" min="-720" max="840" value=")HTML" + String(getTimezoneOffsetMinutes()) + R"HTML(" required>
<button class="btn" type="submit">Save Timezone & Restart</button>
</form>
</section>

<section class="card">
<h3>Admin Login Settings</h3>
<form method="post" action="/settings/admin">
<input name="username" value=")HTML" + getAdminUser() + R"HTML(" placeholder="Admin username" required>
<input name="password" type="password" placeholder="New admin password (8+ characters)" minlength="8" required>
<button class="btn" type="submit">Change Admin Login</button>
</form>
</section>

<section class="card">
<h3>Configuration Backup</h3>
<div class="actions"><a class="btn secondary" href="/backup">Download Safe Backup</a></div>
<p><small>The backup includes non-secret settings and device policies. Wi-Fi passwords and the admin password are intentionally excluded.</small></p>
<form method="post" action="/restore">
<textarea name="backup" rows="8" style="width:100%;background:#091623;border:1px solid var(--line);color:var(--text);border-radius:10px;padding:11px" placeholder="Paste a RangeLink32 backup here"></textarea>
<button class="btn" type="submit">Restore & Restart</button>
</form>
</section>

<section class="card">
<h3>Event Log</h3>
<div class="actions"><button class="btn secondary" onclick="loadLogs()">Refresh logs</button>
<form method="post" action="/logs/clear"><button class="btn danger">Clear logs</button></form></div>
<div id="logs"><small>Loading events…</small></div>
</section>

<section class="card">
<h3>OTA Firmware Update</h3>
<form method="post" action="/update" enctype="multipart/form-data">
<input type="file" name="firmware" accept=".bin,application/octet-stream" required>
<button class="btn" type="submit">Upload Firmware & Restart</button>
</form>
<p><small>Upload only a RangeLink32 firmware <code>.bin</code> built for your ESP32 board. Keep the device powered during the update.</small></p>
</section>

<section class="card">
<h3>System & Recovery</h3>
<div class="row"><div><b>Management IP</b><div class="meta">192.168.50.1</div></div></div>
<div class="row"><div><b>Chip</b><div class="meta">)HTML" + String(ESP.getChipModel()) + R"HTML( · Free heap )HTML" + String(ESP.getFreeHeap()/1024) + R"HTML( KB</div></div></div>
<div class="row"><div><b>Uptime</b><div class="meta">)HTML" + String(millis()/1000) + R"HTML( seconds</div></div></div>
<div class="actions">
<a class="btn secondary" href="/reconnect">Reconnect upstream</a>
<form method="post" action="/system/restart"><button class="btn secondary">Restart ESP32</button></form>
<form method="post" action="/system/factory-reset" onsubmit="return confirm('Erase RangeLink32 settings and restart?')"><button class="btn danger">Factory Reset</button></form>
</div>
</section>

<script>
const esc=s=>String(s).replace(/[&<>"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m]));

const savedTheme=localStorage.getItem('rangelink32-theme');
if(savedTheme) document.documentElement.dataset.theme=savedTheme;
function toggleTheme(){
  const next=document.documentElement.dataset.theme==='light'?'dark':'light';
  document.documentElement.dataset.theme=next;
  localStorage.setItem('rangelink32-theme',next);
}

async function loadNetworks(){
  const data=await (await fetch('/api/networks')).json();
  const box=document.getElementById('networks');
  box.innerHTML=data.length?data.map(n=>`
    <div class="row">
      <div><b>${esc(n.ssid||'<hidden>')}</b><div class="meta">${n.rssi} dBm · CH ${n.channel} · ${n.secure?'Secured':'Open'}</div></div>
      <button class="btn secondary pick-network" type="button" data-ssid="${esc(n.ssid)}">Select</button>
    </div>`).join(''):'<small>No networks found.</small>';

  box.querySelectorAll('.pick-network').forEach(button=>{
    button.addEventListener('click',()=>{
      pickSsid(button.dataset.ssid||'');
    });
  });
}

function bytes(v){
  if(!v) return '0 B';
  const units=['B','KB','MB','GB'];
  let n=Number(v),i=0;
  while(n>=1024&&i<units.length-1){n/=1024;i++;}
  return n.toFixed(i?1:0)+' '+units[i];
}

async function loadClients(){
  const data=await (await fetch('/api/clients')).json();
  const box=document.getElementById('clients');

  box.innerHTML=data.length?data.map(c=>{
    const total=c.rxBytes+c.txBytes;
    const daily=c.dailyRxBytes+c.dailyTxBytes;
    const monthly=c.monthlyRxBytes+c.monthlyTxBytes;
    const dailyQuota=c.dailyQuotaBytes?bytes(c.dailyQuotaBytes):'Unlimited';
    const monthlyQuota=c.monthlyQuotaBytes?bytes(c.monthlyQuotaBytes):'Unlimited';
    const speed=c.bandwidthKbps?c.bandwidthKbps+' kbps':'Unlimited';

    return `
    <div class="row">
      <div style="flex:1;min-width:0">
        <b>${esc(c.hostname||c.mac)}</b>
        <div class="meta">${esc(c.mac)} · ${c.connected?'Connected':'Previously seen'} · IP ${esc(c.ip||'—')}${c.connected?' · '+c.rssi+' dBm':''}</div>
        <div class="meta">Internet: <b>${c.allowed?'Allowed':'Blocked'}</b>${c.guest?' · Guest access active':''}</div>
        <div class="meta">Today: ${bytes(daily)} / ${dailyQuota} · This month: ${bytes(monthly)} / ${monthlyQuota}</div>
        <div class="meta">Total: ${bytes(total)} · Speed cap: ${speed}</div>
        <div class="meta">Schedule: ${c.scheduleEnabled?(c.scheduleStart+':00–'+c.scheduleEnd+':00'):'Always'}</div>

        <div class="client-tools">
          <form method="post" action="/client/name">
            <input type="hidden" name="mac" value="${esc(c.mac)}">
            <label>Device name</label>
            <input name="name" maxlength="32" value="${esc(c.hostname||'')}" placeholder="e.g. My Laptop">
            <button class="btn secondary">Save Name</button>
          </form>

          <form method="post" action="/client/limits">
            <input type="hidden" name="mac" value="${esc(c.mac)}">

            <label>Daily data limit (0 = unlimited)</label>
            <div class="limit-row">
              <input name="dailyLimit" type="number" min="0" step="0.1" value="${((c.dailyQuotaBytes||0)/1048576).toFixed(1)}">
              <select name="dailyUnit"><option value="MB">MB</option><option value="GB">GB</option></select>
            </div>

            <label>Monthly data limit (0 = unlimited)</label>
            <div class="limit-row">
              <input name="monthlyLimit" type="number" min="0" step="0.1" value="${((c.monthlyQuotaBytes||0)/1048576).toFixed(1)}">
              <select name="monthlyUnit"><option value="MB">MB</option><option value="GB">GB</option></select>
            </div>

            <label>Speed limit (0 = unlimited)</label>
            <div class="limit-row">
              <input name="speedLimit" type="number" min="0" step="0.1" value="${c.bandwidthKbps||0}">
              <select name="speedUnit"><option value="Kbps">Kbps</option><option value="Mbps">Mbps</option></select>
            </div>

            <button class="btn secondary">Save Limits</button>
          </form>

          <form method="post" action="/client/schedule">
            <input type="hidden" name="mac" value="${esc(c.mac)}">
            <label><input style="width:auto" type="checkbox" name="enabled" value="1" ${c.scheduleEnabled?'checked':''}> Enable daily schedule</label>
            <label>Start hour (0–23)</label>
            <input name="start" type="number" min="0" max="23" value="${c.scheduleStart}">
            <label>End hour (1–24)</label>
            <input name="end" type="number" min="0" max="24" value="${c.scheduleEnd}">
            <button class="btn secondary">Save Schedule</button>
          </form>

          <form method="post" action="/client/guest">
            <input type="hidden" name="mac" value="${esc(c.mac)}">
            <label>Temporary guest Internet (minutes)</label>
            <input name="minutes" type="number" min="1" max="10080" value="60">
            <button class="btn secondary">Grant Guest Access</button>
          </form>
        </div>
      </div>

      <div class="actions">
        <form method="post" action="/client/approve">
          <input type="hidden" name="mac" value="${esc(c.mac)}">
          <input type="hidden" name="approved" value="${c.approved?'0':'1'}">
          <button class="btn secondary">${c.approved?'Remove approval':'Approve'}</button>
        </form>

        <form method="post" action="/client/block">
          <input type="hidden" name="mac" value="${esc(c.mac)}">
          <input type="hidden" name="blocked" value="${c.blocked?'0':'1'}">
          <button class="btn ${c.blocked?'secondary':'danger'}">${c.blocked?'Unblock Internet':'Block Internet'}</button>
        </form>

        <form method="post" action="/client/reset-usage">
          <input type="hidden" name="mac" value="${esc(c.mac)}">
          <input type="hidden" name="total" value="0">
          <button class="btn secondary">Reset Today</button>
        </form>

        <form method="post" action="/client/reset-monthly">
          <input type="hidden" name="mac" value="${esc(c.mac)}">
          <button class="btn secondary">Reset Month</button>
        </form>

        <form method="post" action="/client/reset-usage" onsubmit="return confirm('Reset all saved usage for this device?')">
          <input type="hidden" name="mac" value="${esc(c.mac)}">
          <input type="hidden" name="total" value="1">
          <button class="btn danger">Reset All Usage</button>
        </form>
      </div>
    </div>`;
  }).join(''):'<small>No devices have connected yet.</small>';
}

async function loadLogs(){
  const data=await (await fetch('/api/logs')).json();
  const box=document.getElementById('logs');
  box.innerHTML=data.length?data.slice().reverse().map(e=>`
    <div class="row">
      <div><b>${esc(e.type)}</b><div class="meta">Boot ${e.boot} · +${e.seconds}s</div><div class="meta">${esc(e.message)}</div></div>
    </div>`).join(''):'<small>No events recorded yet.</small>';
}

async function loadProfiles(){
  const data=await (await fetch('/api/profiles')).json();
  const box=document.getElementById('profiles');
  box.innerHTML=data.length?data.map(p=>`
    <div class="row">
      <div>
        <b>${esc(p.ssid)}</b>
        <div class="meta">Priority ${p.priority}${p.current?' · Connected':''} · Last RSSI ${p.lastRssi} dBm</div>
        <div class="meta">Usage: ↓ ${bytes(p.rxBytes)} · ↑ ${bytes(p.txBytes)} · Total ${bytes(p.rxBytes+p.txBytes)}</div>
      </div>
      <div class="actions">
        <form method="post" action="/profile/connect"><input type="hidden" name="ssid" value="${esc(p.ssid)}"><button class="btn secondary">Connect</button></form>
        <form method="post" action="/profile/reveal" target="_blank">
          <input type="hidden" name="ssid" value="${esc(p.ssid)}">
          <input name="admin_password" type="password" placeholder="Admin password" required>
          <button class="btn secondary">Reveal Password</button>
        </form>
        <form method="post" action="/profile/forget"><input type="hidden" name="ssid" value="${esc(p.ssid)}"><button class="btn danger">Forget</button></form>
      </div>
    </div>`).join(''):'<small>No saved networks yet.</small>';
}

async function loadChannels(){
  const data=await (await fetch('/api/channels')).json();
  const box=document.getElementById('channels');
  box.innerHTML=data.map(c=>{
    const level=c.congestion<=3?'Low':c.congestion<=8?'Medium':'High';
    return `<div class="row"><div><b>Channel ${c.channel}</b><div class="meta">${c.networks} direct network(s) · Congestion ${c.congestion} · ${level}</div></div></div>`;
  }).join('');
}

function pickSsid(ssid){
  document.getElementById('ssid').value=ssid;
  document.getElementById('ssid').scrollIntoView({behavior:'smooth',block:'center'});
}

async function scanNow(){
  await fetch('/scan',{method:'POST'});
  await loadNetworks();
  await loadChannels();
}

loadNetworks();
loadProfiles();
loadChannels();
loadClients();
loadLogs();
setInterval(loadClients,5000);
</script>
</main></body></html>)HTML";

  return html;
}
}

void webAdminBegin() {
  server.on("/", HTTP_GET, []() {
    if (!requireAdmin()) return;
    server.send(200, "text/html; charset=utf-8", renderPage());
  });

  server.on("/api/networks", HTTP_GET, []() {
    if (!requireAdmin()) return;
    server.send(200, "application/json", getWifiScanJson());
  });

  server.on("/api/profiles", HTTP_GET, []() {
    if (!requireAdmin()) return;
    server.send(200, "application/json", getSavedProfilesJson());
  });

  server.on("/api/channels", HTTP_GET, []() {
    if (!requireAdmin()) return;
    server.send(200, "application/json", getChannelAnalysisJson());
  });

  server.on("/api/clients", HTTP_GET, []() {
    if (!requireAdmin()) return;
    server.send(200, "application/json", getClientTableJson());
  });

  server.on("/api/logs", HTTP_GET, []() {
    if (!requireAdmin()) return;
    server.send(200, "application/json", getEventLogJson());
  });

  server.on("/scan", HTTP_POST, []() {
    if (!requireAdmin()) return;
    requestWifiScan();
    server.send(200, "application/json", getWifiScanJson());
  });

  server.on("/connect", HTTP_POST, []() {
    if (!requireAdmin()) return;

    const bool ok = connectUpstream(
      server.arg("ssid"),
      server.arg("password")
    );

    server.sendHeader("Location", ok ? "/" : "/?error=connect");
    server.send(303);
  });

  server.on("/profile/connect", HTTP_POST, []() {
    if (!requireAdmin()) return;
    const bool ok = connectSavedProfile(server.arg("ssid"));
    server.sendHeader("Location", ok ? "/" : "/?error=profile");
    server.send(303);
  });

  server.on("/profile/reveal", HTTP_POST, []() {
    if (!requireAdmin()) return;

    if (
      server.arg("admin_password") !=
      getAdminPassword()
    ) {
      server.send(
        403,
        "text/plain",
        "Admin password verification failed."
      );
      return;
    }

    String secret;
    if (
      !getWifiProfileSecret(
        server.arg("ssid"),
        secret
      )
    ) {
      server.send(
        404,
        "text/plain",
        "Saved network not found."
      );
      return;
    }

    String safeSsid = server.arg("ssid");
    safeSsid.replace("&", "&amp;");
    safeSsid.replace("<", "&lt;");
    safeSsid.replace(">", "&gt;");

    String safeSecret = secret;
    safeSecret.replace("&", "&amp;");
    safeSecret.replace("<", "&lt;");
    safeSecret.replace(">", "&gt;");

    server.send(
      200,
      "text/html; charset=utf-8",
      "<!doctype html><meta name='viewport' content='width=device-width'><title>RangeLink32 Credential</title><body style='font-family:system-ui;padding:24px;background:#07111d;color:#eef7ff'><h2>" +
      safeSsid +
      "</h2><p>Saved password:</p><p style='font-size:1.25rem'><code>" +
      safeSecret +
      "</code></p><p>Close this page when finished.</p></body>"
    );
  });

  server.on("/profile/forget", HTTP_POST, []() {
    if (!requireAdmin()) return;
    forgetSavedProfile(server.arg("ssid"));
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/access/mode", HTTP_POST, []() {
    if (!requireAdmin()) return;

    setAccessMode(
      server.arg("mode") == "allowlist"
        ? AccessMode::AllowlistOnly
        : AccessMode::AllowAll
    );

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/client/approve", HTTP_POST, []() {
    if (!requireAdmin()) return;

    setClientApproval(
      server.arg("mac"),
      server.arg("approved") == "1"
    );

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/client/block", HTTP_POST, []() {
    if (!requireAdmin()) return;

    setClientBlocked(
      server.arg("mac"),
      server.arg("blocked") == "1"
    );

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/client/name", HTTP_POST, []() {
    if (!requireAdmin()) return;

    setClientName(
      server.arg("mac"),
      server.arg("name")
    );

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/client/limits", HTTP_POST, []() {
    if (!requireAdmin()) return;

    const double dailyValue =
      server.arg("dailyLimit").toFloat();
    const double monthlyValue =
      server.arg("monthlyLimit").toFloat();
    const double speedValue =
      server.arg("speedLimit").toFloat();

    const uint64_t dailyMultiplier =
      server.arg("dailyUnit") == "GB"
        ? 1024ULL * 1024ULL * 1024ULL
        : 1024ULL * 1024ULL;

    const uint64_t monthlyMultiplier =
      server.arg("monthlyUnit") == "GB"
        ? 1024ULL * 1024ULL * 1024ULL
        : 1024ULL * 1024ULL;

    const double speedMultiplier =
      server.arg("speedUnit") == "Mbps"
        ? 1000.0
        : 1.0;

    const uint64_t dailyBytes =
      dailyValue <= 0
        ? 0
        : static_cast<uint64_t>(
            dailyValue * dailyMultiplier
          );

    const uint64_t monthlyBytes =
      monthlyValue <= 0
        ? 0
        : static_cast<uint64_t>(
            monthlyValue * monthlyMultiplier
          );

    const uint32_t kbps =
      speedValue <= 0
        ? 0
        : static_cast<uint32_t>(
            speedValue * speedMultiplier
          );

    setClientLimits(
      server.arg("mac"),
      dailyBytes,
      monthlyBytes,
      kbps
    );

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/client/schedule", HTTP_POST, []() {
    if (!requireAdmin()) return;

    const bool enabled =
      server.hasArg("enabled") &&
      server.arg("enabled") == "1";

    const int start =
      server.arg("start").toInt();

    const int end =
      server.arg("end").toInt();

    if (
      start < 0 || start > 23 ||
      end < 0 || end > 24
    ) {
      server.send(
        400,
        "text/plain",
        "Invalid schedule."
      );
      return;
    }

    setClientSchedule(
      server.arg("mac"),
      enabled,
      static_cast<uint8_t>(start),
      static_cast<uint8_t>(end)
    );

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/client/guest", HTTP_POST, []() {
    if (!requireAdmin()) return;

    const long minutes =
      server.arg("minutes").toInt();

    if (
      minutes < 1 ||
      minutes > 10080 ||
      !grantGuestAccess(
        server.arg("mac"),
        static_cast<uint32_t>(minutes)
      )
    ) {
      server.send(
        400,
        "text/plain",
        "Guest access requires synchronized Internet time and a valid duration."
      );
      return;
    }

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/client/reset-monthly", HTTP_POST, []() {
    if (!requireAdmin()) return;

    resetClientMonthlyUsage(
      server.arg("mac")
    );

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/client/reset-usage", HTTP_POST, []() {
    if (!requireAdmin()) return;

    resetClientUsage(
      server.arg("mac"),
      server.arg("total") == "1"
    );

    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/backup", HTTP_GET, []() {
    if (!requireAdmin()) return;

    server.sendHeader(
      "Content-Disposition",
      "attachment; filename=RangeLink32-backup.txt"
    );

    server.send(
      200,
      "text/plain",
      exportSafeSettings()
    );
  });

  server.on("/restore", HTTP_POST, []() {
    if (!requireAdmin()) return;

    if (!importSafeSettings(server.arg("backup"))) {
      server.send(
        400,
        "text/plain",
        "Invalid RangeLink32 backup."
      );
      return;
    }

    appendEventLog(
      "settings",
      "Configuration backup restored"
    );

    server.send(
      200,
      "text/html",
      "<h2>Backup restored.</h2><p>RangeLink32 is restarting…</p>"
    );

    scheduleRestart();
  });

  server.on("/logs/clear", HTTP_POST, []() {
    if (!requireAdmin()) return;
    clearEventLogs();
    appendEventLog("system", "Event log cleared");
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/settings/ap", HTTP_POST, []() {
    if (!requireAdmin()) return;

    const bool ok = setApCredentials(
      server.arg("ssid"),
      server.arg("password")
    );

    if (!ok) {
      server.send(400, "text/plain", "Invalid hotspot settings. Use 8+ characters and choose a password different from the admin password.");
      return;
    }

    appendEventLog("settings", "RangeLink32 hotspot settings changed");
    server.send(
      200,
      "text/html",
      "<h2>RangeLink32 settings saved.</h2><p>The ESP32 is restarting. Reconnect to the new Wi-Fi and open 192.168.50.1.</p>"
    );
    scheduleRestart();
  });

  server.on("/settings/dns", HTTP_POST, []() {
    if (!requireAdmin()) return;

    if (!setCustomDns(server.arg("dns"))) {
      server.send(
        400,
        "text/plain",
        "DNS must be a valid IPv4 address or blank."
      );
      return;
    }

    appendEventLog(
      "settings",
      "Custom DNS changed"
    );

    server.send(
      200,
      "text/html",
      "<h2>DNS saved.</h2><p>RangeLink32 is restarting…</p>"
    );

    scheduleRestart();
  });

  server.on("/settings/timezone", HTTP_POST, []() {
    if (!requireAdmin()) return;

    const int minutes =
      server.arg("minutes").toInt();

    if (minutes < -720 || minutes > 840) {
      server.send(
        400,
        "text/plain",
        "Timezone offset must be between -720 and 840 minutes."
      );
      return;
    }

    setTimezoneOffsetMinutes(minutes);
    appendEventLog(
      "settings",
      "Timezone changed to UTC offset " +
      String(minutes) + " minutes"
    );

    server.send(
      200,
      "text/html",
      "<h2>Timezone saved.</h2><p>RangeLink32 is restarting…</p>"
    );

    scheduleRestart();
  });

  server.on("/settings/admin", HTTP_POST, []() {
    if (!requireAdmin()) return;

    const bool ok = setAdminCredentials(
      server.arg("username"),
      server.arg("password")
    );

    if (!ok) {
      server.send(400, "text/plain", "Invalid admin settings. Use 8+ characters and choose a password different from the RangeLink32 Wi-Fi password.");
      return;
    }

    appendEventLog("settings", "Admin login credentials changed");
    server.send(
      200,
      "text/html",
      "<h2>Admin login updated.</h2><p>RangeLink32 is restarting. Sign in with the new credentials.</p>"
    );
    scheduleRestart();
  });

  server.on(
    "/update",
    HTTP_POST,
    []() {
      if (!requireAdmin()) return;

      const bool ok = !Update.hasError();
      if (ok) {
        appendEventLog("ota", "Firmware update completed");
      } else {
        appendEventLog("ota", "Firmware update failed");
      }

      server.send(
        ok ? 200 : 500,
        "text/html",
        ok
          ? "<h2>Firmware update complete.</h2><p>RangeLink32 is restarting…</p>"
          : "<h2>Firmware update failed.</h2><p>The current firmware remains active.</p>"
      );

      if (ok) scheduleRestart();
    },
    []() {
      if (!server.authenticate(
            getAdminUser().c_str(),
            getAdminPassword().c_str()
          )) {
        return;
      }

      HTTPUpload& upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        Update.begin(UPDATE_SIZE_UNKNOWN);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!Update.hasError()) {
          Update.write(upload.buf, upload.currentSize);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.hasError()) {
          Update.end(true);
        }
      } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
      }
    }
  );

  server.on("/system/restart", HTTP_POST, []() {
    if (!requireAdmin()) return;
    appendEventLog("system", "Manual restart requested");
    server.send(200, "text/html", "<h2>RangeLink32 is restarting…</h2>");
    scheduleRestart();
  });

  server.on("/system/factory-reset", HTTP_POST, []() {
    if (!requireAdmin()) return;
    factoryResetStorage();
    server.send(
      200,
      "text/html",
      "<h2>Factory reset complete.</h2><p>RangeLink32 is restarting with default development settings.</p>"
    );
    scheduleRestart();
  });

  server.on("/reconnect", HTTP_GET, []() {
    if (!requireAdmin()) return;
    reconnectUpstream();
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/qr.svg", HTTP_GET, []() {
    if (!requireAdmin()) return;

    String ssid = getApSsid();
    String pass = getApPassword();

    auto escapeWifi = [](String value) {
      value.replace("\\", "\\\\");
      value.replace(";", "\\;");
      value.replace(",", "\\,");
      value.replace(":", "\\:");
      value.replace("\"", "\\\"");
      return value;
    };

    const String payload =
      "WIFI:T:WPA;S:" +
      escapeWifi(ssid) +
      ";P:" +
      escapeWifi(pass) +
      ";;";

    constexpr uint8_t QR_VERSION = 6;
    std::vector<uint8_t> buffer(
      qrcode_getBufferSize(QR_VERSION)
    );

    QRCode qr;
    qrcode_initText(
      &qr,
      buffer.data(),
      QR_VERSION,
      0,
      payload.c_str()
    );

    const int quiet = 4;
    const int viewSize = qr.size + quiet * 2;

    String svg;
    svg.reserve(12000);

    svg =
      "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 " +
      String(viewSize) +
      " " +
      String(viewSize) +
      "' shape-rendering='crispEdges'><rect width='100%' height='100%' fill='white'/>";

    for (uint8_t y = 0; y < qr.size; ++y) {
      for (uint8_t x = 0; x < qr.size; ++x) {
        if (qrcode_getModule(&qr, x, y)) {
          svg +=
            "<rect x='" +
            String(x + quiet) +
            "' y='" +
            String(y + quiet) +
            "' width='1' height='1' fill='black'/>";
        }
      }
    }

    svg += "</svg>";

    server.send(
      200,
      "image/svg+xml",
      svg
    );
  });

  server.on("/health", HTTP_GET, []() {
    server.send(
      200,
      "application/json",
      "{\"status\":\"ok\",\"project\":\"RangeLink32\"}"
    );
  });

  server.begin();
}

void webAdminLoop() {
  server.handleClient();

  if (restartPending && millis() - restartRequestedAt >= 1000) {
    ESP.restart();
  }
}
