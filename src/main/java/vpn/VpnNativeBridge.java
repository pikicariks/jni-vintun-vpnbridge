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
    native int readPacket(long handle, ByteBuffer buffer);
    native int writePacket(long handle, ByteBuffer buffer, int length);
    native void closeDevice(long handle);

}
