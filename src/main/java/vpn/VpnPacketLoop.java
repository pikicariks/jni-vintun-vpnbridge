package vpn;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
public class VpnPacketLoop {
    private final VpnNativeBridge nativeBridge;
    private final long sessionHandle;
    private volatile boolean running = false;
    private static final int BUFFER_SIZE = 65535;

    public VpnPacketLoop(VpnNativeBridge bridge, long handle) {
        this.nativeBridge = bridge;
        this.sessionHandle = handle;
    }

    public void start() {
        if (running) return;
        running = true;

        ByteBuffer buffer = ByteBuffer.allocateDirect(BUFFER_SIZE);
        buffer.order(ByteOrder.nativeOrder());

        Thread readThread = new Thread(() -> {
            System.out.println("Reading packets...");

            while (running) {
                buffer.clear();

                int packetLength = nativeBridge.readPacket(sessionHandle, buffer);

                if (packetLength > 0) {
                    buffer.limit(packetLength);
                    buffer.position(0);

                    processOutgoingPacket(buffer);
                } else if (packetLength == 0) {
                    try {
                        Thread.sleep(1);
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                    }
                } else {
                    System.err.println("Error reading from Wintun interface.");
                    running = false;
                }
            }
        }, "VPN-Read-Loop");

        readThread.start();
    }

    private void processOutgoingPacket(ByteBuffer packet) {
        byte firstByte = packet.get(0);
        int version = (firstByte >> 4) & 0x0F;

        if (version == 4) {
            int protocol = packet.get(9) & 0xFF;
            short checksum = packet.getShort(10);

            byte[] destIp = new byte[4];
            packet.position(16);
            packet.get(destIp);
            packet.position(0);

            try {
                InetAddress destAddr = InetAddress.getByAddress(destIp);
                System.out.printf("Protocol: %d, Dest: %s, Size: %d bytes\n",
                        protocol, destAddr.getHostAddress(), packet.remaining());


                //TODO Optimize this for garbage collector
                byte[] rawPacket = new byte[packet.remaining()];
                packet.get(rawPacket);
                sendToVpnServer(rawPacket, protocol, destAddr);

            } catch (UnknownHostException e) {
                //TODO Add detailed and robust logging
                e.printStackTrace();
            }
        }
    }

    private void sendToVpnServer(byte[] rawPacket, int protocol, InetAddress destAddr) {
        //TODO Implement sending the packet over tcp/udp socket to server
    }

    public void stop() {
        running = false;
    }
}
