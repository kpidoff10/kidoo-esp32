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
  static void cmdWiFi();
  static void cmdWiFiSet(const String& args);
  static void cmdWiFiConnect();
  static void cmdWiFiDisconnect();
  static void cmdPubNub();
  static void cmdPubNubConnect();
  static void cmdPubNubDisconnect();
  static void cmdPubNubPublish(const String& args);
  static void cmdPubNubRoutes();
  static void cmdRTC();
  static void cmdRTCSet(const String& args);
  static void cmdRTCSync();
  static void cmdPotentiometer();
  static void cmdMemoryDebug();
  static void cmdNFCRead(const String& args);
  static void cmdNFCWrite(const String& args);
  static void cmdConfigGet(const String& args);
  static void cmdConfigSet(const String& args);
  static void cmdConfigList();
  static void cmdLEDTest();
  
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
};

#endif // SERIAL_COMMANDS_H
