#pragma once
#include<glad/glad.h>
#include<chrono>

enum GPULabels {GPURendering, GPULabelCount};
static const int GPUqueriesPerLabel = 10;

class GPUProfiler {
public:
	GPUProfiler();
	~GPUProfiler();

	void beginQuery(GPULabels queryId);
	void endQuery(GPULabels queryId);
	void printQuery();
	void nextFrame();
	double getAverage(int label);

private:
	GLuint queries[GPULabelCount][GPUqueriesPerLabel];
	double querySum[GPULabelCount]{ 0.0 };
	int framePerTick[GPULabelCount]{ 0 };
	std::chrono::steady_clock::time_point lastUpdate;
	int frameCount = 0;
	double averages[GPULabelCount]{ 0 };
};