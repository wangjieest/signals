#pragma once
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <vector>
#include <memory>
#pragma pack(push, 8)

namespace base {
namespace detail_for_signals {
struct key_type {
    const void* param1 = nullptr;  // for this or nothing
    const void* param2 = nullptr;  // for memfun or free fun pointer
    bool operator<(const key_type& rhs) const {
        return (param1 < rhs.param1) || (param2 < rhs.param2);
    }
    bool operator==(const key_type& rhs) const {
        return (param1 == rhs.param1) && (param2 == rhs.param2);
    }
    bool operator==(std::nullptr_t) const { return (param1 == nullptr) && (param2 == nullptr); }
    void clear() {
        param1 = nullptr;
        param2 = nullptr;
    }
    key_type() = default;
    key_type(const key_type& rhs) = default;
    key_type& operator=(const key_type& rhs) = default;
    key_type(const void* p1, const void* p2) : param1(p1), param2(p2) {}
};
template<typename T>
using forward_t = std::conditional_t<std::is_reference_v<T>, T, T&&>;

#define SIGNAL_USE_PROXY 0
#if SIGNAL_USE_PROXY
template<typename T, T>
struct proxy;
template<typename T, typename R, typename... Args, R (T::*mf)(Args...)>
struct proxy<R (T::*)(Args...), mf> {
    static R call(T& obj, Args... args) { return (obj.*mf)(forward_t<Args>(args)...); }
};
#else

template<class TOut, class TIn>
union horrible_union {
    TOut out;
    TIn in;
};
template<class TOut, class TIn>
inline TOut horrible_cast(TIn mIn) noexcept {
    horrible_union<TOut, TIn> u;
    static_assert(sizeof(TIn) == sizeof(u) && sizeof(TIn) == sizeof(TOut),
        "Cannot use horrible_cast<>");
    u.in = mIn;
    return u.out;
}
template<class TOut, class TIn>
inline TOut unsafe_horrible_cast(TIn mIn) noexcept {
    horrible_union<TOut, TIn> u;
    u.in = mIn;
    return u.out;
}
#endif

// hash函数先这样实现,够用就行
template<typename T, typename R, typename... Args>
key_type get_hash(R (T::*const mem_fun)(Args...), T* const obj) {
#if SIGNAL_USE_PROXY
    return key_type{&proxy<decltype(mem_fun), mem_fun>::call, obj};
#else
    return key_type(unsafe_horrible_cast<void*>(mem_fun), horrible_cast<void*>(obj));
#endif
}
template<typename T, typename R, typename... Args>
key_type get_hash(R (T::*const mem_fun)(Args...) const, T const* const obj) {
#if SIGNAL_USE_PROXY
    return key_type{&proxy<decltype(mem_fun), mem_fun>::call, obj};
#else
    return key_type(unsafe_horrible_cast<void*>(mem_fun), horrible_cast<void*>(obj));
#endif
}
template<typename R, typename... Args>
key_type get_hash(R (*const FunctionPtr)(Args...)) {
    return key_type(nullptr, obj);
}

template<typename T, typename R, typename... Args>
std::function<void(Args...)> make_slot(R (T::*const mem_fun)(Args...), T* const obj) {
    return [=](Args... args) { (obj->*mem_fun)(forward_t<Args>(args)...); };
}
template<typename T, typename R, typename... Args>
std::function<void(Args...)> make_slot(R (T::*const mem_fun)(Args...) const, T const* const obj) {
    return [=](Args... args) { (obj->*mem_fun)(forward_t<Args>(args)...); };
}
template<typename R, typename... Args>
std::function<void(Args...)> make_slot(R (*const FunctionPtr)(Args...)) {
    return [=](Args... args) { FunctionPtr(forward_t<Args>(args)...); };
}
}  // namespace detail_for_signals

class connection_t {
  public:
    connection_t() = default;
    connection_t(connection_t&& rhs) : deleter_(std::move(rhs.deleter_)), key_(rhs.key_) {}
    connection_t(const connection_t& rhs) : deleter_(rhs.deleter_), key_(rhs.key_) {}

    connection_t& operator=(connection_t&& rhs) {
        if (rhs.deleter_) {
            deleter_ = std::move(rhs.deleter_);
            key_ = rhs.key_;
        }
#ifdef _DEBUG
        else {
            _ASSERT_EXPR(0, L"连接信号槽失败!请检查!");
        }
#endif
        return *this;
    }
    connection_t& operator=(const connection_t& rhs) {
        if (rhs.deleter_) {
            deleter_ = rhs.deleter_;
            key_ = rhs.key_;
        }
#ifdef _DEBUG
        else {
            _ASSERT_EXPR(0, L"连接信号槽失败!请检查!");
        }
#endif
        return *this;
    }
    connection_t(std::function<void(void)>&& deleter, detail_for_signals::key_type key)
        : deleter_(std::move(deleter)), key_(key) {}

    bool is_valid() const { return deleter_ != 0; }
    void disconnect() {
        if (deleter_) {
            deleter_();
            deleter_ = nullptr;
        }
    }

    bool operator<(const connection_t& rhs) const { return key_ < rhs.key_; }
    detail_for_signals::key_type get_key() const { return key_; }

  private:
    std::function<void(void)> deleter_;
    detail_for_signals::key_type key_;
};

class scoped_connection_t : public connection_t {
  public:
    scoped_connection_t() = default;
    scoped_connection_t(const connection_t& rhs) { connection_t::operator=(rhs); }
    scoped_connection_t(connection_t&& rhs) { connection_t::operator=(std::move(rhs)); }

    scoped_connection_t& operator=(connection_t&& rhs) {
        if (rhs.is_valid()) {
            disconnect();
            connection_t::operator=(std::move(rhs));
        }
#if _DEBUG
        else {
            // 由于重复注册,所以还是保留上一次的信号槽
            _ASSERT_EXPR(0, L"空的connection.重复注册!");
        }
#endif
        return *this;
    }

    scoped_connection_t& operator=(const connection_t& rhs) {
        if (rhs.is_valid()) {
            disconnect();
            connection_t::operator=(rhs);
        }
#if _DEBUG
        else {
            // 由于重复注册,所以还是保留上一次的信号槽
            _ASSERT_EXPR(0, L"空的connection.重复注册!");
        }
#endif
        return *this;
    }

    scoped_connection_t(const scoped_connection_t& rhs) = delete;
    scoped_connection_t& operator=(const scoped_connection_t& rhs) = delete;
    scoped_connection_t(scoped_connection_t&& rhs) : connection_t(std::move(rhs)) {}

    scoped_connection_t& operator=(scoped_connection_t&& rhs) {
        disconnect();
        connection_t::operator=(std::move(rhs));
        return *this;
    }

    ~scoped_connection_t() { disconnect(); }
    bool operator<(const scoped_connection_t& rhs) const { return connection_t::operator<(rhs); }
};

// 只要继承该类, 析构时会自动disconnect连接到该对象上的所有signal.
// 防止野指针回调.
class signal_handle_t {
  public:
    template<typename...>
    friend class signal_t;

    // 断开单个信号的连接
    template<typename... Args>
    void disconnect(signal_t<Args...>& sig) {
        connections_.erase(&sig);
    }
    void disconnect() { connections_.clear(); }

  private:
    using scoped_connection_set_t = std::set<base::scoped_connection_t>;
    bool inner_connect(void* sig, base::scoped_connection_t&& conn) {
        bool ret = true;
        auto it = connections_.find(sig);
        if (it == connections_.end()) {
            it = connections_.emplace(sig, scoped_connection_set_t{}).first;
        }
        auto& conn_set = it->second;
        // 覆盖之前的信号槽
        if (conn_set.find(conn) != conn_set.end()) {
            conn_set.erase(conn);
            ret = false;
        }
        conn_set.emplace(std::move(conn));
        return ret;
    }

    // 可以防止反复用信号槽连接
    std::map<void*, scoped_connection_set_t> connections_;
};

template<typename... Args>
class signal_t {
    using slot_t = std::function<void(Args...)>;

  public:
    bool empty() const { return slot_list_->empty(); }
    void clear() {
        slot_list_.reset(new slot_list_t());
        keys_.reset(new std::vector<detail_for_signals::key_type>());
    }

    signal_t() = default;

    template<typename R, typename T>
    inline connection_t connect(R (T::*const mem_fun)(Args...), T* const obj) {
        return connect_impl(mem_fun, obj, tag_type<T>{});
    }

    template<typename R, typename T>
    inline connection_t connect(R (T::*const mem_fun)(Args...) const, const T* const obj) {
        return connect_impl(mem_fun, obj, tag_type<T>{});
    }

    template<typename R, typename T>
    inline void disconnect(R (T::*const mem_fun)(Args...), T* const obj) {
        auto key = detail_for_signals::get_hash(mem_fun, obj);
        slot_list_->erase(key);
    }

    template<typename R, typename T>
    inline void disconnect(R (T::*const mem_fun)(Args...) const, const T* const obj) {
        auto key = detail_for_signals::get_hash(mem_fun, obj);
        slot_list_->erase(key);
    }

    // free function
    template<typename R, typename T>
    inline connection_t connect(R (*const FunctionPtr)(Args...)) {
        auto key = detail_for_signals::get_hash(FunctionPtr);
        auto slot = detail_for_signals::make_slot(FunctionPtr);
        return connect_custom(key, std::move(slot));
    }

    template<typename R, typename T>
    inline void disconnect(R (*const FunctionPtr)(Args...)) {
        auto key = detail_for_signals::get_hash(FunctionPtr);
        slot_list_->erase(key);
    }

    inline void fire(Args... args) const {
        // add ref
        bool need_erase = false;
        auto slot_list = slot_list_;
        auto keys_ref = keys_;
        auto keys = *keys_ref;
        for (size_t i = 0; i < keys.size(); ++i) {
            auto it = slot_list_->find(keys[i]);
            if (it != slot_list_->end()) {
                it->second(static_cast<Args>(args)...);
            } else {  // 1 先置零
                need_erase = true;
                (*keys_ref)[i].clear();
            }
        }

        if (need_erase) {
            // 2 再擦除
            keys_ref->erase(std::remove(keys_ref->begin(), keys_ref->end(), nullptr),
                        keys_ref->end());
        }
    }
    connection_t connect_custom(detail_for_signals::key_type key, slot_t&& slot) {
        if (!slot_list_->emplace(key, std::move(slot)).second) {
            _ASSERT_EXPR(0, L"重复注册");
            return connection_t(std::function<void(void)>(), detail_for_signals::key_type());
        }
        if (std::find(keys_->begin(), keys_->end(), key) == keys_->end()) {
            keys_->emplace_back(key);
        }
        std::weak_ptr<slot_list_t> wp_slot = slot_list_;

        // clang-format off
        return connection_t(std::function<void(void)>([wp_slot, key] {
            auto slot_list = wp_slot.lock();
            if (slot_list)
                slot_list->erase(key);
        }), key);
        // clang-format on
    }

  private:
    template<typename T>
    using tag_type =
        std::conditional_t<std::is_base_of_v<signal_handle_t, T>, std::true_type, std::false_type>;
    using slot_list_t = std::map<detail_for_signals::key_type, slot_t>;
    std::shared_ptr<slot_list_t> slot_list_ = std::make_shared<slot_list_t>();
    std::shared_ptr<std::vector<detail_for_signals::key_type>> keys_ =
        std::make_shared<std::vector<detail_for_signals::key_type>>();

    template<typename R, typename T>
    inline connection_t connect_impl(R (T::*const mem_fun)(Args...), T* const obj, std::true_type) {
        auto key = detail_for_signals::get_hash(mem_fun, obj);
        auto slot = detail_for_signals::make_slot(mem_fun, obj);
        auto conn = connect_custom(key, std::move(slot));
        obj->inner_connect(this, conn);
        return conn;
    }
    template<typename R, typename T>
    inline connection_t connect_impl(R (T::*const mem_fun)(Args...),
                            T* const obj,
                            std::false_type) {
        auto key = detail_for_signals::get_hash(mem_fun, obj);
        auto slot = detail_for_signals::make_slot(mem_fun, obj);
        return connect_custom(key, std::move(slot));
    }
    template<typename R, typename T>
    inline connection_t connect_impl(R (T::*const mem_fun)(Args...) const,
                            const T* const obj,
                            std::true_type) {
        auto key = detail_for_signals::get_hash(mem_fun, obj);
        auto slot = detail_for_signals::make_slot(mem_fun, obj);
        auto conn = connect_custom(key, std::move(slot));
        obj->inner_connect(this, conn);
        return conn;
    }
    template<typename R, typename T>
    inline connection_t connect_impl(R (T::*const mem_fun)(Args...) const,
                            const T* const obj,
                            std::false_type) {
        auto key = get_hash(obj, mem_fun);
        auto slot = detail_for_signals::make_slot(mem_fun, obj);
        return connect_custom(key, std::move(slot));
    }
};
}  // namespace base

#pragma pack(pop)
// SIGNAL简化宏,一步实现函数声明和成员定义.
// 注意,声明在这个宏之后被被改成protected:
// 最好放到protected:下.
#define SIGNAL_DEFINE_ALL(sig, ...)                \
  public:                                          \
    using sig##_t = base::signal_t<##__VA_ARGS__>; \
    sig##_t& get_sig_##sig() { return $##sig##_; } \
                                                   \
  protected:                                       \
    sig##_t $##sig##_;

// 抽象接口,接口类使用
#define SIGNAL_DEFINE_PURE(sig, ...)               \
    using sig##_t = base::signal_t<##__VA_ARGS__>; \
    virtual sig##_t& get_sig_##sig() = 0;

// 实现接口+函数,直接可以放到private:下
#define SIGNAL_DEFINE_OVERRIDE_IMPL(sig, ...)                     \
    sig##_t& get_sig_##sig() override final { return $##sig##_; } \
    sig##_t $##sig##_;
