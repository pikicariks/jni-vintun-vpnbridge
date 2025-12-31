package vpn;

import java.util.Scanner;

public class Vpn {

    public static void main(String[] args) {
        VpnNativeBridge bridge = new VpnNativeBridge();

        System.out.println("Requesting Wintun adapter creation...");
        long handle = bridge.openDevice("JavaVPN");

        if (handle == 0) {
            System.err.println("Failed to open Wintun device. (Are you Admin?)");
            return;
        }

        System.out.println("Wintun Session Handle acquired: " + handle);

        configureInterface("JavaVPN", "10.0.0.2", "255.255.255.0", 1400);

        String serverIp = "164.92.80.188";
        int serverPort = 51820;

        VpnPacketLoop vpnLoop = new VpnPacketLoop(bridge, handle, serverIp, serverPort);
        vpnLoop.start();

        System.out.println("VPN is running. Press Enter to stop...");

        try (Scanner scanner = new Scanner(System.in)) {
            try {
                scanner.nextLine();
            } catch (java.util.NoSuchElementException e) {
                System.out.println("Input stream closed, shutting down...");
                bridge.closeDevice(handle);
            }
        } catch (Exception e) {
            System.out.println("Error reading input, shutting down...");
            bridge.closeDevice(handle);
        }

        System.out.println("Shutting down...");
        vpnLoop.stop();
        System.out.println("VPN stopped.");
    }

    private static void configureInterface(String name, String ip, String mask, int mtu) {
        try {
            System.out.println("Configuring interface IP and MTU...");
            String ipCmd = String.format("netsh interface ipv4 set address name=\"%s\" static %s %s", name, ip, mask);
            Runtime.getRuntime().exec(ipCmd).waitFor();

            String mtuCmd = String.format("netsh interface ipv4 set subinterface \"%s\" mtu=%d store=persistent", name, mtu);
            Runtime.getRuntime().exec(mtuCmd).waitFor();

            System.out.println("Interface configured: " + ip + " (MTU: " + mtu + ")");
        } catch (Exception e) {
            System.err.println("Failed to configure interface: " + e.getMessage());
        }
    }
}
