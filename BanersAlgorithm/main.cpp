// ģ��ʵ�����м��㷨
// ʹ��C++ʵ�֣�std=c++11

#include <array>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <string>
#include <sstream>
#include <iostream>

using namespace std;

// ��������ĺ���
string id_name(thread::id& id) {
	ostringstream sout;
	sout << id;
	return sout.str();
}
// �����м��㷨�ķ�װ
class Allocator final {
	friend class Resource;
private:
	using List = array<int, 3>;
	using Map = map<thread::id, List>;

	static List available;
	static Map max;
	static Map allocation;
	static Map need;

	static mutex allocatorMutex;

	thread::id id;
public:
	static void initial(int n1, int n2, int n3) {
		unique_lock<mutex> lock(allocatorMutex);
		available[0] = n1;
		available[1] = n2;
		available[2] = n3;
	}
	// ���ø��߳̿ɷ������Դ�������Ŀ
	Allocator(int n1, int n2, int n3) {
		unique_lock<mutex> lock(allocatorMutex);
		id = this_thread::get_id();
		auto& maxList = max[id];
		maxList[0] = n1;
		maxList[1] = n2;
		maxList[2] = n3;
		auto& needList = need[id];
		needList = maxList;
		printf("�̺߳ţ�%s,���������Ҫ�������Դ����Ϊ��%d,%d,%d\n\n", id_name(id).c_str(), n1, n2, n3);
	}
	~Allocator() {
		unique_lock<mutex> lock(allocatorMutex);
		printf("�̺߳ţ�%s,�̲߳�����Ҫ��Դ\n\n", id_name(id).c_str());
		max.erase(id);
		allocation.erase(id);
		need.erase(id);
	}
private:
	void release(List& resource) {
		unique_lock<mutex> lock(allocatorMutex);
		printf("�̺߳ţ�%s���ͷŲ�����Դ���ͷŵ���ĿΪ��%d,%d,%d\n\n", id_name(id).c_str(), resource[0], resource[1], resource[2]);
		add(available, resource);
		sub(allocation[id], resource);
		add(need[id], resource);
	}
	// ������Դ���ɹ�����true����ʱʧ�ܷ���false
	// ��������ܳɹ����׳��쳣
	bool request(List& request) {
		id = this_thread::get_id();
		unique_lock<mutex> lock(allocatorMutex);
		printf("�̺߳ţ�%s,���������Դ����������Ϊ��%d,%d,%d\n", id_name(id).c_str(), request[0], request[1], request[2]);
		if (!less(request, need[id])) {
			throw exception("������Ŀ���������Ŀ������ʧ��\n\n");
		} else if (!less(request, available)) {
			printf("�ܾ�������Դ���������������ɷ�������\n");
			printf("��������Ϊ��%d,%d,%d\n\n", available[0], available[1], available[2]);
			return false;
		} else {
			List availableCopy = available;
			Map allocationCopy = allocation;
			Map needCopy = need;

			sub(availableCopy, request);
			add(allocationCopy[id], request);
			sub(needCopy[id], request);
			if (safety(availableCopy, allocationCopy, needCopy)) {
				available = move(availableCopy);
				allocation = move(allocationCopy);
				need = move(needCopy);
				printf("ͨ����ȫ�Լ�飬��Դ���Է���\n\n");
				return true;
			} else {
				printf("δ��ͨ����ȫ�Լ�飬�ܾ�������Դ���߳���ʱ����\n\n");
				return false;
			}
		}
	}
	bool less(List& l, List& r) {
		return l[0] <= r[0] && l[1] <= r[1] && l[2] <= r[2];
	}
	void add(List& l, List& r) {
		for (int i = 0; i < l.size(); ++i) {
			l[i] += r[i];
		}
	}
	void sub(List& l, List& r) {
		for (int i = 0; i < l.size(); ++i) {
			l[i] -= r[i];
		}
	}
	bool safety(List work, Map& allocation, Map& need) {
		map<thread::id, bool> finish;
		for (auto& elem : need) {
			auto id = elem.first;
			finish[id] = false;
		}
		// ����ṹ������֣�֮���л������ع�
		int i = 0;	// ͳ������ɵĸ���
		while (select_unfinish(finish, work, need)) {
			++i;
		}
		if (i < finish.size()) {
			return false;
		}
		else {
			return true;
		}
	}
	bool select_unfinish(map<thread::id, bool>& finish, List& work, Map& need) {
		for (auto& elem : need) {
			auto& id = elem.first;
			auto& list = elem.second;

			if (finish[id] == false && less(list, work)) {
				add(work, allocation[id]);
				finish[id] = true;
				return true;
			}
		}
		return false;
	}
};
// ��Դ�ĳ���ʹ��RAIIģʽ����
class Resource final {
private:
	Allocator& allocator;
	Allocator::List resource;
public:
	Resource(Allocator& allocator, int n1, int n2, int n3) : allocator(allocator) {
		Allocator::List request{ n1, n2, n3 };
		while (!allocator.request(request)) {
			this_thread::sleep_for(0.5s);
		}
		resource = move(request);
	}
	~Resource() {
		allocator.release(resource);
	}
};

Allocator::List Allocator::available;
Allocator::Map Allocator::max;
Allocator::Map Allocator::allocation;
Allocator::Map Allocator::need;
mutex Allocator::allocatorMutex;

int main() {
	Allocator::initial(10, 5, 7);
	thread t0([]()->void{
		Allocator allocator(6, 4, 3);
		Resource r1(allocator, 0, 1, 0);
		Resource r2(allocator, 0, 2, 0);
		if (time(0) % 10 > 5) {
			Resource r3(allocator, 6, 0, 3);
		} else {
			Resource r3(allocator, 5, 1, 2);
		}
	});
	thread t1([]()->void {
		Allocator allocator(3, 2, 2);
		Resource r1(allocator, 2, 0, 0);
		Resource r2(allocator, 1, 0, 2);
		for (int i = 0; i < 3; ++i) {
			Resource r3(allocator, 0, 2, 0);
		}
	});
	thread t2([]()->void {
		Allocator allocator(3, 0, 2);
		Resource r1(allocator, 3, 0, 2);
	});
	thread t3([]()->void {
		Allocator allocator(2, 1, 1);
		Resource r1(allocator, 2, 1, 1);
	});
	thread t4([]()->void {
		Allocator allocator(3, 3, 3);
		Resource r1(allocator, 0, 0, 2);
		Resource r2(allocator, 3, 3, 0);
	});

	t0.join();
	t1.join();
	t2.join();
	t3.join();
	t4.join();

	return 0;
}