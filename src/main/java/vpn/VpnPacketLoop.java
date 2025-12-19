package vpn;

import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.SecureRandom;

public class VpnPacketLoop {
    private final VpnNativeBridge nativeBridge;
    private final long sessionHandle;
    private volatile boolean running = false;
    private static final int BUFFER_SIZE = 65535;

    private DatagramSocket socket;
    private InetSocketAddress vpnServerAddress;

    private static final String ALGORITHM = "AES/GCM/NoPadding";
    private static final int TAG_BIT_LENGTH = 128;
    private static final int IV_SIZE = 12;
    private final SecretKeySpec secretKey;
    private final SecureRandom secureRandom;

    public VpnPacketLoop(VpnNativeBridge bridge, long handle, String serverIp, int serverPort) {
        this.nativeBridge = bridge;
        this.sessionHandle = handle;
        this.secureRandom = new SecureRandom();

        byte[] keyBytes = "BAKA_POSLA_U_SIPKE_I_NIJE_SE_VRA".getBytes();
        byte[] fixedKey = new byte[32];
        System.arraycopy(keyBytes, 0, fixedKey, 0, Math.min(keyBytes.length, 32));
        this.secretKey = new SecretKeySpec(fixedKey, "AES");

        try {
            this.socket = new DatagramSocket();
            this.vpnServerAddress = new InetSocketAddress(serverIp, serverPort);
        } catch (SocketException e) {
            System.err.println("Could not open VPN socket: " + e.getMessage());
        }
    }

    public void start() {
        if (running || sessionHandle == 0) return;
        running = true;

        startUplinkThread();
        startDownlinkThread();
    }
    private void startUplinkThread() {
        Thread readThread = new Thread(() -> {
            ByteBuffer buffer = ByteBuffer.allocateDirect(BUFFER_SIZE);
            buffer.order(ByteOrder.nativeOrder());
            System.out.println("Uplink thread started");

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
        }, "VPN-Uplink");

        readThread.start();
    }

    private void startDownlinkThread() {
        Thread writeThread = new Thread(() -> {
            byte[] receiveBuffer = new byte[BUFFER_SIZE + 100];
            ByteBuffer buffer = ByteBuffer.allocateDirect(BUFFER_SIZE);
            buffer.order(ByteOrder.nativeOrder());

            System.out.println("Downlink thread started.");
            while (running) {
                try {
                    DatagramPacket packet = new DatagramPacket(receiveBuffer, receiveBuffer.length);
                    socket.receive(packet);

                    byte[] decryptedData = decrypt(packet.getData(), packet.getLength());

                    if (decryptedData != null) {
                        buffer.clear();
                        buffer.put(decryptedData);
                        buffer.flip();

                        nativeBridge.writePacket(sessionHandle, buffer, decryptedData.length);
                    }
                } catch (Exception e) {
                    if (running) System.err.println("Downlink error: " + e.getMessage());
                }
            }
        }, "VPN-Downlink");
        writeThread.start();
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

        try{
            byte[] encryptedData = encrypt(rawPacket);
            DatagramPacket outgoingPacket = new DatagramPacket(
                    encryptedData,
                    encryptedData.length,
                    destAddr,
                    protocol
            );

            socket.send(outgoingPacket);
        } catch (Exception e) {
            System.err.println("Error sending packet to VPN server: "+e.getMessage());
        }
    }

    public void stop() {
        running = false;
        if (socket != null) socket.close();
        nativeBridge.closeDevice(sessionHandle);
    }

    /**
     * Encrypt data using AES-GCM.
     */
    private byte[] encrypt(byte[] data) throws Exception {
        byte[] iv = new byte[IV_SIZE];
        secureRandom.nextBytes(iv);

        Cipher cipher = Cipher.getInstance(ALGORITHM);
        GCMParameterSpec spec = new GCMParameterSpec(TAG_BIT_LENGTH, iv);
        cipher.init(Cipher.ENCRYPT_MODE, secretKey, spec);

        byte[] cipherText = cipher.doFinal(data);

        // Combine IV and CipherText into one payload for the UDP packet
        byte[] encryptedPayload = new byte[iv.length + cipherText.length];
        System.arraycopy(iv, 0, encryptedPayload, 0, iv.length);
        System.arraycopy(cipherText, 0, encryptedPayload, iv.length, cipherText.length);

        return encryptedPayload;
    }

    /**
     * Decrypts data using AES-GCM.
     */
    private byte[] decrypt(byte[] encryptedPayload, int length) {
        try {
            if (length < IV_SIZE + (TAG_BIT_LENGTH / 8)) return null;

            byte[] iv = new byte[IV_SIZE];
            System.arraycopy(encryptedPayload, 0, iv, 0, IV_SIZE);

            int cipherTextLen = length - IV_SIZE;
            byte[] cipherText = new byte[cipherTextLen];
            System.arraycopy(encryptedPayload, IV_SIZE, cipherText, 0, cipherTextLen);

            Cipher cipher = Cipher.getInstance(ALGORITHM);
            GCMParameterSpec spec = new GCMParameterSpec(TAG_BIT_LENGTH, iv);
            cipher.init(Cipher.DECRYPT_MODE, secretKey, spec);

            return cipher.doFinal(cipherText);
        } catch (Exception e) {
            System.err.println("Decryption failed: Integrity check failed or invalid key.");
            return null;
        }
    }
}
