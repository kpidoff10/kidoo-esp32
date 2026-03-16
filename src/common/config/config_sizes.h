#ifndef CONFIG_SIZES_H
#define CONFIG_SIZES_H

/**
 * Constantes centralisées pour les tailles des buffers de configuration
 * Utilisées dans tous les fichiers qui lisent/écrivent les fichiers config
 */

/** Taille maximale des fichiers config (4 KB) */
static constexpr int CONFIG_MAX_SIZE = 4096;

/** Taille des buffers JSON pour configuration */
static constexpr int CONFIG_JSON_BUFFER_SIZE = CONFIG_MAX_SIZE;

#endif // CONFIG_SIZES_H
