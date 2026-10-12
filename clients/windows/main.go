//go:build windows

// Elf Launcher for Windows: a native window hosting Edge WebView2 that loads
// the Elf Launcher page served by the console at http://<console-ip>:1000/.
// When the console can't be reached it offers the bundled page (offline).
package main

import (
	_ "embed"
	"encoding/json"
	"html"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	webview2 "github.com/jchv/go-webview2"
)

const (
	appName     = "Elf Launcher"
	appVersion  = "2.0.4"
	defaultIP   = ""
	defaultPort = "1000"
)

type settings struct {
	ConsoleIP string `json:"consoleIp"`
}

func appDataDir() string {
	base := os.Getenv("APPDATA")
	if base == "" {
		base, _ = os.UserConfigDir()
	}
	return filepath.Join(base, appName)
}

func settingsPath() string { return filepath.Join(appDataDir(), "settings.json") }

func loadSettings() settings {
	s := settings{ConsoleIP: defaultIP}
	if b, err := os.ReadFile(settingsPath()); err == nil {
		var t settings
		if json.Unmarshal(b, &t) == nil && strings.TrimSpace(t.ConsoleIP) != "" {
			s.ConsoleIP = strings.TrimSpace(t.ConsoleIP)
		}
	}
	return s
}

func saveSettings(s settings) {
	_ = os.MkdirAll(appDataDir(), 0o755)
	b, _ := json.MarshalIndent(s, "", "  ")
	_ = os.WriteFile(settingsPath(), b, 0o644)
}

// normalize turns "192.168.1.50", "http://192.168.1.50:1000/" etc. into
// "host:port" (port defaults to 1000).
func normalize(in string) string {
	s := strings.TrimSpace(in)
	s = strings.TrimPrefix(s, "http://")
	s = strings.TrimPrefix(s, "https://")
	if i := strings.IndexAny(s, "/?#"); i >= 0 {
		s = s[:i]
	}
	if s == "" {
		return ""
	}
	if _, _, err := net.SplitHostPort(s); err != nil {
		s = net.JoinHostPort(strings.Trim(s, "[]"), defaultPort)
	}
	return s
}

// probe returns nil if Elf Launcher answers at hostport.
func probe(hostport string) error {
	c := &http.Client{Timeout: 4 * time.Second}
	resp, err := c.Get("http://" + hostport + "/version")
	if err != nil {
		return err
	}
	resp.Body.Close()
	return nil
}

const pageStyle = `<style>
html,body{margin:0;height:100%;background:#14161c;color:#e6f0ff;font-family:"Segoe UI",system-ui,sans-serif}
.wrap{min-height:100%;display:flex;align-items:center;justify-content:center}
.card{background:#1d2029;border:1px solid #6d4aff;border-radius:14px;padding:28px 32px;max-width:440px;width:90%;box-shadow:0 0 30px #1f5fd055}
h1{margin:0 0 6px;font-size:22px} .sub{color:#6aa8ff;letter-spacing:.25em;font-size:12px;margin-bottom:18px}
p{color:#b9c8e6;line-height:1.45} .err{color:#ff8f8f;font-size:13px;word-break:break-word}
label{display:block;font-size:13px;color:#b9c8e6;margin:14px 0 6px}
.row{display:flex;gap:8px}
input{flex:1;padding:10px 12px;border-radius:8px;border:1px solid #6d4aff;background:#14161c;color:#fff;font-size:15px}
button{padding:10px 18px;border-radius:8px;border:0;background:#6d4aff;color:#fff;font-size:15px;cursor:pointer}
button:disabled{opacity:.6;cursor:default}
.spin{display:inline-block;width:14px;height:14px;border:2px solid #6aa8ff;border-top-color:transparent;border-radius:50%;animation:s 1s linear infinite;vertical-align:-2px;margin-right:8px}
@keyframes s{to{transform:rotate(360deg)}}
</style>`

func connectingPage(hostport string) string {
	return `<!doctype html><html><head><meta charset="utf-8">` + pageStyle + `</head><body><div class="wrap"><div class="card">
<h1>Elf Launcher</h1><div class="sub">FOR PS5 &middot; 2.0.4</div>
<p><span class="spin"></span>Connecting to ` + html.EscapeString(hostport) + ` ...</p></div></div></body></html>`
}

func errorPage(ip string, hostport string, errMsg string) string {
	return `<!doctype html><html><head><meta charset="utf-8">` + pageStyle + `</head><body><div class="wrap"><div class="card">
<h1>Elf Launcher</h1><div class="sub">FOR PS5 &middot; 2.0.4</div>
<p>Can't reach Elf Launcher at <b>http://` + html.EscapeString(hostport) + `/</b>.</p>
<p>Make sure the PS5 is on, Elf Launcher is running and this PC is on the same network. You can also open the offline page.</p>
<div class="err">` + html.EscapeString(errMsg) + `</div>
<label for="ip">Console IP</label>
<div class="row"><input id="ip" value="` + html.EscapeString(ip) + `" spellcheck="false" autofocus>
<button id="retry">Connect</button></div>
<p><button id="off" style="background:#3a4660">Open offline page</button></p></div></div>
<script>
const b=document.getElementById('retry'),i=document.getElementById('ip');
function go(){b.disabled=true;b.textContent='Connecting...';window.hbConnect(i.value);}
b.onclick=go;i.addEventListener('keydown',e=>{if(e.key==='Enter')go();});
document.getElementById('off').onclick=()=>window.elfOffline();
i.focus();i.select();
</script></body></html>`
}

//go:embed web/index.html
var offlinePage []byte

// serveOffline serves the bundled page on a loopback port so it can fetch the catalog like a normal site.
func serveOffline() string {
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		return ""
	}
	go http.Serve(ln, http.HandlerFunc(func(rw http.ResponseWriter, r *http.Request) {
		rw.Header().Set("Content-Type", "text/html; charset=utf-8")
		rw.Write(offlinePage)
	}))
	return "http://" + ln.Addr().String() + "/"
}

func main() {
	local := os.Getenv("LOCALAPPDATA")
	if local == "" {
		local = appDataDir()
	}
	dataPath := filepath.Join(local, appName, "WebView2")

	w := webview2.NewWithOptions(webview2.WebViewOptions{
		Debug:     false,
		AutoFocus: true,
		DataPath:  dataPath,
		WindowOptions: webview2.WindowOptions{
			Title:  appName + " " + appVersion,
			Width:  1280,
			Height: 820,
			IconId: 1, // RT_GROUP_ICON #1 (winres/winres.json)
			Center: true,
		},
	})
	if w == nil {
		msgBox(appName,
			"Could not start Microsoft Edge WebView2.\n\n"+
				"Install the WebView2 Runtime from\nhttps://developer.microsoft.com/microsoft-edge/webview2/\nand try again.")
		os.Exit(1)
	}
	defer w.Destroy()
	w.SetSize(640, 480, webview2.HintMin)

	st := loadSettings()

	var connect func(ip string)
	connect = func(ip string) {
		hp := normalize(ip)
		if hp == "" {
			w.SetHtml(errorPage(ip, ip, "Enter the PS5 IP address."))
			return
		}
		w.SetHtml(connectingPage(hp))
		go func() {
			err := probe(hp)
			w.Dispatch(func() {
				if err != nil {
					w.SetHtml(errorPage(strings.TrimSpace(ip), hp, err.Error()))
					return
				}
				st.ConsoleIP = strings.TrimSpace(ip)
				saveSettings(st)
				w.Navigate("http://" + hp + "/")
			})
		}()
	}

	// Called from the error page's Connect button. Returns at once; the probe
	// runs in the background so the window stays responsive.
	_ = w.Bind("hbConnect", func(ip string) {
		w.Dispatch(func() { connect(ip) })
	})

	offURL := ""
	_ = w.Bind("elfOffline", func() {
		w.Dispatch(func() {
			if offURL == "" {
				offURL = serveOffline()
			}
			if offURL != "" {
				w.Navigate(offURL)
			}
		})
	})

	connect(st.ConsoleIP)
	w.Run()
}

func msgBox(title, text string) {
	t, _ := syscallUTF16(title)
	m, _ := syscallUTF16(text)
	procMessageBoxW.Call(0, uintptr(m), uintptr(t), 0x10) // MB_ICONERROR
}

