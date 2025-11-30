JniWintun-VpnBridge

Cross-platform VPN core implemented in Java, leveraging JNI to interface directly with high-performance native tunnel drivers (like Wintun on Windows) for Layer 3 packet handling.

Project Description

This project serves as the native bridge and core logic for a user-space VPN application. The goal is to provide a clean, high-performance Java interface for creating and managing a virtual network adapter (TUN device) on various operating systems, starting with Windows using the modern, minimal Wintun driver.

The core communication and I/O (packet reading and writing) between the Java Virtual Machine (JVM) and the operating system's tunneling mechanism are achieved through the Java Native Interface (JNI).

Technology Stack

Core Logic: Java

Build System: Gradle (Recommended for complex JNI/Native builds)

Native Bridge: C++ (with JNI headers)

Windows Driver: Wintun (Dynamic Link Library - wintun.dll)

Key Features

High Performance: Utilizes the minimal Wintun driver to minimize overhead.

Direct Buffer I/O: Designed to use java.nio.ByteBuffer (Direct Buffers) to reduce memory copies between Java and C++ layers.

Clear Abstraction: Provides a simple Java API (openDevice, readPacket, writePacket) to manage the complex native tunnel interface.

🛠️ Local Setup and Build Instructions

Follow these steps to set up your environment, compile the Java code, generate the JNI headers, and prepare the C++ wrapper structure.

1. Prerequisites

JDK (Java Development Kit) 17+

C++ Compiler (MSVC recommended for Windows builds)

2. Java Compilation and JNI Header Generation

Ensure you are in the project root directory (where the vpn folder is located).

# Compile the Java class
javac vpn/VpnNativeBridge.java

# Generate the C/C++ header file (vpn_VpnNativeBridge.h)
# The -h . flag places the header file in the current directory.
javac -h . vpn/VpnNativeBridge.java


This step creates the vpn_VpnNativeBridge.h file. This header contains the exact C function signatures your native code must implement to be callable from Java.

3. Native Dependencies (Wintun)

You need the Wintun files to build your native bridge.

Download: Obtain the official Wintun distribution ZIP (containing signed DLLs and the header file) from the project's official website or GitHub release page.

Integrate Header: Copy the wintun.h file into your C++ source directory (e.g., alongside the JNI C++ wrapper file you will create).

DLL Placement: Ensure the wintun.dll (for the correct architecture, e.g., amd64) is available in a location where the JVM can find it at runtime (e.g., the directory containing your compiled VpnNative.dll or your Java executable).

4. C++ Implementation (The Wrapper)

You will create a C++ source file (e.g., VpnNative.cpp) where you include both the generated JNI header (vpn_VpnNativeBridge.h) and the Wintun header (wintun.h), and implement the functions defined in the JNI header.

C++ Wrapper File (VpnNative.cpp) structure:

#include "vpn_VpnNativeBridge.h"
#include "wintun.h" // Wintun API Definitions
#include <windows.h> // For LoadLibraryA, GetProcAddress, etc.

// C++ implementation of Java_vpn_VpnNativeBridge_openDevice
JNIEXPORT jlong JNICALL Java_vpn_VpnNativeBridge_openDevice(...) {
// 1. Dynamically load wintun.dll
// 2. Resolve WintunOpenAdapter function pointer
// 3. Call WintunOpenAdapter and return the handle as a jlong
}

// C++ implementation of Java_vpn_VpnNativeBridge_readPacket
JNIEXPORT jint JNICALL Java_vpn_VpnNativeBridge_readPacket(...) {
// Use WintunReceivePacket, write to the direct ByteBuffer, and return length
}

// ... other native methods


 License

This project is licensed under the MIT License.