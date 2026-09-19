#include <WebServer.h>
#include "web_admin.h"
#include "wifi_manager.h"
#include "storage.h"

namespace {
WebServer server(80);

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
  html.reserve(9000);

  html += R"HTML(<!doctype html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RangeLink32 Admin</title>
<style>
:root{--bg:#07111d;--panel:#0d1d2d;--line:#20354a;--text:#eef7ff;--muted:#8ea7bd;--accent:#49d3ff;--ok:#42d392}
*{box-sizing:border-box}body{margin:0;background:linear-gradient(155deg,#06101b,#0b1d2e);font-family:system-ui,sans-serif;color:var(--text)}
.wrap{max-width:1100px;margin:auto;padding:20px}.brand h1{margin:0;font-size:1.45rem}.brand p{margin:5px 0 0;color:var(--muted)}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-top:18px}
.card{background:rgba(13,29,45,.96);border:1px solid var(--line);border-radius:16px;padding:16px}
.k{font-size:.72rem;text-transform:uppercase;color:var(--muted);letter-spacing:.08em}
.v{font-size:1.35rem;font-weight:800;margin-top:7px}
section{margin-top:14px}.btn{display:inline-block;border:0;border-radius:10px;padding:10px 13px;background:var(--accent);color:#041019;font-weight:800;cursor:pointer;text-decoration:none}
input{width:100%;background:#091623;border:1px solid var(--line);color:var(--text);border-radius:10px;padding:11px;margin:6px 0}
small{color:var(--muted);line-height:1.5}@media(max-width:760px){.grid{grid-template-columns:1fr 1fr}}@media(max-width:430px){.grid{grid-template-columns:1fr}}
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
          String(s.connectedClients) + "</div></div></div>";

  html += R"HTML(
<section class="card">
<h3>Connect upstream Wi-Fi</h3>
<form method="post" action="/connect">
<input name="ssid" placeholder="Wi-Fi name (SSID)" required>
<input name="password" type="password" placeholder="Wi-Fi password" minlength="8" required>
<button class="btn" type="submit">Connect</button>
</form>
<p><small>The nearby network picker and saved-profile manager are the next dashboard milestone.</small></p>
</section>

<section class="card">
<h3>Management</h3>
<p><small>The management AP remains available at <b>192.168.50.1</b> even when upstream Wi-Fi is unavailable.</small></p>
<a class="btn" href="/reconnect">Reconnect upstream</a>
</section>
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

  server.on("/scan", HTTP_POST, []() {
    if (!requireAdmin()) return;
    requestWifiScan();
    server.sendHeader("Location", "/");
    server.send(303);
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
}
