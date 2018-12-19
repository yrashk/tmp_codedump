#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "uECC.h"
#include "sss.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "seclet_key.h"

struct State;
/**
 * Task state.
 */
typedef struct State {
    TaskKeyParameters *parameters;
    /**
     * Currently provided shares.
     */
    sss_Share *shares;
    /**
     * The number of currently provided shares.
     */
    uint8_t share_counter;
    /**
     * The number of shares that were provided internally.
     *
     * When combining N out of M shares fails, or when a reset
     * is requested, only this number of shares [0..share_lwm]
     * (low water mark) is kept in .shares.
     */
    uint8_t share_lwm;
    /**
     * Private key.
     *
     * When shares reset is requested, private key MUST be wiped out
     * by TaskKey
     */
    uint8_t private_key[sss_MLEN];
} State;

#define state_max_shares(state) state->parameters->n

static const char* TAG = "key";

/**
 * Initialized key shares.
 *
 * If no key shares found, it will generate a new keypair, split it into shares
 * and keep `TaskKeyParameters.keep` shares, firing away other shares.
 */
void init_key_shares(uint8_t keep, State *state)
{
    nvs_handle nvs;
    esp_err_t err;

    // Use `seclet` namespace on the main NVS partition.
    // FIXME: this is perhaps is not perfect as it should probably use an
    // encrypted NVS partition, encrypted with a eFused key (when this gets
    // closer to production)
    err = nvs_open("seclet", NVS_READWRITE, &nvs);
    // Crash if we can't do this (not much we can do after this failure)
    ESP_ERROR_CHECK(err);

init_key_shares_announce:

    // Get the number of shares available
    err = nvs_get_u8(nvs, "key_shares", &state->share_lwm);
    // If there shares available:
    if (ESP_OK == err) {
        ESP_LOGI(TAG, "got %d stored key shares", state->share_lwm);
        char key_share_id[15] = { 0 };
        // for every share
        for (uint8_t i = 0; i < state->share_lwm; i++) {
            sprintf(key_share_id, "key_share_%d", i);
            size_t sz = sizeof(sss_Share);
            sss_Share share;
            err = nvs_get_blob(nvs, key_share_id, &share, &sz);
            ESP_ERROR_CHECK(err);
            // post it as KEY_EVENT/KEY_SHARE_PROVIDED so it can be picked up
            err = esp_event_post_to(state->parameters->event_loop, KEY_EVENT, KEY_SHARE_PROVIDED,
                                    &share, sizeof(sss_Share), 10);
            ESP_ERROR_CHECK(err);
        }
        // cleanup
        nvs_close(nvs);
        // that's it
        return;
    }
    // if there's an actual error, crash
    if (ESP_ERR_NVS_NOT_FOUND != err) {
        ESP_ERROR_CHECK(err);
    }

    // public key
    uint8_t public[uECC_curve_public_key_size(state->parameters->curve)];

    // Generate the keypair

    // Suspend the scheduler to make sure the RNG
    // setting to uECC doesn't affect others
    vTaskSuspendAll();
    uECC_RNG_Function old_rng = uECC_get_rng();
    uECC_set_rng((uECC_RNG_Function)esp_fill_random);
    uECC_make_key(public, state->private_key, state->parameters->curve);
    uECC_set_rng(old_rng);
    // Resume the scheduler
    xTaskResumeAll();
    ESP_LOGI(TAG, "new keypair generated");
    err = esp_event_post_to(state->parameters->event_loop, KEY_EVENT, KEY_KEYPAIR_GENERATED,
                            NULL, 0, 10);
    ESP_ERROR_CHECK(err);

    sss_create_shares(state->shares, state->private_key, state->parameters->n, state->parameters->m);

    // Check that the shares can be combined back
    uint8_t restored[sss_MLEN];
    int sss_ret = sss_combine_shares(restored, state->shares, state->parameters->m);
    assert(sss_ret == 0);
    assert(memcmp(restored, state->private_key, sss_MLEN) == 0);
    ESP_LOGI(TAG, "key shares combined successfully");

    // wipe the private key
    memset(state->private_key, 0, sizeof(sss_MLEN));

    // write down the number of shares we're keeping
    err = nvs_set_u8(nvs, "key_shares", keep);
    // crash if we can't
    ESP_ERROR_CHECK(err);

    char key_share_id[15] = { 0 };
    // for every share we're keeping
    for (uint8_t i = 0; i < keep; i++) {
        sprintf(key_share_id, "key_share_%d", i);
        // write it down to NVS
        err = nvs_set_blob(nvs, key_share_id, state->shares[i], sss_SHARE_LEN);
        // crash if we can't
        ESP_ERROR_CHECK(err);
    }
    // zero out the rest of the shares that were generating so
    // that we're not keeping them around
    for (uint8_t i = keep; i < state_max_shares(state); i++) {
        memset(state->shares[i], 0, sizeof(sss_Share));
    }

    // to send kept shares, go back to the beginning
    goto init_key_shares_announce;
}

ESP_EVENT_DEFINE_BASE(KEY_EVENT);

/**
 * KEY_EVENT/KEY_SHARES_RESET handler.
 */
void on_key_shares_reset(State *state, esp_event_base_t base, int32_t id, void *data)
{
    ESP_LOGI(TAG, "resetting provided key shares");
    // wipe out the private key
    memset(state->private_key, 0, sss_MLEN);
    // reset the counter to the low water mark
    state->share_counter = state->share_lwm;
    // wipe out previous externally received key shares
    for (uint8_t i = state->share_lwm; i < state_max_shares(state); i++) {
        memset(state->shares[i], 0, sss_SHARE_LEN);
    }
}

/**
 * KEY_EVENT/KEY_SHARE_PROVIDED handler.
 */
void on_key_share(State *state, esp_event_base_t base, int32_t id, sss_Share *share)
{
    if (state->share_counter == state->parameters->n) {
        // there's nothing we need to do with key shares when we already
        // have the private key
        ESP_LOGI(TAG, "got a key share that's superfluous, discarding");
        return;
    }
    // copy the new share in and increment the counter
    memcpy(state->shares[state->share_counter++], share, sizeof(sss_Share));
    ESP_LOGI(TAG, "got a key share %i out of %i required", state->share_counter, state->parameters->n);
    // if we reached the required number of shares
    if (state->share_counter == state->parameters->n) {
        // attempt to combine them to reconstruct the key
        int ret = sss_combine_shares(state->private_key, state->shares, state->parameters->m);
        if (ret) {
            // reconstruction failed
            ESP_LOGI(TAG, "private key reconstruction failed, resetting share counter");
            // reset the counter to the low water mark
            state->share_counter = state->share_lwm;
        } else {
            // otherwise, we have the key
            ESP_LOGI(TAG, "private key successfully reconstructed");
        }
    }
}

/**
 * Task loop implementation
 */
void TaskKey(TaskKeyParameters *parameters)
{
    // ensure the private key size fits
    if (uECC_curve_private_key_size(parameters->curve) > sss_MLEN) {
        ESP_LOGE(TAG, "private key size does not fit (%d > %d), halting", uECC_curve_private_key_size(parameters->curve), sss_MLEN);
        // terminate the task
        goto TaskKey_terminate;
    }

    // Prepare the state
    State *state = malloc(sizeof(State));
    memset(state, 0, sizeof(State));
    state->parameters = parameters;
    state->shares = malloc(parameters->n * sizeof(sss_Share));

    // Register event handlers
    esp_event_handler_register_with(parameters->event_loop,
                                    KEY_EVENT, KEY_SHARE_PROVIDED,
                                    (esp_event_handler_t) on_key_share, state);

    esp_event_handler_register_with(parameters->event_loop,
                                    KEY_EVENT, KEY_SHARES_RESET,
                                    (esp_event_handler_t) on_key_shares_reset, state);

    // Initialize key shares
    init_key_shares(parameters->keep_shares, state);

    for (;;) {
        // Run the event loop
        esp_event_loop_run(parameters->event_loop, 1);
        taskYIELD();
    }

    // Cleanup the resources
    free(state->shares);
    free(state);

TaskKey_terminate:
    vTaskDelete(NULL);
}


#define QUEUE_SIZE 32

void TaskKeyParametersCreateStatic(uint8_t keep_shares,
                                   uint8_t n, uint8_t m, TaskKeyParameters *parameters)
{
    esp_event_loop_args_t args = {
        .queue_size = QUEUE_SIZE,
    };
    esp_event_loop_create(&args, &parameters->event_loop);
    parameters->keep_shares = keep_shares;
    parameters->n = n;
    parameters->m = m;
    parameters->curve = uECC_secp256r1();
}
