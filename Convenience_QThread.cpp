////////1664667894@qq.com|Dai Yorki
#include "Convenience_QThread.h"
using namespace std;
Convenience_QThread::~Convenience_QThread()
{
	if (m_is_async_delete == false)
	{
		cancel_all_task();
		disconnect(m_callback_executer);
		delete m_callback_executer;
		wait(); // 等待线程结束再删除线程
	}

}
Convenience_QThread::Convenience_QThread(QThread* parent) :QThread(parent) {
	m_callback_executer = new QObject();
	m_child_ThreadID = this->currentThreadId();
	m_callback_executer->connect(this, &Convenience_QThread::please_execute_callback, m_callback_executer, [=]() {
		while (1)
		{
			//1.主线程前检查
			m_boiled_mutex.lock();
			auto first_element = m_boiled_tasks.begin();
			if (first_element == m_boiled_tasks.end())
			{
				m_boiled_status = THREAD_IDLE;
				m_boiled_mutex.unlock();
				return;
			}
			if ( m_boiled_status == TASK_NEED_TO_BEEN_STOP)
			{
				m_boiled_tasks.erase(first_element);
				m_boiled_status = THREAD_IDLE;
				m_boiled_mutex.unlock();
				return;
			}
			int task_ID = first_element->first;
			std::function<void()>main_thread_task = first_element->second;
			m_boiled_tasks.erase(first_element);
			m_boiled_status = task_ID;
			m_boiled_mutex.unlock();
			//1.主线程前检查结束
			main_thread_task();
			//2.主线程后检查
			m_boiled_mutex.lock();
			m_boiled_status = THREAD_IDLE;
			m_boiled_mutex.unlock();
			//2.主线程后检查结束
		}
		});
}
int  Convenience_QThread::async_task(std::function<void()>order1, std::function<void()> order2)
{
	m_raw_mutex.lock();
	m_uuid_maker++;
	m_raw_tasks.insert(make_pair(m_uuid_maker.load(), make_pair(order1, order2)));
	m_raw_mutex.unlock();
	start();
	return  m_uuid_maker.load();
}
int  Convenience_QThread::sync_task(std::function<void()>order1, std::function<void()> order2)
{
	m_uuid_maker++;
	order1();
	order2();
	return m_uuid_maker.load();
}
void Convenience_QThread::run()
{
	while (1)
	{
		//1.子线程前检查
		m_raw_mutex.lock();
		auto first_element = m_raw_tasks.begin();

		if (first_element == m_raw_tasks.end() ){
			m_raw_status = THREAD_IDLE;
				m_raw_mutex.unlock();
				return;
		}
		m_raw_tasks.erase(first_element);
		int  taskID = first_element->first;
		if ( m_raw_status == TASK_NEED_TO_BEEN_STOP)
		{
			m_raw_status = THREAD_IDLE;
			m_raw_mutex.unlock();
			if (m_is_async_delete && taskID == m_delete_task_uuid)
				emit delete_finish();
			return;
		}
		std::function<void()>child_thread_task = first_element->second.first;
		std::function<void()>main_thread_task = first_element->second.second;
		
		m_raw_status = taskID;
		m_raw_mutex.unlock();
		//1.子线程前检查结束
		child_thread_task();
		//2.子线程后检查

		m_raw_mutex.lock();
		if (m_raw_status < 0) {
			m_raw_status = THREAD_IDLE;
			m_raw_mutex.unlock();
			if (m_is_async_delete && taskID == m_delete_task_uuid)
				emit delete_finish();
			return;

		}
		m_raw_status = THREAD_IDLE;
		m_raw_mutex.unlock();
		//2.子线程后检查结束

		//3.插入主线程
		m_boiled_mutex.lock();
		m_boiled_tasks.insert(make_pair(taskID, main_thread_task));
		m_boiled_mutex.unlock();
		emit please_execute_callback(taskID);
		if (m_is_async_delete && taskID == m_delete_task_uuid)
			emit delete_finish();
	}
}
void Convenience_QThread::Async_delete() {
	m_is_async_delete = true;
	cancel_all_task();
	disconnect(m_callback_executer);
	delete m_callback_executer;
	m_delete_task_uuid = m_uuid_maker.load() + 1;
	async_task([]() {}, []() {});
}
void  Convenience_QThread::add_important_mark(int uuid) {
	m_raw_mutex.lock();
	m_boiled_mutex.lock();
	m_important_tasks.insert(uuid);
	m_boiled_mutex.unlock();
	m_raw_mutex.unlock();
};
void Convenience_QThread::cancel_task(int uuid) {
	m_raw_mutex.lock();
	m_raw_tasks.erase(uuid);
	if (m_raw_status == uuid) m_raw_status = TASK_NEED_TO_BEEN_STOP;
	m_raw_mutex.unlock();

	m_boiled_mutex.lock();
	m_boiled_tasks.erase(uuid);
	if (m_boiled_status == uuid) m_boiled_status = TASK_NEED_TO_BEEN_STOP;
	m_boiled_mutex.unlock();
}
void Convenience_QThread::cancel_all_task() {
	m_raw_mutex.lock();
	if (m_raw_status >= 0 && m_important_tasks.find(m_raw_status) == m_important_tasks.end())m_raw_status = TASK_NEED_TO_BEEN_STOP;
	for (auto it = m_raw_tasks.begin(); it != m_raw_tasks.end(); ) {
		if (m_important_tasks.find(it->first) == m_important_tasks.end()) {
			it = m_raw_tasks.erase(it);
		}
		else {
			++it;
		}
	}
	m_raw_mutex.unlock();

	m_boiled_mutex.lock();
	if (m_boiled_status >= 0 && m_important_tasks.find(m_boiled_status) == m_important_tasks.end())m_boiled_status = TASK_NEED_TO_BEEN_STOP;
	for (auto it = m_boiled_tasks.begin(); it != m_boiled_tasks.end(); ) {
		if (m_important_tasks.find(it->first) == m_important_tasks.end()) {
			it = m_boiled_tasks.erase(it);
		}
		else {
			++it;
		}
	}
	m_boiled_mutex.unlock();
}
Qt::HANDLE current_thread_ID() { return QThread::currentThreadId(); }
bool  Convenience_QThread::Convenience_QThread::check_eligible() {
	if (current_thread_ID() == m_child_ThreadID)
	{
		if (m_boiled_status >= 0)return true;
		return false;
	}
	else
	{
		if (m_raw_status >= 0)return true;
		return false;
	}
}
