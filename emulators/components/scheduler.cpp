#include "scheduler.h"
#include "buffer.h"
#include <cassert>

void Scheduler::reset() {
    for (int i = 0; i < m_heapSize; i++) {
        m_heap[i]->active = false;
        m_heap[i]->heapIndex = -1;
    }
    m_heapSize = 0;
    m_now = 0;
}

u64 Scheduler::nextEventTime() const {
    if (m_heapSize == 0) return UINT64_MAX;
    return m_heap[0]->timestamp;
}

int Scheduler::cyclesUntilNextEvent() const {
    if (m_heapSize == 0) return INT_MAX;
    u64 next = m_heap[0]->timestamp;
    if (next <= m_now) return 0;
    u64 diff = next - m_now;
    return diff > static_cast<u64>(INT_MAX) ? INT_MAX : static_cast<int>(diff);
}

void Scheduler::addCycles(int cycles) {
    u64 target = m_now + cycles;
    processEvents(target);
    m_now = target;
}

void Scheduler::schedule(SchedulerEvent& event, int delay) {
    assert(delay >= 0);

    // If already scheduled, remove first
    if (event.active) {
        heapRemove(event.heapIndex);
        event.active = false;
        event.heapIndex = -1;
    }

    event.timestamp = m_now + delay;
    event.active = true;
    heapPush(&event);
}

void Scheduler::cancel(SchedulerEvent& event) {
    if (!event.active) return;
    heapRemove(event.heapIndex);
    event.active = false;
    event.heapIndex = -1;
}

void Scheduler::processEvents(u64 target) {
    while (m_heapSize > 0 && m_heap[0]->timestamp <= target) {
        SchedulerEvent* event = m_heap[0];
        m_now = event->timestamp;

        // Remove from heap before calling callback
        // (callback may reschedule this or other events)
        heapRemove(0);
        event->active = false;
        event->heapIndex = -1;

        if (event->callback) {
            event->callback(event->context, event->userData);
        }
    }
}

// --- Min-heap operations ---

void Scheduler::heapPush(SchedulerEvent* event) {
    assert(m_heapSize < SCHEDULER_MAX_EVENTS);
    int index = m_heapSize++;
    m_heap[index] = event;
    event->heapIndex = index;
    heapSiftUp(index);
}

void Scheduler::heapRemove(int index) {
    assert(index >= 0 && index < m_heapSize);
    int last = --m_heapSize;

    if (index == last) {
        return;
    }

    heapSwap(index, last);

    // Fix heap: try sifting up first, then down
    if (index > 0 && m_heap[index]->timestamp < m_heap[(index - 1) / 2]->timestamp) {
        heapSiftUp(index);
    } else {
        heapSiftDown(index);
    }
}

void Scheduler::heapSiftUp(int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (m_heap[index]->timestamp < m_heap[parent]->timestamp) {
            heapSwap(index, parent);
            index = parent;
        } else {
            break;
        }
    }
}

void Scheduler::heapSiftDown(int index) {
    while (true) {
        int smallest = index;
        int left = 2 * index + 1;
        int right = 2 * index + 2;

        if (left < m_heapSize && m_heap[left]->timestamp < m_heap[smallest]->timestamp) {
            smallest = left;
        }
        if (right < m_heapSize && m_heap[right]->timestamp < m_heap[smallest]->timestamp) {
            smallest = right;
        }

        if (smallest != index) {
            heapSwap(index, smallest);
            index = smallest;
        } else {
            break;
        }
    }
}

void Scheduler::heapSwap(int i, int j) {
    SchedulerEvent* tmp = m_heap[i];
    m_heap[i] = m_heap[j];
    m_heap[j] = tmp;
    m_heap[i]->heapIndex = i;
    m_heap[j]->heapIndex = j;
}

void Scheduler::saveState(Buffer* buf) {
    buffer_write(buf, &m_now, sizeof(m_now));
}

void Scheduler::loadState(Buffer* buf) {
    // Cancel all current events
    for (int i = 0; i < m_heapSize; i++) {
        m_heap[i]->active = false;
        m_heap[i]->heapIndex = -1;
    }
    m_heapSize = 0;
    buffer_read(buf, &m_now, sizeof(m_now));
}
