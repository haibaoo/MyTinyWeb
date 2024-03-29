#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <assert.h>
#include <condition_variable>

/**
 * 模板类的定义方式：
 * 这里相当于是将std::deque封装了一层，封装成为线程安全的双端队列
*/
template <class T>
class BlockDeque{
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);//不允许隐式转换，注意这不是默认构造、默认构造没参数

    ~BlockDeque();

    void clear();

    bool empty();
    
    bool full();

    void Close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);

    void push_front(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque<T> deq_;

    size_t capacity_;

    std::mutex mtx_;

    bool isClose_;

    std::condition_variable condConsumer_;
    
    std::condition_variable condProducer_;

};


template <class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity):capacity_(Maxcapacity){
    assert(MaxCapacity);
    isClose_ = false;
}

template <class T>
BlockDeque<T>::~BlockDeque(){
    Close();
}

template <class T>//
void BlockDeque<T>::Close(){
    {
        std::lock_guard(std::mutex) locker(mtx_);//lock_guard自动申请释放mtx
        deq_.clear();
        isClose_ = true;
    }
    condProducer_.notify_all();
    condConsumer_.notify_all();
}

template <class T>
void BlockDeque<T>::flush(){
    condConsumer_.notify_one();
}

template <class T>//
void BlockDeque<T>::clear(){
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

template <class T>
T BlockDeque<T>::front(){//front的元素
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

template <class T>
T BlockDeque<T>::back(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

template <class T>
void BlockDeque<T>::push_back(const T &item){
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_){
        condProducer_.wait(locker)//会自动解锁unique_lock
    }
    deq_.push_back(item);
    condConsumer_.notify_one();
}   

template <class T>
void BlockDeque<T>::push_front(const T &item){
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_){
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}


template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

/**
 * @param item 存放弹出的元素
*/
template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        condConsumer_.wait(locker);
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

/**
 * @param item 存放弹出的元素
 * 
 * @param timeout 只等待timeout秒
*/
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout))//带有超时保护的条件变量
                == std::cv_status::timeout){
            return false;
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}


#endif