#ifndef SERIAL_COMMANDS_H
#define SERIAL_COMMANDS_H

#include <Arduino.h>

/**
 * Commandes Serial communes à tous les modèles
 * 
 * Ce fichier contient les commandes Serial de base
 * qui sont disponibles sur tous les modèles
 */

class SerialCommands {
public:
  /**
   * Initialiser le système de commandes Serial
   */
  static void init();
  
  /**
   * Traiter une commande reçue via Serial
   * @param command La commande à traiter
   */
  static void processCommand(const String& command);
  
  /**
   * Afficher l'aide des commandes disponibles
   */
  static void printHelp();
  
  /**
   * Vérifier et traiter les commandes en attente
   * À appeler régulièrement dans loop()
   */
  static void update();

private:
  // Commandes communes
  static void cmdHelp();
  static void cmdReboot(const String& args);
  static void cmdInfo();
  static void cmdMemory();
  static void cmdClear();
  static void cmdBrightness(const String& args);
  static void cmdSleep(const String& args);
  static void cmdBLE();
  static void cmdBLEStart();
  static void cmdBLEStop();
  static void cmdWiFi();
  static void cmdWiFiSet(const String& args);
  static void cmdWiFiConnect();
  static void cmdWiFiDisconnect();
  static void cmdWiFiScan();
  static void cmdConfigRetry();
  static void cmdDeviceKey();
  static void cmdPubNub();
  static void cmdPubNubConnect();
  static void cmdPubNubDisconnect();
  static void cmdPubNubPublish(const String& args);
  static void cmdPubNubRoutes();
  static void cmdRTC();
  static void cmdRTCSet(const String& args);
  static void cmdRTCSync();
  static void cmdApiPing();
  static void cmdPotentiometer();
  static void cmdMemoryDebug();
  static void cmdNFCRead(const String& args);
  static void cmdNFCWrite(const String& args);
  static void cmdConfigGet(const String& args);
  static void cmdConfigSet(const String& args);
  static void cmdConfigList();
  static void cmdLEDTest();
  static void cmdLCDTest();
  static void cmdLCDReset();
  static void cmdLCDFps();
  static void cmdLCDPlayMjpeg(const String& args);
  static void cmdOta(const String& args);
  
  // Commandes vibreur
  static void cmdVibrator(const String& args);
  
  // Commandes touch (TTP223)
  static void cmdTouch(const String& args);
  
  // Commandes capteur environnement (AHT20 + BMP280)
  static void cmdEnv(const String& args);
  
  // Commandes audio
  static void cmdAudio();
  static void cmdAudioPlay(const String& args);
  static void cmdAudioStop();
  static void cmdAudioPause();
  static void cmdAudioResume();
  static void cmdAudioVolume(const String& args);
  static void cmdAudioList(const String& args);
  
  static bool initialized;
  static String inputBuffer;

  // Historique des commandes
  static constexpr uint8_t HISTORY_MAX = 10;
  static String history[HISTORY_MAX];
  static uint8_t historyCount;
  static int8_t historyIndex;     // -1 = pas en navigation
  static String tempBuffer;       // Sauvegarde du buffer avant navigation
  static uint8_t escState;        // 0=normal, 1=reçu ESC, 2=reçu ESC[

  static void replaceInputBuffer(const String& newContent);
};

#endif // SERIAL_COMMANDS_H
