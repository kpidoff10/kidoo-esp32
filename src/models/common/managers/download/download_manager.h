#ifndef DOWNLOAD_MANAGER_H
#define DOWNLOAD_MANAGER_H

#include <Arduino.h>

/**
 * Gestionnaire de téléchargement (HTTP/HTTPS -> fichier)
 *
 * Permet de télécharger un fichier depuis une URL et de l'écrire sur la carte SD
 * (stream par blocs, comme pour l'OTA firmware). Utilisé par sync-emotions (Gotchi)
 * ou tout autre besoin de download (firmware, assets, etc.).
 *
 * Nécessite HAS_SD et HAS_WIFI. Sans l'un des deux, downloadUrlToFile() retourne false.
 */

/** Callback appelé après chaque fichier téléchargé (current, total, localPath, success). Passer nullptr si pas besoin. */
typedef void (*DownloadProgressCallback)(int current, int total, const char* localPath, bool success);

class DownloadManager {
public:
  /**
   * Télécharge une URL (HTTP ou HTTPS) et écrit le contenu dans un fichier sur la SD.
   * Crée les répertoires parents du chemin si nécessaire.
   *
   * @param url       URL complète (http:// ou https://)
   * @param localPath Chemin du fichier sur la SD (ex: /characters/xxx/emotions/FOOD/vid/video.mjpeg)
   * @return true si le téléchargement et l'écriture ont réussi, false sinon
   */
  static bool downloadUrlToFile(const char* url, const char* localPath);

  /**
   * Télécharge plusieurs fichiers en réutilisant la connexion TCP/TLS pour le même hôte.
   * Beaucoup plus rapide que d'appeler downloadUrlToFile() en boucle (1 handshake TLS par hôte au lieu d'1 par fichier).
   *
   * @param urls       Tableau d'URLs (doit vivre pendant l'appel)
   * @param paths     Tableau de chemins SD (même ordre que urls)
   * @param count     Nombre d'éléments dans les tableaux
   * @param onProgress Callback optionnel appelé après chaque fichier (current, total, localPath, success)
   * @return Nombre de téléchargements réussis
   */
  static int downloadUrlsToFiles(const char* urls[], const char* paths[], int count,
                                 DownloadProgressCallback onProgress = nullptr);

  /**
   * Crée les répertoires parents d'un chemin fichier.
   * Ex: /a/b/c/file.txt -> crée /a, /a/b, /a/b/c
   * Utile avant d'écrire un fichier dans un sous-dossier.
   *
   * @param filePath Chemin complet du fichier (ou d'un répertoire)
   */
  static void ensureParentDirs(const String& filePath);
};

#endif // DOWNLOAD_MANAGER_H
