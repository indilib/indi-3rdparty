package info.melda.sala.pktriggercord;

import android.util.Log;
import android.app.Application;
import android.content.res.AssetManager;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import java.io.*;
import java.util.ArrayList;
import java.util.List;
import java.util.Arrays;
import android.os.SystemClock;
import java.security.MessageDigest;

public class PkTriggerCord extends Application {
    public static final String TAG = "PkTriggerCord";
    private String cliHome;
    private static final int MARK_LIMIT = 1000000;
    private SharedPreferences prefs;
    private boolean cliInstalled;

    @Override
    public void onCreate() {
        super.onCreate();

        cliHome = getApplicationContext().getFilesDir().getAbsolutePath();

        PreferenceManager.setDefaultValues(this, R.xml.prefs, false);
        prefs = PreferenceManager.getDefaultSharedPreferences(this);
        Log.v( TAG, "Before installCli");
        cliInstalled = installCli();
        Log.v( TAG, "After installCli");
        if (cliInstalled) {
            startCli();
        }
    }

    public boolean isShowPreview() {
        return prefs.getBoolean("showpreview", true);
    }

    public String getSaveDir() {
        return prefs.getString("savedir", "/mnt/sdcard");
    }

    private void startCli() {
        try {
            new ProcessBuilder()
            .command("su", "-c", cliHome+"/pktriggercord-cli --servermode")
            .start();
            SystemClock.sleep(1000);
        } catch ( Exception e ) {
            Log.e( TAG, e.getMessage(), e);
        }
    }

    private static final char[] hexArray = "0123456789ABCDEF".toCharArray();

    private static String bytesToHex(byte[] bytes) {
        char[] hexChars = new char[bytes.length * 2];
        for ( int j = 0; j < bytes.length; j++ ) {
            int v = bytes[j] & 0xFF;
            hexChars[j * 2] = hexArray[v >>> 4];
            hexChars[j * 2 + 1] = hexArray[v & 0x0F];
        }
        return new String(hexChars);
    }

    private byte []calculateDigest(InputStream in) {
        try {
            MessageDigest digester = MessageDigest.getInstance("MD5");
            byte[] bytes = new byte[8192];
            int byteCount;
            while ((byteCount = in.read(bytes)) > 0) {
                digester.update(bytes, 0, byteCount);
            }
            byte[] digest = digester.digest();
            Log.v( TAG, "digest: "+bytesToHex(digest));
            return digest;
        } catch ( Exception e ) {
            return null;
        }
    }

    private boolean installCli() {
        try {
            copyAssetIfNeeded("pktriggercord-cli");
            return true;
        } catch ( Exception e ) {
            Log.e( TAG, e.getMessage(), e );
            return false;
        }
    }

    interface ProgressCallback {
        void progressUpdate(double progress);
    }

    static int copyStream(InputStream in, OutputStream out, int length, ProgressCallback callback) throws IOException {
        byte[] buffer = new byte[16*1024];
        int currentRead = 0;
        int totalRead = 0;
        while ( totalRead < length && currentRead >= 0 ) {
            currentRead = in.read( buffer, 0, Math.min(length-totalRead, buffer.length) );
            if ( currentRead != -1 ) {
                out.write( buffer, 0, currentRead );
                totalRead += currentRead;
                //      Log.v( TAG, "Read "+currentRead+" bytes" );
            }
            if ( callback != null ) {
                callback.progressUpdate( 1.0 * totalRead / length);
            }
        }
        return totalRead;
    }

    static int copyStream(InputStream in, OutputStream out, int length) throws IOException {
        return copyStream(in, out, length, null);
    }

    private static int copyStream(InputStream in, OutputStream out) throws IOException {
        return copyStream( in, out, Integer.MAX_VALUE);
    }

    private void simpleSudoWrapper(String command) throws Exception {
        Process p = Runtime.getRuntime().exec(new String[] {"su", "-c", command});
        p.waitFor();
        p.destroy();
    }

    private void simpleSudoWrapper(List<String> commands) throws Exception {
        StringBuilder appendedCommands = new StringBuilder();
        String sep = "";
        for ( String command : commands ) {
            appendedCommands.append(sep).append(command);
            sep = " && ";
        }
        simpleSudoWrapper(appendedCommands.toString());
    }

    private void copyAssetIfNeeded(final String fileName) throws Exception {
        boolean needCliInstall;
        AssetManager assetManager = getAssets();
        InputStream in = assetManager.open(fileName);
        final String fullFileName = cliHome + "/" + fileName;

        try {
            FileInputStream fis = new FileInputStream(fullFileName);
            byte []fmd5sum = calculateDigest(fis);
            fis.close();
            in.mark(MARK_LIMIT);
            byte[] md5sum = calculateDigest(in);
            in.reset();
            needCliInstall = !Arrays.equals( fmd5sum, md5sum );
        } catch ( Exception e ) {
            Log.e( TAG, e.getMessage(), e);
            needCliInstall = true;
        }

        if ( needCliInstall ) {
            Log.v( TAG, "needCliInstall");
            final int myUid = android.os.Process.myUid();
            Log.v( TAG, "myUid:"+myUid);
            simpleSudoWrapper( new ArrayList<String>() {{
                add("mkdir -p "+cliHome);
                add("chown "+myUid+" "+cliHome);
                add("chmod 777 "+cliHome);
            }});
            Log.v( TAG, "After first simpleSudoWrapper");
            simpleSudoWrapper("rm -f "+fullFileName);
            Log.v( TAG, "After rm -f");
            OutputStream out = new FileOutputStream(fullFileName);
            int copiedBytes = copyStream(in, out);
            Log.v( TAG, "copied "+copiedBytes+" bytes");
            in.close();
            out.flush();
            out.close();
            Log.v( TAG, "before chmod 4777");
            simpleSudoWrapper("chmod 4777 "+fullFileName);
            Log.v( TAG, "End of needCliInstall");
        }
    }

    boolean isCliInstalled() {
        return cliInstalled;
    }
}
