/*
 * LoRaWAN sample application
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// INSTGateway-External ->  EUI: 0016C001F152FD00
//#define LORAWAN_JOIN_EUI    { 0x00, 0x16, 0xC0, 0x01, 0xF1, 0x52, 0xFD, 0x00 }  // MSB Format
// INSTGateway-Dev ->       EUI: 0016C001F152F960
#define LORAWAN_JOIN_EUI    { 0x00, 0x16, 0xC0, 0x01, 0xF1, 0x52, 0xF9, 0x60 }  // MSB Format

// tag-d4f3d2 -> DevEUI: f4ce36c17cd4f3d2
// tag-fc1344 -> DevEUI: f4ce36c44bfc1344
// tag-556ee8 -> DevEUI: f4ce36d07f556ee8
// tag-0623c2 -> DevEUI: f4ce36eb5b0623c2
// tag-7dcb8a -> DevEUI: f4ce364ca37dcb8a
// tag-c3c7bd -> DevEUI: f4ce366381c3c7bd
// tag-b5c60b -> DevEUI: f4ce367a9db5c60b

// AppKey: 1100016BDC5A34004F94C1962713DAAB
#define LORAWAN_APP_KEY     { 0x11, 0x00, 0x01, 0x6B, 0xDC, 0x5A, 0x34, 0x00, 0x4F, 0x94, 0xC1, 0x96, 0x27, 0x13, 0xDA, 0xAB }  // MSB Format
// NwkKey: DC7EB919F278B9929E7E6BE427D8EBD0
#define LORAWAN_NWK_KEY     { 0xDC, 0x7E, 0xB9, 0x19, 0xF2, 0x78, 0xB9, 0x92, 0x9E, 0x7E, 0x6B, 0xE4, 0x27, 0xD8, 0xEB, 0xD0 }  // MSB Format