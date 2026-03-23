import socket
import time
from camera import Camera, PixelFormat, FrameSize

INDEX_HTML = """<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Signal Light Detector</title>
  <style>
    *{box-sizing:border-box}
    body{margin:0;background:#0d1117;color:#e6edf3;font-family:'Segoe UI',Arial,sans-serif;display:flex;flex-direction:column;align-items:center;min-height:100vh}
    h1{margin:18px 0 4px;font-size:1.3em;color:#58a6ff}
    .sub{font-size:0.8em;color:#8b949e;margin-bottom:14px}
    .main{display:flex;flex-wrap:wrap;justify-content:center;gap:20px;padding:0 16px;width:100%%;max-width:1000px}
    .cam-box{flex:1;min-width:300px;max-width:640px}
    .cam-box img{width:100%%;border:2px solid #30363d;border-radius:10px}
    .panel{flex:0 0 260px;display:flex;flex-direction:column;gap:14px}
    .signal-display{background:#161b22;border:2px solid #30363d;border-radius:14px;padding:20px;text-align:center}
    .signal-light{width:100px;height:100px;border-radius:50%%;margin:10px auto;border:4px solid #30363d;transition:all 0.3s}
    .signal-light.red{background:radial-gradient(circle,#ff4444 30%%,#cc0000 100%%);box-shadow:0 0 30px #ff0000aa;border-color:#ff4444}
    .signal-light.green{background:radial-gradient(circle,#44ff44 30%%,#00cc00 100%%);box-shadow:0 0 30px #00ff00aa;border-color:#44ff44}
    .signal-light.none{background:radial-gradient(circle,#333 30%%,#1a1a1a 100%%);box-shadow:none}
    .signal-label{font-size:1.4em;font-weight:700;margin-top:8px;text-transform:uppercase;letter-spacing:2px}
    .signal-label.red{color:#ff4444}
    .signal-label.green{color:#44ff44}
    .signal-label.none{color:#8b949e}
    .stats{background:#161b22;border:2px solid #30363d;border-radius:14px;padding:16px}
    .stats h3{margin:0 0 10px;font-size:0.95em;color:#58a6ff}
    .stat-row{display:flex;justify-content:space-between;padding:4px 0;font-size:0.85em}
    .stat-row .label{color:#8b949e}
    .stat-row .val{font-weight:600}
    .stat-row .val.red{color:#ff4444}
    .stat-row .val.green{color:#44ff44}
    .bar-container{height:8px;background:#21262d;border-radius:4px;margin-top:6px;overflow:hidden;display:flex}
    .bar-red{background:#ff4444;transition:width 0.3s}
    .bar-green{background:#44ff44;transition:width 0.3s}
    @media(max-width:680px){.panel{flex:1 1 100%%}}
  </style>
</head>
<body>
  <h1>Signal Light Detector</h1>
  <p class="sub">Freenove ESP32-S3 &mdash; MicroPython &mdash; Real-time Detection</p>
  <div class="main">
    <div class="cam-box"><img id="stream" src="/capture" /></div>
    <div class="panel">
      <div class="signal-display">
        <div class="signal-light none" id="light"></div>
        <div class="signal-label none" id="label">SCANNING...</div>
      </div>
      <div class="stats">
        <h3>Detection Stats</h3>
        <div class="stat-row"><span class="label">Red pixels</span><span class="val red" id="rcount">0</span></div>
        <div class="stat-row"><span class="label">Green pixels</span><span class="val green" id="gcount">0</span></div>
        <div class="stat-row"><span class="label">Sampled</span><span class="val" id="tcount">0</span></div>
        <div class="bar-container">
          <div class="bar-red" id="rbar" style="width:0%%"></div>
          <div class="bar-green" id="gbar" style="width:0%%"></div>
        </div>
      </div>
    </div>
  </div>
  <script>
    function refresh(){
      document.getElementById('stream').src='/capture?t='+Date.now();
      fetch('/status').then(r=>r.json()).then(d=>{
        var light=document.getElementById('light');
        var label=document.getElementById('label');
        var cls=d.detected==1?'red':d.detected==2?'green':'none';
        light.className='signal-light '+cls;
        label.className='signal-label '+cls;
        label.textContent=d.detected==1?'RED SIGNAL':d.detected==2?'GREEN SIGNAL':'NO SIGNAL';
        document.getElementById('rcount').textContent=d.red;
        document.getElementById('gcount').textContent=d.green;
        document.getElementById('tcount').textContent=d.total;
        var sum=d.red+d.green||1;
        document.getElementById('rbar').style.width=(d.red/sum*100)+'%%';
        document.getElementById('gbar').style.width=(d.green/sum*100)+'%%';
      }).catch(()=>{});
    }
    setInterval(refresh, 800);
  </script>
</body>
</html>"""


class WebServer:
    def __init__(self, cam, detect_state, port=80):
        self.cam = cam
        self.detect_state = detect_state
        self.port = port

    def start(self):
        addr = socket.getaddrinfo("0.0.0.0", self.port)[0][-1]
        self.sock = socket.socket()
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(addr)
        self.sock.listen(2)
        self.sock.settimeout(0.1)
        print("Web server on port", self.port)

    def handle_once(self):
        try:
            cl, addr = self.sock.accept()
        except OSError:
            return

        try:
            cl.settimeout(2)
            request = cl.recv(1024).decode("utf-8")
            path = request.split(" ")[1] if " " in request else "/"

            if path.startswith("/status"):
                d = self.detect_state
                body = '{{"detected":{},"red":{},"green":{},"total":{}}}'.format(
                    d["detected"], d["red"], d["green"], d["total"]
                )
                cl.send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n")
                cl.send(body)

            elif path.startswith("/capture"):
                self.cam.reconfigure(
                    pixel_format=PixelFormat.JPEG,
                    frame_size=FrameSize.SVGA,
                    fb_count=2,
                )
                img = self.cam.capture()
                cl.send("HTTP/1.0 200 OK\r\nContent-Type: image/jpeg\r\n\r\n")
                cl.send(bytes(img))

            else:
                cl.send("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n")
                cl.send(INDEX_HTML)
        except Exception as e:
            print("Request error:", e)
        finally:
            cl.close()
