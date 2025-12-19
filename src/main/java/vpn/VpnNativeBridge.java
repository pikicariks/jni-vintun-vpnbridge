package vpn;

import java.io.File;
import java.nio.ByteBuffer;

public class VpnNativeBridge {

    static {
        try {
            String root = System.getProperty("user.dir");

            String dllPath = root + File.separator + "native" + File.separator + "VpnNative.dll";

            File dllFile = new File(dllPath);
            if (dllFile.exists()) {
                System.load(dllPath);
                System.out.println("Native library loaded securely from: " + dllPath);
            } else {
                System.err.println("Warning: Secure path not found: " + dllPath);
                System.err.println("Attempting fallback to default search path...");
                System.loadLibrary("VpnNative");
            }
        } catch (UnsatisfiedLinkError e) {
            System.err.println("CRITICAL: Failed to load VpnNative.dll.");
            System.err.println("Ensure 'VpnNative.dll' and 'wintun.dll' are inside the 'native' folder.");
            e.printStackTrace();
        }
    }

    public static void main (String[] args) {
        return;
    }

    public native long openDevice(String deviceName);
    public native int readPacket(long handle, ByteBuffer buffer);
    public native int writePacket(long handle, ByteBuffer buffer, int length);
    public native void closeDevice(long handle);

}
