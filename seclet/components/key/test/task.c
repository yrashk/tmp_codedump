#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "unity.h"

#include "seclet_key.h"

void on_keypair_generated(bool *flag, esp_event_base_t base, int32_t id, void *data)
{
    *flag = true;
}

TEST_CASE("should generate a key if there is none", "[key]")
{
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());

    static TaskKeyParameters taskKeyParameters;

    TaskKeyParametersCreateStatic(2, 2, 2, &taskKeyParameters);

    static bool keypair_generated = false;

    esp_event_handler_register_with(taskKeyParameters.event_loop,
                                    KEY_EVENT, KEY_KEYPAIR_GENERATED,
                                    (esp_event_handler_t) on_keypair_generated, &keypair_generated);


    TaskHandle_t task;
    xTaskCreate((TaskFunction_t) TaskKey, "key",
                TaskKey_stackDepth, &taskKeyParameters,
                1,
                &task);


    TickType_t start_time = xTaskGetTickCount();
    while (!keypair_generated) {
        if ((xTaskGetTickCount() - start_time) > 5000 * portTICK_PERIOD_MS) {
            TEST_ASSERT_MESSAGE(false, "timeout");
        }
        taskYIELD();
    }

    TEST_ASSERT(keypair_generated);

    vTaskDelete(task);

}

void on_share_provided(uint8_t *counter, esp_event_base_t base, int32_t id, void *data)
{
    (*counter)++;
}

TEST_CASE("should announce key shares it keeps", "[key]")
{
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());

    static TaskKeyParameters taskKeyParameters;

    TaskKeyParametersCreateStatic(2, 2, 2, &taskKeyParameters);

    static uint8_t shares_provided = 0;

    esp_event_handler_register_with(taskKeyParameters.event_loop,
                                    KEY_EVENT, KEY_SHARE_PROVIDED,
                                    (esp_event_handler_t) on_share_provided, &shares_provided);

    TaskHandle_t task;
    xTaskCreate((TaskFunction_t) TaskKey, "key",
                            TaskKey_stackDepth, &taskKeyParameters,
                            1,
                            &task);

    TickType_t start_time = xTaskGetTickCount();
    while (shares_provided < 2) {
        if ((xTaskGetTickCount() - start_time) > 5000 * portTICK_PERIOD_MS) {
            TEST_ASSERT_MESSAGE(false, "timeout");
        }
        taskYIELD();
    }

    TEST_ASSERT_EQUAL(shares_provided, 2);

    vTaskDelete(task);

}

TEST_CASE("should not regenerate a key if there is one present", "[key]")
{
    // generate key pair first
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());

    bool keypair_generated = false;

    {
        static TaskKeyParameters taskKeyParameters;

        TaskKeyParametersCreateStatic(2, 2, 2, &taskKeyParameters);
        esp_event_handler_register_with(taskKeyParameters.event_loop,
                                        KEY_EVENT, KEY_KEYPAIR_GENERATED,
                                        (esp_event_handler_t) on_keypair_generated, &keypair_generated);

        TaskHandle_t task;
        xTaskCreate((TaskFunction_t) TaskKey, "key",
                    TaskKey_stackDepth, &taskKeyParameters,
                    1,
                    &task);

        TickType_t start_time = xTaskGetTickCount();
        while (!keypair_generated) {
            if ((xTaskGetTickCount() - start_time) > 5000 * portTICK_PERIOD_MS) {
                TEST_ASSERT_MESSAGE(false, "timeout");
            }
            taskYIELD();
        }

        TEST_ASSERT(keypair_generated);

        vTaskDelete(task);
    }

    // now, restart this once again:

    keypair_generated = false;

    TaskKeyParameters taskKeyParameters;

    TaskKeyParametersCreateStatic(2, 2, 2, &taskKeyParameters);
    esp_event_handler_register_with(taskKeyParameters.event_loop,
                                    KEY_EVENT, KEY_KEYPAIR_GENERATED,
                                    (esp_event_handler_t) on_keypair_generated, &keypair_generated);

    TaskHandle_t task;
    xTaskCreate((TaskFunction_t) TaskKey, "key",
                TaskKey_stackDepth, &taskKeyParameters,
                1,
                &task);

    vTaskDelay(5000 / portTICK_PERIOD_MS); // wait 5 seconds

    TEST_ASSERT(!keypair_generated);

    vTaskDelete(task);
}
