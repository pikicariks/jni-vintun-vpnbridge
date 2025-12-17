JniWintun-VpnBridge

Cross-platform VPN core implemented in Java, leveraging JNI to interface directly with high-performance native tunnel drivers (like Wintun on Windows).

- Linking Native and Java

This repository contains both the Java high-level API and the C++ JNI bridge.

Development Workflow

Java Side: Defines the native methods in VpnNativeBridge.java.

JNI Header: Generated using javac -h ..

C++ Side: Implements the logic in VpnNative.cpp, consuming the generated header and the wintun.h SDK.

Build: The C++ side must be compiled into VpnNative.dll and placed in the Java runtime path.

 - Native Build Requirements (Windows)

To recompile the native bridge (VpnNative.dll):

Visual Studio 2022 (with Desktop Development with C++)

JDK 17+ (for JNI headers)

Wintun SDK (wintun.h and wintun.dll)

# Example MSVC Build Command
cl.exe /LD /I "%JAVA_HOME%\include" /I "%JAVA_HOME%\include\win32" VpnNative.cpp /Fe:VpnNative.dll Advapi32.lib


 - Runtime Setup

For the Java application to run successfully, the following must be in the application root:

VpnNative.dll (Your compiled bridge)

wintun.dll (The official signed driver)

Note: The application must be run with Administrator Privileges to allow Wintun to create the network interface.