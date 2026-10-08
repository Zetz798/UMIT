package org.example.ticketdemo;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import javax.net.ssl.HttpsURLConnection;

public final class PhantomBridge {
    private PhantomBridge() {}

    public static boolean openPhantom(Context context, String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setPackage("app.phantom");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            context.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException | SecurityException exception) {
            return false;
        }
    }

    // Called from a Qt worker, never from the Android or QML UI thread.
    public static String devnetBalance(String json) {
        HttpsURLConnection connection = null;
        try {
            connection = (HttpsURLConnection) new URL("https://api.devnet.solana.com").openConnection();
            connection.setConnectTimeout(10000);
            connection.setReadTimeout(10000);
            connection.setInstanceFollowRedirects(false);
            connection.setRequestMethod("POST");
            connection.setRequestProperty("Content-Type", "application/json");
            connection.setDoOutput(true);
            byte[] payload = json.getBytes(StandardCharsets.UTF_8);
            connection.setFixedLengthStreamingMode(payload.length);
            try (OutputStream out = connection.getOutputStream()) { out.write(payload); }
            if (connection.getResponseCode() != 200) return "";
            try (InputStream input = connection.getInputStream();
                 ByteArrayOutputStream result = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[1024];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    if (result.size() + count > 16384) return "";
                    result.write(buffer, 0, count);
                }
                return result.toString("UTF-8");
            }
        } catch (Exception exception) {
            return ""; // Do not log callback URLs, sessions or wallet addresses.
        } finally {
            if (connection != null) connection.disconnect();
        }
    }
}
