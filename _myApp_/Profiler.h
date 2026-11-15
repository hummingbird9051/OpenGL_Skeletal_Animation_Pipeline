#pragma once
#include<chrono>
#include<vector>

enum Labels { BoneUpdate, Rendering, LabelCount };

class Profiler {
public:
	Profiler(Labels label);
	~Profiler();
	static void printPeriod(Labels label, std::chrono::duration<double, std::milli> dur);

private:
	std::chrono::steady_clock::time_point start;
	Labels label;
};