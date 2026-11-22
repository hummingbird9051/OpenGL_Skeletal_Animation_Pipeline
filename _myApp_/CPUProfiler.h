#pragma once
#include<chrono>
#include<vector>

enum CPULabels { CPUBoneUpdate, CPUFrame, CPUSubmit, CPULabelCount };

class CPUProfiler {
public:
	CPUProfiler(CPULabels label);
	~CPUProfiler();
	static void printPeriod(CPULabels label, std::chrono::duration<double, std::milli> dur);
	static double getAverage(int label);

private:
	std::chrono::steady_clock::time_point start;
	CPULabels label;
};