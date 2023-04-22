#pragma once
#include <functional>
#include <iostream>
extern "C" {
#include <libavutil\mem.h>     //for av_mallocz and av_free
}

using namespace std;

// define a datapacket for a frame
class DataPacket
{
private:
    unsigned char*   DataPtr;
    int              Capacity;
    int              Size;

    int              RefID;

public:
    // constructor
    DataPacket(const int sz = 0) :
        DataPtr(nullptr), Capacity(0), Size(0), RefID(0xFFFFFFFF)
    {
        resize(sz);
    }

    // copy constructor
    DataPacket(const DataPacket& d):
        DataPtr(nullptr), Capacity(0), Size(0)
    {
        resize(d.Size);
        if (d.Size != 0) {
            memcpy(DataPtr, d.DataPtr, d.Size);
        }

        RefID = d.RefID;
    }
    // move constructor
    DataPacket(DataPacket&& d) :
        DataPtr(nullptr), Capacity(0), Size(0), RefID(0xFFFFFFFF)
    {
        swap(d);
    }

    ~DataPacket() {
        if (DataPtr != nullptr) {
            av_free(DataPtr);
            DataPtr = nullptr;
            Capacity = 0;
            Size = 0;
        }
    }

    int ref_id(void) const {
        return RefID;
    }

    void set_ref_id(int id) {
        RefID = id;
    }

    void resize(const int& sz) {
        if (sz < 0) {
            cout << "invalid data packet size" << endl;
            return;
        }
        if (sz > Capacity) {
            if (DataPtr != nullptr) {
                av_free(DataPtr);
                Capacity = 0;
                DataPtr = nullptr;
            }
            if (sz > 0) {
                DataPtr = (uint8_t*)av_mallocz(sz);
                Capacity = sz;
            }
        }
        Size = sz;
    }

    void clear()
    {
        resize(0);
    }

    void copy(const uint8_t* buff, const int& sz) {
        if (sz <= 0) return;
        if (sz > Size) {
            resize(sz);
        }

        memcpy(DataPtr, buff, sz);
    }

    bool empty(void) const {
        return (Size == 0);
    }

    int size(void) const {
        return Size;
    }

    uint8_t* data_ptr(void) {
        return DataPtr;
    }

    void swap(DataPacket& d) {
        std::swap(DataPtr, d.DataPtr);
        std::swap(Size, d.Size);
        std::swap(Capacity, d.Capacity);
        std::swap(RefID, d.RefID);
    }
};


/// <summary>
/// a queue for datapacket buffer
/// </summary>
/// <typeparam name="sz"></typeparam>
template<std::size_t sz>
class PacketCircularQueue {
private:
    const std::size_t BufferSize; // buffer max size
    std::size_t DataSize;  // buffer current size
    std::size_t DataFront;
    std::size_t DataRear;
public:
    PacketCircularQueue() :
        DataSize(0), BufferSize(sz), DataFront(0), DataRear(sz - 1)
    {
    }

    size_t size() const {
        return DataSize;
    }

    size_t capacity() const {
        return BufferSize;
    }

    bool full() const {
        return (DataSize == BufferSize);
    }

    bool empty() const {
        return (DataSize == 0);
    }
    /* Insert a new element from the end of the queue and return its position (logical) */
    size_t push_back() {
        ++DataRear;
        if (DataRear == BufferSize) {
            DataRear = 0;
        }
        if (DataSize == BufferSize) {
            pop_front();
        }
        else {
            ++DataSize;
        }
        return DataRear;
    }

    /* Remove an element from the head of the queue and return its position */
    size_t pop_front() {
        size_t pos = 0xFFFFFFFF;
        if (DataSize > 0) {
            pos = DataFront;
            ++DataFront;
            if (DataFront == BufferSize) {
                DataFront = 0;
            }
            --DataSize;
        };
        return pos;
    }

    size_t front() const {
        return DataFront;
    }

    size_t rear() const {
        return DataRear;
    }
};