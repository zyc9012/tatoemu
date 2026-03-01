#pragma once

#include "../types.h"
#include <climits>

struct Buffer;

constexpr int SCHEDULER_MAX_EVENTS = 64;

struct SchedulerEvent {
    u64 timestamp = 0;
    void (*callback)(void* context, int userData) = nullptr;
    void* context = nullptr;
    int userData = 0;
    int heapIndex = -1;
    bool active = false;
};

class Scheduler {
public:
    Scheduler() = default;

    void reset();

    u64 now() const { return m_now; }
    u64 nextEventTime() const;
    int cyclesUntilNextEvent() const;

    // Advance time by `cycles`, firing all due events in order
    void addCycles(int cycles);

    // Schedule event to fire after `delay` cycles from now.
    // If the event is already scheduled, it is rescheduled.
    void schedule(SchedulerEvent& event, int delay);

    // Cancel a previously scheduled event (no-op if not active)
    void cancel(SchedulerEvent& event);

    bool isScheduled(const SchedulerEvent& event) const { return event.active; }

    // Save/Load (scheduler timestamp only; components reschedule their own events)
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    void processEvents(u64 target);

    // Min-heap operations
    void heapPush(SchedulerEvent* event);
    void heapRemove(int index);
    void heapSiftUp(int index);
    void heapSiftDown(int index);
    void heapSwap(int i, int j);

    SchedulerEvent* m_heap[SCHEDULER_MAX_EVENTS] = {};
    int m_heapSize = 0;
    u64 m_now = 0;
};
