#ifndef KEY_H_
#define KEY_H_

/**
 * @brief Key/button IDs
 */
typedef enum {
    KEY1 = 0,
    KEY2,
    KEY3,
    KEY4,
    KEY_MAX
} key_id_t;

/**
 * @brief Key event types
 */
typedef enum {
    KEY_EVENT_PRESSED = 0,
    KEY_EVENT_RELEASED,
} key_event_t;

/**
 * @brief Callback function type for key events
 * @param key_id  ID of the key that triggered the event
 * @param event   Type of key event (pressed/released)
 */
typedef void (*key_callback_t)(key_id_t key_id, key_event_t event);

/**
 * @brief Register a callback for key events
 * @param key_id  Key to monitor
 * @param cb      Callback to invoke on event
 * @return 0 on success, negative error code on failure
 */
int key_register_callback(key_id_t key_id, key_callback_t cb);

#endif /* KEY_H_ */
