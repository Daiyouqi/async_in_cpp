////////1664667894@qq.com|Dai Yorki
#pragma once
#include <map>
#include <shared_mutex>
#include <QThread>
#include <set>
class Convenience_QThread : public QThread
{
	Q_OBJECT
public:
	/*外部接口1:添加任务接口*/
	int async_task(std::function<void()>task1, std::function<void()> task2);
	//主接口,方便的将同步代码转成异步代码.第一个调用对象为qthread执行,第二个这个类所在线程执行的回调
	int  sync_task(std::function<void()>task1, std::function<void()> task2);//同步测试代码

	/*外部接口2:任务管理接口,如你不满意可自行添加*/
	void cancel_task(int uuid);//删除指定任务,线程安全
	void cancel_all_task();//删除所有任务,线程安全
	void add_important_mark(int uuid);
	//使一个任务为重要任务,无法被cancel_all_order撤单
	//(这么做的原因是有些类如QTCpSocket锁定线程,在使用时用户在先初始化socket,后发送数据.
	//用户可能有"清除所有的发送数据task,但保留初始化task"的需求

	/*外部接口3,删除接口,异步*/
	void Async_delete();//请连接 delete_finish信号
	//这个类是个QThread子类,在你删除它之前,
	//请先调用这个函数将将其disable掉,这个操作是异步的,异步删除思路为主动取消所有待执行任务,再断开回调执行对象.
        //最后查询当前任务自增ID,添加一个空任务,当这个空任务执行完成后,发送delete_finish信号. 然后,可以按对未执行的QThread的方式删除对象.

	/*外部接口4,用户的异步函数和回调函数调用,检查当前任务是否应该被提前终止*/
	bool check_eligible();

	Convenience_QThread(QThread* parent = nullptr);
	~Convenience_QThread();/*对象可直接delete不崩溃,但不推荐,推荐用Async_delete*/
private:

	const int THREAD_IDLE = -1;
	const int TASK_NEED_TO_BEEN_STOP = -2;
	QObject* m_callback_executer;
	std::set<int>m_important_tasks;
	std::atomic<int> m_uuid_maker = 0;

	std::shared_mutex m_raw_mutex;//注意,这个先锁 
	std::map<int, std::pair<std::function<void()>, std::function<void()>>> m_raw_tasks;
	int m_raw_status = THREAD_IDLE;

	std::shared_mutex m_boiled_mutex;//注意,这个后锁 
	std::map<int, std::function<void()>> m_boiled_tasks;
	int m_boiled_status = THREAD_IDLE;

	Qt::HANDLE m_child_ThreadID;
	bool m_is_async_delete = false;
	int m_delete_task_uuid;
protected:
	void run();
signals:
	void please_execute_callback(int uuid);//请勿使用，这个信号是类内部使用的
	void delete_finish();//当调用Async_delete异步删除接口后，准备删除工作完成时会发送这个信号
};
