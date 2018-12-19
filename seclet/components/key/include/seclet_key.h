#include "seclet_common.h"

#include "sss.h"
#include "esp_event.h"

// Event base
ESP_EVENT_DECLARE_BASE(KEY_EVENT);

// Event subtypes
enum {
    /**
     * Key share provided (externallly or internally)
     *
     * Payload type: TaskKeyMessage_KeyShareProvided
     *
     */
    KEY_SHARE_PROVIDED,
    /**
     * Externally provided key shares reset requested
     *
     * Payload type: none
     */
    KEY_SHARES_RESET,
    /**
     * Key pair has been generated
     *
     */
    KEY_KEYPAIR_GENERATED
};

struct TaskKeyMessage_KeyShareProvided;
/**
 * Key share provided.
 */
typedef struct TaskKeyMessage_KeyShareProvided {
    sss_Share share;
} TaskKeyMessage_KeyShareProvided;

/**
 * TaskKey paramaters.
 */
struct TaskKeyParameters;
typedef struct TaskKeyParameters {
    esp_event_loop_handle_t event_loop;
    uint8_t keep_shares;
    uint8_t n;
    uint8_t m;
    const struct uECC_Curve_t * curve;
} TaskKeyParameters;

/**
 * Populates allocated TaskKeyParameters.
 *
 * Parameters:
 *
 * * `keep_shares` - number of shares to keep on the device
 * * `n`, `m` - `n` out of `m` secret sharing
 * * `parameters` - pointer to allocated TaskKeyParameters
 *
 */
void TaskKeyParametersCreateStatic(uint8_t keep_shares,
                                   uint8_t n, uint8_t m, TaskKeyParameters *parameters);

#define TaskKey_stackDepth 8192
/**
 * The TaskKey loop.
 */
void TaskKey(TaskKeyParameters *parameters);
