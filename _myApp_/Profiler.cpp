#include "Profiler.h"
#include<iostream>


static const char* labelName[LabelCount] = { "BoneUpdate", "Rendering" };

struct Status {
	std::chrono::duration<double, std::milli> period{ 0 };
	std::chrono::duration<double, std::milli> max_period{ 0 };
	int count = 0;
};

static Status stats[LabelCount];

static std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();

Profiler::Profiler(Labels label) {
	start = std::chrono::steady_clock::now();
	this->label = label;
}

Profiler::~Profiler() {
	std::chrono::duration<double, std::milli> dur = std::chrono::steady_clock::now() - start;
	printPeriod(label, dur);
}

void Profiler::printPeriod(Labels label, std::chrono::duration<double, std::milli> dur) {
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	stats[label].period += dur;
	stats[label].count++;
	stats[label].max_period = stats[label].max_period < dur ? dur : stats[label].max_period;
	if (now - lastUpdate > std::chrono::seconds(1)) {
		for (int i = 0; i < LabelCount; i++) {
			std::cout << labelName[i] << "'s average period : " << (stats[i].period.count() / stats[i].count) << std::endl;
			stats[i].count = 0;
			stats[i].period = std::chrono::duration<double, std::milli>(0);
			lastUpdate = now;
			stats[i].max_period = std::chrono::duration<double, std::milli>(0);
		}
		
	}
}