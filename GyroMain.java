package app.gyro;

import android.app.*;
import android.content.*;
import android.hardware.*;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.net.*;

public class GyroMain extends Activity {
    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.VERTICAL);
        l.setPadding(40, 80, 40, 40);
        TextView tv = new TextView(this);
        tv.setText("GyroAim\n1. 先在 Shizuku 里启动 gyromouse\n2. 点下面按钮");
        tv.setTextSize(16);
        Button on = new Button(this);
        on.setText("启动");
        Button off = new Button(this);
        off.setText("停止");
        l.addView(tv); l.addView(on); l.addView(off);
        setContentView(l);
        on.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                startForegroundService(new Intent(GyroMain.this, GyroService.class));
                Toast.makeText(GyroMain.this, "已启动", Toast.LENGTH_SHORT).show();
            }
        });
        off.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                stopService(new Intent(GyroMain.this, GyroService.class));
                Toast.makeText(GyroMain.this, "已停止", Toast.LENGTH_SHORT).show();
            }
        });
    }
}

class GyroService extends Service implements SensorEventListener {
    SensorManager sm;
    Sensor gyro;
    Socket sock;
    DataOutputStream out;
    long lastNs = 0;
    float scale = 3.0f;

    public IBinder onBind(Intent i) { return null; }

    public int onStartCommand(Intent i, int f, int s) {
        NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        String ch = "gyro";
        nm.createNotificationChannel(new NotificationChannel(ch, "GyroAim", NotificationManager.IMPORTANCE_LOW));
        Notification n = new Notification.Builder(this, ch)
            .setContentTitle("GyroAim 运行中")
            .setSmallIcon(android.R.drawable.ic_menu_compass)
            .build();
        startForeground(1, n);
        sm = (SensorManager) getSystemService(SENSOR_SERVICE);
        gyro = sm.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
        if (gyro != null) sm.registerListener(this, gyro, SensorManager.SENSOR_DELAY_GAME);
        return START_STICKY;
    }

    public void onSensorChanged(SensorEvent e) {
        long t = System.nanoTime();
        if (lastNs == 0) { lastNs = t; return; }
        float dt = (t - lastNs) / 1000000000.0f;
        lastNs = t;
        float gx = e.values[0] * 57.29578f;
        float gz = e.values[2] * 57.29578f;
        int dx = Math.round(-gz * dt * scale);
        int dy = Math.round(-gx * dt * scale);
        if (dx == 0 && dy == 0) return;
        try {
            if (out == null) {
                sock = new Socket("127.0.0.1", 7788);
                sock.setTcpNoDelay(true);
                out = new DataOutputStream(sock.getOutputStream());
            }
            out.writeShort(dx);
            out.writeShort(dy);
            out.writeShort(0);
            out.writeShort(0);
            out.flush();
        } catch (Exception ex) {
            try { if (sock != null) sock.close(); } catch (Exception e2) {}
            sock = null; out = null;
        }
    }

    public void onAccuracyChanged(Sensor s, int a) {}

    public void onDestroy() {
        if (sm != null) sm.unregisterListener(this);
        try { if (sock != null) sock.close(); } catch (Exception e) {}
        super.onDestroy();
    }
}