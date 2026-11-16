#include "CPUProfiler.h"
#include<iostream>


static const char* labelName[CPULabelCount] = { "CPUBoneUpdate", "CPURendering" };

struct Status {
	std::chrono::duration<double, std::milli> period{ 0 };
	std::chrono::duration<double, std::milli> max_period{ 0 };
	int count = 0;
};

static Status stats[CPULabelCount];

static std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();

CPUProfiler::CPUProfiler(CPULabels label) {
	start = std::chrono::steady_clock::now();
	this->label = label;
}

CPUProfiler::~CPUProfiler() {
	std::chrono::duration<double, std::milli> dur = std::chrono::steady_clock::now() - start;
	printPeriod(label, dur);
}

void CPUProfiler::printPeriod(CPULabels label, std::chrono::duration<double, std::milli> dur) {
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	stats[label].period += dur;
	stats[label].count++;
	stats[label].max_period = stats[label].max_period < dur ? dur : stats[label].max_period;
	if (now - lastUpdate > std::chrono::seconds(1)) {
		for (int i = 0; i < CPULabelCount; i++) {
			if (stats[i].count == 0) continue;
			std::cout << labelName[i] << "'s average period : " << (stats[i].period.count() / stats[i].count) << std::endl;
			stats[i].count = 0;
			stats[i].period = std::chrono::duration<double, std::milli>(0);
			lastUpdate = now;
			stats[i].max_period = std::chrono::duration<double, std::milli>(0);
		}
		
	}
}