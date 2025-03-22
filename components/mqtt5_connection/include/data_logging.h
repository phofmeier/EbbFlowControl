#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_DATA_LOGGING
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_DATA_LOGGING

#include "cJSON.h"

/**
 * @brief Send data to the mqtt broker with a timestamp.
 *
 * @param channel channel to send the data to
 * @param data data to send
 */
void send_timed_data(const char *channel, cJSON *data);

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_DATA_LOGGING */
