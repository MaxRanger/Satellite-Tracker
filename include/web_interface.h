/*
 * web_interface.h - WiFi and web server interface
 */

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "shared_data.h"
#include "motor_control.h"

// Initialize web interface
void initWebInterface();

// Handle web client requests
void handleWebClient();

// Web server handlers
void handleRoot();
void handleTLE();
void handleHome();
void handleStop();
void handleNotFound();

#endif // WEB_INTERFACE_H
