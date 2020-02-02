#include "main.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define LOG_TAG "MAIN"

// --------------------------------------------------------------------------------
bool Main::setup(void)
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = 1 << signal_pin;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_set_level(signal_pin, 1);

	return start_all_tasks();
}

// --------------------------------------------------------------------------------
void Main::run(void)
{
#if defined(MEMORY_DEBUGGING)
	log_mem();
	vTaskDelay(pdMS_TO_TICKS(MEMORY_LOG_INTERVAL_MS));
#else
	vTaskDelay(portMAX_DELAY);
#endif
}

// --------------------------------------------------------------------------------
// Function to start tasks by notification
bool Main::start_all_tasks(void)
{
	bool ret_status
	{ true };

	return ret_status;
}

// --------------------------------------------------------------------------------
// Function to log memory to the mediator as a debug message
#if defined(MEMORY_DEBUGGING)
void Main::log_mem(void)
{
	static constexpr size_t buf_len = 255;

	// Create some buffers on the heap for the human readable strings
	char* heap_buf = new char[buf_len];
	char* stack_buf = new char[buf_len];

	// Clear the buffers to all NULL.  We're going to be using strlen and strcat.
	memset(heap_buf, 0, buf_len);
	memset(stack_buf, 0, buf_len);

	// ----------------------------------------
	// HEAP MEMORY
	multi_heap_info_t heap_info;
	heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);

#if defined(MEMORY_WARN_LOW)
	if (heap_info.minimum_free_bytes < MEMORY_HEAP_MIN ||
			heap_info.largest_free_block < MEMORY_HEAP_MIN)
#endif
#if (defined(MEMORY_VERBOSE) || defined(MEMORY_WARN_LOW))
	{
		snprintf(heap_buf, buf_len,
				"Heap:\tSize = %uk\tFree = %uk\tLargest block = %uk\tMin = %uk",
				(heap_info.total_allocated_bytes + heap_info.total_free_bytes)
						/ 1024, heap_info.total_free_bytes / 1024,
				heap_info.largest_free_block / 1024,
				heap_info.minimum_free_bytes / 1024);
	}
#endif
	// ----------------------------------------

	// STACK MEMORY
	TaskStatus_t *pxTaskStatusArray;
	volatile UBaseType_t uxArraySize, x;
	unsigned int ulTotalRunTime;

	uxArraySize = uxTaskGetNumberOfTasks();

	// Create some memory for our FreeRTOS task list
	pxTaskStatusArray = (TaskStatus_t*)pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

	if (pxTaskStatusArray != nullptr)
	{
		// Generate raw status information about each task.
		uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

		// Print a header if VERBOSE or if one of our task stacks is low
#if defined(MEMORY_VERBOSE)
		snprintf(stack_buf, buf_len, "Stack min bytes:");
#elif defined(MEMORY_WARN_LOW)
		for (x = 0 ; x < uxArraySize ; ++x)
		{
			if (pxTaskStatusArray[x].usStackHighWaterMark < MEMORY_STACK_MIN)
			{
				snprintf(stack_buf, buf_len, "Stack min bytes:");
				break;
			}
		}
#endif
		// --------------------

		// Sort; log for tasks with no core affinity first
		for (x = 0 ; x < uxArraySize ; ++x)
		{
#if defined(MEMORY_WARN_LOW)
			if (pxTaskStatusArray[x].usStackHighWaterMark < MEMORY_STACK_MIN)
#endif
#if (defined(MEMORY_VERBOSE) || defined(MEMORY_WARN_LOW))
			{
				if (pxTaskStatusArray[x].xCoreID > 1)
				{
					snprintf(stack_buf + strlen(stack_buf), buf_len - strlen(stack_buf),
							"\t%s = %u",
							pxTaskStatusArray[x].pcTaskName,
							pxTaskStatusArray[x].usStackHighWaterMark);
				}
			}
#endif
		}
		// --------------------

		// Sort; log for tasks with pinned to core 0 (PRO_CPU)
		for (x = 0 ; x < uxArraySize ; ++x)
		{
#if defined(MEMORY_WARN_LOW)
			if (pxTaskStatusArray[x].usStackHighWaterMark < MEMORY_STACK_MIN)
#endif
#if (defined(MEMORY_VERBOSE) || defined(MEMORY_WARN_LOW))
			{
				if (pxTaskStatusArray[x].xCoreID == 0)
				{
					snprintf(stack_buf + strlen(stack_buf), buf_len - strlen(stack_buf),
							"\t[0] %s = %u",
							pxTaskStatusArray[x].pcTaskName,
							pxTaskStatusArray[x].usStackHighWaterMark);
				}
			}
#endif
		}
		// --------------------

		// Sort; log for tasks with pinned to core 1 (APP_CPU)
		for (x = 0 ; x < uxArraySize ; ++x)
		{
#if defined(MEMORY_WARN_LOW)
			if (pxTaskStatusArray[x].usStackHighWaterMark < MEMORY_STACK_MIN)
#endif
#if (defined(MEMORY_VERBOSE) || defined(MEMORY_WARN_LOW))
			{
				if (pxTaskStatusArray[x].xCoreID == 1)
				{
					snprintf(stack_buf + strlen(stack_buf), buf_len - strlen(stack_buf),
							"\t[1] %s = %u",
							pxTaskStatusArray[x].pcTaskName,
							pxTaskStatusArray[x].usStackHighWaterMark);
				}
			}
#endif
		}
		// --------------------

		vPortFree(pxTaskStatusArray);
	}
	// ----------------------------------------

	// Assemble the human readable string if VERBOSE or heap is low or a stack is low
	if (strlen(heap_buf) > 0 || strlen(stack_buf) > 0)
	{
		// Send the debug message to the DEBUG console through the H7
		ESP_LOGI(LOG_TAG, "%s", heap_buf);
		ESP_LOGI(LOG_TAG, "%s", stack_buf);
	}

	delete[] heap_buf;
	delete[] stack_buf;
}
#endif

// --------------------------------------------------------------------------------
// --------------------------------------------------------------------------------
extern "C" void app_main(void)
{
	Main main_class;

	while (!main_class.setup())
	{
		vTaskDelay(100);
	}

	while (true)
	{
		main_class.run();
	}
}