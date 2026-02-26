/**
 * DownloadManager - Téléchargement HTTP/HTTPS vers fichier (SD)
 *
 * Stream par blocs (comme OTA), gère HTTP et HTTPS (WiFiClientSecure).
 */

#include "download_manager.h"

#if defined(HAS_SD) && defined(HAS_WIFI)

#include <SD.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

static const size_t DOWNLOAD_CHUNK_SIZE = 2048;
static const int DOWNLOAD_TIMEOUT_MS = 15000;       // 15 s par requête (fichiers souvent petits)
static const unsigned long NO_PROGRESS_TIMEOUT_MS = 5000;  // 5 s sans donnée -> abandon
static const int POLL_DELAY_MS = 1;                 // 1 ms au lieu de 10 entre paquets

/** Extrait l'hôte d'une URL (ex: "https://bucket.r2.dev/path" -> "bucket.r2.dev") */
static void getHostFromUrl(const char* url, String& hostOut) {
  hostOut = "";
  const char* p = strstr(url, "://");
  if (!p) return;
  p += 3;
  const char* end = strchr(p, '/');
  if (end) {
    hostOut = String(p).substring(0, (unsigned)(end - p));
  } else {
    hostOut = p;
  }
}

void DownloadManager::ensureParentDirs(const String& filePath) {
  int lastSlash = filePath.lastIndexOf('/');
  if (lastSlash <= 0) return;
  String dir = filePath.substring(0, lastSlash);
  for (int i = 1; i < (int)dir.length(); i++) {
    if (dir[i] == '/') {
      String sub = dir.substring(0, i);
      if (sub.length() > 0 && !SD.exists(sub.c_str())) {
        SD.mkdir(sub.c_str());
      }
    }
  }
  if (dir.length() > 0 && !SD.exists(dir.c_str())) {
    SD.mkdir(dir.c_str());
  }
}

bool DownloadManager::downloadUrlToFile(const char* url, const char* localPath) {
  if (!url || url[0] == '\0') return false;

  ensureParentDirs(String(localPath));

  File outFile = SD.open(localPath, FILE_WRITE);
  if (!outFile) {
    return false;
  }

  bool useHttps = (strncmp(url, "https://", 8) == 0);
  bool ok = false;

  if (useHttps) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(DOWNLOAD_TIMEOUT_MS);
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(DOWNLOAD_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    if (code == 200) {
      Stream* stream = http.getStreamPtr();
      if (stream) {
        uint8_t buf[DOWNLOAD_CHUNK_SIZE];
        unsigned long lastProgress = millis();
        while (http.connected() || stream->available()) {
          int avail = stream->available();
          if (avail <= 0) {
            if (!http.connected()) break;
            if (millis() - lastProgress > NO_PROGRESS_TIMEOUT_MS) break;
            delay(POLL_DELAY_MS);
            continue;
          }
          size_t toRead = (avail > (int)sizeof(buf)) ? sizeof(buf) : (size_t)avail;
          int n = stream->readBytes(buf, toRead);
          if (n > 0) {
            outFile.write(buf, (size_t)n);
            lastProgress = millis();
          }
        }
        ok = true;
      }
    }
    http.end();
    client.stop();
  } else {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(DOWNLOAD_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    if (code == 200) {
      Stream* stream = http.getStreamPtr();
      if (stream) {
        uint8_t buf[DOWNLOAD_CHUNK_SIZE];
        unsigned long lastProgress = millis();
        while (http.connected() || stream->available()) {
          int avail = stream->available();
          if (avail <= 0) {
            if (!http.connected()) break;
            if (millis() - lastProgress > NO_PROGRESS_TIMEOUT_MS) break;
            delay(POLL_DELAY_MS);
            continue;
          }
          size_t toRead = (avail > (int)sizeof(buf)) ? sizeof(buf) : (size_t)avail;
          int n = stream->readBytes(buf, toRead);
          if (n > 0) {
            outFile.write(buf, (size_t)n);
            lastProgress = millis();
          }
        }
        ok = true;
      }
    }
    http.end();
  }

  outFile.close();
  return ok;
}

static bool streamResponseToFile(HTTPClient& http, File& outFile) {
  Stream* stream = http.getStreamPtr();
  if (!stream) return false;
  uint8_t buf[DOWNLOAD_CHUNK_SIZE];
  unsigned long lastProgress = millis();
  while (http.connected() || stream->available()) {
    int avail = stream->available();
    if (avail <= 0) {
      if (!http.connected()) break;
      if (millis() - lastProgress > NO_PROGRESS_TIMEOUT_MS) break;
      delay(POLL_DELAY_MS);
      continue;
    }
    size_t toRead = (avail > (int)sizeof(buf)) ? sizeof(buf) : (size_t)avail;
    int n = stream->readBytes(buf, toRead);
    if (n > 0) {
      outFile.write(buf, (size_t)n);
      lastProgress = millis();
    }
  }
  return true;
}

int DownloadManager::downloadUrlsToFiles(const char* urls[], const char* paths[], int count,
                                         DownloadProgressCallback onProgress) {
  if (!urls || !paths || count <= 0) return 0;
  int okCount = 0;
  String lastHost;
  WiFiClientSecure secureClient;

  for (int i = 0; i < count; i++) {
    const char* url = urls[i];
    const char* localPath = paths[i];
    if (!url || url[0] == '\0') {
      if (onProgress) onProgress(i + 1, count, localPath ? localPath : "", false);
      continue;
    }

    ensureParentDirs(String(localPath));
    File outFile = SD.open(localPath, FILE_WRITE);
    if (!outFile) {
      if (onProgress) onProgress(i + 1, count, localPath, false);
      continue;
    }

    bool useHttps = (strncmp(url, "https://", 8) == 0);
    bool ok = false;

    if (useHttps) {
      String host;
      getHostFromUrl(url, host);
      if (host != lastHost) {
        secureClient.stop();
        lastHost = host;
        secureClient.setInsecure();
        secureClient.setTimeout(DOWNLOAD_TIMEOUT_MS);
      }
      HTTPClient http;
      http.begin(secureClient, url);
      http.setTimeout(DOWNLOAD_TIMEOUT_MS);
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      int code = http.GET();
      if (code == 200) {
        ok = streamResponseToFile(http, outFile);
      }
      http.end();
      if (ok) okCount++;
    } else {
      secureClient.stop();
      lastHost = "";
      HTTPClient http;
      http.begin(url);
      http.setTimeout(DOWNLOAD_TIMEOUT_MS);
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      int code = http.GET();
      if (code == 200) {
        ok = streamResponseToFile(http, outFile);
      }
      http.end();
      if (ok) okCount++;
    }
    outFile.close();
    if (onProgress) onProgress(i + 1, count, localPath, ok);
  }
  secureClient.stop();
  return okCount;
}

#else

void DownloadManager::ensureParentDirs(const String& filePath) {
  (void)filePath;
}

bool DownloadManager::downloadUrlToFile(const char* url, const char* localPath) {
  (void)url;
  (void)localPath;
  return false;
}

int DownloadManager::downloadUrlsToFiles(const char* urls[], const char* paths[], int count,
                                         DownloadProgressCallback onProgress) {
  (void)urls;
  (void)paths;
  (void)count;
  (void)onProgress;
  return 0;
}

#endif // HAS_SD && HAS_WIFI
