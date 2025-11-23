#ifndef COMPONENTS_OTA_UPDATER_INCLUDE_OTA_UPDATER
#define COMPONENTS_OTA_UPDATER_INCLUDE_OTA_UPDATER
/***
 * @file ota_updater.h
 * @brief Header file for OTA updater component
 * This component handles over-the-air firmware updates.
 */

/***
 * @brief Mark the currently running application version as valid to
 *        prevent rollback
 */
void mark_running_app_version_valid();

/***
 * @brief Task to perform OTA update
 */
void ota_updater_task(void *pvParameter);

/***
 * @brief Initialize the OTA updater
 */
void initialize_ota_updater();

#endif /* COMPONENTS_OTA_UPDATER_INCLUDE_OTA_UPDATER */
