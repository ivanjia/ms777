#pragma once
#include <memory>

namespace ms777 {
template <typename T>
class SpIntrusiveList
{
public:
    class Hook
    {
    public:
        Hook() = default;
        Hook(const Hook &) = default;

        Hook &operator=(const Hook &)
        {
            return *this;
        }

    private:
        friend class SpIntrusiveList<T>;
        std::weak_ptr<T> prev_;
        std::shared_ptr<T> next_;
    };

    SpIntrusiveList() : size_(0)
    {
    }

    ~SpIntrusiveList()
    {
        clear();
    }

    const std::shared_ptr<T> &front() const
    {
        return front_;
    }

    static std::shared_ptr<T> prev(const std::shared_ptr<T> &t)
    {
        return getHook(*t).prev_.lock();
    }

    static const std::shared_ptr<T> &next(const std::shared_ptr<T> &t)
    {
        return getHook(*t).next_;
    }

    void addFront(const std::shared_ptr<T> &t)
    {
        Hook &tHook = getHook(*t);
        tHook.next_ = front_;
        if(front_) {
            Hook &frontHook = getHook(*front_);
            frontHook.prev_ = t;
        }
        front_ = t;
        ++size_;
    }

    void erase(const std::shared_ptr<T> &t)
    {
        Hook &tHook = getHook(*t);
        if(t == front_) {
            front_ = tHook.next_;
        }
        const std::shared_ptr<T> prev = tHook.prev_.lock();
        if(prev) {
            Hook &prevHook = getHook(*prev);
            prevHook.next_ = tHook.next_;
        }
        if(tHook.next_) {
            Hook &nextHook = getHook(*tHook.next_);
            nextHook.prev_ = tHook.prev_;
        }
        tHook.prev_.reset();
        tHook.next_.reset();
        --size_;
    }

    void clear()
    {
        while(front_) {
            Hook &frontHook = getHook(*front_);
            std::shared_ptr<T> tmp;
            tmp.swap(frontHook.next_);
            frontHook.prev_.reset();
            front_.swap(tmp);
        }
        size_ = 0;
    }

    std::size_t size() const
    {
        return size_;
    }

    bool empty() const
    {
        return 0 == size_;
    }

private:
    static Hook &getHook(T &t)
    {
        return static_cast<Hook &>(t);
    }

private:
    std::size_t size_;
    std::shared_ptr<T> front_;
};
}
