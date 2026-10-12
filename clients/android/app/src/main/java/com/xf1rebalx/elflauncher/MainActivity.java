package com.xf1rebalx.elflauncher;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.text.InputType;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.Toast;

import java.net.HttpURLConnection;
import java.net.URL;

/** Elf Launcher for Android: asks for the PS5 IP once, opens http://<ip>:1000,
 *  and falls back to the bundled page when the console can't be reached.
 *  Long-press anywhere to change the IP. */
public class MainActivity extends Activity {
    static final String PORT = "1000";
    static final String OFFLINE = "file:///android_asset/web/index.html";
    WebView web;
    SharedPreferences prefs;
    String consoleBase = "";

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        prefs = getSharedPreferences("elf", MODE_PRIVATE);
        web = new WebView(this);
        setContentView(web);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setAllowFileAccess(true);
        web.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView v, WebResourceRequest r) {
                Uri u = r.getUrl(); String us = u.toString();
                if (us.startsWith("file:///android_asset/") || (!consoleBase.isEmpty() && us.startsWith(consoleBase))) return false;
                try { startActivity(new Intent(Intent.ACTION_VIEW, u)); } catch (Exception e) { /* no browser */ }
                return true;
            }
            @Override public void onReceivedError(WebView v, WebResourceRequest r, WebResourceError e) {
                if (r.isForMainFrame() && !consoleBase.isEmpty() && r.getUrl().toString().startsWith(consoleBase)) goOffline();
            }
        });
        web.setOnLongClickListener(v -> { askIp(); return true; });
        String ip = prefs.getString("ip", "");
        if (ip.isEmpty()) askIp(); else connect(ip);
    }

    @Override public void onBackPressed() { if (web.canGoBack()) web.goBack(); else super.onBackPressed(); }

    static String host(String in) {
        String s = in.trim().replaceFirst("^https?://", "");
        int i = s.indexOf('/'); if (i >= 0) s = s.substring(0, i);
        if (s.isEmpty()) return "";
        return s.contains(":") ? s : s + ":" + PORT;
    }

    void askIp() {
        final EditText et = new EditText(this);
        et.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        et.setHint("192.168.1.50");
        et.setText(prefs.getString("ip", ""));
        new AlertDialog.Builder(this)
            .setTitle("PS5 IP address")
            .setMessage("Run Elf Launcher on the PS5, then enter the console's IP. Long-press anywhere later to change it.")
            .setView(et)
            .setPositiveButton("Connect", (d, w) -> {
                String ip = et.getText().toString().trim();
                prefs.edit().putString("ip", ip).apply();
                connect(ip);
            })
            .setNegativeButton("Offline", (d, w) -> goOffline())
            .setCancelable(false)
            .show();
    }

    void connect(String ip) {
        final String hp = host(ip);
        if (hp.isEmpty()) { goOffline(); return; }
        consoleBase = "http://" + hp + "/";
        new Thread(() -> {
            boolean ok;
            try {
                HttpURLConnection c = (HttpURLConnection) new URL(consoleBase + "version").openConnection();
                c.setConnectTimeout(3000); c.setReadTimeout(3000);
                ok = c.getResponseCode() > 0; c.disconnect();
            } catch (Exception e) { ok = false; }
            final boolean up = ok;
            runOnUiThread(() -> { if (up) web.loadUrl(consoleBase); else goOffline(); });
        }).start();
    }

    void goOffline() {
        Toast.makeText(this, "Console not reachable, showing the offline page. Long-press to change the IP.", Toast.LENGTH_LONG).show();
        web.loadUrl(OFFLINE);
    }
}
