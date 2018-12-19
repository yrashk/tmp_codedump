#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "nvs_flash.h"

#include "seclet.h"

void app_main(void)
{
    nvs_flash_init();

    /// [MAIN KEY] task
    static StaticTask_t taskKey;
    static StackType_t taskKey_stack[TaskKey_stackDepth];
    static TaskKeyParameters taskKeyParameters;

    // We're currently using 2 out of 2 key shares with both key shares kept in
    // the device for now as there's no way to supply the key shares from
    // external or pseudo-external sources
    // FIXME: switch to a real key sharing scheme when feasible
    TaskKeyParametersCreateStatic(2, 2, 2, &taskKeyParameters);

    // If this task is created before BT or WiFi are started,
    // the RNG will be a PRNG.
    xTaskCreateStaticPinnedToCore((TaskFunction_t) TaskKey, "key",
                                  TaskKey_stackDepth, &taskKeyParameters,
                                  1,
                                  taskKey_stack, &taskKey, 1);

    for( ;; );
    ESP_LOGI(TAG,"");
}
