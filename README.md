# signal_slot

#### 暂不支持多线程。
#### 发送信号或接收信号的类需要继承自 Object。
#### 在类中使用 Signal(signal_name, type1, type2, ...) 定义信号。
#### 发生信号 emit this->signal_name(arg1, arg2)。
#### 支持Unique连接。
#
#### 连接信号使用
#### this->signal_name.connect(slot)
#### 可以连接Lambda，函数对象，普通函数指针。
#
#### this->signal_name.connect(obj, slot)
#### 可以连接成员函数，信号，Lambda，函数对象，普通函数指针。
#### 在obj对象析构时，此信号槽连接会自动断开。
#
#### 断开信号
#### this->disconnect()
#### 断开this连接的所有信号。
#
#### this->disconnect(obj)
#### 断开this连接的所有来自obj信号。
#
#### this->signal_name.disconnect()
#### 断开此信号的所有连接。
#
#### this->signal_name.disconnect(obj)
#### 断开此信号与obj的所有连接。
#
#### this->signal_name.disconnect(slot)
#### 断开此信号与某个槽的单个连接。(通过 this->signal_name.connect(slot) 连接的槽， 槽对象必须是可以比较相等的)。
#
#### this->signal_name.disconnect(obj, slot)
#### 断开此信号与某个槽的单个连接。(通过 this->signal_name.connect(obj, slot) 连接的槽， 槽对象必须是可以比较相等的)。
#
#### Object* sender() 
#### 获取当前的信号 sender
#
#### 辅助方法
#### overload<>
#### constOverload<>
#### nonConstOverload<>
#### 取重载函数的指针。例如 overload< int >(&func), overload< int >(&Class::func)。
