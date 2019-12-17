#include <iostream>
#include "windows.h"
#include <map>
#include <functional>
#include "utils.h"
#include <sstream>

#include "process.h"
#include "time.h"

#include <algorithm>

#include <thread>
#include <mutex>
#include <fstream>

using std::string;
using std::cout;
using std::cin;
using std::endl;

std::tuple<string, std::vector<string>> parse_line(string& line) {

	string command = line.substr(0, line.find(' '));
	std::vector<string> args;
	if (command.size() != line.size())
		args = util::split(line.substr(command.size() + 1), ' ');

	return std::make_tuple(command, args);
}

const int max_priority = 5;
const int max_cpu_burst = 5;


int next_pid = 0;

std::vector<Process> processes;

bool trace = false;

bool running = false;
std::mutex mutex;

int current = 0;
int prev = 0;

void tick() {
	std::ofstream fout(std::string("last_session_log")+".txt");
	
	int tick_count = 0;

	while (true) {

		if (running && processes.size()) {
			mutex.lock();
				
			//сортировать по приоритету -> [0 0 1 1 1 2 2 3 3 4 4 4 4]
			std::sort(processes.begin(), processes.end(), [](Process p1, Process p2) {return p1.priority < p2.priority; });

			//динамические приоритеты -> повысить приоритет у процесса с самым низким приоритетом каждые Size тиков
			if (tick_count >= processes.size()) {
				processes[processes.size() - 1].priority = max(0, processes[processes.size() - 1].priority - 1);
				std::sort(processes.begin(), processes.end(), [](Process p1, Process p2) {return p1.priority < p2.priority; });
				tick_count = 0;
			}
			//перейти в начало когда дошел до границы приоритета
			if (current > processes.size() - 1 || processes[current].priority != processes[0].priority) {
				current = 0;
				prev = 0;
			}
			//вывод в файл лога
			fout << "-------------------------------------" << endl;
			for (auto& process : processes) {
				fout << "priority: " << process.priority;
				fout << "\tcpu_burst: " << process.cpu_burst << "  \tpid: " << process.pid << "  \tppid: " << process.ppid;
				if (current <= processes.size() && process.pid == processes[current].pid) {
					if (trace) { 
						cout << "-------------------------------------" << endl;
						cout << "priority: " << process.priority;
						cout << "\tcpu_burst: " << process.cpu_burst << "  \tpid: " << process.pid << "  \tppid: " << process.ppid;
						cout << " \tR\n";
					}
					fout << " \tR";
				}
				else
					fout << " \tS";
				fout << endl;
			}
			//выполнить и убить процесс если завершился
			prev = current;
			processes[current].cpu_burst = max(0, processes[current].cpu_burst - 1);//do
			if (processes[current].cpu_burst == 0) {
				processes.erase(processes.begin() + current);
				prev = 0;
				if (prev == current && !processes.empty() && processes[current].priority == processes[prev].priority) {
					current = -1;//на начале шага будет 0
				}
			}
			
			current++;
			
			mutex.unlock();

		}
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		tick_count++;
	}
	fout.close();
}

int main() {
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);
	setlocale(0, "rus");

	std::thread t(tick);
	t.detach();

	static std::map<std::string, std::function<void(std::vector<std::string>)>> cmds;
	cmds["exit"] = [](std::vector<string> args) {
		exit(0);
	};
	cmds["clear"] = [](std::vector<string> args) {
		system("cls");
	};
	cmds["ps"] = [](std::vector<string> args) {
		mutex.lock();
		if(!processes.empty())
			std::sort(processes.begin(), processes.end(), [](Process p1, Process p2) {return p1.priority < p2.priority; });
		cout << "-------------------------------------" << endl;
		for (auto& process : processes) {
			cout << "priority: " << process.priority;
			cout << "\tcpu_burst: " << process.cpu_burst << "  \tpid: " << process.pid << "  \tppid: " << process.ppid;
			if (current < processes.size() && process.pid == processes[current].pid)
				cout << " \tR";
			else
				cout << " \tS";
			cout << endl;
		}
		mutex.unlock();
	};

	cmds["kill"] = [](std::vector<string> args) {
		if (args.size()) {
			int pid = stoi(args[0]);
			std::remove_if(processes.begin(), processes.end(), [&pid](Process p) { return (p.ppid == pid) || (p.pid == pid); });
		}
	};

	cmds["nice"] = [](std::vector<string> args) {
		if (args.size()>1) {
			int pid = stoi(args[0]);
			int pr = stoi(args[1]);
			auto a = std::find_if(processes.begin(), processes.end(), [&pid, &pr](Process& p) {
				if(p.pid == pid)
					p.priority = pr;
				return p.pid == pid; 
			});
		}
	};

	cmds["trace"] = [](std::vector<string> args) {
		if (trace) {
			trace = false;
			cout << "трассировка выключена" << endl;
		}
		else {
			trace = true;
			cout << "трассировка включена" << endl;
		}
	};

	cmds["add"] = [](std::vector<string> args) {
		if (!args.empty() && args[0]=="rand") {
			int count = 1;
			if (args.size() > 1)
				count = std::stoi(args[1]);

			for (int i = 0; i < count; i++) {
				Process p;
				p.priority = (rand() % max_priority);
				p.pid = next_pid++;

				if (rand() % 10 == 0 && processes.size()!=0) {
					p.ppid = processes[rand() % processes.size()].pid;
				}
				else {
					p.ppid = -1;
				}
				p.cpu_burst = (rand() % max_cpu_burst) + 1;

				processes.push_back(p);
			}
		}
		else {
			Process p;
			cout << "priority: ";
			cin >> p.priority;
			if (p.priority > max_priority)
				p.priority = max_priority-1;

			p.pid = next_pid++;

			cout << "ppid: ";
			cin >> p.ppid;
			if (processes.empty())
				p.ppid = -1;
			for (auto& item : processes)
				if (item.pid == p.ppid)
					p.ppid = -1;

			cout << "cpu_burst: ";
			cin >> p.cpu_burst;
			while (getchar() != '\n');
			if (p.cpu_burst > max_cpu_burst)
				p.cpu_burst = max_cpu_burst - 1;
			processes.push_back(p);
		}
	};

	cmds["start"] = [](std::vector<string> args) {
		running = true;
	};
	cmds["stop"] = [](std::vector<string> args) {
		running = false;
	};

	cmds["help"] = [](std::vector<string> args) {
		cout << "start - запустить выполнение планировщика" << endl;
		cout << "stop - остановить выполнение планировщика" << endl;
		cout << "add <rand> <100> - добавить задачи" << endl;
		cout << "kill <pid> - убить задачу pid" << endl;
		cout << "nice <pid> <priority> - установить процессу pid приоритет prity" << endl;
		cout << "ps - получить список задач" << endl;
		cout << "trace - трасировка на консоль во время выполнения" << endl;
		cout << "exit - выход из прогаммы" << endl;
		cout << "clear - очистить окно" << endl;
	};

	srand(time(0));
	string line;
	while (true) {
		cout << "$ ";
		std::getline(std::cin, line);

		auto [command, args] = parse_line(line);

		bool worked = false;
		for (auto& it : cmds) {
			if (it.first == command) {
				worked = true;
				it.second(args);
				break;
			}
		}
		if (!worked && command !="") {
			cout << "Не найдена комманда " << command << endl;
		}

	}

	return(0);
}
