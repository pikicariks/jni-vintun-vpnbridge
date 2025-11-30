package vpn;

import java.nio.ByteBuffer;

public class VpnNativeBridge {

    static {
        System.loadLibrary("VpnNative");
    }

    public static void main (String[] args) {
        return;
    }

    private native long openDevice(String deviceName);
    private native int readPacket(long handle, ByteBuffer buffer);
    private native int writePacket(long handle, ByteBuffer buffer, int length);
    private native void closeDevice(long handle);

}
