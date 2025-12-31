package vpn;

import java.io.*;
import java.net.*;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.security.SecureRandom;

/**
 * Linux-based VPN Server
 * * Logic:
 * 1. Receives UDP packet from Windows Client.
 * 2. Decrypts AES-GCM payload.
 * 3. Writes raw IP packet to Linux 'tun0' device.
 * 4. Linux Kernel handles NAT/Routing to the Internet.
 * 5. Linux Kernel writes response back to 'tun0'.
 * 6. Server reads from 'tun0', encrypts, and sends back to Client via UDP.
 */
public class VpnServer {
    private static final int UDP_PORT = 51820;
    private static final String ALGORITHM = "AES/GCM/NoPadding";
    private static final int TAG_BIT_LENGTH = 128;
    private static final int IV_SIZE = 12;

    private final SecretKeySpec secretKey;
    private final SecureRandom secureRandom;
    private InetSocketAddress lastClientAddress;

    public VpnServer(String keyString) {
        byte[] keyBytes = new byte[32];
        byte[] inputBytes = keyString.getBytes();
        System.arraycopy(inputBytes, 0, keyBytes, 0, Math.min(inputBytes.length, 32));
        this.secretKey = new SecretKeySpec(keyBytes, "AES");
        this.secureRandom = new SecureRandom();
    }

    public void start() throws Exception {
        // TODO  For a production on Linux, we will use a small JNI call for ioctl(TUNSETIFF).
        setupLinuxRouting("tun0");

        RandomAccessFile tunFile = new RandomAccessFile("/dev/net/tun", "rw");
        FileDescriptor tunFd = tunFile.getFD();
        FileInputStream tunIn = new FileInputStream(tunFd);
        FileOutputStream tunOut = new FileOutputStream(tunFd);

        DatagramSocket serverSocket = new DatagramSocket(UDP_PORT);
        System.out.println("VPN Server started on UDP port " + UDP_PORT);

        Thread uploadThread = new Thread(() -> {
            byte[] buffer = new byte[65535];
            while (true) {
                try {
                    DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                    serverSocket.receive(packet);

                    lastClientAddress = (InetSocketAddress) packet.getSocketAddress();

                    byte[] decrypted = decrypt(packet.getData(), packet.getLength());
                    if (decrypted != null) {
                        tunOut.write(decrypted);
                    }
                } catch (Exception e) {
                    System.err.println("Upload Error: " + e.getMessage());
                }
            }
        });
        uploadThread.start();

        byte[] tunBuffer = new byte[65535];
        while (true) {
            int len = tunIn.read(tunBuffer);
            if (len > 0 && lastClientAddress != null) {
                try {
                    byte[] rawPacket = Arrays.copyOfRange(tunBuffer, 0, len);
                    byte[] encrypted = encrypt(rawPacket);

                    DatagramPacket outPacket = new DatagramPacket(
                            encrypted,
                            encrypted.length,
                            lastClientAddress
                    );
                    serverSocket.send(outPacket);
                } catch (Exception e) {
                    System.err.println("Download Error: " + e.getMessage());
                }
            }
        }
    }

    private void setupLinuxRouting(String dev) throws Exception {
        runCommand("ip tuntap add dev " + dev + " mode tun");
        runCommand("ip addr add 10.0.0.1/24 dev " + dev);
        runCommand("ip link set dev " + dev + " up");
        runCommand("sysctl -w net.ipv4.ip_forward=1");
        runCommand("iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE");
    }

    private void runCommand(String cmd) throws Exception {
        Process p = Runtime.getRuntime().exec(cmd);
        p.waitFor();
    }

    private byte[] encrypt(byte[] data) throws Exception {
        byte[] iv = new byte[IV_SIZE];
        secureRandom.nextBytes(iv);
        Cipher cipher = Cipher.getInstance(ALGORITHM);
        cipher.init(Cipher.ENCRYPT_MODE, secretKey, new GCMParameterSpec(TAG_BIT_LENGTH, iv));
        byte[] cipherText = cipher.doFinal(data);
        byte[] combined = new byte[iv.length + cipherText.length];
        System.arraycopy(iv, 0, combined, 0, iv.length);
        System.arraycopy(cipherText, 0, combined, iv.length, cipherText.length);
        return combined;
    }

    private byte[] decrypt(byte[] payload, int len) throws Exception {
        if (len < IV_SIZE + (TAG_BIT_LENGTH / 8)) return null;
        byte[] iv = Arrays.copyOfRange(payload, 0, IV_SIZE);
        byte[] cipherText = Arrays.copyOfRange(payload, IV_SIZE, len);
        Cipher cipher = Cipher.getInstance(ALGORITHM);
        cipher.init(Cipher.DECRYPT_MODE, secretKey, new GCMParameterSpec(TAG_BIT_LENGTH, iv));
        return cipher.doFinal(cipherText);
    }

    public static void main(String[] args) throws Exception {
        new VpnServer("BAKA_POSLA_U_SIPKE_I_NIJE_SE_VR").start();
    }
}