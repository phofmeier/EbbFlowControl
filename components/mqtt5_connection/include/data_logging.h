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

/**
 * @brief Remove the message from the sent map.
 *
 * @param msg_id id of the message to remove.
 */
void remove_from_sent_map(const int msg_id);

/**
 * @brief Resend all saved messages.
 *
 */
void resend_saved_messages();

/**
 * @brief Reschedule an already enqueued message.
 *
 * @param msg_id id of the message to reschedule.
 */
void reschedule_message(const int msg_id);

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_DATA_LOGGING */
