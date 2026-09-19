#include <WebServer.h>
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
*{box-sizing:border-box}body{margin:0;background:linear-gradient(155deg,#06101b,#0b1d2e);font-family:system-ui,sans-serif;color:var(--text)}
.wrap{max-width:1100px;margin:auto;padding:20px}.brand h1{margin:0;font-size:1.45rem}.brand p{margin:5px 0 0;color:var(--muted)}
.grid{display:grid;grid-template-columns:repeat(5,1fr);gap:12px;margin-top:18px}
.card{background:rgba(13,29,45,.96);border:1px solid var(--line);border-radius:16px;padding:16px}
.k{font-size:.72rem;text-transform:uppercase;color:var(--muted);letter-spacing:.08em}.v{font-size:1.25rem;font-weight:800;margin-top:7px}
section{margin-top:14px}.btn{display:inline-block;border:0;border-radius:10px;padding:10px 13px;background:var(--accent);color:#041019;font-weight:800;cursor:pointer;text-decoration:none}
.btn.secondary{background:#132b40;color:var(--text);border:1px solid var(--line)}.btn.danger{background:#5a2030;color:#ffd5dd}
input{width:100%;background:#091623;border:1px solid var(--line);color:var(--text);border-radius:10px;padding:11px;margin:6px 0}
.row{display:flex;gap:10px;align-items:center;justify-content:space-between;border-bottom:1px solid var(--line);padding:11px 0}.row:last-child{border-bottom:0}
.meta{color:var(--muted);font-size:.78rem;margin-top:4px}.actions{display:flex;gap:7px;flex-wrap:wrap}
small{color:var(--muted);line-height:1.5}@media(max-width:850px){.grid{grid-template-columns:1fr 1fr}}@media(max-width:430px){.grid{grid-template-columns:1fr}.row{align-items:flex-start;flex-direction:column}}
</style></head><body><main class="wrap">
<div class="brand"><h1>RangeLink32</h1><p>Smart ESP32 Wi-Fi Extender & Managed Gateway</p></div>
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
          R"HTML(</b>. In allowlist mode, devices that are not approved are disconnected from the RangeLink32 AP. Per-device NAPT-only blocking without disconnecting is a later routing milestone.</small></p>
<div id="clients"><small>Loading client inventory…</small></div>
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
<h3>Admin Login Settings</h3>
<form method="post" action="/settings/admin">
<input name="username" value=")HTML" + getAdminUser() + R"HTML(" placeholder="Admin username" required>
<input name="password" type="password" placeholder="New admin password (8+ characters)" minlength="8" required>
<button class="btn" type="submit">Change Admin Login</button>
</form>
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

async function loadNetworks(){
  const data=await (await fetch('/api/networks')).json();
  const box=document.getElementById('networks');
  box.innerHTML=data.length?data.map(n=>`
    <div class="row">
      <div><b>${esc(n.ssid||'<hidden>')}</b><div class="meta">${n.rssi} dBm · CH ${n.channel} · ${n.secure?'Secured':'Open'}</div></div>
      <button class="btn secondary" onclick='pickSsid(${JSON.stringify(n.ssid)})'>Select</button>
    </div>`).join(''):'<small>No networks found.</small>';
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
  box.innerHTML=data.length?data.map(c=>`
    <div class="row">
      <div>
        <b>${esc(c.mac)}</b>
        <div class="meta">${c.connected?'Connected':'Previously seen'} · IP ${esc(c.ip||'—')}${c.connected?' · '+c.rssi+' dBm':''}</div>
        <div class="meta">Internet policy: ${c.allowed?'Allowed':'Blocked'} · Usage counters: ${bytes(c.rxBytes+c.txBytes)}</div>
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
          <button class="btn ${c.blocked?'secondary':'danger'}">${c.blocked?'Unblock':'Block'}</button>
        </form>
      </div>
    </div>`).join(''):'<small>No devices have connected yet.</small>';
}

async function loadProfiles(){
  const data=await (await fetch('/api/profiles')).json();
  const box=document.getElementById('profiles');
  box.innerHTML=data.length?data.map(p=>`
    <div class="row">
      <div><b>${esc(p.ssid)}</b><div class="meta">Priority ${p.priority}${p.current?' · Connected':''}</div></div>
      <div class="actions">
        <form method="post" action="/profile/connect"><input type="hidden" name="ssid" value="${esc(p.ssid)}"><button class="btn secondary">Connect</button></form>
        <form method="post" action="/profile/forget"><input type="hidden" name="ssid" value="${esc(p.ssid)}"><button class="btn danger">Forget</button></form>
      </div>
    </div>`).join(''):'<small>No saved networks yet.</small>';
}

function pickSsid(ssid){
  document.getElementById('ssid').value=ssid;
  document.getElementById('ssid').scrollIntoView({behavior:'smooth',block:'center'});
}

async function scanNow(){
  await fetch('/scan',{method:'POST'});
  await loadNetworks();
}

loadNetworks();
loadProfiles();
loadClients();
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

  server.on("/api/clients", HTTP_GET, []() {
    if (!requireAdmin()) return;
    server.send(200, "application/json", getClientTableJson());
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

  server.on("/settings/ap", HTTP_POST, []() {
    if (!requireAdmin()) return;

    const bool ok = setApCredentials(
      server.arg("ssid"),
      server.arg("password")
    );

    if (!ok) {
      server.send(400, "text/plain", "Invalid hotspot settings.");
      return;
    }

    server.send(
      200,
      "text/html",
      "<h2>RangeLink32 settings saved.</h2><p>The ESP32 is restarting. Reconnect to the new Wi-Fi and open 192.168.50.1.</p>"
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
      server.send(400, "text/plain", "Invalid admin settings.");
      return;
    }

    server.send(
      200,
      "text/html",
      "<h2>Admin login updated.</h2><p>RangeLink32 is restarting. Sign in with the new credentials.</p>"
    );
    scheduleRestart();
  });

  server.on("/system/restart", HTTP_POST, []() {
    if (!requireAdmin()) return;
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
