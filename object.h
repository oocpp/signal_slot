#pragma once
#include <vector>
#include <algorithm>
#include <utility>
#include <atomic>
#include <thread>
#include <cassert>

/// <summary>
/// 暂不支持多线程。
/// 发送信号或接收信号的类需要继承自 Object。
/// 在类中使用 Signal(signal_name, type1, type2, ...) 定义信号。
/// 发生信号 emit this->signal_name(arg1, arg2)。
/// 
/// 连接信号使用
/// this->signal_name.connect(slot)
/// 可以连接Lambda，函数对象，普通函数指针。
/// 
/// this->signal_name.connect(obj, slot)
/// 可以连接成员函数，信号，Lambda，函数对象，普通函数指针。
/// 在obj对象析构时，此信号槽连接会自动断开。
/// 
/// 断开信号
/// this->disconnect()
/// 断开this连接的所有信号。
/// 
/// this->disconnect(obj)
/// 断开this连接的所有来自obj信号。
/// 
/// this->signal_name.disconnect()
/// 断开此信号的所有连接。
/// 
/// this->signal_name.disconnect(obj)
/// 断开此信号与obj的所有连接。
/// 
/// this->signal_name.disconnect(slot)
/// 断开此信号与某个槽的单个连接。(通过 this->signal_name.connect(slot) 连接的槽， 槽对象必须是可以比较相等的)。
/// 
/// this->signal_name.disconnect(obj, slot)
/// 断开此信号与某个槽的单个连接。(通过 this->signal_name.connect(obj, slot) 连接的槽， 槽对象必须是可以比较相等的)。
/// 
/// Object* sender() 
/// 获取当前的信号 sender
/// 
/// 辅助方法
/// overload<>
/// constOverload<>
/// nonConstOverload<>
/// 取重载函数的指针。例如 overload<int>(&func), overload<int>(&Class::func)。
/// </summary>


class Object;

namespace objectImpl
{
    template<typename ...Args>
    struct List {
        static constexpr size_t size = sizeof...(Args);
    };

    template<>
    struct List<void> {
        static constexpr size_t size = 0;
    };

    template <size_t N, typename FirstType, typename... Types, typename... SlectTypes>
    constexpr auto List_Left_Impl(List<FirstType, Types...>, List<SlectTypes...>) {
        if constexpr (N == 0) {
            return List<SlectTypes...>{};
        }
        else {
            return List_Left_Impl<N - 1>(List<Types...>{}, List<SlectTypes..., FirstType>{});
        }
    }

    template<typename Types, size_t N>
    constexpr auto List_Left() {
        if constexpr (Types::size == N) {
            return Types{};
        }
        else if constexpr (N == 1 && Types::size < 1) {
            static_assert(false, "A compilation error has occurred somewhere ahead.");
            return List<>{};
        }
        else {
            return List_Left_Impl<N>(Types{}, List<>{});
        }
    }

    template<typename T>
    using remove_rv_t = std::remove_volatile_t<std::remove_reference_t<T>>;

    template<typename T>
    using remove_rcv_t = std::remove_cv_t<std::remove_reference_t<T>>;

    template<typename T, typename = void>
    struct hasEqualOperator : public std::false_type {
        static constexpr bool value = false;
    };

    template<typename T>
    struct hasEqualOperator<T, std::void_t<decltype(std::declval<remove_rcv_t<T>>() == std::declval<remove_rcv_t<T>>())>>
        :public std::true_type {
    };

    template <typename List1, typename List2>
    struct checkCompatibleArguments :public std::false_type {
    };

    template <typename... Args1, typename... Args2>
    struct checkCompatibleArguments<List<Args1...>, List<Args2...>>
    {
        static constexpr bool value = (... && std::is_convertible_v<Args1, Args2>);
    };

    template <typename Func, typename Args, typename = void>
    struct canInvokable :public std::false_type {
    };

    template <typename Func, typename... Args>
    struct canInvokable<Func, List<Args...>, std::void_t<std::invoke_result_t<Func, Args...>>>
        :public std::true_type {
    };

    template <typename Func, typename Args, bool = canInvokable<Func, Args>::value>
    struct ComputeFunctorArgument;

    template <typename Func, typename... Args>
    struct ComputeFunctorArgument<Func, List<Args...>, false> {
        using _T = ComputeFunctorArgument<Func, decltype(List_Left<List<Args...>, sizeof...(Args) - 1>())>;
        using type = typename _T::type;
        static constexpr int value = _T::value;
    };

    template <typename Func>
    struct ComputeFunctorArgument<Func, List<>, false> {
        using type = List<>;
        static constexpr int value = -1;
    };

    template <typename Func, typename...Args>
    struct ComputeFunctorArgument<Func, List<Args...>, true> {
        using type = List<Args...>;
        static constexpr int value = (int)type::size;
    };


    enum class CallableObjectType {
        Function,
        MemberFuncion,
        FuncionObject,
        Signal
    };

    template<typename Func>
    struct CallableObject
    {
        static constexpr bool isCallable = true;
        using FunctionType = Func;
        using ObjectType = const Object;
        static constexpr CallableObjectType callableOjectType = CallableObjectType::FuncionObject;

        template<size_t... Index, typename... SigArgs>
        static void call(FunctionType& func, ObjectType*, void** arg, List<SigArgs...>, std::index_sequence<Index...>) {
            func((*reinterpret_cast<std::remove_reference_t<SigArgs>*>(arg[Index]))...);
        }
    };

    template<typename... Args>
    class SignalImpl;

    template<typename Obj, typename ...Args>
    struct CallableObject<SignalImpl<Args...> Obj::*>
    {
        static constexpr bool isCallable = std::is_base_of_v<Object, Obj>;

        using FunctionType = SignalImpl<Args...> Obj::*;
        using ObjectType = Obj;
        using ArguementTypes = List<Args...>;
        static constexpr CallableObjectType callableOjectType = CallableObjectType::Signal;

        template<size_t... Index, typename... SigArgs>
        static void call(FunctionType sig, ObjectType* obj, void** arg, List<SigArgs...>, std::index_sequence<Index...>) {
            (obj->*sig)((*reinterpret_cast<std::remove_reference_t<SigArgs>*>(arg[Index]))...);
        }
    };

    template<typename Obj, typename ...Args>
    struct CallableObject<const SignalImpl<Args...> Obj::*>
    {
        static constexpr bool isCallable = std::is_base_of_v<Object, Obj>;

        using FunctionType = const SignalImpl<Args...> Obj::*;
        using ObjectType = const Obj;
        using ArguementTypes = List<Args...>;
        static constexpr CallableObjectType callableOjectType = CallableObjectType::Signal;

        template<size_t... Index, typename... SigArgs>
        static void call(FunctionType sig, ObjectType* obj, void** arg, List<SigArgs...>, std::index_sequence<Index...>) {
            (obj->*sig)((*reinterpret_cast<std::remove_reference_t<SigArgs>*>(arg[Index]))...);
        }
    };

    template<typename Obj, typename Ret, typename ...Args>
    struct CallableObject<Ret(Obj::*)(Args...)>
    {
        static constexpr bool isCallable = std::is_base_of_v<Object, Obj>;
        static constexpr bool isConstMemberFunction = false;
        using FunctionType = Ret(Obj::*)(Args...);
        using ObjectType = Obj;
        using ArguementTypes = List<Args...>;
        static constexpr CallableObjectType callableOjectType = CallableObjectType::MemberFuncion;

        template<size_t... Index, typename... SigArgs>
        static void call(FunctionType func, ObjectType* obj, void** arg, List<SigArgs...>, std::index_sequence<Index...>) {
            (obj->*func)((*reinterpret_cast<std::remove_reference_t<SigArgs>*>(arg[Index]))...);
        }
    };

    template<typename Obj, typename Ret, typename ...Args>
    struct CallableObject<Ret(Obj::*)(Args...) const>
    {
        static constexpr bool isCallable = std::is_base_of_v<Object, Obj>;
        static constexpr bool isConstMemberFunction = true;
        using FunctionType = Ret(Obj::*)(Args...) const;
        using ObjectType = const Obj;
        using ArguementTypes = List<Args...>;
        static constexpr CallableObjectType callableOjectType = CallableObjectType::MemberFuncion;

        template<size_t... Index, typename... SigArgs>
        static void call(FunctionType func, ObjectType* obj, void** arg, List<SigArgs...>, std::index_sequence<Index...>) {
            (obj->*func)((*reinterpret_cast<std::remove_reference_t<SigArgs>*>(arg[Index]))...);
        }
    };

    template<typename Obj, typename Ret, typename ...Args>
    struct CallableObject<Ret(Obj::*)(Args...) const noexcept>
    {
        static constexpr bool isCallable = std::is_base_of_v<Object, Obj>;
        static constexpr bool isConstMemberFunction = true;
        using FunctionType = Ret(Obj::*)(Args...) const noexcept;
        using ObjectType = const Obj;
        using ArguementTypes = List<Args...>;
        static constexpr CallableObjectType callableOjectType = CallableObjectType::MemberFuncion;

        template<size_t... Index, typename... SigArgs>
        static void call(FunctionType func, ObjectType* obj, void** arg, List<SigArgs...>, std::index_sequence<Index...>) {
            (obj->*func)((*reinterpret_cast<std::remove_reference_t<SigArgs>*>(arg[Index]))...);
        }
    };

    template<typename Obj, typename Ret, typename ...Args>
    struct CallableObject<Ret(Obj::*)(Args...) noexcept>
    {
        static constexpr bool isCallable = std::is_base_of_v<Object, Obj>;
        static constexpr bool isConstMemberFunction = false;
        using FunctionType = Ret(Obj::*)(Args...) noexcept;
        using ObjectType = Obj;
        using ArguementTypes = List<Args...>;
        static constexpr CallableObjectType callableOjectType = CallableObjectType::MemberFuncion;

        template<size_t... Index, typename... SigArgs>
        static void call(FunctionType func, ObjectType* obj, void** arg, List<SigArgs...>, std::index_sequence<Index...>) {
            (obj->*func)((*reinterpret_cast<std::remove_reference_t<SigArgs>*>(arg[Index]))...);
        }
    };

    template<typename Ret, typename ...Args>
    struct CallableObject<Ret(*)(Args...)>
    {
        static constexpr bool isCallable = true;
        using FunctionType = Ret(*)(Args...);
        using ObjectType = const Object;
        using ArguementTypes = List<Args...>;
        static constexpr CallableObjectType callableOjectType = CallableObjectType::Function;

        template<size_t... Index, typename... SigArgs>
        static void call(FunctionType func, ObjectType*, void** arg, List<SigArgs...>, std::index_sequence<Index...>) {
            func((*reinterpret_cast<std::remove_reference_t<SigArgs>*>(arg[Index]))...);
        }
    };

    template<typename Ret, typename ...Args>
    struct CallableObject<Ret(*)(Args...) noexcept>
    {
        static constexpr bool isCallable = true;
        using FunctionType = Ret(*)(Args...) noexcept;
        using ObjectType = const Object;
        using ArguementTypes = List<Args...>;
        static constexpr CallableObjectType callableOjectType = CallableObjectType::Function;

        template<size_t... Index, typename... SigArgs>
        static void call(FunctionType func, ObjectType*, void** arg, List<SigArgs...>, std::index_sequence<Index...>) {
            func((*reinterpret_cast<std::remove_reference_t<SigArgs>*>(arg[Index]))...);
        }
    };

    class SlotObjectBase {
        // don't use virtual functions here; we don't want the
        // compiler to create tons of per-polymorphic-class stuff that
        // we'll never need. We just use one function pointer.
        typedef void (*ImplFn)(int which, SlotObjectBase* this_, Object* receiver, void** args, bool* ret);
        ImplFn const m_impl;
    protected:
        enum Operation {
            Call,
            Compare,
        };
    public:
        explicit SlotObjectBase(ImplFn fn) : m_impl(fn) {}
        inline bool compare(void** a) { bool ret = false; m_impl(Compare, this, nullptr, a, &ret); return ret; }
        inline void call(Object* r, void** a) { m_impl(Call, this, r, a, nullptr); }
        ~SlotObjectBase() {}
    };

    template<typename Func, typename SigArgs>
    class SlotObject : public SlotObjectBase
    {
        Func function;
        static void impl(int which, SlotObjectBase* this_, Object* recv, void** a, bool* ret)
        {
            SlotObject* _this = static_cast<SlotObject*>(this_);
            switch (which) {
            case Call:
                using FunctionInfo = CallableObject<Func>;
                FunctionInfo::call(_this->function, static_cast<typename FunctionInfo::ObjectType*>(recv), a
                    , SigArgs{}, std::make_index_sequence<SigArgs::size>());
                break;
            case Compare:
                if constexpr (hasEqualOperator<Func>::value) {
                    *ret = *reinterpret_cast<Func*>(a) == _this->function;
                }
            }
        }
    public:
        template<typename T>
        explicit SlotObject(T&& f) : SlotObjectBase(&impl), function(std::forward<T>(f)) {
        }
    };

    struct Connection
    {
        Connection(Object* sender, const Object* recver, SlotObjectBase* slot)
            :sender(sender), recver(const_cast<Object*>(recver)), slot(slot)
        {
        }

        ~Connection() {
            delete slot;
            slot = nullptr;
        }

        bool release() {
            int expect = 2;
            if (!ref.compare_exchange_strong(expect, 1)) {
                if (expect == 1) {
                    if (ref.compare_exchange_strong(expect, 0)) {
                        delete this;
                        return true;
                    }
                }
            }
            return false;
        }

        std::atomic<int> ref = 2;
        Object* recver = nullptr;
        Object* sender = nullptr;
        SlotObjectBase* slot = nullptr;
    };

    inline static thread_local Object* g_currentSender = nullptr;
    struct SenderGuard {
        explicit SenderGuard(Object* sender) noexcept {
            old_sender = g_currentSender;
            g_currentSender = sender;
        }

        ~SenderGuard() noexcept {
            g_currentSender = old_sender;
        }
        Object* old_sender = nullptr;
    };

    inline bool addConnection(Object* obj, Connection* conn);

    template<typename... Args>
    class SignalImpl {
        using SigArgs = List<Args...>;

    public:
        SignalImpl(const SignalImpl&) = delete;
        SignalImpl& operator=(const SignalImpl&) = delete;
        explicit SignalImpl(Object* parent) noexcept :m_parent(parent) {}

        ~SignalImpl() {
            disconnect();
            assert(m_nested == 0);
        }

        void operator()(Args...args) const {
            const_cast<SignalImpl*>(this)->operator()(args...);
        }

        void operator()(Args...args) {
            SenderGuard sender(m_parent);
            void* _a[] = { const_cast<void*>(reinterpret_cast<const void*>(&args))..., 0 };
            size_t count = 0;
            ++m_nested;
            for (auto& conn : m_conns) {
                if (!conn) {
                    ++count;
                    continue;
                }

                if (conn->ref == 2) {
                    conn->slot->call(conn->recver, _a);
                }
                else {
                    if (conn->release()) {
                        conn = nullptr;
                    }
                }
            }

            --m_nested;
            if (m_nested == 0) {
                if (count > m_conns.size() * 0.2) {
                    auto iter = std::remove(m_conns.begin(), m_conns.end(), (Connection*)(nullptr));
                    m_conns.erase(iter, m_conns.end());
                }

                if (m_waitForConns && !m_waitForConns->empty()) {
                    for (auto& conn : *m_waitForConns) {
                        m_conns.push_back(conn);
                    }
                    delete m_waitForConns;
                    m_waitForConns = nullptr;
                }
            }
        }

        bool disconnect() {
            for (auto& conn : m_conns) {
                if (conn) {
                    if (!conn->recver) {
                        delete conn;
                    }
                    else {
                        conn->release();
                    }
                    conn = nullptr;
                }
            }

            if (!m_conns.empty()) {
                if (m_nested == 0) {
                    m_conns.clear();
                }
                return true;
            }
            return false;
        }

        bool disconnect(const Object* obj) {
            return disconnectImpl(obj);
        }

        template<typename Slot>
        bool disconnect(const typename CallableObject<remove_rv_t<Slot>>::ObjectType* obj, const Slot& slot) {
            void** _a = reinterpret_cast<void**>(const_cast<void*>(reinterpret_cast<const void*>(&slot)));
            return disconnectImpl(obj, _a);
		}

		template<typename Slot>
		bool disconnect(const typename CallableObject<remove_rv_t<Slot>>::ObjectType& obj, const Slot& slot) {
            return disconnect(&obj, slot);
		}

		template<typename Slot>
		std::enable_if_t<!std::is_base_of_v<Object, std::remove_pointer_t<remove_rcv_t<Slot>>>, bool> 
            disconnect(const Slot& slot) {
            using _CallableObject = CallableObject<remove_rv_t<Slot>>;
			return disconnect(static_cast<typename _CallableObject::ObjectType*>(nullptr), slot);
		}

        template<typename Slot>
        bool connect(typename CallableObject<remove_rv_t<Slot>>::ObjectType* recv, Slot&& slot) {
            using _Slot = remove_rv_t<Slot>;
            using _CallableObject = CallableObject<_Slot>;
            static_assert(_CallableObject::isCallable, "slot is not a callable object");

            if constexpr (_CallableObject::callableOjectType != CallableObjectType::FuncionObject) {
                static_assert(SigArgs::size >= _CallableObject::ArguementTypes::size, "The slot requires more arguments than the signal provides.");

                using LeftSigArgs = decltype(List_Left<SigArgs, _CallableObject::ArguementTypes::size>());
                if constexpr (LeftSigArgs::size > 0) {
                    static_assert(checkCompatibleArguments<LeftSigArgs, typename _CallableObject::ArguementTypes>::value,
                        "Signal and slot arguments are not compatible.");
                }
                return createConnect<LeftSigArgs, Slot>(recv, std::forward<Slot>(slot));
            }
            else {
                using types = ComputeFunctorArgument<Slot, SigArgs>;
                static_assert(types::value >= 0, "Signal and slot arguments are not compatible. There is no operator() overload that can be called.");
                return createConnect<typename types::type, Slot>(recv, std::forward<Slot>(slot));
            }
        }

        template<typename Slot>
        bool connect(typename CallableObject<remove_rv_t<Slot>>::ObjectType& recv, Slot&& slot) {
            return connect(&recv, std::forward<Slot>(slot));
        }

        template<typename Slot>
        bool connect(Slot&& slot) {
            using _CallableObject = CallableObject<remove_rv_t<Slot>>;
            static_assert(_CallableObject::callableOjectType != CallableObjectType::MemberFuncion, "member function can not use this connect.");
            static_assert(_CallableObject::callableOjectType != CallableObjectType::Signal, "signal can not use this connect.");
            return connect(static_cast<typename _CallableObject::ObjectType*>(nullptr), std::forward<Slot>(slot));
        }

        template<typename Obj, typename Slot>
        std::enable_if_t<!std::is_base_of_v<Object, remove_rcv_t<std::remove_pointer_t<Obj>>>, bool>
            connect(Obj&& recv, Slot&& slot) const {
            static_assert(false, "The first parameter is not a subclass of Object");
        }

        template<typename Slot>
        bool connect(typename CallableObject<remove_rv_t<Slot>>::ObjectType* recv, Slot&& slot) const {
            static_assert(false, "signal cannot be constant member");
        }

        template<typename Slot>
        bool connect(typename CallableObject<remove_rv_t<Slot>>::ObjectType& recv, Slot&& slot) const {
            static_assert(false, "signal cannot be constant member");
        }

        template<typename Slot>
        bool connect(Slot&& slot) const {
            static_assert(false, "signal cannot be constant member");
        }

    private:
		bool disconnectImpl(const Object* obj, void** arg = nullptr) {
			bool res = false;
			for (auto& conn : m_conns) {
				if (!conn) {
					continue;
				}

				if (conn->recver == obj) {
					if (!arg || conn->slot->compare(arg)) {
						conn->release();
						conn = nullptr;
						res = true;
                        if (arg) {
                            break;
                        }
					}
				}
			}
			return res;
		}

        template<typename SigArgs, typename Slot>
        inline bool createConnect(const Object* obj, Slot&& slot) {
            using _Slot = remove_rv_t<Slot>;
            auto slotObj = new SlotObject<_Slot, SigArgs>(std::forward<Slot>(slot));
            auto conn = new Connection(m_parent, obj, slotObj);
            if (obj) {
                addConnection(const_cast<Object*>(obj), conn);
            }

            if (m_nested == 0) {
				if (m_conns.size() >= m_conns.capacity()) {
					auto iter = std::remove(m_conns.begin(), m_conns.end(), (Connection*)(nullptr));
					m_conns.erase(iter, m_conns.end());
				}
                m_conns.push_back(conn);
            }
            else {
                if (!m_waitForConns) {
                    m_waitForConns = new std::vector<Connection*>{};
                }
                m_waitForConns->push_back(conn);
            }

            return true;
        }

    private:
        size_t m_nested = 0;
        std::vector<Connection*> m_conns;
        std::vector<Connection*>* m_waitForConns = nullptr;
        Object* const m_parent;
    };

    template<>
    class SignalImpl<void> :public SignalImpl<> {
        using SignalImpl<>::SignalImpl;
    };

    template <typename... Args>
    struct NonConstOverload
    {
        template <typename R, typename T>
        constexpr auto operator()(R(T::* ptr)(Args...)) const noexcept -> decltype(ptr)
        {
            return ptr;
        }

        template <typename R, typename T>
        constexpr auto operator()(R(T::* ptr)(Args...) noexcept) const noexcept -> decltype(ptr)
        {
            return ptr;
        }
    };

    template <typename... Args>
    struct ConstOverload
    {
        template <typename R, typename T>
        constexpr auto operator()(R(T::* ptr)(Args...) const) const noexcept -> decltype(ptr)
        {
            return ptr;
        }

        template <typename R, typename T>
        constexpr auto operator()(R(T::* ptr)(Args...)const noexcept) const noexcept -> decltype(ptr)
        {
            return ptr;
        }
    };

    template <typename... Args>
    struct Overload : ConstOverload<Args...>, NonConstOverload<Args...>
    {
        using ConstOverload<Args...>::operator();
        using NonConstOverload<Args...>::operator();

        template <typename R>
        constexpr auto operator()(R(*ptr)(Args...)) const noexcept -> decltype(ptr)
        {
            return ptr;
        }

        template <typename R>
        constexpr auto operator()(R(*ptr)(Args...) noexcept) const noexcept -> decltype(ptr)
        {
            return ptr;
        }
    };

    template <>
    struct ConstOverload<void> :public ConstOverload<> {
    };

    template <>
    struct NonConstOverload<void> :public NonConstOverload<> {
    };

    template <>
    struct Overload<void> :public Overload<> {
    };
}

inline Object* sender() {
    return objectImpl::g_currentSender;
}

#define Signal(name, ...) objectImpl::SignalImpl<__VA_ARGS__> name{this};
#define emit
#define slots

class Object {
public:
    explicit Object(Object* parent = nullptr) : m_parent(parent) {
        if (parent) {
            parent->m_children.push_back(this);
        }
    }

    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    virtual ~Object() {
        emit destory(this);
        for (auto child : m_children) {
            delete child;
        }

        disconnect();
    }

    bool disconnect(const Object* obj) {
        if (!obj) {
            return false;
        }

        bool res = false;
		for (auto& conn : m_connections) {
            if (conn && conn->sender == obj) {
                conn->release();
                conn = nullptr;
                res = true;
            }
		}
        return res;
	}

    bool disconnect(){
        for (auto& conn : m_connections) {
            conn->release();
        }
        bool res = !m_connections.empty();
        m_connections.clear();
        return res;
    }

    Signal(destory, Object*)
private:
    bool addConnection(objectImpl::Connection* conn) {
        if (m_connections.size() >= m_connections.capacity()) {
            auto iter = std::remove(m_connections.begin(), m_connections.end(), (objectImpl::Connection*)(nullptr));
            m_connections.erase(iter, m_connections.end());
        }

        m_connections.push_back(conn);
        return true;
    }
private:
    friend bool objectImpl::addConnection(Object* obj, objectImpl::Connection* conn);

    Object* m_parent = nullptr;
    std::vector<objectImpl::Connection*> m_connections;
    std::vector<Object*> m_children;
};

namespace objectImpl {
    bool addConnection(Object* obj, Connection* conn) {    
        return obj->addConnection(conn);
    }
}

template <typename... Args>
constexpr objectImpl::Overload<Args...> overload = {};

template <typename... Args>
constexpr objectImpl::ConstOverload<Args...> constOverload = {};

template <typename... Args>
constexpr objectImpl::NonConstOverload<Args...> nonConstOverload = {};
