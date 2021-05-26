#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"

#include <cassert>
#include <memory>
#include <mutex>
#include <utility>

namespace TASKMESSAGING
{

class QueueInterface_t
{
public:
    QueueInterface_t(void) = delete;

    const size_t        queue_len{0};
    const size_t        item_n_bytes{0};
    const size_t        id{0};

    constexpr static const TickType_t wait_ticks{pdMS_TO_TICKS(0)};

    operator bool() const { return h_queue ? true : false; }

    template <typename T>
    esp_err_t           send(T& item, const TickType_t wait_ticks=wait_ticks)
    {
        std::scoped_lock _guard(send_mutx);

        if (*this)
        {
            if (!full())
            {
                if (pdPASS == xQueueSend(h_queue.get(), &item, wait_ticks))
                {
                    return ESP_OK;
                }
                return ESP_FAIL;
            }
            return ESP_ERR_NO_MEM;
        }
        return ESP_ERR_INVALID_STATE;
    }

    template <typename T>
    esp_err_t           send_to_front(T& item, const TickType_t wait_ticks=wait_ticks)
    {
        std::scoped_lock _guard(send_mutx);

        if (*this)
        {
            if (!full())
            {
                if (pdPASS == xQueueSendToFront(h_queue.get(), &item, wait_ticks))
                {
                    return ESP_OK;
                }
                return ESP_FAIL;
            }
            return ESP_ERR_NO_MEM;
        }
        return ESP_ERR_INVALID_STATE;
    }

    template <typename T>
    esp_err_t           receive(T& item, const TickType_t wait_ticks=wait_ticks)
    {
        std::scoped_lock _guard(receive_mutx);

        if (*this)
        {
            if (!empty())
            {
                if (pdPASS == xQueueReceive(h_queue.get(), &item, wait_ticks))
                {
                    return ESP_OK;
                }
                return ESP_ERR_TIMEOUT;
            }
            return ESP_ERR_NOT_FOUND;
        }
        return ESP_ERR_INVALID_STATE;
    }

    template <typename T>
    esp_err_t           peek(T& item, const TickType_t wait_ticks=wait_ticks) const
    {
        std::scoped_lock _guard(receive_mutx);

        if (*this)
        {
            if (!empty())
            {
                if (pdPASS == xQueuePeek(h_queue.get(), &item, wait_ticks))
                {
                    return ESP_OK;
                }
                return ESP_ERR_TIMEOUT;
            }
            return ESP_ERR_NOT_FOUND;
        }
        return ESP_ERR_INVALID_STATE;
    }

    size_t              n_items_waiting(void)   const
    {
        std::scoped_lock _guard(receive_mutx);

        if (h_queue)
            return uxQueueMessagesWaiting(h_queue.get());
        return 0;
    }

    size_t              n_free_spaces(void)     const
    {
        std::scoped_lock _guard(receive_mutx);

        if (h_queue)
            return uxQueueSpacesAvailable(h_queue.get());
        return 0;
    }

    bool                empty(void)             const
        { return 0 == n_items_waiting(); }

    bool                full(void)              const 
        { return 0 == n_free_spaces(); }

    bool                clear(void)
    { 
        std::scoped_lock _guard(send_mutx, receive_mutx);

        return pdPASS == xQueueReset(h_queue.get()); 
    }

protected:
    // Normal constructor
    QueueInterface_t(std::shared_ptr<QueueDefinition> h_queue,
                        const size_t n_items,
                        const size_t item_n_bytes, 
                        const size_t id) :
        queue_len{n_items},
        item_n_bytes{item_n_bytes},
        id{id},
        h_queue{h_queue}        
    {}

    // Copy constructor
    QueueInterface_t(const QueueInterface_t& other) :
        queue_len{other.queue_len},
        item_n_bytes{other.item_n_bytes},
        id{other.id},
        h_queue{other.h_queue}
    {
        assert(0 < queue_len); assert(0 < item_n_bytes);
    }

    std::shared_ptr<QueueDefinition>    h_queue;
    mutable std::recursive_mutex        send_mutx{};
    mutable std::recursive_mutex        receive_mutx{};
};

class DynamicQueue_t : public QueueInterface_t
{
public:
    DynamicQueue_t(const size_t n_items,
                    const size_t item_n_bytes, 
                    const size_t id) :
        QueueInterface_t{std::move(create_queue_shared_ptr(n_items, item_n_bytes)),
                            n_items, item_n_bytes, id}
    {}

private:
    QueueHandle_t create_queue(const size_t n_items, const size_t item_n_bytes)
    {
        return xQueueCreate(n_items, item_n_bytes);
    }

    void delete_queue(QueueHandle_t h)
    {
        std::scoped_lock _guard(send_mutx, receive_mutx);
        vQueueDelete(h);
    }

    std::shared_ptr<QueueDefinition> create_queue_shared_ptr(const size_t n_items, const size_t item_n_bytes)
    {
        return {create_queue(n_items, item_n_bytes), 
                    [this](auto h){delete_queue(h);}};
    }
};

template<typename T, size_t len>
class StaticQueue_t  : public QueueInterface_t
{
public:
    StaticQueue_t(const size_t id) :
        QueueInterface_t{create_queue(len, sizeof(T)),
                            len, sizeof(T), id}
    {}

    // TODO what if this class was destroyed but it had been copied?
    StaticQueue_t(const StaticQueue_t&) = delete;

private:
    QueueHandle_t create_queue(const size_t n_items, const size_t item_n_bytes)
    {
        return xQueueCreateStatic(n_items, item_n_bytes,
                                    queue_storage, &queue_control_block);
    }

    void delete_queue(QueueHandle_t h)
    {
        std::scoped_lock _guard(send_mutx, receive_mutx);
        vQueueDelete(h);
    }

    std::shared_ptr<QueueDefinition> create_queue_shared_ptr(const size_t n_items, const size_t item_n_bytes)
    {
        return {create_queue(n_items, item_n_bytes), 
                    [this](auto h){delete_queue(h);}};
    }

    StaticQueue_t   queue_control_block{};
    uint8_t         queue_storage[len * sizeof(T)]{};
};

} // namespace TASKMESSAGING